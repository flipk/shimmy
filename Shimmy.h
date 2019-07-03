
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
    // for use with select(2) if you want.
    int  get_read_fd(void) const { return child_fds.fds[0]; }
    bool get_msg(google::protobuf::Message *msg);
    bool send_msg(const google::protobuf::Message *msg);
};

// a parent instantiates this to fork and talk to a shimmy child.
class Child : public Common {
    pid_t pid;
    bool running;
public:
    Child(void);
    ~Child(void);
    bool start(const std::string &path);
    void stop(void);
};

// a child instantiates this to talk back to the parent, as one does.
class Parent : public Common {
public:
    Parent(void);
    ~Parent(void);
    bool init(void);
};

#define SHIMMY_FDS_ENV_VAR "SHIMMY_FDS"



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
    /** note you will need to re-fill rfds, wfds, efds, and tv every
     * time you call one of the select methods */
    pxfe_select(void) { }
    ~pxfe_select(void) { }
    /** call select with a timeout of forever (ignore "tv" member) */
    int select_forever(int *e = NULL) {
        return _select(NULL, e);
    }
    /** call select with a timeout (using "tv" member) */
    int select(int *e = NULL) {
        return _select(tv(), e);
    }
    /** call select if you have your own timeval* you want to pass in */
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
