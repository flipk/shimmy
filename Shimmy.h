
#ifndef __SHIMMY_H__
#define __SHIMMY_H__ 1

#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <string>
#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

namespace Shimmy {

static inline int get_tid(void ) { return syscall(SYS_gettid); }

struct fd_pair {
    int fds[2];
    fd_pair(void) { fds[0] = fds[1] = -1; }
    ~fd_pair(void) { close(); }
    bool make(const char *func)
    {
        if (::pipe(fds) < 0)
        {
            int e = errno;
            const char *msg = strerror(e);
            fprintf(stderr, "Shimmy::%s: ERROR making pipe: %d (%s)\n",
                    func, e, msg);
            return false;
        }
        return true;
    }
    void close(void)
    {
        if (fds[0] != -1)
        {
            ::close(fds[0]);
            fds[0] = -1;
        }
        if (fds[1] != -1)
        {
            ::close(fds[1]);
            fds[1] = -1;
        }
    }
    int  read_end(void) const { return fds[0]; }
    int write_end(void) const { return fds[1]; }
    int take_read_end (void) { int fd = fds[0]; fds[0] = -1; return fd; }
    int take_write_end(void) { int fd = fds[1]; fds[1] = -1; return fd; }
    void close_read_end(void)
    {
        if (fds[0] != -1)
        {
            ::close(fds[0]);
            fds[0] = -1;
        }
    }
    void close_write_end(void)
    {
        if (fds[1] != -1)
        {
            ::close(fds[1]);
            fds[1] = -1;
        }
    }
};

/** common methods between a Shimmy::Parent and Shimmy::Child */
class Common {
    pthread_mutex_t mutex;
    void   _lock(void) { pthread_mutex_lock  (&mutex); }
    void _unlock(void) { pthread_mutex_unlock(&mutex); }
protected:
    Common(void);
    ~Common(void);
    fd_pair closer_pipe;
    fd_pair child_fds;
    google::protobuf::io::FileInputStream  *pFis; // child_fds[0]
    google::protobuf::io::FileOutputStream *pFos; // child_fds[1]
    class Lock {
        Common *obj;
        bool locked;
    public:
        Lock(Common *_obj) : obj(_obj) { lock(); }
        ~Lock(void) { unlock(); }
        void   lock(void) { if (!locked) obj->  _lock(); locked = true ; }
        void unlock(void) { if ( locked) obj->_unlock(); locked = false; }
    };
    friend class Lock;
public:
    /** return the descriptor for the read-end.
     * this is useful if you wish to use select(2) and only
     * invoke \ref get_msg when you know there is something to service. */
    int  get_read_fd(void) const { return child_fds.fds[0]; }
    /** read the next message from the read-pipe and return it.
     * if there is no message available, this function blocks until
     * there is one, or until \ref Child::stop is called. */
    bool get_msg(google::protobuf::Message *msg);
    /** send a message through the write-pipe. */
    bool send_msg(const google::protobuf::Message *msg);
};

/** a parent instantiates a Shimmy::Child to fork a child process and
 * begin exchanging messages with it. */
class Child : public Common {
    pid_t pid;
    bool running;
    bool waitfor_closure(int fd, int shutdown_time);
    void print_wait_status(int wstatus);
public:
    Child(void);
    /** destructor; if a child exists, invokes \ref stop. */
    ~Child(void);
    /** starts a child process and opens the pipes. once this 
     * returns, the user may begin calling \ref get_msg and 
     * \ref send_msg to communicate with the child. */
    bool start(const std::string &path);
    /** stops a child process by closing the pipes and waiting
     * for the child to exit on its own. if it does not exit 
     * after a reasonable interval, escalates to TERM and KILL
     * signals to force it to die. */
    void stop(int shutdown_time = default_shutdown_time);
    static const int default_shutdown_time = 2;
};

/** a child instantiates a Shimmy::Parent to communicate with the
 * parent process. */
class Parent : public Common {
public:
    Parent(void);
    ~Parent(void);
    /** if this process was started by \ref Shimmy::Child, this method
     * will find the pipes and begin communicating. one this returns
     * true, the user may begin using \ref get_msg and \ref send_msg
     * to communicate with the parent. */
    bool init(void);
};

#define SHIMMY_FDS_ENV_VAR "SHIMMY_FDS"


// miscellaneous helpers to wrapper ugly posix boilerplate

struct pxfe_timeval : public timeval
{
    pxfe_timeval(void) { tv_sec = 0; tv_usec = 0; }
    pxfe_timeval(time_t s, long u) { set(s,u); }
    void set(time_t s, long u) { tv_sec = s; tv_usec = u; }
    timeval *operator()(void) { return this; }
};

class pxfe_fd_set {
    fd_set  fds;
    int     max_fd;
public:
    pxfe_fd_set(void) { zero(); }
    ~pxfe_fd_set(void) { /*nothing for now*/ }
    void zero(void) { FD_ZERO(&fds); max_fd=-1; }
    void set(int fd) { FD_SET(fd, &fds); if (fd > max_fd) max_fd = fd; }
    void clr(int fd) { FD_CLR(fd, &fds); }
    bool is_set(int fd) { return FD_ISSET(fd, &fds) != 0; }
    fd_set *operator()(void) { return max_fd==-1 ? NULL : &fds; }
    int nfds(void) { return max_fd + 1; }
};

struct pxfe_select {
    pxfe_fd_set  rfds;
    pxfe_fd_set  wfds;
    pxfe_fd_set  efds;
    pxfe_timeval   tv;
    /* note you will need to re-fill rfds, wfds, efds, and tv every
     * time you call one of the select methods */
    pxfe_select(void) { }
    ~pxfe_select(void) { }
    /* call select with a timeout of forever (ignore "tv" member) */
    int select_forever(int *e = NULL) {
        return _select(NULL, e);
    }
    /* call select with a timeout (using "tv" member) */
    int select(int *e = NULL) {
        return _select(tv(), e);
    }
    /* call select if you have your own timeval* you want to pass in */
    int _select(struct timeval *tvp, int *e = NULL) {
        int n = rfds.nfds(), n2 = wfds.nfds(), n3 = efds.nfds();
        if (n < n2) n = n2;
        if (n < n3) n = n3;
        int cc = ::select(n, rfds(), wfds(), efds(), tvp);
        if (cc < 0)
            if (e) *e = errno;
        return cc;
    }
};

};

#endif /* __SHIMMY_H__ */

/** \mainpage libShimmy API user's manual

This is the user's manual for the libShimmy API.

The purpose of this library is to abstract protobuf communication
between a parent process and a child it forks. A Shimmy::Child object
is used in the parent to start the child process. A pair of pipes are
created, and the file descriptors for these pipes are inherited to the
child process.  One pipe is for parent-to-child messages and the other
is for child-to-parent messages.  On the child side, a Shimmy::Parent
object looks for and attaches to the descriptors inherited to the child
by the Shimmy::Child in the parent.

Interesting pages:

<ul>
<li> \ref Shimmy::Child  object
<li> \ref Shimmy::Parent  object
<li> \ref SampleParent
<li> \ref SampleChild
</ul>

 */

/** \page SampleParent Sample code for the parent

\include testParent.cc

 */

/** \page SampleChild Sample code for the child

\include testChild.cc

 */
