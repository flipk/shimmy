
#include "Shimmy.h"

namespace Shimmy {

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
    bool ret;

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
        ret = codedStream.ReadVarint32(&msgLen);
        if (ret == false)
            return false;
    } // codedStream destroyed here

    ret = msg->ParseFromBoundedZeroCopyStream(pFis, msgLen);
    if (ret == false)
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
