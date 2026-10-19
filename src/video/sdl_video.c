#include "sdl_video.h"
#include <SDL.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct seedless_sdl_video_impl {
    seedless_video_display_t displays[SEEDLESS_VIDEO_MAX_OUTPUTS];
    SDL_Window *windows[SEEDLESS_VIDEO_MAX_OUTPUTS];
    SDL_Renderer *renderers[SEEDLESS_VIDEO_MAX_OUTPUTS];
    SDL_Surface *surfaces[SEEDLESS_VIDEO_MAX_OUTPUTS];
    uint8_t *surface_pixels[SEEDLESS_VIDEO_MAX_OUTPUTS];
    size_t surface_pixels_capacity[SEEDLESS_VIDEO_MAX_OUTPUTS];
    Uint32 surface_pixel_format[SEEDLESS_VIDEO_MAX_OUTPUTS];
    int surface_bytes_per_pixel[SEEDLESS_VIDEO_MAX_OUTPUTS];
    int wayland_surface_presenter;
    SDL_Texture *textures[2];
    unsigned physical_count;
    unsigned texture_count;
    uint64_t presented_sequence[SEEDLESS_VIDEO_MAX_OUTPUTS];
    int logged_geometry[SEEDLESS_VIDEO_MAX_OUTPUTS];
    int last_renderer_width[SEEDLESS_VIDEO_MAX_OUTPUTS];
    int last_renderer_height[SEEDLESS_VIDEO_MAX_OUTPUTS];
    uint64_t last_sequence;
    seedless_video_preferences_t preferences;
    int sdl_started;
} seedless_sdl_video_impl_t;

static int fail(char *error, size_t capacity, const char *operation)
{
    if (error && capacity)
        snprintf(error, capacity, "%s: %s", operation, SDL_GetError());
    return 0;
}

static const char *mode_name(seedless_video_effective_mode_t mode)
{
    return mode == SEEDLESS_VIDEO_EFFECTIVE_DUAL ? "dual_screen" : "single_screen";
}

static const char *requested_mode_name(seedless_video_requested_mode_t mode)
{
    switch (mode) {
    case SEEDLESS_VIDEO_MODE_AUTO: return "auto";
    case SEEDLESS_VIDEO_MODE_DUAL_SCREEN: return "dual_screen";
    case SEEDLESS_VIDEO_MODE_SINGLE_SCREEN: return "single_screen";
    default: return "invalid";
    }
}

static const char *layout_name(seedless_video_single_layout_t layout)
{
    return layout == SEEDLESS_VIDEO_LAYOUT_HORIZONTAL ? "horizontal" : "vertical";
}

static void print_version(void)
{
    SDL_version compiled, linked;
    SDL_VERSION(&compiled);
    SDL_GetVersion(&linked);
    fprintf(stderr,
            "seedlessds[video]: SDL compiled=%u.%u.%u runtime=%u.%u.%u driver=%s\n",
            compiled.major, compiled.minor, compiled.patch,
            linked.major, linked.minor, linked.patch,
            SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "unknown");
}

static void select_default_video_driver(void)
{
    if (getenv("SDL_VIDEODRIVER")) return;
    for (int i = 0; i < SDL_GetNumVideoDrivers(); ++i) {
        const char *name = SDL_GetVideoDriver(i);
        if (name && !strcmp(name, "wayland")) {
            SDL_SetHintWithPriority(SDL_HINT_VIDEODRIVER, "wayland", SDL_HINT_NORMAL);
            fprintf(stderr, "seedlessds[video]: default SDL driver set to wayland\n");
            return;
        }
    }
}

