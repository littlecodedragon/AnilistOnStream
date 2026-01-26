#include "mal-source.hpp"
#include <graphics/image-file.h>
#include <graphics/graphics.h>
#include <util/platform.h>
#include <util/threading.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include "font5x7.hpp"

static const char *mal_source_get_name(void *unused)
{
    UNUSED_PARAMETER(unused);
    return "MyAnimeList Scroll";
}

static void mal_source_update(void *data, obs_data_t *settings);
static void mal_source_render(void *data, gs_effect_t *effect);
static void mal_source_tick(void *data, float seconds);
static void process_pending_gpu_frees(mal_source *ctx);

static inline uint32_t pack_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    return ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | (uint32_t)a;
}

static std::string truncate_text(const std::string &text, size_t max_chars)
{
    if (text.size() <= max_chars) return text;
    if (max_chars <= 3) return text.substr(0, max_chars);
    return text.substr(0, max_chars - 3) + "...";
}

static void trim_left(std::string &s)
{
    size_t p = s.find_first_not_of(' ');
    if (p == std::string::npos) {
        s.clear();
    } else if (p > 0) {
        s.erase(0, p);
    }
}

static std::vector<std::string> wrap_lines(const std::string &text, size_t max_line, size_t max_lines)
{
    std::vector<std::string> lines;
    if (max_line == 0 || max_lines == 0) return lines;

    std::string remaining = text;
    trim_left(remaining);

    while (!remaining.empty() && lines.size() < max_lines) {
        if (remaining.size() <= max_line) {
            lines.push_back(remaining);
            break;
        }

        size_t break_pos = remaining.rfind(' ', max_line);
        if (break_pos == std::string::npos || break_pos < max_line / 2) {
            break_pos = max_line;
        }

        std::string line = remaining.substr(0, break_pos);
        while (!line.empty() && line.back() == ' ') line.pop_back();
        lines.push_back(line);

        remaining = remaining.substr(break_pos);
        trim_left(remaining);

        if (lines.size() == max_lines - 1 && remaining.size() > max_line) {
            std::string last = remaining.substr(0, max_line);
            if (max_line > 3) {
                last = last.substr(0, max_line - 3) + "...";
            }
            lines.push_back(last);
            break;
        }
    }

    return lines;
}

static uint32_t status_color_rgba(const std::string &status)
{
    if (status == "READING" || status == "WATCHING") return pack_rgba(0x4C, 0xAF, 0x50, 0xFF); // green
    if (status == "COMPLETED") return pack_rgba(0x21, 0x96, 0xF3, 0xFF); // blue
    if (status == "PAUSED") return pack_rgba(0xFF, 0xC1, 0x07, 0xFF); // amber
    if (status == "DROPPED") return pack_rgba(0xF4, 0x43, 0x36, 0xFF); // red
    if (status == "PLANNING") return pack_rgba(0x9E, 0x9E, 0x9E, 0xFF); // gray
    return pack_rgba(0xFF, 0xFF, 0xFF, 0xFF); // default white
}

