
#define SHIMMY_INTERNAL 1
#include "Shimmy.h"
#include <stdio.h>

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
    char * env = getenv(SHIMMY_FDS_ENV_VAR);
    if (env == NULL)
    {
        fprintf(stderr, "ShimmyParent::init: ERROR: %s env var not set!\n");
        return false;
    }
    if (sscanf(env, "%d:%d", &read_fd, &write_fd) != 2)
    {
        fprintf(stderr, "ShimmyParent::init: ERROR: %s parse failure!\n");
        return false;
    }
    printf("ShimmyParent::init: fd from parent = %d, fd to parent = %d\n",
           read_fd, write_fd);
    pFis = new google::protobuf::io::FileInputStream(read_fd);
    pFos = new google::protobuf::io::FileOutputStream(write_fd);
    return true;
}