static int refresh_geometry(seedless_sdl_video_t *video,
                            char *error, size_t capacity)
{
    seedless_sdl_video_impl_t *impl = video->implementation;
    int geometry_changed = 0;
    for (unsigned i = 0; i < impl->texture_count; ++i) {
        Uint32 flags = SDL_GetWindowFlags(impl->windows[i]);
        if (!(flags & SDL_WINDOW_FULLSCREEN))
            return fail(error, capacity, "fullscreen state was lost");
        int actual_display = SDL_GetWindowDisplayIndex(impl->windows[i]);
        if (actual_display != (int)i)
            return fail(error, capacity, "fullscreen window moved to another display");
        int width = 0, height = 0;
        if (impl->wayland_surface_presenter) {
            impl->surfaces[i] = SDL_GetWindowSurface(impl->windows[i]);
            if (!impl->surfaces[i])
                return fail(error, capacity, "querying Wayland window surface failed");
            width = impl->surfaces[i]->w;
            height = impl->surfaces[i]->h;
        } else if (SDL_GetRendererOutputSize(impl->renderers[i], &width, &height) != 0) {
            return fail(error, capacity, "querying renderer pixel dimensions failed");
        }
        impl->displays[i].renderer_width_px = width;
        impl->displays[i].renderer_height_px = height;
        if (!impl->logged_geometry[i] || width != impl->last_renderer_width[i] ||
            height != impl->last_renderer_height[i]) {
            int window_width = 0, window_height = 0, pixel_width = 0, pixel_height = 0;
            int drawable_width = 0, drawable_height = 0, logical_width = 0, logical_height = 0;
            int x = 0, y = 0;
            SDL_GetWindowSize(impl->windows[i], &window_width, &window_height);
            SDL_GetWindowSizeInPixels(impl->windows[i], &pixel_width, &pixel_height);
            SDL_GL_GetDrawableSize(impl->windows[i], &drawable_width, &drawable_height);
            if (!impl->wayland_surface_presenter)
                SDL_RenderGetLogicalSize(impl->renderers[i], &logical_width, &logical_height);
            SDL_GetWindowPosition(impl->windows[i], &x, &y);
            fprintf(stderr,
                    "seedlessds[video]: output_geometry output=%u display=%d window=%dx%d@%d,%d pixels=%dx%d drawable=%dx%d presenter=%s pixels_out=%dx%d logical=%dx%d fullscreen=%s desktop=%dx%d\n",
                    i, actual_display, window_width, window_height, x, y,
                    pixel_width, pixel_height, drawable_width, drawable_height,
                    impl->wayland_surface_presenter ? "window_surface" : "renderer",
                    width, height, logical_width, logical_height,
                    (flags & SDL_WINDOW_FULLSCREEN) ? "true" : "false",
                    impl->displays[i].desktop_width, impl->displays[i].desktop_height);
            impl->logged_geometry[i] = 1;
            impl->last_renderer_width[i] = width;
            impl->last_renderer_height[i] = height;
            geometry_changed = 1;
        }
    }
    if (!seedless_video_compute_layout(&impl->preferences, impl->displays,
                                       impl->physical_count, &video->layout,
                                       error, capacity))
        return 0;
    if (geometry_changed) {
        fprintf(stderr,
                "seedlessds[video]: layout_update effective_mode=%s outputs=%u destinations=%u common_scale=%u\n",
                mode_name(video->layout.effective_mode), video->layout.output_count,
                video->layout.destination_count, video->layout.common_scale);
        for (unsigned i = 0; i < video->layout.destination_count; ++i) {
            const seedless_video_destination_t *d = &video->layout.destinations[i];
            fprintf(stderr,
                    "seedlessds[video]: layout_destination=%u display=%u image=%s integer_scale=%u rect=%d,%d %dx%d\n",
                    i, d->display_index, d->image_index == 0 ? "top" : "bottom",
                    d->integer_scale, d->rect.x, d->rect.y,
                    d->rect.width, d->rect.height);
        }
    }
    return 1;
}

static void destroy_impl(seedless_sdl_video_impl_t *impl)
{
    if (!impl) return;
    for (unsigned i = 0; i < 2; ++i) {
        if (impl->textures[i]) SDL_DestroyTexture(impl->textures[i]);
        impl->textures[i] = NULL;
    }
    for (unsigned i = 0; i < SEEDLESS_VIDEO_MAX_OUTPUTS; ++i) {
        if (impl->renderers[i]) SDL_DestroyRenderer(impl->renderers[i]);
        impl->renderers[i] = NULL;
        free(impl->surface_pixels[i]);
        impl->surface_pixels[i] = NULL;
        impl->surfaces[i] = NULL;
        if (impl->windows[i]) SDL_DestroyWindow(impl->windows[i]);
        impl->windows[i] = NULL;
    }
    if (impl->sdl_started) SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    free(impl);
}

static SDL_Texture *create_screen_texture(SDL_Renderer *renderer,
                                          char *error, size_t capacity)
{
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING, SEEDLESS_VIDEO_SCREEN_WIDTH,
        SEEDLESS_VIDEO_SCREEN_HEIGHT);
    if (!texture) {
        fail(error, capacity, "creating RGBA32 streaming texture failed");
        return NULL;
    }
    SDL_ScaleMode actual_mode = SDL_ScaleModeNearest;
    if (SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest) != 0 ||
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE) != 0 ||
        SDL_GetTextureScaleMode(texture, &actual_mode) != 0 ||
        actual_mode != SDL_ScaleModeNearest) {
        fail(error, capacity, "configuring nearest unblended texture failed");
        SDL_DestroyTexture(texture);
        return NULL;
    }
    return texture;
}

