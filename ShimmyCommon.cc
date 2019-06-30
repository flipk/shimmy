
#define SHIMMY_INTERNAL 1
#include "Shimmy.h"
#include <stdio.h>

/////////////////////////////// ShimmyCommon ///////////////////////////////

ShimmyCommon::ShimmyCommon(void)
{
    read_fd = write_fd = -1;
    pFis = NULL;
    pFos = NULL;
}

ShimmyCommon::~ShimmyCommon(void)
{
    if (pFis)
        delete pFis;
    if (pFos)
        delete pFos;
    if (read_fd != -1)
        close(read_fd);
    if (write_fd != -1)
        close(write_fd);
}

bool
ShimmyCommon::get_msg(google::protobuf::Message *msg)
{
    if (pFis == NULL)
        return false;

    uint32_t msgLen;
    bool ret;

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
ShimmyCommon::send_msg(const google::protobuf::Message *msg)
{
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
