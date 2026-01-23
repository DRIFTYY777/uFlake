target extended-remote :3333
mon reset halt
maintenance flush register-cache
thb app_main
c