static int upload_frame(SDL_Texture *textures[2],
                        const seedless_video_layout_t *layout,
                        const seedless_video_frame_pair_t *pair,
                        char *error, size_t capacity)
{
    for (unsigned i = 0; i < layout->destination_count; ++i) {
        const seedless_video_destination_t *destination = &layout->destinations[i];
        const void *pixels = destination->image_index == 0 ? pair->top_rgba :
                                                            pair->bottom_rgba;
        if (SDL_UpdateTexture(textures[i], NULL, pixels, SEEDLESS_VIDEO_PITCH) != 0)
            return fail(error, capacity, "uploading RGBA frame failed");
    }
    return 1;
}

static int render_output(SDL_Renderer *renderer, SDL_Texture *textures[2],
                         const seedless_video_layout_t *layout,
                         unsigned output_index, char *error, size_t capacity)
{
    if (SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255) != 0 ||
        SDL_RenderClear(renderer) != 0)
        return fail(error, capacity, "clearing renderer failed");
    for (unsigned i = 0; i < layout->destination_count; ++i) {
        const seedless_video_destination_t *destination = &layout->destinations[i];
        if (destination->display_index != output_index) continue;
        SDL_Rect target = {destination->rect.x, destination->rect.y,
                           destination->rect.width, destination->rect.height};
        if (SDL_RenderCopy(renderer, textures[i], NULL, &target) != 0)
            return fail(error, capacity, "drawing scaled frame failed");
    }
    SDL_RenderPresent(renderer);
    return 1;
}

static int draw_surface_pixels(seedless_sdl_video_impl_t *impl,
                               SDL_Surface *surface,
                               const seedless_video_layout_t *layout,
                               const seedless_video_frame_pair_t *pair,
                               unsigned output_index,
                               char *error, size_t capacity)
{
    if (!surface || surface->format->BytesPerPixel != 4)
        return fail(error, capacity, "Wayland window surface is unavailable or not 32-bit");

    size_t converted_size = (size_t)SEEDLESS_VIDEO_PITCH * SEEDLESS_VIDEO_SCREEN_HEIGHT;
    if (impl->surface_pixels_capacity[output_index] < converted_size ||
        impl->surface_pixel_format[output_index] != surface->format->format) {
        uint8_t *converted = realloc(impl->surface_pixels[output_index], converted_size);
        if (!converted) return fail(error, capacity, "allocating Wayland surface frame failed");
        impl->surface_pixels[output_index] = converted;
        impl->surface_pixels_capacity[output_index] = converted_size;
        impl->surface_pixel_format[output_index] = surface->format->format;
    }

    if (SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, 0, 0, 0)) != 0)
        return fail(error, capacity, "clearing Wayland window surface failed");
    int locked = 0;
    if (SDL_MUSTLOCK(surface)) {
        if (SDL_LockSurface(surface) != 0)
            return fail(error, capacity, "locking Wayland window surface failed");
        locked = 1;
    }
    for (unsigned i = 0; i < layout->destination_count; ++i) {
        const seedless_video_destination_t *destination = &layout->destinations[i];
        if (destination->display_index != output_index) continue;
        const uint8_t *source = destination->image_index == 0 ? pair->top_rgba : pair->bottom_rgba;
        const int scale = (int)destination->integer_scale;
        const SDL_Rect rect = {destination->rect.x, destination->rect.y,
                               destination->rect.width, destination->rect.height};
        if (scale < 1 || rect.x < 0 || rect.y < 0 ||
            rect.x + rect.w > surface->w || rect.y + rect.h > surface->h ||
            rect.w != SEEDLESS_VIDEO_SCREEN_WIDTH * scale ||
            rect.h != SEEDLESS_VIDEO_SCREEN_HEIGHT * scale) {
            if (locked) SDL_UnlockSurface(surface);
            return fail(error, capacity, "Wayland integer-scaled destination exceeds its surface");
        }
        if (SDL_ConvertPixels(SEEDLESS_VIDEO_SCREEN_WIDTH, SEEDLESS_VIDEO_SCREEN_HEIGHT,
                              SDL_PIXELFORMAT_RGBA32, source, SEEDLESS_VIDEO_PITCH,
                              surface->format->format, impl->surface_pixels[output_index],
                              SEEDLESS_VIDEO_PITCH) != 0) {
            if (locked) SDL_UnlockSurface(surface);
            return fail(error, capacity, "converting frame for Wayland window surface failed");
        }
        uint8_t *row = malloc((size_t)rect.w * 4u);
        if (!row) {
            if (locked) SDL_UnlockSurface(surface);
            return fail(error, capacity, "allocating Wayland scaled row failed");
        }
        for (int sy = 0; sy < SEEDLESS_VIDEO_SCREEN_HEIGHT; ++sy) {
            const uint8_t *src = impl->surface_pixels[output_index] + (size_t)sy * SEEDLESS_VIDEO_PITCH;
            for (int sx = 0; sx < SEEDLESS_VIDEO_SCREEN_WIDTH; ++sx) {
                Uint32 pixel;
                memcpy(&pixel, src + (size_t)sx * 4u, sizeof pixel);
                for (int dx = 0; dx < scale; ++dx)
                    ((Uint32 *)row)[sx * scale + dx] = pixel;
            }
            for (int dy = 0; dy < scale; ++dy) {
                uint8_t *dst = (uint8_t *)surface->pixels +
                    (size_t)(rect.y + sy * scale + dy) * surface->pitch +
                    (size_t)rect.x * 4u;
                memcpy(dst, row, (size_t)rect.w * 4u);
            }
        }
        free(row);
    }
    if (locked) SDL_UnlockSurface(surface);
    return 1;
}

