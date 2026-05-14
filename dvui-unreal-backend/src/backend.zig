const std = @import("std");
const dvui = @import("dvui");
const app = @import("app");

// Unreal Backend for DVUI
// Implements the full dvui.Backend interface and bridges to Unreal Engine
// via C callbacks.

pub const kind: dvui.enums.Backend = .custom;

pub const UnrealBackend = @This();
pub const Context = *UnrealBackend;

/// Opaque handle to the Unreal-side context (actually FDVUIRenderer*)
pub const UnrealContext = opaque {};

/// Vertex structure for the C ABI boundary (matches DVUIVertex in DVUI.h)
pub const CVertex = extern struct {
    x: f32,
    y: f32,
    u: f32,
    v: f32,
    color: u32,
};

/// Callbacks from Unreal to the backend
pub const UnrealCallbacks = extern struct {
    render_triangles: ?*const fn (
        ctx: *UnrealContext,
        vertices: [*]const CVertex,
        vertex_count: u32,
        indices: [*]const u32,
        index_count: u32,
        texture_ptr: usize,
        clip_rect: ?*const [4]f32,
    ) callconv(.c) void,

    get_time_ns: ?*const fn (ctx: *UnrealContext) callconv(.c) u64,
    get_clipboard: ?*const fn (ctx: *UnrealContext) callconv(.c) [*:0]const u8,
    set_clipboard: ?*const fn (ctx: *UnrealContext, text: [*:0]const u8) callconv(.c) void,
    get_dpi_scale: ?*const fn (ctx: *UnrealContext) callconv(.c) f32,
    get_pixel_size: ?*const fn (ctx: *UnrealContext, width: *u32, height: *u32) callconv(.c) void,

    // Texture lifecycle. The id is the dvui-side pixel buffer pointer cast to
    // usize, used as a stable handle on the host side.
    texture_create: ?*const fn (
        ctx: *UnrealContext,
        texture_id: usize,
        pixels: [*]const u8,
        width: u32,
        height: u32,
    ) callconv(.c) void,
    texture_destroy: ?*const fn (
        ctx: *UnrealContext,
        texture_id: usize,
    ) callconv(.c) void,
};

// Backend state
parent_allocator: std.mem.Allocator,
arena: std.heap.ArenaAllocator,
allocator: std.mem.Allocator = undefined,
unreal_ctx: *UnrealContext,
callbacks: UnrealCallbacks,
frame_arena: std.mem.Allocator = undefined,

width: u32 = 1920,
height: u32 = 1080,
dpi_scale: f32 = 1.0,

window: ?dvui.Window = null,

// Monotonic ID we hand out as the dvui.Texture.ptr (and as the host-side
// handle in texture_create/destroy callbacks). Using the heap pointer as
// the handle is unsafe with dvui's textureDestroyLater path: the old
// pixel buffer can be freed AFTER a new texture is created at the same
// address, so the host-side cache key collides and the deferred destroy
// evicts the wrong (newer) entry. A monotonic ID never collides.
next_tex_id: usize = 1,
// Map from our id -> the actual pixel buffer pointer + size, so destroy
// can free the right backing memory.
tex_table: std.AutoHashMapUnmanaged(usize, TexEntry) = .empty,

const TexEntry = struct {
    pixels: [*]u8,
    byte_size: usize,
};

pub fn create(allocator: std.mem.Allocator, unreal_ctx: *UnrealContext, callbacks: UnrealCallbacks) !*UnrealBackend {
    // Allocate self with the parent allocator. We can't use a local arena
    // here: std.mem.Allocator is a fat pointer that captures the *address*
    // of the arena state, so storing self.allocator = local_arena.allocator()
    // would dangle as soon as `create` returns.
    const self = try allocator.create(UnrealBackend);
    errdefer allocator.destroy(self);

    self.* = .{
        .parent_allocator = allocator,
        .arena = std.heap.ArenaAllocator.init(allocator),
        .unreal_ctx = unreal_ctx,
        .callbacks = callbacks,
    };
    // Take the allocator from the arena AFTER it's at its final address in
    // self. (Allocator interface stores a pointer back to the arena state.)
    self.allocator = self.arena.allocator();

    self.window = dvui.Window.init(
        @src(),
        allocator,
        self.backend(),
        .{},
    ) catch return error.BackendError;

    // Optional one-shot app init, called after the dvui Window exists but
    // before any frame. Sets dvui.current_window so init() can call dvui
    // APIs that require it (theme setup, font loading) without needing a
    // real frame's begin/end pair.
    if (@hasDecl(app, "init")) {
        const InitFn = @TypeOf(app.init);
        const init_info = @typeInfo(InitFn).@"fn";

        const prev_current = dvui.current_window;
        dvui.current_window = &self.window.?;
        defer dvui.current_window = prev_current;

        const res: anyerror!void = if (init_info.params.len == 1)
            app.init(&self.window.?)
        else if (init_info.params.len == 0)
            app.init()
        else
            {};

        res catch |err| {
            std.log.err("dvui app init failed: {}", .{err});
            self.window.?.deinit();
            self.arena.deinit();
            allocator.destroy(self);
            return error.BackendError;
        };
    }

    return self;
}

