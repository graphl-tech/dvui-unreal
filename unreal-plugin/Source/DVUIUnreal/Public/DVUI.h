// Copyright Epic Games, Inc. All Rights Reserved.
//
// C ABI between the Zig DVUI backend (libdvui_unreal.a) and the UE plugin.
// Stable codes for keys/buttons/cursors/modifiers — translated to dvui's
// own enums inside main.zig so a dvui dep bump can't silently change the
// wire format.

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DVUIUnrealBackend DVUIUnrealBackend;
typedef struct DVUIUnrealContext DVUIUnrealContext;

// Vertex structure (matches Zig CVertex: x, y, u, v, color)
// Color is RGBA packed as 0xAABBGGRR on little-endian (premultiplied alpha)
typedef struct {
    float x;
    float y;
    float u;
    float v;
    uint32_t color;
} DVUIVertex;

// Callback function types

// Render a batch of indexed triangles
// clip_rect is NULL when no clipping is needed, otherwise [x, y, w, h]
// texture_ptr is 0 for no texture (use default white), otherwise an opaque pointer
typedef void (*DVUIRenderTrianglesCallback)(
    DVUIUnrealContext* ctx,
    const DVUIVertex* vertices,
    uint32_t vertex_count,
    const uint32_t* indices,
    uint32_t index_count,
    uintptr_t texture_ptr,
    const float* clip_rect
);

typedef uint64_t (*DVUIGetTimeNsCallback)(DVUIUnrealContext* ctx);
typedef const char* (*DVUIGetClipboardCallback)(DVUIUnrealContext* ctx);
typedef void (*DVUISetClipboardCallback)(DVUIUnrealContext* ctx, const char* text);
typedef float (*DVUIGetDpiScaleCallback)(DVUIUnrealContext* ctx);
typedef void (*DVUIGetPixelSizeCallback)(DVUIUnrealContext* ctx, uint32_t* width, uint32_t* height);

// Texture lifecycle. The id is the dvui-side heap pointer of the pixel
// buffer, used as a stable key. Pixels are RGBA (R first), 4 bytes per
// pixel, no padding, w*h*4 total bytes. The host should copy out anything
// it needs before returning — dvui retains ownership of the buffer.
typedef void (*DVUITextureCreateCallback)(
    DVUIUnrealContext* ctx,
    uintptr_t texture_id,
    const uint8_t* pixels,
    uint32_t width,
    uint32_t height
);
typedef void (*DVUITextureDestroyCallback)(
    DVUIUnrealContext* ctx,
    uintptr_t texture_id
);

// Callbacks structure (must match Zig definition)
typedef struct {
    DVUIRenderTrianglesCallback render_triangles;
    DVUIGetTimeNsCallback get_time_ns;
    DVUIGetClipboardCallback get_clipboard;
    DVUISetClipboardCallback set_clipboard;
    DVUIGetDpiScaleCallback get_dpi_scale;
    DVUIGetPixelSizeCallback get_pixel_size;
    DVUITextureCreateCallback texture_create;
    DVUITextureDestroyCallback texture_destroy;
} DVUIUnrealCallbacks;

// C API exported from Zig

DVUIUnrealBackend* dvui_unreal_backend_create(
    DVUIUnrealContext* unreal_ctx,
    const DVUIUnrealCallbacks* callbacks
);

void dvui_unreal_backend_destroy(DVUIUnrealBackend* backend);

void dvui_backend_render_frame(DVUIUnrealBackend* backend);

void dvui_unreal_backend_get_pixel_size(
    DVUIUnrealBackend* backend,
    uint32_t* width,
    uint32_t* height
);

float dvui_unreal_backend_get_content_scale(DVUIUnrealBackend* backend);

// =============================================================================
// Input event surface (mirrors blender-dvui src/lib.zig). Coordinates are in
// the same space as dvui's canvas (matches what get_pixel_size returns).
// =============================================================================

// Stable mouse button IDs.
#define DVUI_BTN_LEFT   1
#define DVUI_BTN_MIDDLE 2
#define DVUI_BTN_RIGHT  3

// Stable key codes — independent of dvui's internal enum so the host doesn't
// silently break when the Zig dep is bumped.
#define DVUI_KEY_BACKSPACE 1
#define DVUI_KEY_TAB       2
#define DVUI_KEY_ENTER     3
#define DVUI_KEY_ESCAPE    4
#define DVUI_KEY_SPACE     5
#define DVUI_KEY_LEFT      10
#define DVUI_KEY_RIGHT     11
#define DVUI_KEY_UP        12
#define DVUI_KEY_DOWN      13
#define DVUI_KEY_HOME      14
#define DVUI_KEY_END       15
#define DVUI_KEY_PAGE_UP   16
#define DVUI_KEY_PAGE_DOWN 17
#define DVUI_KEY_INSERT    18
#define DVUI_KEY_DELETE    19
#define DVUI_KEY_F1        30   // F1..F12 = 30..41
#define DVUI_KEY_LSHIFT    50
#define DVUI_KEY_RSHIFT    51
#define DVUI_KEY_LCTRL     52
#define DVUI_KEY_RCTRL     53
#define DVUI_KEY_LALT      54
#define DVUI_KEY_RALT      55
#define DVUI_KEY_LCMD      56
#define DVUI_KEY_RCMD      57
#define DVUI_KEY_A_BASE    100  // letters: BASE + (lowercase - 'a')
#define DVUI_KEY_NUM_BASE  200  // digits: BASE + (digit - '0')

// Modifier bitmask.
#define DVUI_MOD_NONE  0
#define DVUI_MOD_SHIFT 1
#define DVUI_MOD_CTRL  2
#define DVUI_MOD_ALT   4
#define DVUI_MOD_CMD   8

void dvui_event_mouse_motion(DVUIUnrealBackend* backend, float x, float y);
void dvui_event_mouse_button(DVUIUnrealBackend* backend, int button, int pressed);
void dvui_event_mouse_wheel(DVUIUnrealBackend* backend, float dx, float dy);
void dvui_event_key(DVUIUnrealBackend* backend, int key, int pressed, int mods);
void dvui_event_text(DVUIUnrealBackend* backend, const char* utf8, uint32_t len);
void dvui_event_window_close(DVUIUnrealBackend* backend);
int  dvui_text_input_active(DVUIUnrealBackend* backend);

// Cursor enum (matches dvui's enums.Cursor enum(u8) order — keep in sync).
#define DVUI_CURSOR_ARROW       0
#define DVUI_CURSOR_IBEAM       1
#define DVUI_CURSOR_WAIT        2
#define DVUI_CURSOR_WAIT_ARROW  3
#define DVUI_CURSOR_CROSSHAIR   4
#define DVUI_CURSOR_NW_SE       5
#define DVUI_CURSOR_NE_SW       6
#define DVUI_CURSOR_W_E         7
#define DVUI_CURSOR_N_S         8
#define DVUI_CURSOR_ALL         9
#define DVUI_CURSOR_BAD         10
#define DVUI_CURSOR_HAND        11
#define DVUI_CURSOR_HIDDEN      12

int  dvui_cursor_requested(DVUIUnrealBackend* backend);

#ifdef __cplusplus
}
#endif
