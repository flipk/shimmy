
#include "Shimmy.h"
#include SHIMMY_PROTO_HDR
#include <stdio.h>

// set this to 1 to test if SIGTERM/SIGKILL escalation works
#define TEST_ESCALATION 0

int
main()
{
    Shimmy::Parent  parent;
    bool done = false;

    printf("%d: child: started\n", Shimmy::get_tid());

    if (parent.init() == false)
    {
        printf("child: failure to init ShimmyParent; am I a ShimmyChild?!\n");
        return 1;
    }

    shimmySample::CtrlToShim_m   c2s;
    shimmySample::ShimToCtrl_m   s2c;

#if TEST_ESCALATION
    signal(SIGTERM, SIG_IGN);
#endif

    s2c.set_type(shimmySample::S2C_ICD_VERSION);
    s2c.mutable_icd_version()->set_version(shimmySample::ICD_VERSION);
    s2c.mutable_icd_version()->set_pid(Shimmy::get_tid());
    s2c.mutable_icd_version()->set_shutdown_time(5);
    if (parent.send_msg(&s2c) == false)
    {
        printf("child: failure to send msg to parent\n");
        goto bail;
    }

    while (!done)
    {
        if (!parent.get_msg(&c2s))
            break;
        switch (c2s.type())
        {
        case shimmySample::C2S_ICD_VERSION:
            printf("%d: child: got ICD version %d from parent %d\n",
                   Shimmy::get_tid(),
                   c2s.icd_version().version(),
                   c2s.icd_version().pid());
            break;

        case shimmySample::C2S_CRASH_CMD:
            printf("%d: child: told to exit\n", Shimmy::get_tid());
            done = true;
            break;

        case shimmySample::C2S_DATA:
            printf("%d: child: got DATA from parent\n", Shimmy::get_tid());
            s2c.set_type(shimmySample::S2C_DATA);
            s2c.mutable_data()->set_stuff(1);
            s2c.mutable_data()->set_data("SOME DATA");
            parent.send_msg(&s2c);
            s2c.Clear();
            break;

        default:
            printf("child: got unhandled type %d\n", c2s.type());
        }
    }

bail:
    printf("%d: child: exiting\n", Shimmy::get_tid());

#if TEST_ESCALATION
    sleep(20);
#endif

    return 0;
}