static void fetch_entries_async(mal_source *ctx)
{
    ctx->fetching = true;

    try {
        std::vector<MALEntry> entries;
        
        // If status is "ALL", fetch all status categories separately to get status info per entry
        if (ctx->status == "ALL") {
            std::vector<std::string> statuses = {"WATCHING", "COMPLETED", "PAUSED", "DROPPED", "PLANNING"};
            
            if (ctx->media == "both") {
                for (const auto &status : statuses) {
                    MALFetcher mangaFetcher(ctx->username);
                    MALFetcher animeFetcher(ctx->username);
                    auto m = mangaFetcher.fetchList(status == "WATCHING" ? "READING" : status, "manga");
                    auto a = animeFetcher.fetchList(status, "anime");
                    entries.insert(entries.end(), m.begin(), m.end());
                    entries.insert(entries.end(), a.begin(), a.end());
                }
            } else {
                for (const auto &status : statuses) {
                    auto status_to_fetch = status;
                    if (ctx->media == "manga" && status == "WATCHING") status_to_fetch = "READING";
                    if (ctx->media == "anime" && status == "READING") status_to_fetch = "WATCHING";
                    
                    MALFetcher fetcher(ctx->username);
                    auto list = fetcher.fetchList(status_to_fetch, ctx->media);
                    entries.insert(entries.end(), list.begin(), list.end());
                }
            }
        } else if (ctx->media == "both") {
            MALFetcher mangaFetcher(ctx->username);
            MALFetcher animeFetcher(ctx->username);
            auto m = mangaFetcher.fetchList(ctx->status == "WATCHING" ? "READING" : ctx->status, "manga");
            auto a = animeFetcher.fetchList(ctx->status == "READING" ? "WATCHING" : ctx->status, "anime");
            entries.reserve(m.size() + a.size());
            entries.insert(entries.end(), m.begin(), m.end());
            entries.insert(entries.end(), a.begin(), a.end());
        } else {
            entries = ctx->fetcher->fetchList(ctx->status, ctx->media);
        }

        std::lock_guard<std::mutex> lock(ctx->data_mutex);

        ctx->entries = std::move(entries);
        blog(LOG_INFO, "[MAL] ===== Loaded %zu entries from fetcher =====", ctx->entries.size());
        if (!ctx->entries.empty()) {
            blog(LOG_INFO, "[MAL] First entry: '%s' (status=%s, media=%s)",
                 ctx->entries[0].title.c_str(), ctx->entries[0].status.c_str(), ctx->entries[0].media.c_str());
        }

        for (auto &img : ctx->images) {
            if (img.image) {
                ctx->pending_images_free.push_back(img.image);
                img.image = nullptr;
            }
            if (img.title_tex) {
                ctx->pending_textures_free.push_back(img.title_tex);
                img.title_tex = nullptr;
            }
            if (img.title2_tex) {
                ctx->pending_textures_free.push_back(img.title2_tex);
                img.title2_tex = nullptr;
            }
            if (img.title3_tex) {
                ctx->pending_textures_free.push_back(img.title3_tex);
                img.title3_tex = nullptr;
            }
            if (img.title4_tex) {
                ctx->pending_textures_free.push_back(img.title4_tex);
                img.title4_tex = nullptr;
            }
            if (img.status_tex) {
                ctx->pending_textures_free.push_back(img.status_tex);
                img.status_tex = nullptr;
            }
        }
        ctx->images.clear();

        for (const auto &entry : ctx->entries) {
            mal_source::LoadedImage loaded;
            loaded.url = entry.coverImage;
            loaded.image = nullptr;
            loaded.loaded = false;
            loaded.title_tex = nullptr;
            loaded.title_w = loaded.title_h = 0;
            loaded.title2_tex = nullptr;
            loaded.title2_w = loaded.title2_h = 0;
            loaded.title3_tex = nullptr;
            loaded.title3_w = loaded.title3_h = 0;
            loaded.title4_tex = nullptr;
            loaded.title4_w = loaded.title4_h = 0;
            loaded.status_tex = nullptr;
            loaded.status_w = loaded.status_h = 0;
            ctx->images.push_back(loaded);
        }

    } catch (const std::exception &e) {
        blog(LOG_ERROR, "Failed to fetch entries: %s", e.what());
    }

    ctx->fetching = false;
    ctx->last_fetch_time = os_gettime_ns();
}

static void *mal_source_create(obs_data_t *settings, obs_source_t *source)
{
    blog(LOG_INFO, "[MAL] mal_source_create called");
    mal_source *ctx = new mal_source();
    ctx->source = source;
    ctx->scroll_offset = 0.0f;
    ctx->last_update_time = os_gettime_ns();
    ctx->fetching = false;
    ctx->last_fetch_time = 0;
    ctx->refresh_interval = 300; // 5 minutes
    ctx->text_scale = 1.0f;
    ctx->white_texture = nullptr;

    mal_source_update(ctx, settings);

    return ctx;
}

static void mal_source_destroy(void *data)
{
    mal_source *ctx = (mal_source *)data;

    if (ctx->fetch_thread.joinable()) {
        ctx->fetch_thread.join();
    }

    process_pending_gpu_frees(ctx);
    
    if (ctx->white_texture) {
        obs_enter_graphics();
        gs_texture_destroy(ctx->white_texture);
        obs_leave_graphics();
    }

    for (auto &img : ctx->images) {
        if (img.image) {
            obs_enter_graphics();
            gs_image_file_free(img.image);
            obs_leave_graphics();
            delete img.image;
        }
        if (img.title_tex || img.status_tex) {
            obs_enter_graphics();
            if (img.title_tex) {
                gs_texture_destroy(img.title_tex);
            }
            if (img.title2_tex) {
                gs_texture_destroy(img.title2_tex);
            }
            if (img.title3_tex) {
                gs_texture_destroy(img.title3_tex);
            }
            if (img.title4_tex) {
                gs_texture_destroy(img.title4_tex);
            }
            if (img.status_tex) {
                gs_texture_destroy(img.status_tex);
            }
            obs_leave_graphics();
        }
    }

    delete ctx;
}

