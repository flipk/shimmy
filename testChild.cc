
#include "Shimmy.h"
#include SHIMMY_PROTO_HDR
#include <stdio.h>

int
main()
{
    ShimmyParent  parent;

    printf("child: started\n");

    if (parent.init() == false)
    {
        printf("child: failure to init ShimmyParent; am I a ShimmyChild?!\n");
        return 1;
    }

    shimmySample::CtrlToShim_m   c2s;
    shimmySample::ShimToCtrl_m   s2c;

    s2c.set_type(shimmySample::SHIM2CTRL_ICD_VERSION);
    s2c.mutable_icd_version()->set_version(shimmySample::ICD_VERSION);
    if (parent.send_msg(&s2c) == false)
    {
        printf("child: failure to send msg to parent\n");
        goto bail;
    }

    while (1)
    {
        if (!parent.get_msg(&c2s))
            break;
        // xxx
    }

bail:
    printf("child: exiting\n");
    return 0;
}