pub fn destroy(self: *UnrealBackend) void {
    // Optional app deinit, called after we've signalled close events but
    // before the dvui window is torn down.
    if (@hasDecl(app, "deinit")) {
        const prev_current = dvui.current_window;
        if (self.window) |*w| dvui.current_window = w;
        defer dvui.current_window = prev_current;
        app.deinit();
    }

    if (self.window) |*w| w.deinit();
    self.tex_table.deinit(self.parent_allocator);
    self.arena.deinit();
    self.parent_allocator.destroy(self);
}

/// Run a full frame: begin → user app → end. Sets dvui.current_window so
/// the app's `frame()` can use dvui APIs that rely on the global. App can
/// expose either `pub fn frame() !void` or `pub fn frame(*dvui.Window) !void`
/// (we detect at comptime).
pub fn renderFrame(self: *UnrealBackend) void {
    var win = &(self.window orelse return);

    const prev_current = dvui.current_window;
    dvui.current_window = win;
    defer dvui.current_window = prev_current;

    win.begin(self.nanoTime()) catch return;

    const FrameFn = @TypeOf(app.frame);
    const frame_info = @typeInfo(FrameFn).@"fn";
    const has_error_return = @typeInfo(frame_info.return_type.?) == .error_union;

    if (has_error_return) {
        if (frame_info.params.len == 1) {
            app.frame(win) catch |err| std.log.err("dvui app frame failed: {}", .{err});
        } else {
            app.frame() catch |err| std.log.err("dvui app frame failed: {}", .{err});
        }
    } else {
        if (frame_info.params.len == 1) app.frame(win) else app.frame();
    }

    _ = win.end(.{}) catch return;
}

// ============================================================================
// dvui.Backend interface implementation
// ============================================================================

pub fn nanoTime(self: *UnrealBackend) i128 {
    if (self.callbacks.get_time_ns) |get_time| {
        return @intCast(get_time(self.unreal_ctx));
    }
    return std.time.nanoTimestamp();
}

pub fn sleep(_: *UnrealBackend, _: u64) void {}

pub fn begin(self: *UnrealBackend, frame_arena: std.mem.Allocator) !void {
    self.frame_arena = frame_arena;

    if (self.callbacks.get_pixel_size) |get_size| {
        get_size(self.unreal_ctx, &self.width, &self.height);
    }
    if (self.callbacks.get_dpi_scale) |get_dpi| {
        self.dpi_scale = get_dpi(self.unreal_ctx);
    }
}

pub fn end(_: *UnrealBackend) !void {}

pub fn pixelSize(self: *UnrealBackend) dvui.Size.Physical {
    return .{ .w = @floatFromInt(self.width), .h = @floatFromInt(self.height) };
}

pub fn windowSize(self: *UnrealBackend) dvui.Size.Natural {
    return .{ .w = @floatFromInt(self.width), .h = @floatFromInt(self.height) };
}

pub fn contentScale(self: *UnrealBackend) f32 {
    return self.dpi_scale;
}

pub fn drawClippedTriangles(
    self: *UnrealBackend,
    texture: ?dvui.Texture,
    vtx: []const dvui.Vertex,
    idx: []const dvui.Vertex.Index,
    clipr: ?dvui.Rect.Physical,
) !void {
    if (vtx.len == 0 or idx.len == 0) return;
    const render_fn = self.callbacks.render_triangles orelse return;

    // Convert DVUI vertices to C ABI format
    const c_verts = try self.frame_arena.alloc(CVertex, vtx.len);
    for (vtx, 0..) |v, i| {
        c_verts[i] = .{
            .x = v.pos.x,
            .y = v.pos.y,
            .u = v.uv[0],
            .v = v.uv[1],
            .color = @bitCast(v.col),
        };
    }

    // Convert indices to u32 for C ABI
    const c_indices = try self.frame_arena.alloc(u32, idx.len);
    for (idx, 0..) |index, i| {
        c_indices[i] = @intCast(index);
    }

    const texture_ptr: usize = if (texture) |t| @intFromPtr(t.ptr) else 0;

    var clip: [4]f32 = undefined;
    const clip_ptr: ?*const [4]f32 = if (clipr) |r| blk: {
        clip = .{ r.x, r.y, r.w, r.h };
        break :blk &clip;
    } else null;

    render_fn(
        self.unreal_ctx,
        c_verts.ptr,
        @intCast(c_verts.len),
        c_indices.ptr,
        @intCast(c_indices.len),
        texture_ptr,
        clip_ptr,
    );
}