static void load_image_if_needed(mal_source *ctx, size_t index)
{
    if (index >= ctx->images.size()) return;

    auto &img = ctx->images[index];
    if (img.loaded || img.url.empty()) return;

    img.image = new gs_image_file_t();
    gs_image_file_init(img.image, img.url.c_str());

    obs_enter_graphics();
    gs_image_file_init_texture(img.image);
    obs_leave_graphics();

    if (img.image->texture) {
        img.loaded = true;
        blog(LOG_DEBUG, "Loaded image: %s", img.url.c_str());
    } else {
        blog(LOG_WARNING, "Failed to load image: %s", img.url.c_str());
        gs_image_file_free(img.image);
        delete img.image;
        img.image = nullptr;
    }
}

static gs_texture_t *make_text_texture(const std::string &text, uint32_t rgba, uint32_t &w, uint32_t &h)
{
    std::vector<uint8_t> pixels;
    font5x7_render(text, rgba, pixels, w, h);
    if (pixels.empty() || w == 0 || h == 0) return nullptr;
    const uint8_t *level_data[1] = { pixels.data() };
    return gs_texture_create(w, h, GS_RGBA, 1, level_data, GS_DYNAMIC);
}

static void process_pending_gpu_frees(mal_source *ctx)
{
    if (ctx->pending_images_free.empty() && ctx->pending_textures_free.empty()) return;

    obs_enter_graphics();
    for (auto *img : ctx->pending_images_free) {
        if (img) {
            gs_image_file_free(img);
            delete img;
        }
    }
    for (auto *tex : ctx->pending_textures_free) {
        if (tex) {
            gs_texture_destroy(tex);
        }
    }
    obs_leave_graphics();

    ctx->pending_images_free.clear();
    ctx->pending_textures_free.clear();
}

static void ensure_textures_for_entry(mal_source *ctx, size_t index)
{
    if (index >= ctx->entries.size() || index >= ctx->images.size()) return;

    auto &img = ctx->images[index];
    const auto &entry = ctx->entries[index];

    // Check if we need to create any textures
    bool need_title = !img.title_tex;
    bool need_status = !img.status_tex;
    
    if (!need_title && !need_status) return;

    auto title_lines = wrap_lines(truncate_text(entry.title, 100), 20, 4);
    std::string title_text = title_lines.size() > 0 ? title_lines[0] : "";
    std::string title_text2 = title_lines.size() > 1 ? title_lines[1] : "";
    std::string title_text3 = title_lines.size() > 2 ? title_lines[2] : "";
    std::string title_text4 = title_lines.size() > 3 ? title_lines[3] : "";
    
    // Build status badge text
    std::string status_text = entry.status.empty() ? "" : entry.status;
    
    // Convert READING<->WATCHING based on media type
    if (status_text == "READING" && entry.media == "anime") {
        status_text = "WATCHING";
    } else if (status_text == "WATCHING" && entry.media == "manga") {
        status_text = "READING";
    }
    
    std::string base_status = entry.status;
    
    // If both media types are shown, include the medium in the badge text only if enabled
    bool show_media = ctx->show_media_tag && (ctx->media == "both");
    std::string media_tag = entry.media == "anime" ? "ANIME" : "MANGA";
    std::string status_label = "";
    
    if (!status_text.empty()) {
        status_label = show_media ? media_tag + " " + status_text : status_text;
    }

    uint32_t title_w = 0, title_h = 0;
    uint32_t status_w = 0, status_h = 0;
    
    if (!img.title_tex) {
        gs_texture_t *title_tex = make_text_texture(title_text, ctx->title_color, title_w, title_h);
        if (title_tex) {
            img.title_tex = title_tex;
            img.title_w = title_w;
            img.title_h = title_h;
        }
    }
    if (!img.title2_tex && !title_text2.empty()) {
        gs_texture_t *title_tex2 = make_text_texture(title_text2, ctx->title_color, title_w, title_h);
        if (title_tex2) {
            img.title2_tex = title_tex2;
            img.title2_w = title_w;
            img.title2_h = title_h;
        }
    }
    if (!img.title3_tex && !title_text3.empty()) {
        gs_texture_t *title_tex3 = make_text_texture(title_text3, ctx->title_color, title_w, title_h);
        if (title_tex3) {
            img.title3_tex = title_tex3;
            img.title3_w = title_w;
            img.title3_h = title_h;
        }
    }
    if (!img.title4_tex && !title_text4.empty()) {
        gs_texture_t *title_tex4 = make_text_texture(title_text4, ctx->title_color, title_w, title_h);
        if (title_tex4) {
            img.title4_tex = title_tex4;
            img.title4_w = title_w;
            img.title4_h = title_h;
        }
    }
    if (!img.status_tex && !status_label.empty()) {
        uint32_t status_col = ctx->status_use_color ? status_color_rgba(base_status) : ctx->status_color;
        gs_texture_t *status_tex = make_text_texture(status_label, status_col, status_w, status_h);
        if (status_tex) {
            img.status_tex = status_tex;
            img.status_w = status_w;
            img.status_h = status_h;
        }
    }
}

