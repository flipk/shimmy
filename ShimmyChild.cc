
#include "Shimmy.h"
#include <fcntl.h>
#include <sys/wait.h>

namespace Shimmy {

Child :: Child(void)
{
    pid = -1;
    running = false;
}

Child :: ~Child(void)
{
    if (running)
        stop();
}

bool
Child :: start(const std::string &path)
{
    Lock l(this);
    int e = 0;

    fd_pair fds_from_child;
    fd_pair fds_to_child;
    fd_pair rendezvous_fds;

    // if a closer pipe was left around by a previous child,
    // clean it up here.
    closer_pipe.close();
    if (fds_from_child.make("Shimmy::Child::start") == false)
        return false;
    if (fds_to_child.make("Shimmy::Child::start") == false)
        return false;
    if (rendezvous_fds.make("Shimmy::Child::start") == false)
        return false;

    // tell the child (ShimmyParent class) what fds it can write to
    // and read from for protobuf msgs.
    char fds_str[20];
    snprintf(fds_str, sizeof(fds_str), "%d:%d",
             fds_to_child.read_end(), fds_from_child.write_end());
    // do the setenv before the fork, because setenv is not on the list
    // of POSIX.1 async-signal-safe functions (see signal-safety(7)).
    setenv(SHIMMY_FDS_ENV_VAR, fds_str, /*overwrite*/ 1);

    // when yer fork'n a manythreaded process, the
    // child ain't good fer much but exec'n.
    pid = fork();
    e = errno;

    if (pid < 0)
    {
        char * errmsg = strerror(e);
        fds_from_child.close();
        fds_to_child.close();
        rendezvous_fds.close();
        fprintf(stderr, "Shimmy::Child::start: cannot fork: %d (%s)\n",
                e, errmsg);
        return false;
    }

    if (pid == 0)
    {
        // child

        fds_to_child.close_write_end();
        fds_from_child.close_read_end();
        rendezvous_fds.close_read_end();

        // close ALL fds we don't need to avoid passing them to
        // the child which doesn't even know they exist and will
        // thus never clean them up.
        // (keep 0,1,2 for stdin, stdout, stderr)
        for (int fd = 3; fd < (int)sysconf(_SC_OPEN_MAX); fd++)
        {
            if (fd != fds_to_child.read_end()     &&
                fd != fds_from_child.write_end()  &&
                fd != rendezvous_fds.write_end()  )
            {
                close(fd);
            }
        }

        // mark the rendezvous pipe as close-on-exec,
        // so if the exec succeeds, the parent gets a zero
        // read, but if it fails, it gets a sizeof(errno) read.
        fcntl(rendezvous_fds.write_end(), F_SETFD, FD_CLOEXEC);

        // try to exec the binary specified.
        execl( path.c_str(), path.c_str(), NULL );

        // if the execl fubard, save the errno;
        // we cain't print nuthin, on accounta some stuff,
        // so send it to poppa for print'n.
        e = errno;
        if (::write(rendezvous_fds.write_end(),
                    &e, sizeof(int)) != sizeof(int))
        {
            // we can't call printf or cerr cuz !async-signal-safe
            int r =
                write(2, "ShimmyChild::start: CANT WRITE ERRNO TO PARENT\n",
                  strlen("ShimmyChild::start: CANT WRITE ERRNO TO PARENT\n"));
            if (r < 0)
            { /* what to do? just quiet the compiler. */ }
        }

        // internet says to avoid calling any
        // registered atexit(3) handlers and just die already.
        _exit(99);
    }
    // else, parent

    // the setenv was for the child. parent doesn't need it anymore.
    unsetenv(SHIMMY_FDS_ENV_VAR);

    // close pipe ends we won't use.
    fds_to_child.close_read_end();
    fds_from_child.close_write_end();
    rendezvous_fds.close_write_end();

    e = 0;
    int readRet = ::read(rendezvous_fds.read_end(), &e, sizeof(e));
    close(rendezvous_fds.read_end());
    if (readRet == sizeof(e))
    {
        // the exec failed!
        char * errmsg = strerror(e);
        fprintf(stderr, "ShimmyChild::start: exec of '%s' failed: %d (%s)\n",
                path.c_str(), e, errmsg);

        // collect the dead zombie.
        int wstatus = 0;
        waitpid(pid, &wstatus, 0);
        print_wait_status(wstatus);

        // cleanup stuff no longer needed.
        fds_to_child  .close();
        fds_from_child.close();
        rendezvous_fds.close();
        return false;
    }

    if (closer_pipe.make("Shimmy::Child::start") == false)
    {
        // cleanup stuff no longer needed.
        fds_to_child  .close();
        fds_from_child.close();
        rendezvous_fds.close();
        return false;
    }

    child_fds.fds[0] = fds_from_child.take_read_end();
    child_fds.fds[1] = fds_to_child  .take_write_end();

    pFis = new google::protobuf::io::FileInputStream (child_fds.fds[0]);
    pFos = new google::protobuf::io::FileOutputStream(child_fds.fds[1]);

    running = true;
    return true;
}

void
Child :: stop(int shutdown_time)
{
    Lock l(this);

    if (running == false)
        return;

    // this is supposed to indicate to the child that it
    // is now supposed to die.
    delete pFos;
    pFos = NULL;
    child_fds.close_write_end();
    delete pFis;
    pFis = NULL;

    // if a thread on this end is currently blocked in get_msg,
    // signal it to wake up and return false.
    char c = 1;
    if (::write(closer_pipe.write_end(), &c, 1) != 1)
        fprintf(stderr, "Shimmy::Child::stop: unable to write closer pipe\n");

    // wait for the child to exit and close the pipe back to us.
    bool do_wait = true;
    if (!waitfor_closure(child_fds.read_end(), shutdown_time))
    {
        printf("Shimmy::Child::stop: ESCALATING to SIGTERM\n");
        kill(pid, SIGTERM);
        if (!waitfor_closure(child_fds.read_end(), shutdown_time))
        {
            printf("Shimmy::Child::stop: ESCALATING to SIGKILL\n");
            kill(pid, SIGKILL);
            if (!waitfor_closure(child_fds.read_end(), shutdown_time))
            {
                fprintf(stderr, "Shimmy::Child::stop: FAILURE TO KILL\n");
                // don't hang up this thread blocking in waitpid for
                // a zombie that may never happen.
                do_wait = false;
            }
        }
    }

    child_fds.close_read_end();

    if (do_wait)
    {
        // collect the dead zombie.
        int wstatus = 0;
        waitpid(pid, &wstatus, 0);
        print_wait_status(wstatus);
    }

    running = false;

    // don't close the closer_pipe here; give any reader thread
    // currently blocked in get_msg a chance to process it and wake
    // up.
    // it's okay, because if Shimmy::Child is deleted, the closer pipe
    // will be cleaned up then; or if the user wants to ::start a new
    // one, the closer pipe will be cleaned up there and a new one
    // created.
}

bool
Child::waitfor_closure(int fd, int shutdown_time)
{
    char buf[20];
    while (1)
    {
        pxfe_select sel;
        sel.rfds.set(fd);
        sel.tv.set(shutdown_time,0);
        if (sel.select() == 0)
        {
            fprintf(stderr, "Shimmy::Child::stop: child did not close\n");
            return false;
        }
        if (sel.rfds.is_set(fd))
        {
            int r = ::read(fd, buf, sizeof(buf));
            if (r == 0)
                return true;
            int e = errno;
            if (r < 0)
            {
                fprintf(stderr, "Shimmy::Child::stop: read: %d (%s)\n",
                        e, strerror(e));
                return false;
            }
        }
    }
}

void
Child :: print_wait_status(int wstatus)
{
    if (WIFEXITED(wstatus))
    {
        printf("Shimmy::Child: exited with status %d\n",
               WEXITSTATUS(wstatus));
    }
    if (WIFSIGNALED(wstatus))
    {
        fprintf(stderr, "Shimmy::Child: terminated with signal %d%s\n",
                WTERMSIG(wstatus),
                WCOREDUMP(wstatus) ? " (core dumped)" : "");
    }
}

};