static int render_surface_output(seedless_sdl_video_impl_t *impl,
                                const seedless_video_layout_t *layout,
                                const seedless_video_frame_pair_t *pair,
                                unsigned output_index,
                                char *error, size_t capacity)
{
    SDL_Surface *surface = SDL_GetWindowSurface(impl->windows[output_index]);
    if (!surface) return fail(error, capacity, "querying Wayland window surface failed");
    impl->surfaces[output_index] = surface;
    if (!draw_surface_pixels(impl, surface, layout, pair, output_index,
                             error, capacity)) return 0;
    if (SDL_UpdateWindowSurface(impl->windows[output_index]) != 0)
        return fail(error, capacity, "presenting Wayland window surface failed");
    return 1;
}

void seedless_sdl_video_destroy(seedless_sdl_video_t *video)
{
    if (!video) return;
    destroy_impl(video->implementation);
    memset(video, 0, sizeof *video);
}

int seedless_sdl_video_init(seedless_sdl_video_t *video,
                            const seedless_video_preferences_t *preferences,
                            char *error, size_t error_capacity)
{
    if (error && error_capacity) error[0] = '\0';
    if (!video || !preferences)
        return fail(error, error_capacity, "invalid SDL video initialization input");
    memset(video, 0, sizeof *video);
    seedless_sdl_video_impl_t *impl = calloc(1, sizeof *impl);
    if (!impl) return fail(error, error_capacity, "allocating SDL video state failed");
    video->implementation = impl;

    select_default_video_driver();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        fail(error, error_capacity, "initializing SDL video/events failed");
        seedless_sdl_video_destroy(video);
        return 0;
    }
    impl->sdl_started = 1;
    impl->preferences = *preferences;
    print_version();
    const char *driver = SDL_GetCurrentVideoDriver();
    impl->wayland_surface_presenter = driver && !strcmp(driver, "wayland");
    if (!driver || !strcmp(driver, "dummy") || !strcmp(driver, "offscreen")) {
        if (error && error_capacity)
            snprintf(error, error_capacity,
                     "SDL video driver '%s' is not a physical gameplay backend",
                     driver ? driver : "unknown");
        seedless_sdl_video_destroy(video);
        return 0;
    }
    int count = SDL_GetNumVideoDisplays();
    if (count <= 0) {
        fail(error, error_capacity, "enumerating SDL displays failed");
        seedless_sdl_video_destroy(video);
        return 0;
    }
    impl->physical_count = (unsigned)count;
    for (int i = 0; i < count; ++i) {
        SDL_Rect bounds = {0};
        SDL_DisplayMode desktop = {0};
        const char *name = SDL_GetDisplayName(i);
        if (SDL_GetDisplayBounds(i, &bounds) != 0 || SDL_GetDesktopDisplayMode(i, &desktop) != 0) {
            fail(error, error_capacity, "querying SDL display inventory failed");
            seedless_sdl_video_destroy(video);
            return 0;
        }
        fprintf(stderr,
                "seedlessds[video]: display index=%d role=%s name=%s bounds=%d,%d %dx%d desktop=%dx%d\n",
                i, i == 0 ? "primary" : i == 1 ? "secondary" : "unused",
                name ? name : "unknown", bounds.x, bounds.y, bounds.w, bounds.h,
                desktop.w, desktop.h);
        if (i < SEEDLESS_VIDEO_MAX_OUTPUTS) {
            seedless_video_display_t *display = &impl->displays[i];
            display->sdl_index = i;
            snprintf(display->name, sizeof display->name, "%s", name ? name : "unknown");
            display->bounds = (seedless_video_rect_t){bounds.x, bounds.y, bounds.w, bounds.h};
            display->desktop_width = desktop.w;
            display->desktop_height = desktop.h;
            display->role = i == 0 ? SEEDLESS_VIDEO_DISPLAY_PRIMARY :
                                     SEEDLESS_VIDEO_DISPLAY_SECONDARY;
        }
    }

    impl->texture_count = preferences->requested_mode == SEEDLESS_VIDEO_MODE_SINGLE_SCREEN ||
                          count == 1 ? 1u : 2u;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");
    for (unsigned i = 0; i < impl->texture_count; ++i) {
        int width = 256, height = 192;
        impl->windows[i] = SDL_CreateWindow("SeedlessDS",
            SDL_WINDOWPOS_CENTERED_DISPLAY((int)i), SDL_WINDOWPOS_CENTERED_DISPLAY((int)i),
            width, height, SDL_WINDOW_FULLSCREEN_DESKTOP);
        if (!impl->windows[i]) {
            fail(error, error_capacity, "creating fullscreen output window failed");
            seedless_sdl_video_destroy(video);
            return 0;
        }
    }

    /* Wayland applies fullscreen asynchronously. Wait for each window to
       settle on its assigned output before creating its presentation surface. */
    for (unsigned i = 0; i < impl->texture_count; ++i) {
        int width = 0, height = 0, display_index = -1;
        Uint32 flags = 0;
        int attempts;
        for (attempts = 0; attempts < 200; ++attempts) {
            SDL_PumpEvents();
            display_index = SDL_GetWindowDisplayIndex(impl->windows[i]);
            flags = SDL_GetWindowFlags(impl->windows[i]);
            SDL_GetWindowSize(impl->windows[i], &width, &height);
            if (display_index == (int)i && (flags & SDL_WINDOW_FULLSCREEN) &&
                width >= impl->displays[i].desktop_width &&
                height >= impl->displays[i].desktop_height)
                break;
            SDL_Delay(10);
        }
        if (attempts == 200) {
            fprintf(stderr,
                    "seedlessds[video]: fullscreen_config_timeout output=%u display=%d window=%dx%d desktop=%dx%d flags=0x%x\n",
                    i, display_index, width, height, impl->displays[i].desktop_width,
                    impl->displays[i].desktop_height,
                    flags);
            if (error && error_capacity)
                snprintf(error, error_capacity,
                         "fullscreen output %u did not configure on display %d at desktop size (%dx%d, got %dx%d)",
                         i, display_index, impl->displays[i].desktop_width,
                         impl->displays[i].desktop_height, width, height);
            seedless_sdl_video_destroy(video);
            return 0;
        }
    }

    for (unsigned i = 0; i < impl->texture_count; ++i) {
        if (impl->wayland_surface_presenter) {
            impl->surfaces[i] = SDL_GetWindowSurface(impl->windows[i]);
            if (!impl->surfaces[i]) {
                fail(error, error_capacity, "creating Wayland window surface failed");
                seedless_sdl_video_destroy(video);
                return 0;
            }
            int width = impl->surfaces[i]->w, height = impl->surfaces[i]->h;
            impl->displays[i].renderer_width_px = width;
            impl->displays[i].renderer_height_px = height;
            fprintf(stderr,
                    "seedlessds[video]: output=%u presenter=window_surface format=%s bpp=%u pixels=%dx%d desktop=%dx%d\n",
                    i, SDL_GetPixelFormatName(impl->surfaces[i]->format->format),
                    impl->surfaces[i]->format->BytesPerPixel, width, height,
                    impl->displays[i].desktop_width, impl->displays[i].desktop_height);
            continue;
        }
        impl->renderers[i] = SDL_CreateRenderer(impl->windows[i], -1,
                                                SDL_RENDERER_ACCELERATED);
        if (!impl->renderers[i])
            impl->renderers[i] = SDL_CreateRenderer(impl->windows[i], -1,
                                                    SDL_RENDERER_SOFTWARE);
        if (!impl->renderers[i]) {
            fail(error, error_capacity, "creating SDL renderer failed");
            seedless_sdl_video_destroy(video);
            return 0;
        }
        SDL_RendererInfo info = {0};
        int width = 0, height = 0;
        SDL_GetRendererOutputSize(impl->renderers[i], &width, &height);
        int pixel_width = 0, pixel_height = 0, drawable_width = 0, drawable_height = 0;
        SDL_GetWindowSizeInPixels(impl->windows[i], &pixel_width, &pixel_height);
        SDL_GL_GetDrawableSize(impl->windows[i], &drawable_width, &drawable_height);
        if (SDL_GetRendererInfo(impl->renderers[i], &info) == 0)
            fprintf(stderr,
                    "seedlessds[video]: output=%u renderer=%s flags=0x%x renderer_pixels=%dx%d window_pixels=%dx%d drawable=%dx%d desktop=%dx%d\n",
                    i, info.name ? info.name : "unknown", info.flags, width, height,
                    pixel_width, pixel_height, drawable_width, drawable_height,
                    impl->displays[i].desktop_width, impl->displays[i].desktop_height);
    }

    if (!refresh_geometry(video, error, error_capacity)) {
        seedless_sdl_video_destroy(video);
        return 0;
    }
    fprintf(stderr,
            "seedlessds[video]: requested_mode=%s effective_mode=%s layout=%s swap=%s\n",
            requested_mode_name(preferences->requested_mode),
            mode_name(video->layout.effective_mode), layout_name(preferences->single_layout),
            preferences->swap_screens ? "true" : "false");
    if (impl->physical_count < 2 &&
        preferences->requested_mode != SEEDLESS_VIDEO_MODE_SINGLE_SCREEN)
        fprintf(stderr, "seedlessds[video]: dual output requested; using one-output fallback\n");

    for (unsigned i = 0; !impl->wayland_surface_presenter &&
                         i < video->layout.destination_count; ++i) {
        impl->textures[i] = create_screen_texture(
            impl->renderers[video->layout.destinations[i].display_index],
            error, error_capacity);
        if (!impl->textures[i]) {
            seedless_sdl_video_destroy(video);
            return 0;
        }
    }
    for (unsigned i = 0; i < video->layout.output_count; ++i) {
        if (impl->wayland_surface_presenter) {
            SDL_Surface *surface = SDL_GetWindowSurface(impl->windows[i]);
            if (!surface || SDL_FillRect(surface, NULL,
                    SDL_MapRGB(surface->format, 0, 0, 0)) != 0 ||
                SDL_UpdateWindowSurface(impl->windows[i]) != 0) {
                fail(error, error_capacity, "clearing fullscreen Wayland output failed");
                seedless_sdl_video_destroy(video);
                return 0;
            }
            continue;
        }
        if (SDL_SetRenderDrawColor(impl->renderers[i], 0, 0, 0, 255) != 0 ||
            SDL_RenderClear(impl->renderers[i]) != 0) {
            fail(error, error_capacity, "clearing fullscreen output failed");
            seedless_sdl_video_destroy(video);
            return 0;
        }
        SDL_RenderPresent(impl->renderers[i]);
    }
    for (unsigned i = 0; i < video->layout.destination_count; ++i) {
        const seedless_video_destination_t *destination = &video->layout.destinations[i];
        const seedless_video_display_t *display = &impl->displays[destination->display_index];
        fprintf(stderr,
                "seedlessds[video]: destination=%u display=%s pixels=%dx%d image=%s slot=%u scale=%u rect=%d,%d %dx%d\n",
                i, destination->display_index == 0 ? "primary" : "secondary",
                display->renderer_width_px, display->renderer_height_px,
                destination->image_index == 0 ? "top" : "bottom",
                destination->slot_index, destination->integer_scale,
                destination->rect.x, destination->rect.y,
                destination->rect.width, destination->rect.height);
    }
    video->initialized = 1;
    return 1;
}