static void mal_source_update(void *data, obs_data_t *settings)
{
    mal_source *ctx = (mal_source *)data;

    ctx->username = obs_data_get_string(settings, "username");
    ctx->status = obs_data_get_string(settings, "status");
    ctx->media = obs_data_get_string(settings, "media");
    ctx->scroll_speed = (int)obs_data_get_int(settings, "scroll_speed");
    ctx->item_width = (int)obs_data_get_int(settings, "item_width");
    ctx->item_gap = (int)obs_data_get_int(settings, "item_gap");
    ctx->refresh_interval = (int)obs_data_get_int(settings, "refresh_interval");
    ctx->text_scale = (float)obs_data_get_double(settings, "text_scale");
    if (ctx->text_scale <= 0.0f) ctx->text_scale = 1.0f;
    
    // Text colors
    ctx->title_color = (uint32_t)obs_data_get_int(settings, "title_color");
    ctx->status_use_color = obs_data_get_bool(settings, "status_use_color");
    ctx->status_color = (uint32_t)obs_data_get_int(settings, "status_color");
    ctx->show_media_tag = obs_data_get_bool(settings, "show_media_tag");
    ctx->text_background = obs_data_get_bool(settings, "text_background");
    ctx->background_color = (uint32_t)obs_data_get_int(settings, "background_color");
    ctx->background_padding = (float)obs_data_get_double(settings, "background_padding");
    ctx->background_opacity = (float)obs_data_get_double(settings, "background_opacity");
    if (ctx->background_opacity < 0.0f) ctx->background_opacity = 0.0f;
    if (ctx->background_opacity > 1.0f) ctx->background_opacity = 1.0f;

    // Clear all text textures to force recreation with new settings
    std::lock_guard<std::mutex> lock(ctx->data_mutex);
    for (auto &img : ctx->images) {
        if (img.title_tex) {
            ctx->pending_textures_free.push_back(img.title_tex);
            img.title_tex = nullptr;
        }
        if (img.title2_tex) {
            ctx->pending_textures_free.push_back(img.title2_tex);
            img.title2_tex = nullptr;
        }
        if (img.title3_tex) {
            ctx->pending_textures_free.push_back(img.title3_tex);
            img.title3_tex = nullptr;
        }
        if (img.title4_tex) {
            ctx->pending_textures_free.push_back(img.title4_tex);
            img.title4_tex = nullptr;
        }
        if (img.status_tex) {
            ctx->pending_textures_free.push_back(img.status_tex);
            img.status_tex = nullptr;
        }
    }

    if (ctx->username.empty()) {
        blog(LOG_WARNING, "No username set");
        return;
    }

    ctx->fetcher = std::make_unique<MALFetcher>(ctx->username);

    {
        std::lock_guard<std::mutex> lock(ctx->data_mutex);
        for (auto &img : ctx->images) {
            if (img.title_tex) ctx->pending_textures_free.push_back(img.title_tex), img.title_tex = nullptr;
            if (img.title2_tex) ctx->pending_textures_free.push_back(img.title2_tex), img.title2_tex = nullptr;
            if (img.title3_tex) ctx->pending_textures_free.push_back(img.title3_tex), img.title3_tex = nullptr;
            if (img.title4_tex) ctx->pending_textures_free.push_back(img.title4_tex), img.title4_tex = nullptr;
            if (img.status_tex) ctx->pending_textures_free.push_back(img.status_tex), img.status_tex = nullptr;
        }
    }

    if (!ctx->fetching) {
        if (ctx->fetch_thread.joinable()) {
            ctx->fetch_thread.join();
        }
        ctx->fetch_thread = std::thread(fetch_entries_async, ctx);
    }
}

