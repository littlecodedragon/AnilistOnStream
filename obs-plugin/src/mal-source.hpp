#pragma once

#include <obs-module.h>
#include <graphics/image-file.h>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <string>
#include "mal-fetcher.hpp"

struct mal_source {
    obs_source_t *source;
    
    // Settings
    std::string username;
    std::string status;
    std::string media;
    int scroll_speed; // pixels per second
    int item_width;
    int item_gap;
    int width;
    int height;
    float text_scale;
    
    // Text appearance
    uint32_t title_color;
    uint32_t status_use_color; // if true, use status colors; else use status_color
    uint32_t status_color;
    bool show_media_tag;
    bool text_background;
    uint32_t background_color;
    float background_padding;
    float background_opacity; // 0..1
    
    // Data
    std::unique_ptr<MALFetcher> fetcher;
    std::vector<MALEntry> entries;
    std::mutex data_mutex;

    // Deferred GPU frees (must happen on render thread)
    std::vector<gs_image_file_t*> pending_images_free;
    std::vector<gs_texture_t*> pending_textures_free;

    // Images
    struct LoadedImage {
        gs_image_file_t *image;
        std::string url;
        bool loaded;
        gs_texture_t *title_tex;
        uint32_t title_w;
        uint32_t title_h;
        gs_texture_t *title2_tex;
        uint32_t title2_w;
        uint32_t title2_h;
        gs_texture_t *title3_tex;
        uint32_t title3_w;
        uint32_t title3_h;
        gs_texture_t *title4_tex;
        uint32_t title4_w;
        uint32_t title4_h;
        gs_texture_t *status_tex;
        uint32_t status_w;
        uint32_t status_h;
    };
    std::vector<LoadedImage> images;

    // Animation
    float scroll_offset;
    uint64_t last_update_time;
    
    // Cached white texture for backgrounds
    gs_texture_t *white_texture;

    // Background fetch
    std::atomic<bool> fetching;
    std::thread fetch_thread;

    // Refresh timer
    uint64_t last_fetch_time;
    int refresh_interval; // seconds
};

void mal_source_register();
