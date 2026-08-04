#include "uFlakeAppReg.h"
#include "appLoader_simple.h"

extern const app_bundle_t counter_app;    // From Apps/counter_app/app_main.c
extern const app_bundle_t launcher_app;   // From Apps/launcher/launcher.cpp
extern const app_bundle_t test_app;       // From Apps/test_app/app_main.c
// extern const app_bundle_t adc_reader_app; // From Apps/read_ADC/app_main.c
extern const app_bundle_t settings_app;   // From Apps/settings_app/settings_app.cpp

void register_builtin_apps(void)
{
    // Initialize app loader
    app_loader_init();

    // Register built-in apps
    app_loader_register_internal_app(&counter_app, false);
    app_loader_register_internal_app(&launcher_app, true);   // Launcher is special
    // app_loader_register_internal_app(&adc_reader_app, false);
    app_loader_register_internal_app(&settings_app, false);

    // Launch the launcher app
    uint32_t launcher_id = app_loader_get_launcher_id();
    if (launcher_id != 0)
    {
        app_loader_launch_app(launcher_id);
    }
}