static void mal_source_tick(void *data, float seconds)
{
    mal_source *ctx = (mal_source *)data;

    if (ctx->entries.empty()) return;

    float speed_pixels_per_second = (float)ctx->scroll_speed;
    ctx->scroll_offset += speed_pixels_per_second * seconds;

    float total_width = (ctx->item_width + ctx->item_gap) * ctx->entries.size();
    if (total_width > 0.0f) {
        ctx->scroll_offset = fmodf(ctx->scroll_offset, total_width);
        if (ctx->scroll_offset < 0.0f)
            ctx->scroll_offset += total_width;
    }

    int visible_start = (int)(ctx->scroll_offset / (ctx->item_width + ctx->item_gap));
    int visible_count = 6; // Reduziert von 10 für bessere Performance

    // Removed synchronous image loading - causes lag

    uint64_t now = os_gettime_ns();
    uint64_t refresh_ns = (uint64_t)ctx->refresh_interval * 1000000000ULL;
    if (now - ctx->last_fetch_time > refresh_ns && !ctx->fetching) {
        if (ctx->fetch_thread.joinable()) {
            ctx->fetch_thread.join();
        }
        ctx->fetch_thread = std::thread(fetch_entries_async, ctx);
    }
}

static void mal_source_render(void *data, gs_effect_t *effect)
{
    mal_source *ctx = (mal_source *)data;

    if (ctx->entries.empty()) return;

    std::lock_guard<std::mutex> lock(ctx->data_mutex);

    process_pending_gpu_frees(ctx);

    gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_DEFAULT);
    gs_technique_t *tech = gs_effect_get_technique(solid, "Draw");

    float total_width = (ctx->item_width + ctx->item_gap) * ctx->entries.size();
    float x_offset = -ctx->scroll_offset;

    int images_loaded_this_frame = 0;
    const int max_images_per_frame = 1; // Only load 1 image per frame to avoid stuttering

    for (int pass = 0; pass < 2; pass++) {
        float base_x = x_offset + (pass > 0 ? total_width : 0.0f);

        for (size_t i = 0; i < ctx->entries.size(); i++) {
            float x = base_x + i * (ctx->item_width + ctx->item_gap);

            uint32_t source_width = obs_source_get_width(ctx->source);
            if (x + ctx->item_width < 0 || x > source_width) continue;

            // Load image asynchronously if needed (non-blocking), but limit per frame
            if (i < ctx->images.size() && !ctx->images[i].loaded && !ctx->images[i].url.empty() && images_loaded_this_frame < max_images_per_frame) {
                auto &img = ctx->images[i];
                if (!img.image) {
                    img.image = new gs_image_file_t();
                    gs_image_file_init(img.image, img.url.c_str());
                    gs_image_file_init_texture(img.image);
                    if (img.image->texture) {
                        img.loaded = true;
                    }
                    images_loaded_this_frame++;
                }
            }

            // Always create text textures for visible items
            if (i < ctx->images.size()) {
                ensure_textures_for_entry(ctx, i);
            }

            if (i < ctx->images.size() && ctx->images[i].loaded && ctx->images[i].image && ctx->images[i].image->texture) {
                auto &img = ctx->images[i];

                gs_effect_set_texture(gs_effect_get_param_by_name(solid, "image"), img.image->texture);

                uint32_t img_width = gs_texture_get_width(img.image->texture);
                uint32_t img_height = gs_texture_get_height(img.image->texture);
                float scale = (float)ctx->item_width / img_width;
                float scaled_height = img_height * scale;

                // Cover
                gs_technique_begin(tech);
                gs_technique_begin_pass(tech, 0);

                gs_matrix_push();
                gs_matrix_translate3f(x, 0.0f, 0.0f);
                gs_matrix_scale3f(scale, scale, 1.0f);
                gs_draw_sprite(img.image->texture, 0, img_width, img_height);
                gs_matrix_pop();

                gs_technique_end_pass(tech);
                gs_technique_end(tech);

                auto draw_tex = [&](gs_texture_t *tex, uint32_t tw, uint32_t th, float draw_x, float draw_y, float tex_scale) {
                    if (!tex || tw == 0 || th == 0 || tex_scale <= 0.0f) return;
                    
                    // Draw background if enabled
                    if (ctx->text_background) {
                        // Lazy-create white texture if needed
                        if (!ctx->white_texture) {
                            uint32_t white_pixel = 0xFFFFFFFF;
                            const uint8_t *white_data = (const uint8_t*)&white_pixel;
                            ctx->white_texture = gs_texture_create(1, 1, GS_RGBA, 1, &white_data, 0);
                        }
                        
                        if (ctx->white_texture) {
                            float bg_width = tw * tex_scale + ctx->background_padding * 2.0f;
                            float bg_height = th * tex_scale + ctx->background_padding * 2.0f;
                            float bg_x = draw_x - ctx->background_padding;
                            float bg_y = draw_y - ctx->background_padding;
                            
                            gs_effect_t *solid_bg = obs_get_base_effect(OBS_EFFECT_SOLID);
                            gs_eparam_t *color_param = gs_effect_get_param_by_name(solid_bg, "color");
                            gs_technique_t *solid_tech = gs_effect_get_technique(solid_bg, "Solid");
                            
                            if (color_param && solid_tech) {
                                struct vec4 bg_color;
                                uint32_t c = ctx->background_color;
                                bg_color.x = (float)((c >> 0) & 0xFF) / 255.0f;  // R
                                bg_color.y = (float)((c >> 8) & 0xFF) / 255.0f;  // G
                                bg_color.z = (float)((c >> 16) & 0xFF) / 255.0f; // B
                                bg_color.w = ctx->background_opacity;             // Alpha from slider
                                gs_effect_set_vec4(color_param, &bg_color);
                                
                                gs_technique_begin(solid_tech);
                                gs_technique_begin_pass(solid_tech, 0);
                                gs_matrix_push();
                                gs_matrix_translate3f(bg_x, bg_y, 0.0f);
                                gs_matrix_scale3f(bg_width, bg_height, 1.0f);
                                gs_draw_sprite(ctx->white_texture, 0, 1, 1);
                                gs_matrix_pop();
                                gs_technique_end_pass(solid_tech);
                                gs_technique_end(solid_tech);
                            }
                        }
                    }
                    
                    gs_effect_set_texture(gs_effect_get_param_by_name(solid, "image"), tex);
                    gs_technique_begin(tech);
                    gs_technique_begin_pass(tech, 0);
                    gs_matrix_push();
                    gs_matrix_translate3f(draw_x, draw_y, 0.0f);
                    gs_matrix_scale3f(tex_scale, tex_scale, 1.0f);
                    gs_draw_sprite(tex, 0, tw, th);
                    gs_matrix_pop();
                    gs_technique_end_pass(tech);
                    gs_technique_end(tech);
                };

                // Status badge near the top-left of the cover
                if (img.status_tex && img.status_w > 0 && img.status_h > 0) {
                    blog(LOG_WARNING, "[MAL-DEBUG] Rendering status badge at x=%.1f tex=%p w=%d h=%d", x + 6.0f, img.status_tex, img.status_w, img.status_h);
                    float avail = (float)ctx->item_width - 12.0f - (ctx->text_background ? ctx->background_padding * 2.0f : 0.0f);
                    if (avail < 1.0f) avail = 1.0f;
                    // Mindestens Skalierung 1.5 für bessere Sichtbarkeit
                    float badge_scale = (img.status_w > 0 && avail > 0.0f)
                        ? std::max(1.5f, std::min(3.0f, std::min(ctx->text_scale, avail / (float)img.status_w)))
                        : std::max(1.5f, ctx->text_scale);
                    draw_tex(img.status_tex, img.status_w, img.status_h, x + 6.0f, 6.0f, badge_scale);
                } else {
                    blog(LOG_WARNING, "[MAL-DEBUG] Status badge not rendered: tex=%p w=%d h=%d", 
                         img.status_tex, img.status_w, img.status_h);
                }

                // Title under the cover
                if (img.title_tex) {
                    float avail = (float)ctx->item_width - 12.0f - (ctx->text_background ? ctx->background_padding * 2.0f : 0.0f);
                    if (avail < 1.0f) avail = 1.0f;
                    float base_y = scaled_height + 8.0f;
                    float line_spacing = 4.0f;
                    float y = base_y;

                    auto draw_line = [&](gs_texture_t *tex, uint32_t tw, uint32_t th) {
                        if (!tex || tw == 0 || th == 0) return;
                        float scale = (tw > 0 && avail > 0.0f)
                            ? std::min(3.0f, std::min(ctx->text_scale, avail / (float)tw))
                            : ctx->text_scale;
                        draw_tex(tex, tw, th, x + 6.0f, y, scale);
                        y += th * scale + line_spacing;
                    };

                    draw_line(img.title_tex, img.title_w, img.title_h);
                    draw_line(img.title2_tex, img.title2_w, img.title2_h);
                    draw_line(img.title3_tex, img.title3_w, img.title3_h);
                    draw_line(img.title4_tex, img.title4_w, img.title4_h);
                }
            } else {
                // Render text/status even without cover image
                if (i < ctx->images.size()) {
                    auto &img = ctx->images[i];
                    
                    // Status badge
                    if (img.status_tex && img.status_w > 0 && img.status_h > 0) {
                        float avail = (float)ctx->item_width - 12.0f - (ctx->text_background ? ctx->background_padding * 2.0f : 0.0f);
                        if (avail < 1.0f) avail = 1.0f;
                        float badge_scale = (img.status_w > 0 && avail > 0.0f)
                            ? std::max(1.5f, std::min(3.0f, std::min(ctx->text_scale, avail / (float)img.status_w)))
                            : std::max(1.5f, ctx->text_scale);
                        
                        auto draw_tex = [&](gs_texture_t *tex, uint32_t tw, uint32_t th, float draw_x, float draw_y, float tex_scale) {
                            if (!tex || tw == 0 || th == 0 || tex_scale <= 0.0f) return;
                            
                            if (ctx->text_background) {
                                if (!ctx->white_texture) {
                                    uint32_t white_pixel = 0xFFFFFFFF;
                                    const uint8_t *white_data = (const uint8_t*)&white_pixel;
                                    ctx->white_texture = gs_texture_create(1, 1, GS_RGBA, 1, &white_data, 0);
                                }
                                
                                if (ctx->white_texture) {
                                    float bg_width = tw * tex_scale + ctx->background_padding * 2.0f;
                                    float bg_height = th * tex_scale + ctx->background_padding * 2.0f;
                                    float bg_x = draw_x - ctx->background_padding;
                                    float bg_y = draw_y - ctx->background_padding;
                                    
                                    gs_effect_t *solid_bg = obs_get_base_effect(OBS_EFFECT_SOLID);
                                    gs_eparam_t *color_param = gs_effect_get_param_by_name(solid_bg, "color");
                                    gs_technique_t *solid_tech = gs_effect_get_technique(solid_bg, "Solid");
                                    
                                    if (color_param && solid_tech) {
                                        struct vec4 bg_color;
                                        uint32_t c = ctx->background_color;
                                        bg_color.x = (float)((c >> 0) & 0xFF) / 255.0f;
                                        bg_color.y = (float)((c >> 8) & 0xFF) / 255.0f;
                                        bg_color.z = (float)((c >> 16) & 0xFF) / 255.0f;
                                        bg_color.w = ctx->background_opacity;
                                        gs_effect_set_vec4(color_param, &bg_color);
                                        
                                        gs_technique_begin(solid_tech);
                                        gs_technique_begin_pass(solid_tech, 0);
                                        gs_matrix_push();
                                        gs_matrix_translate3f(bg_x, bg_y, 0.0f);
                                        gs_matrix_scale3f(bg_width, bg_height, 1.0f);
                                        gs_draw_sprite(ctx->white_texture, 0, 1, 1);
                                        gs_matrix_pop();
                                        gs_technique_end_pass(solid_tech);
                                        gs_technique_end(solid_tech);
                                    }
                                }
                            }
                            
                            gs_effect_set_texture(gs_effect_get_param_by_name(solid, "image"), tex);
                            gs_technique_begin(tech);
                            gs_technique_begin_pass(tech, 0);
                            gs_matrix_push();
                            gs_matrix_translate3f(draw_x, draw_y, 0.0f);
                            gs_matrix_scale3f(tex_scale, tex_scale, 1.0f);
                            gs_draw_sprite(tex, 0, tw, th);
                            gs_matrix_pop();
                            gs_technique_end_pass(tech);
                            gs_technique_end(tech);
                        };
                        
                        draw_tex(img.status_tex, img.status_w, img.status_h, x + 6.0f, 6.0f, badge_scale);
                    }
                }
            }
        }
    }
}

