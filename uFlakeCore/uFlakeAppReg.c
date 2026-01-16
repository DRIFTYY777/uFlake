#include "uFlakeAppReg.h"
#include "appLoader.h"

extern const app_bundle_t counter_app; // From Apps/counter_app/app_main.c

void register_builtin_apps(void)
{
    // Registration of built-in applications would be implemented here
    app_loader_init();

    // Register and launch the counter app as an example
    app_loader_launch(app_loader_register(&counter_app)); // Launch counter app
}