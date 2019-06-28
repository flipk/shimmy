
#ifndef __SHIMMY_H__
#define __SHIMMY_H__

#include <sys/types.h>
#include <unistd.h>
#include <string>
#include <google/protobuf/message.h>

// a parent instantiates this to fork and talk to a shimmy child.
class ShimmyChild {
    pid_t pid;
    bool running;
    int fd_from_child;
    int fd_to_child;
public:
    ShimmyChild(void);
    ~ShimmyChild(void);

    bool start(const std::string &path);
    void stop(void);
    bool get_msg(google::protobuf::Message *msg);
    bool send_msg(const google::protobuf::Message *msg);
};

// a shimmy child instantiates this after being started to talk back
// to the shimmy parent.
class ShimmyParent {
    int fd_from_parent;
    int fd_to_parent;
public:
    ShimmyParent(void);
    ~ShimmyParent(void);

    bool init(void);
    bool get_msg(google::protobuf::Message *msg);
    bool send_msg(const google::protobuf::Message *msg);
};


#if SHIMMY_INTERNAL
// format of this var is ("%d:%d", read_end, write_end)
#define SHIMMY_RENDEZVOUS_ENV_VAR "SHIMMY_FDS"
#endif

#endif /* __SHIMMY_H__ */
