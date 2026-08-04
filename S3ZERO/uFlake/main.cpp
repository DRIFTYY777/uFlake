#include <stdio.h>

#include "uFlakeCore.h"
#include "kernel.h"

extern "C"
{
    void app_main(void)
    {
        // Initialize and start the entire OS
        uflake_core_init();
    }
}
