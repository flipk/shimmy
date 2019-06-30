
#include "Shimmy.h"
#include SHIMMY_PROTO_HDR
#include <stdio.h>

void *readerThread(void *arg);

bool dostart(ShimmyChild &child)
{
    printf("parent: starting child.\n");
    if (child.start("obj/testChild") == false)
    {
        printf("starting child failed\n");
        return false;
    }
    printf("parent: child has started. doing stuff.\n");

    pthread_t id;
    pthread_attr_t                attr;
    pthread_attr_init           (&attr);
    pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_destroy        (&attr);
    pthread_create(&id, &attr, &readerThread, (void*) &child);

    return true;
}

int
main()
{
    ShimmyChild child;
    bool done = false;

    if (dostart(child) == false)
        return 1;

    shimmySample::CtrlToShim_m   c2s;

    c2s.set_type(shimmySample::C2S_ICD_VERSION);
    c2s.mutable_icd_version()->set_version(shimmySample::ICD_VERSION);
    if (child.send_msg(&c2s) == false)
    {
        printf("parent: failure to send msg to child\n");
        goto bail;
    }
    c2s.Clear();

    while (!done)
    {
        printf("1) send msg\n"
               "2) send crash req\n"
               "3) stopstart\n"
               "4) stop and exit\n");
        int req;
        std::cin >> req;
        switch (req)
        {
        case 1:
        {
            c2s.set_type(shimmySample::C2S_DATA);
            c2s.mutable_data()->set_stuff(1);
            c2s.mutable_data()->set_data("SOME DATA");
            child.send_msg(&c2s);
            c2s.Clear();
            break;
        }
        case 2:
        {
            printf("parent: telling child to crash\n");
            c2s.set_type(shimmySample::C2S_CRASH_CMD);
            child.send_msg(&c2s);
            c2s.Clear();
            break;
        }
        case 3:
        {
            printf("parent: calling child.stop\n");
            child.stop();
            printf("parent: child has exited.\n");
            if (dostart(child) == false)
                return 1;
            break;
        }
        case 4:
        {
            done = true;
            break;
        }
        }
    }

bail:
    printf("parent: calling child.stop\n");
    child.stop();
    printf("parent: child has exited.\n");

    return 0;
}


void *
readerThread(void *arg)
{
    ShimmyChild *child = (ShimmyChild*) arg;
    shimmySample::ShimToCtrl_m   s2c;

    while (1)
    {
        if (!child->get_msg(&s2c))
            break;
        switch (s2c.type())
        {
        case shimmySample::S2C_ICD_VERSION:
            printf("parent: got ICD version %d from child\n",
                   s2c.icd_version().version());
            break;

        case shimmySample::S2C_DATA:
            printf("parent: got DATA from child\n");
            break;

        default:
            printf("parent: got unhandled msg type %d\n", s2c.type());
        }
    }

    printf("parent: child->get_msg is done\n");
    return NULL;
}
