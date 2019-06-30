
#include "Shimmy.h"
#include SHIMMY_PROTO_HDR
#include <stdio.h>

void *readerThread(void *arg);

int
main()
{
    ShimmyChild child;

    printf("parent: starting child.\n");
    if (child.start("obj/testChild") == false)
    {
        printf("starting child failed\n");
        return 1;
    }
    printf("parent: child has started. doing stuff.\n");

    pthread_t id;
    pthread_attr_t                attr;
    pthread_attr_init           (&attr);
    pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_destroy        (&attr);
    pthread_create(&id, &attr, &readerThread, (void*) &child);

    shimmySample::CtrlToShim_m   c2s;

    c2s.set_type(shimmySample::C2S_ICD_VERSION);
    c2s.mutable_icd_version()->set_version(shimmySample::ICD_VERSION);
    if (child.send_msg(&c2s) == false)
    {
        printf("parent: failure to send msg to child\n");
        goto bail;
    }
    c2s.Clear();

    sleep(1);

bail:
    printf("parent: calling child.stop\n");
    child.stop();
    printf("parent: child has exited.\n");

    sleep(1);

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
        }
    }

    printf("parent: child->get_msg is done\n");
    return NULL;
}