static uint32_t mal_source_get_width(void *data)
{
    UNUSED_PARAMETER(data);
    return 1920;
}

static uint32_t mal_source_get_height(void *data)
{
    UNUSED_PARAMETER(data);
    return 600;
}

static void mal_source_get_defaults(obs_data_t *settings)
{
    obs_data_set_default_string(settings, "username", "");
    obs_data_set_default_string(settings, "status", "READING");
    obs_data_set_default_string(settings, "media", "manga");
    obs_data_set_default_int(settings, "scroll_speed", 50);
    obs_data_set_default_int(settings, "item_width", 250);
    obs_data_set_default_int(settings, "item_gap", 30);
    obs_data_set_default_int(settings, "refresh_interval", 300);
    obs_data_set_default_double(settings, "text_scale", 2.5);
    
    // Text colors - RGBA format (0xRRGGBBAA)
    obs_data_set_default_int(settings, "title_color", 0xFFFFFFFF); // white
    obs_data_set_default_bool(settings, "status_use_color", true); // use automatic status colors
    obs_data_set_default_int(settings, "status_color", 0xFFFFFFFF); // white fallback
    obs_data_set_default_bool(settings, "show_media_tag", false);
    obs_data_set_default_bool(settings, "text_background", true); // enabled for readability
    obs_data_set_default_int(settings, "background_color", 0x000000); // color picker is RGB; alpha via opacity
    obs_data_set_default_double(settings, "background_padding", 6.0);
    obs_data_set_default_double(settings, "background_opacity", 0.8);
}

