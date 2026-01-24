#include <obs-module.h>
#include "mal-source.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-mal-scroll", "en-US")

bool obs_module_load(void)
{
    mal_source_register();
    blog(LOG_INFO, "MAL Scroll plugin loaded successfully");
    return true;
}

void obs_module_unload(void)
{
    blog(LOG_INFO, "MAL Scroll plugin unloaded");
}