int seedless_sdl_video_present(seedless_sdl_video_t *video,
                               const seedless_video_frame_pair_t *pair,
                               char *error, size_t error_capacity)
{
    if (!video || !video->initialized || !pair || !pair->top_rgba ||
        !pair->bottom_rgba || pair->sequence <=
        ((seedless_sdl_video_impl_t *)video->implementation)->last_sequence)
        return fail(error, error_capacity, "invalid or repeated frame pair");
    seedless_sdl_video_impl_t *impl = video->implementation;
    if (!impl->wayland_surface_presenter &&
        !upload_frame(impl->textures, &video->layout, pair, error, error_capacity))
        return 0;
    for (unsigned i = 0; i < video->layout.output_count; ++i) {
        if (impl->wayland_surface_presenter) {
            if (!render_surface_output(impl, &video->layout, pair, i,
                                      error, error_capacity)) return 0;
        } else if (!render_output(impl->renderers[i], impl->textures,
                                  &video->layout, i, error, error_capacity)) {
            return 0;
        }
        if (!impl->presented_sequence[i] || pair->sequence / 300u != impl->presented_sequence[i] / 300u) {
            fprintf(stderr,
                    "seedlessds[video]: frame_present sequence=%" PRIu64 " output=%u display=%d renderer=%dx%d\n",
                    pair->sequence, i, SDL_GetWindowDisplayIndex(impl->windows[i]),
                    impl->displays[i].renderer_width_px,
                    impl->displays[i].renderer_height_px);
        }
        impl->presented_sequence[i] = pair->sequence;
    }
    impl->last_sequence = pair->sequence;
    return 1;
}

