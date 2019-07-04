
#include "Shimmy.h"
#include SHIMMY_PROTO_HDR
#include <stdio.h>

void *readerThread(void *arg);

bool dostart(Shimmy::Child &child)
{
    printf("%d: parent: starting child.\n", Shimmy::get_tid());
    if (child.start("obj/testChild") == false)
    {
        printf("starting child failed\n");
        return false;
    }
    printf("%d: parent: child has started. doing stuff.\n", Shimmy::get_tid());

    pthread_t id;
    pthread_attr_t                attr;
    pthread_attr_init           (&attr);
    pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_destroy        (&attr);
    pthread_create(&id, &attr, &readerThread, (void*) &child);

    return true;
}

bool thread_running = false;
int shutdown_time = Shimmy::Child::default_shutdown_time;

int
main()
{
    Shimmy::Child child;
    bool done = false;

    if (dostart(child) == false)
        return 1;

    shimmyTest::CtrlToShim_m   c2s;

    c2s.set_type(shimmyTest::C2S_ICD_VERSION);
    c2s.mutable_icd_version()->set_version(shimmyTest::ICD_VERSION);
    c2s.mutable_icd_version()->set_pid(Shimmy::get_tid());
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
            if (thread_running)
            {
                c2s.set_type(shimmyTest::C2S_DATA);
                c2s.mutable_data()->set_stuff(1);
                c2s.mutable_data()->set_data("SOME DATA");
                child.send_msg(&c2s);
                c2s.Clear();
            }
            else
                printf("no child?\n");
            break;
        }
        case 2:
        {
            if (thread_running)
            {
                printf("%d: parent: telling child to crash\n",
                       Shimmy::get_tid());
                c2s.set_type(shimmyTest::C2S_CRASH_CMD);
                child.send_msg(&c2s);
                c2s.Clear();
            }
            else
                printf("no child?\n");
            break;
        }
        case 3:
        {
            printf("%d: parent: calling child.stop\n", Shimmy::get_tid());
            child.stop(shutdown_time);
            printf("%d: parent: child has exited.\n", Shimmy::get_tid());
            // wait for thread .. always wait for thread to die after
            // issuing a stop -- make sure it's dead before starting
            // another one.
            while (thread_running)
                usleep(10000);
            if (dostart(child) == false)
                return 1;
            c2s.set_type(shimmyTest::C2S_ICD_VERSION);
            c2s.mutable_icd_version()->set_version(shimmyTest::ICD_VERSION);
            c2s.mutable_icd_version()->set_pid(Shimmy::get_tid());
            if (child.send_msg(&c2s) == false)
            {
                printf("parent: failure to send msg to child\n");
                goto bail;
            }
            c2s.Clear();
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
    printf("%d: parent: calling child.stop\n", Shimmy::get_tid());
    child.stop(shutdown_time);
    printf("%d: parent: child has exited.\n", Shimmy::get_tid());
    // wait for thread. always wait for thread to die before
    // destroying a Child.
    while (thread_running)
        usleep(10000);

    return 0;
}


void *
readerThread(void *arg)
{
    Shimmy::Child *child = (Shimmy::Child*) arg;
    shimmyTest::ShimToCtrl_m   s2c;

    printf("%d: parent: reader thread started\n", Shimmy::get_tid());
    thread_running = true;

    while (1)
    {
        if (!child->get_msg(&s2c))
            break;
        switch (s2c.type())
        {
        case shimmyTest::S2C_ICD_VERSION:
            printf("%d: parent: got ICD version %d from child %d\n",
                   Shimmy::get_tid(),
                   s2c.icd_version().version(),
                   s2c.icd_version().pid());
            if (s2c.icd_version().has_shutdown_time())
            {
                shutdown_time = s2c.icd_version().shutdown_time();
                printf("%d: child overrides shutdown time to %d seconds\n",
                       Shimmy::get_tid(), shutdown_time);
            }
            break;

        case shimmyTest::S2C_DATA:
            printf("%d: parent: got DATA from child\n", Shimmy::get_tid());
            break;

        default:
            printf("%d: parent: got unhandled msg type %d\n",
                   Shimmy::get_tid(), s2c.type());
        }
    }

    printf("%d: parent: child->get_msg is done\n", Shimmy::get_tid());
    thread_running = false;
    return NULL;
}
