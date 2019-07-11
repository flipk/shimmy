
#include "Shimmy.h"

namespace Shimmy {

Parent :: Parent(void)
{
}

Parent :: ~Parent(void)
{
    int d = closer_pipe.write_end();
    if (d != -1)
    {
        char c = 1;
        if (::write(d, &c, 1) != 1)
            fprintf(stderr, "Shimmy::Parent::~Parent: cannot write closer\n");
    }
}

bool
Parent :: init(void)
{
    Lock l(this);

    if (closer_pipe.make("Shimmy::Parent::init") == false)
        return false;

    char * env = getenv(SHIMMY_FDS_ENV_VAR);
    if (env == NULL)
    {
        fprintf(stderr, "Shimmy::Parent::init: ERROR: %s env var not set!\n",
                SHIMMY_FDS_ENV_VAR);
        return false;
    }
    int read_fd, write_fd;
    if (sscanf(env, "%d:%d", &read_fd, &write_fd) != 2)
    {
        fprintf(stderr, "Shimmy::Parent::init: ERROR: %s parse failure!\n",
                SHIMMY_FDS_ENV_VAR);
        return false;
    }

    child_fds.set(read_fd, write_fd);

    pFis = new google::protobuf::io::FileInputStream ( read_fd  );
    pFos = new google::protobuf::io::FileOutputStream( write_fd );

    return true;
}

};
