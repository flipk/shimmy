
#define SHIMMY_INTERNAL 1
#include "Shimmy.h"

/////////////////////////////// ShimmyParent ///////////////////////////////


ShimmyParent::ShimmyParent(void)
{
    // xxx todo
}

ShimmyParent::~ShimmyParent(void)
{
    // xxx todo
}

bool
ShimmyParent::init(void)
{
    char * env = getenv(SHIMMY_RENDEZVOUS_ENV_VAR);
    if (env == NULL)
    {
        fprintf(stderr, "ShimmyParent::init: ERROR: %s env var not set!\n");
        return false;
    }
    if (sscanf(env, "%d:%d", &fd_from_parent, &fd_to_parent) != 2)
    {
        fprintf(stderr, "ShimmyParent::init: ERROR: %s parse failure!\n");
        return false;
    }
    printf("ShimmyParent::init: fd from parent = %d, fd to parent = %d\n",
           fd_from_parent, fd_to_parent);
    return true;
}

bool
ShimmyParent::get_msg(google::protobuf::Message *msg)
{
    char buf[100];
    int readRet = ::read(fd_from_parent, buf, sizeof(buf));
    if (readRet == 0)
        return false;
    // xxx todo
    return true;
}

bool
ShimmyParent::send_msg(const google::protobuf::Message *msg)
{
    // xxx todo
    return true;
}
