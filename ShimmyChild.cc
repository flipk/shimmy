
#define SHIMMY_INTERNAL 1
#include "Shimmy.h"
#include <stdio.h>
#include <iostream>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

/////////////////////////////// ShimmyChild ///////////////////////////////

ShimmyChild::ShimmyChild(void)
{
    running = false;
}

ShimmyChild::~ShimmyChild(void)
{
    if (running)
        stop();
}

struct fd_pair
{
    int fds[2];
    inline fd_pair(void) { fds[0] = fds[1] = -1; }
    inline bool init(const char *funcname) {
        if (::pipe(fds) < 0) {
            int e = errno;
            char * errmsg = strerror(e);
            fprintf(stderr,
                    "%s: unable to create pipe: %d (%s)\n",
                    funcname, e, errmsg);
            fds[0] = fds[1] = -1;
            return false;
        }
        return true;
    }
    inline void close(void) {
        if (fds[0] != -1)
            ::close(fds[0]);
        if (fds[1] != -1)
            ::close(fds[1]);
    }
    inline int  read_end(void) { return fds[0]; }
    inline int write_end(void) { return fds[1]; }
};

bool
ShimmyChild::start(const std::string &path)
{
    fd_pair fds_from_child;
    fd_pair fds_to_child;
    fd_pair rendezvous_fds;

    if (!fds_from_child.init("ShimmyChild::start"))
        return false;
    if (!fds_to_child.init("ShimmyChild::start"))
    {
        fds_from_child.close();
        return false;
    }
    if (!rendezvous_fds.init("ShimmyChild::start"))
    {
        fds_from_child.close();
        fds_to_child.close();
        return false;
    }

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
    if (pid < 0)
    {
        int e = errno;
        char * errmsg = strerror(e);
        fds_from_child.close();
        fds_to_child.close();
        rendezvous_fds.close();
        fprintf(stderr, "ShimmyChild::start: cannot fork: %d (%s)\n",
                e, errmsg);
        return false;
    }
    if (pid == 0)
    {
        // child

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
        int e = errno;
        if (::write(rendezvous_fds.write_end(),
                    &e, sizeof(int)) != sizeof(int))
        {
            // we can't call printf or cerr cuz !async-signal-safe
            write(2, "ShimmyChild::start: CANT WRITE ERRNO TO PARENT\n",
                  strlen("ShimmyChild::start: CANT WRITE ERRNO TO PARENT\n"));
        }

        // internet says to avoid calling any
        // registered atexit(3) handlers and just die already.
        _exit(99);
    }
    // else, parent

    // close pipe ends we won't use.
    close(fds_to_child.read_end());
    close(fds_from_child.write_end());
    close(rendezvous_fds.write_end());
    int e;
    int readRet = ::read(rendezvous_fds.read_end(), &e, sizeof(e));
    close(rendezvous_fds.read_end());
    if (readRet == sizeof(e))
    {
        // the exec failed!
        char * errmsg = strerror(e);
        // cleanup stuff no longer needed.
        close(fds_to_child.write_end());
        close(fds_from_child.read_end());
        fprintf(stderr, "ShimmyChild::start: exec of '%s' failed: %d (%s)\n",
                path.c_str(), e, errmsg);
        return false;
    }

    read_fd = fds_from_child.read_end();
    write_fd = fds_to_child.write_end();

    pFis = new google::protobuf::io::FileInputStream(read_fd);
    pFos = new google::protobuf::io::FileOutputStream(write_fd);

    running = true;
    return true;
}

void
ShimmyChild::stop(void)
{
    if (running == false)
        return;

    // this is supposed to indicate to the child that it
    // is now supposed to die.
    delete pFos;
    pFos = NULL;
    close(write_fd);
    write_fd = -1;
    printf("parent: closed write pipe\n");

    // wait for it to die, where it EOF's the pipe back to us.
    delete pFis;
    pFis = NULL;
    char buf[100];
    printf("parent: blocking in read\n");
    while (::read(read_fd, buf, sizeof(buf)) > 0)
        ;
    close(read_fd);
    read_fd = -1;
    printf("parent: read pipe closed\n");

    // collect the dead zombie.
    int wstatus = 0;
    waitpid(pid, &wstatus, 0);
    running = false;

    printf("parent: child wstatus = %d\n", wstatus);

}
