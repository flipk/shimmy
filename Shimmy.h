
#ifndef __SHIMMY_H__
#define __SHIMMY_H__

#include <sys/types.h>
#include <unistd.h>
#include <string>
#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

class ShimmyCommon {
protected:
    int read_fd;
    google::protobuf::io::FileInputStream *pFis;
    int write_fd;
    google::protobuf::io::FileOutputStream *pFos;
    ShimmyCommon(void);
    ~ShimmyCommon(void);
public:
    bool get_msg(google::protobuf::Message *msg);
    bool send_msg(const google::protobuf::Message *msg);
};

// a parent instantiates this to fork and talk to a shimmy child.
class ShimmyChild : public ShimmyCommon {
    pid_t pid;
    bool running;
public:
    ShimmyChild(void);
    ~ShimmyChild(void);

    bool start(const std::string &path);
    void stop(void);
};

// a shimmy child instantiates this after being started to talk back
// to the shimmy parent.
class ShimmyParent : public ShimmyCommon {
public:
    ShimmyParent(void);
    ~ShimmyParent(void);

    bool init(void);
};

#if SHIMMY_INTERNAL
// format of this var is ("%d:%d", read_end, write_end)
#define SHIMMY_FDS_ENV_VAR "SHIMMY_FDS"
#endif

#endif /* __SHIMMY_H__ */