pub fn textureCreate(self: *UnrealBackend, pixels: [*]const u8, w: u32, h: u32, _: dvui.enums.TextureInterpolation, format: dvui.enums.TexturePixelFormat) !dvui.Texture {
    const size = w * h * 4;
    const stored = try self.allocator.alloc(u8, size);
    @memcpy(stored, pixels[0..size]);

    // Allocate a fresh, never-reused id for this texture. Hand it back to
    // dvui as the texture handle (cast into a ptr) so render_triangles
    // brings the same id back to us as `texture_ptr`. Critically, the id
    // is independent of the heap address, so dvui's textureDestroyLater
    // can't cause a stale destroy to evict a newer texture's host entry.
    const id = self.next_tex_id;
    self.next_tex_id += 1;

    self.tex_table.put(self.parent_allocator, id, .{
        .pixels = stored.ptr,
        .byte_size = size,
    }) catch {
        self.allocator.free(stored);
        return error.OutOfMemory;
    };

    if (self.callbacks.texture_create) |create_cb| {
        create_cb(self.unreal_ctx, id, stored.ptr, w, h);
    }

    return .{ .ptr = @ptrFromInt(id), .width = w, .height = h, .format = format };
}

pub fn textureDestroy(self: *UnrealBackend, texture: dvui.Texture) void {
    const id = @intFromPtr(texture.ptr);
    if (self.callbacks.texture_destroy) |destroy_cb| {
        destroy_cb(self.unreal_ctx, id);
    }
    if (self.tex_table.fetchRemove(id)) |kv| {
        self.allocator.free(kv.value.pixels[0..kv.value.byte_size]);
    }
}

pub fn textureCreateTarget(_: *UnrealBackend, _: u32, _: u32, _: dvui.enums.TextureInterpolation, _: dvui.enums.TexturePixelFormat) !dvui.TextureTarget {
    return error.TextureCreate;
}

pub fn textureReadTarget(_: *UnrealBackend, _: dvui.TextureTarget, _: [*]u8) !void {
    return error.TextureRead;
}

pub fn textureClearTarget(_: *UnrealBackend, _: dvui.TextureTarget) void {}

pub fn textureDestroyTarget(_: *UnrealBackend, _: dvui.Texture.Target) void {}

pub fn textureFromTarget(_: *UnrealBackend, target: dvui.TextureTarget) !dvui.Texture {
    return .{ .ptr = target.ptr, .width = target.width, .height = target.height, .format = target.format };
}

pub fn textureFromTargetTemp(_: *UnrealBackend, target: dvui.TextureTarget) !dvui.Texture {
    return .{ .ptr = target.ptr, .width = target.width, .height = target.height, .format = target.format };
}

pub fn renderTarget(_: *UnrealBackend, _: ?dvui.TextureTarget) !void {}

pub fn clipboardText(self: *UnrealBackend) ![]const u8 {
    if (self.callbacks.get_clipboard) |get_clip| {
        const text = get_clip(self.unreal_ctx);
        return std.mem.span(text);
    }
    return "";
}

pub fn clipboardTextSet(self: *UnrealBackend, text: []const u8) !void {
    if (self.callbacks.set_clipboard) |set_clip| {
        const null_term = try self.frame_arena.allocSentinel(u8, text.len, 0);
        @memcpy(null_term, text);
        set_clip(self.unreal_ctx, null_term.ptr);
    }
}

pub fn openURL(_: *UnrealBackend, _: []const u8, _: bool) !void {}

pub fn preferredColorScheme(_: *UnrealBackend) ?dvui.enums.ColorScheme {
    return null;
}

pub fn refresh(_: *UnrealBackend) void {}

pub fn backend(self: *UnrealBackend) dvui.Backend {
    return dvui.Backend.init(self);
}
