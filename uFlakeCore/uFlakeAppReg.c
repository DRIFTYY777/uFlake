#include "uFlakeAppReg.h"
#include "appLoader.h"
#include "input.h"

extern const app_bundle_t counter_app;     // From Apps/counter_app/app_main.c
extern const app_bundle_t launcher_app;    // From uAppLoader/appLoader.c
extern const app_bundle_t test_app;        // From Apps/test_app/app_main.c
extern const app_bundle_t gui_test_app;    // From Apps/gui_app/app_main.c
// extern const app_bundle_t counter_cpp_app; // From Apps/gui_app/app_main.c

void register_builtin_apps(void)
{
    // Registration of built-in applications
    app_loader_init();

    input_init();

    // Register apps
    app_loader_register(&counter_app);
    app_loader_register(&launcher_app);

    app_loader_launch(app_loader_register(&test_app));
    app_loader_launch(app_loader_register(&gui_test_app));
    // app_loader_launch(app_loader_register(&counter_cpp_app));
}