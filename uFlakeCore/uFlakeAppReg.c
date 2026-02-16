#include "uFlakeAppReg.h"
#include "appLoader.h"
#include "appService.h"

extern const app_bundle_t counter_app;  // From Apps/counter_app/app_main.c
extern const app_bundle_t launcher_app; // From uAppLoader/appLoader.c
extern const app_bundle_t test_app;     // From Apps/test_app/app_main.c
extern const app_bundle_t gui_demo_app; // From Apps/gui_app/app_main.c
// extern const app_bundle_t counter_cpp_app; // From Apps/gui_app/app_main.c
extern const app_bundle_t adc_reader_app; // From Apps/read_ADC/app_main.c

// Service bundles
extern const service_bundle_t input_bundle; // From uServices/input/input.c

void register_builtin_apps(void)
{
    // Registration of built-in applications
    app_loader_init();
    service_manager_init();

    service_start(service_register(&input_bundle));

    // Register apps
    app_loader_register(&counter_app);
    app_loader_register(&launcher_app);
    app_loader_register(&adc_reader_app);

    app_loader_launch(app_loader_register(&test_app));

    // app_loader_launch(app_loader_register(&gui_demo_app));
    // app_loader_launch(app_loader_register(&counter_cpp_app));
}