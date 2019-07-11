
#include "Shimmy.h"

namespace Shimmy {

//static
const char * Common :: SHIMMY_FDS_ENV_VAR = "SHIMMY_FDS";

Common :: Common(void)
{
    pthread_mutexattr_t  attr;
    pthread_mutexattr_init(&attr);
    pthread_mutex_init(&mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    pFis = NULL;
    pFos = NULL;
}

Common :: ~Common(void)
{
    _lock();
    if (pFos) delete pFos;
    if (pFis) delete pFis;
    pthread_mutex_destroy(&mutex);
}

bool
Common::get_msg(google::protobuf::Message *msg)
{
    Lock l(this);

    if (pFis == NULL)
        return false;

    uint32_t msgLen;

    while (1)
    {
        pxfe_select sel;
        sel.rfds.set(closer_pipe.read_end());
        sel.rfds.set(child_fds.read_end());
        sel.tv.set(1,0);
        l.unlock();
        int e = 0;
        sel.select(&e);
        if (e < 0)
            fprintf(stderr, "select errno %d (%s)\n", e, strerror(e));
        l.lock();
        if (sel.rfds.is_set(closer_pipe.read_end()))
            return false;
        if (sel.rfds.is_set(child_fds.read_end()))
            break;
    }

    if (pFis == NULL)
        return false;

    {
        google::protobuf::io::CodedInputStream codedStream(pFis);
        if (!codedStream.ReadVarint32(&msgLen))
            return false;
    } // codedStream destroyed here

    if (!msg->ParseFromBoundedZeroCopyStream(pFis, msgLen))
        return false;

    return true;
}

bool
Common::send_msg(const google::protobuf::Message *msg)
{
    Lock l(this);

    if (pFos == NULL)
        return false;

    uint32_t msgLen = msg->ByteSize();

    {
        google::protobuf::io::CodedOutputStream codedStream(pFos);
        codedStream.WriteVarint32(msgLen);
        msg->SerializeToCodedStream( &codedStream );
    } // codedStream destroyed here

    pFos->Flush();

    return true;
}

};
