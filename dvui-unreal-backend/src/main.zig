// C API Exports for the DVUI Unreal Backend
//
// This module provides the C-callable interface that the Unreal plugin links against.
// The actual backend implementation is in backend.zig.

const std = @import("std");
const dvui = @import("dvui");
const Backend = @import("backend");
const UnrealBackend = Backend.UnrealBackend;

export fn dvui_unreal_backend_create(
    unreal_ctx: *Backend.UnrealContext,
    callbacks: *const Backend.UnrealCallbacks,
) ?*UnrealBackend {
    return UnrealBackend.create(
        std.heap.c_allocator,
        unreal_ctx,
        callbacks.*,
    ) catch null;
}

export fn dvui_unreal_backend_destroy(self: *UnrealBackend) void {
    self.destroy();
}

export fn dvui_backend_render_frame(self: *UnrealBackend) void {
    self.renderFrame();
}

export fn dvui_unreal_backend_get_pixel_size(self: *UnrealBackend, w: *u32, h: *u32) void {
    const size = self.pixelSize();
    w.* = @intFromFloat(size.w);
    h.* = @intFromFloat(size.h);
}

export fn dvui_unreal_backend_get_content_scale(self: *UnrealBackend) f32 {
    return self.contentScale();
}

// =============================================================================
// Input event surface — stable C-ABI codes mapped to dvui enums internally
// so a dvui dep bump can't silently change the wire format.
// =============================================================================

const KEY_BACKSPACE: c_int = 1;
const KEY_TAB: c_int = 2;
const KEY_ENTER: c_int = 3;
const KEY_ESCAPE: c_int = 4;
const KEY_SPACE: c_int = 5;
const KEY_LEFT: c_int = 10;
const KEY_RIGHT: c_int = 11;
const KEY_UP: c_int = 12;
const KEY_DOWN: c_int = 13;
const KEY_HOME: c_int = 14;
const KEY_END: c_int = 15;
const KEY_PAGE_UP: c_int = 16;
const KEY_PAGE_DOWN: c_int = 17;
const KEY_INSERT: c_int = 18;
const KEY_DELETE: c_int = 19;
const KEY_F1: c_int = 30; // F1..F12 = 30..41
const KEY_LSHIFT: c_int = 50;
const KEY_RSHIFT: c_int = 51;
const KEY_LCTRL: c_int = 52;
const KEY_RCTRL: c_int = 53;
const KEY_LALT: c_int = 54;
const KEY_RALT: c_int = 55;
const KEY_LCMD: c_int = 56;
const KEY_RCMD: c_int = 57;
const KEY_A_BASE: c_int = 100;
const KEY_NUM_BASE: c_int = 200;

const MOD_SHIFT: c_int = 1;
const MOD_CTRL: c_int = 2;
const MOD_ALT: c_int = 4;
const MOD_CMD: c_int = 8;

const BTN_LEFT: c_int = 1;
const BTN_MIDDLE: c_int = 2;
const BTN_RIGHT: c_int = 3;

fn mapButton(b: c_int) ?dvui.enums.Button {
    return switch (b) {
        BTN_LEFT => .left,
        BTN_MIDDLE => .middle,
        BTN_RIGHT => .right,
        else => null,
    };
}

fn mapKey(k: c_int) ?dvui.enums.Key {
    return switch (k) {
        KEY_BACKSPACE => .backspace,
        KEY_TAB => .tab,
        KEY_ENTER => .enter,
        KEY_ESCAPE => .escape,
        KEY_SPACE => .space,
        KEY_LEFT => .left,
        KEY_RIGHT => .right,
        KEY_UP => .up,
        KEY_DOWN => .down,
        KEY_HOME => .home,
        KEY_END => .end,
        KEY_PAGE_UP => .page_up,
        KEY_PAGE_DOWN => .page_down,
        KEY_INSERT => .insert,
        KEY_DELETE => .delete,
        KEY_LSHIFT => .left_shift,
        KEY_RSHIFT => .right_shift,
        KEY_LCTRL => .left_control,
        KEY_RCTRL => .right_control,
        KEY_LALT => .left_alt,
        KEY_RALT => .right_alt,
        KEY_LCMD => .left_command,
        KEY_RCMD => .right_command,
        else => blk: {
            if (k >= KEY_F1 and k <= KEY_F1 + 11) {
                const f_idx = k - KEY_F1;
                break :blk @as(dvui.enums.Key, @enumFromInt(@intFromEnum(dvui.enums.Key.f1) + @as(u32, @intCast(f_idx))));
            }
            if (k >= KEY_A_BASE and k < KEY_A_BASE + 26) {
                const letter_idx = k - KEY_A_BASE;
                break :blk @as(dvui.enums.Key, @enumFromInt(@intFromEnum(dvui.enums.Key.a) + @as(u32, @intCast(letter_idx))));
            }
            if (k >= KEY_NUM_BASE and k < KEY_NUM_BASE + 10) {
                const digit_idx = k - KEY_NUM_BASE;
                break :blk @as(dvui.enums.Key, @enumFromInt(@intFromEnum(dvui.enums.Key.zero) + @as(u32, @intCast(digit_idx))));
            }
            break :blk null;
        },
    };
}