static obs_properties_t *mal_source_get_properties(void *data)
{
    UNUSED_PARAMETER(data);

    obs_properties_t *props = obs_properties_create();

    obs_properties_add_text(props, "username", "MyAnimeList Username", OBS_TEXT_DEFAULT);

    obs_property_t *media_list = obs_properties_add_list(props, "media", "Media Type",
                                                         OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(media_list, "Manga", "manga");
    obs_property_list_add_string(media_list, "Anime", "anime");
    obs_property_list_add_string(media_list, "Both", "both");

    obs_property_t *status_list = obs_properties_add_list(props, "status", "Status Filter",
                                                          OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(status_list, "Reading/Watching", "READING");
    obs_property_list_add_string(status_list, "Completed", "COMPLETED");
    obs_property_list_add_string(status_list, "On Hold", "PAUSED");
    obs_property_list_add_string(status_list, "Dropped", "DROPPED");
    obs_property_list_add_string(status_list, "Plan to Read/Watch", "PLANNING");
    obs_property_list_add_string(status_list, "All", "ALL");

    obs_properties_add_int_slider(props, "scroll_speed", "Scroll Speed (px/s)", 10, 200, 5);
    obs_properties_add_int_slider(props, "item_width", "Item Width", 100, 500, 10);
    obs_properties_add_int_slider(props, "item_gap", "Gap Between Items", 0, 100, 5);
    obs_properties_add_int_slider(props, "refresh_interval", "Refresh Interval (seconds)", 60, 3600, 60);
    obs_properties_add_float_slider(props, "text_scale", "Text Scale", 0.5, 5.0, 0.1);
    
    // Text appearance
    obs_properties_add_color(props, "title_color", "Title Color");
    obs_properties_add_bool(props, "status_use_color", "Use Automatic Status Colors");
    obs_properties_add_color(props, "status_color", "Status Badge Color (if not automatic)");
    obs_properties_add_bool(props, "show_media_tag", "Show Media Tag in Status Badge (only when media=Both)");
    obs_properties_add_bool(props, "text_background", "Enable Text Background");
    obs_properties_add_color(props, "background_color", "Text Background Color");
    obs_properties_add_float_slider(props, "background_opacity", "Background Opacity", 0.0, 1.0, 0.05);
    obs_properties_add_float_slider(props, "background_padding", "Background Padding", 0.0, 20.0, 0.5);

    return props;
}

void mal_source_register()
{
    struct obs_source_info info = {};
    info.id = "mal_scroll_source";
    info.type = OBS_SOURCE_TYPE_INPUT;
    info.output_flags = OBS_SOURCE_VIDEO;
    info.get_name = mal_source_get_name;
    info.create = mal_source_create;
    info.destroy = mal_source_destroy;
    info.update = mal_source_update;
    info.get_width = mal_source_get_width;
    info.get_height = mal_source_get_height;
    info.get_defaults = mal_source_get_defaults;
    info.get_properties = mal_source_get_properties;
    info.video_tick = mal_source_tick;
    info.video_render = mal_source_render;

    obs_register_source(&info);
}