#ifdef SEEDLESS_VIDEO_TESTING
static void readback_source_pixel(const uint8_t *image, unsigned x, unsigned y,
                                  uint8_t color[4])
{
    memcpy(color, image + ((size_t)y * SEEDLESS_VIDEO_SCREEN_WIDTH + x) * 4, 4);
}

int seedless_sdl_video_readback_test(char *error, size_t error_capacity)
{
    if (error && error_capacity) error[0] = '\0';
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(
        0, 1024, 768, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surface) return fail(error, error_capacity, "creating readback surface failed");
    SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(surface);
    if (!renderer) {
        fail(error, error_capacity, "creating readback software renderer failed");
        SDL_FreeSurface(surface);
        return 0;
    }
    uint8_t *images = malloc(2u * SEEDLESS_VIDEO_IMAGE_BYTES);
    uint8_t *readback = malloc(1024u * 768u * 4u);
    if (!images || !readback) {
        if (error && error_capacity) snprintf(error, error_capacity, "readback test allocation failed");
        free(images); free(readback);
        SDL_DestroyRenderer(renderer); SDL_FreeSurface(surface);
        return 0;
    }
    seedless_video_frame_pair_t pair = {
        .top_rgba = images,
        .bottom_rgba = images + SEEDLESS_VIDEO_IMAGE_BYTES,
        .sequence = 1,
    };
    for (unsigned y = 0; y < SEEDLESS_VIDEO_SCREEN_HEIGHT; ++y) {
        for (unsigned x = 0; x < SEEDLESS_VIDEO_SCREEN_WIDTH; ++x) {
            uint8_t *top = pair.top_rgba + ((size_t)y * 256 + x) * 4;
            uint8_t *bottom = pair.bottom_rgba + ((size_t)y * 256 + x) * 4;
            top[0] = x % 3 == 0 ? 255 : 0;
            top[1] = x % 3 == 1 ? 255 : 0;
            top[2] = x % 3 == 2 ? 255 : 0;
            top[3] = 255;
            bottom[0] = y % 3 == 0 ? 255 : 0;
            bottom[1] = y % 3 == 1 ? 255 : 0;
            bottom[2] = y % 3 == 2 ? 255 : 0;
            bottom[3] = 255;
        }
    }
    seedless_video_display_t display = { .renderer_width_px = 1024,
                                          .renderer_height_px = 768 };
    int okay = 1;
    for (unsigned horizontal = 0; horizontal < 2 && okay; ++horizontal) {
        for (unsigned swapped = 0; swapped < 2 && okay; ++swapped) {
            seedless_video_preferences_t preferences = {
                .requested_mode = SEEDLESS_VIDEO_MODE_SINGLE_SCREEN,
                .single_layout = horizontal ? SEEDLESS_VIDEO_LAYOUT_HORIZONTAL :
                                              SEEDLESS_VIDEO_LAYOUT_VERTICAL,
                .swap_screens = (int)swapped,
            };
            seedless_video_layout_t layout;
            if (!seedless_video_compute_layout(&preferences, &display, 1,
                                               &layout, error, error_capacity)) {
                okay = 0;
                break;
            }
            SDL_Texture *textures[2] = {0};
            for (unsigned i = 0; i < 2; ++i) {
                textures[i] = create_screen_texture(renderer, error, error_capacity);
                if (!textures[i]) { okay = 0; break; }
            }
            if (okay && upload_frame(textures, &layout, &pair, error, error_capacity) &&
                render_output(renderer, textures, &layout, 0, error, error_capacity) &&
                SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_RGBA32,
                                     readback, 1024 * 4) == 0) {
                uint8_t border[4];
                memcpy(border, readback, sizeof border);
                if (border[0] || border[1] || border[2] || border[3] != 255) okay = 0;
                for (unsigned d = 0; d < layout.destination_count && okay; ++d) {
                    const seedless_video_destination_t *destination = &layout.destinations[d];
                    const uint8_t *image = destination->image_index == 0 ?
                                           pair.top_rgba : pair.bottom_rgba;
                    unsigned scale = destination->integer_scale;
                    for (unsigned y = 0; y < 192 && okay; ++y) {
                        for (unsigned x = 0; x < 256 && okay; ++x) {
                            uint8_t expected[4];
                            readback_source_pixel(image, x, y, expected);
                            for (unsigned dy = 0; dy < scale && okay; ++dy) {
                                for (unsigned dx = 0; dx < scale; ++dx) {
                                    size_t offset = ((size_t)(destination->rect.y + (int)y * (int)scale + (int)dy) * 1024u +
                                                     (size_t)(destination->rect.x + (int)x * (int)scale + (int)dx)) * 4u;
                                    if (memcmp(readback + offset, expected, 4)) {
                                        okay = 0;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
                if (!okay && error && error_capacity)
                    snprintf(error, error_capacity,
                             "nearest readback differed from expected source pixels");
            } else if (okay) {
                fail(error, error_capacity, "rendering single-layout readback failed");
                okay = 0;
            }
            if (okay) {
                seedless_sdl_video_impl_t surface_impl = {0};
                if (!draw_surface_pixels(&surface_impl, surface, &layout, &pair,
                                         0, error, error_capacity)) {
                    okay = 0;
                } else {
                    for (unsigned d = 0; d < layout.destination_count && okay; ++d) {
                        const seedless_video_destination_t *destination = &layout.destinations[d];
                        const uint8_t *image = destination->image_index == 0 ?
                                               pair.top_rgba : pair.bottom_rgba;
                        const unsigned scale = destination->integer_scale;
                        for (unsigned y = 0; y < 192 && okay; ++y) {
                            for (unsigned x = 0; x < 256 && okay; ++x) {
                                uint8_t expected[4];
                                readback_source_pixel(image, x, y, expected);
                                for (unsigned dy = 0; dy < scale && okay; ++dy) {
                                    for (unsigned dx = 0; dx < scale; ++dx) {
                                        const size_t offset =
                                            (size_t)(destination->rect.y + (int)y * (int)scale + (int)dy) * surface->pitch +
                                            (size_t)(destination->rect.x + (int)x * (int)scale + (int)dx) * 4u;
                                        if (memcmp((const uint8_t *)surface->pixels + offset,
                                                   expected, 4)) {
                                            okay = 0;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    if (!okay && error && error_capacity)
                        snprintf(error, error_capacity,
                                 "Wayland surface scaling differed from exact source pixels");
                }
                free(surface_impl.surface_pixels[0]);
            }
            for (unsigned i = 0; i < 2; ++i)
                if (textures[i]) SDL_DestroyTexture(textures[i]);
        }
    }
    free(images); free(readback);
    SDL_DestroyRenderer(renderer);
    SDL_FreeSurface(surface);
    return okay;
}
#endif

int seedless_sdl_video_poll(seedless_sdl_video_t *video, int *should_quit,
                            char *error, size_t error_capacity)
{
    if (!video || !video->initialized || !should_quit)
        return fail(error, error_capacity, "invalid SDL event poll input");
    *should_quit = 0;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT ||
            (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE))
            *should_quit = 1;
        if (event.type == SDL_DISPLAYEVENT &&
            event.display.event == SDL_DISPLAYEVENT_DISCONNECTED &&
            (unsigned)event.display.display < video->layout.output_count)
            return fail(error, error_capacity, "selected SDL display changed or was removed");
    }
    return refresh_geometry(video, error, error_capacity);
}