fn mapMods(m: c_int) dvui.enums.Mod {
    var bits: u16 = 0;
    if (m & MOD_SHIFT != 0) bits |= @intFromEnum(dvui.enums.Mod.lshift);
    if (m & MOD_CTRL != 0) bits |= @intFromEnum(dvui.enums.Mod.lcontrol);
    if (m & MOD_ALT != 0) bits |= @intFromEnum(dvui.enums.Mod.lalt);
    if (m & MOD_CMD != 0) bits |= @intFromEnum(dvui.enums.Mod.lcommand);
    return @enumFromInt(bits);
}

export fn dvui_event_mouse_motion(self: *UnrealBackend, x: f32, y: f32) void {
    const win = if (self.window) |*w| w else return;
    _ = win.addEventMouseMotion(.{ .pt = .{ .x = x, .y = y } }) catch {};
}

export fn dvui_event_mouse_button(self: *UnrealBackend, button: c_int, pressed: c_int) void {
    const win = if (self.window) |*w| w else return;
    const b = mapButton(button) orelse return;
    const action: dvui.Event.Mouse.Action = if (pressed != 0) .press else .release;
    _ = win.addEventMouseButton(b, action) catch {};
}

export fn dvui_event_mouse_wheel(self: *UnrealBackend, dx: f32, dy: f32) void {
    const win = if (self.window) |*w| w else return;
    if (dy != 0) _ = win.addEventMouseWheel(dy, .vertical) catch {};
    if (dx != 0) _ = win.addEventMouseWheel(dx, .horizontal) catch {};
}

export fn dvui_event_key(self: *UnrealBackend, key: c_int, pressed: c_int, mods: c_int) void {
    const win = if (self.window) |*w| w else return;
    const k = mapKey(key) orelse return;
    _ = win.addEventKey(.{
        .code = k,
        .action = if (pressed != 0) .down else .up,
        .mod = mapMods(mods),
    }) catch {};
}

export fn dvui_event_text(self: *UnrealBackend, utf8: [*]const u8, len: u32) void {
    const win = if (self.window) |*w| w else return;
    const text = utf8[0..len];
    // Unreal's Slate delivers control codes (backspace 0x08, delete 0x7F,
    // tab, enter, escape) through its character/text events; the OS
    // text-input APIs other dvui backends use never do — they surface those
    // only as key events. dvui doesn't filter text it's handed, so an
    // unprintable codepoint would render as a "tofu" box. Strip ASCII
    // control chars and forward only the printable runs. UTF-8 multi-byte
    // sequences are all >= 0x80, so this never splits a codepoint.
    var i: usize = 0;
    while (i < text.len) {
        while (i < text.len and isControlByte(text[i])) i += 1;
        const start = i;
        while (i < text.len and !isControlByte(text[i])) i += 1;
        if (i > start) _ = win.addEventText(.{ .text = text[start..i] }) catch {};
    }
}

fn isControlByte(b: u8) bool {
    return b < 0x20 or b == 0x7F;
}

export fn dvui_text_input_active(self: *UnrealBackend) c_int {
    const win = if (self.window) |*w| w else return 0;
    return if (win.text_input_rect != null) 1 else 0;
}

export fn dvui_cursor_requested(self: *UnrealBackend) c_int {
    const win = if (self.window) |*w| w else return 0;
    return @intFromEnum(win.cursorRequested());
}

/// Notify the dvui window that the host is closing — gives widgets a chance
/// to run close handlers before the backend tears down.
export fn dvui_event_window_close(self: *UnrealBackend) void {
    const win = if (self.window) |*w| w else return;
    win.addEventWindow(.{ .action = .close }) catch {};
}
