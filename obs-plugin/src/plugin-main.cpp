#include <obs-module.h>
#include <curl/curl.h>
#include "mal-source.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-mal-scroll", "en-US")

static bool g_curl_initialized = false;

bool obs_module_load(void)
{
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) {
        blog(LOG_ERROR, "Failed to initialize cURL");
        return false;
    }
    g_curl_initialized = true;

    mal_source_register();
    blog(LOG_INFO, "MAL Scroll plugin loaded successfully");
    return true;
}

void obs_module_unload(void)
{
    if (g_curl_initialized) {
        curl_global_cleanup();
        g_curl_initialized = false;
    }
    blog(LOG_INFO, "MAL Scroll plugin unloaded");
}
