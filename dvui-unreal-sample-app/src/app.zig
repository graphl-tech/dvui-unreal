//! Sample DVUI app for the Unreal backend's smoke harness.
//!
//! Exercises the surface a real app would touch: icon buttons (atlas
//! texture lifecycle), cursor changes, slider drags, click counters, and
//! a dark theme.

const std = @import("std");
const dvui = @import("dvui");
const entypo = dvui.entypo;

var click_count: u32 = 0;
var frame_counter: u32 = 0;
var theme_initialized: bool = false;
var slider_val: f32 = 0.5;

pub fn frame(win: *dvui.Window) void {
    _ = win;

    if (!theme_initialized) {
        dvui.themeSet(dvui.Theme.builtin.adwaita_dark);
        theme_initialized = true;
    }

    var float = dvui.floatingWindow(@src(), .{}, .{
        .name = "DVUI in Unreal",
        .rect = .{ .x = 80, .y = 80, .w = 520, .h = 460 },
    });
    defer float.deinit();

    dvui.label(@src(), "Hello from DVUI in Unreal!", .{}, .{});

    frame_counter +%= 1;
    var fbuf: [64]u8 = undefined;
    const ftext = std.fmt.bufPrint(&fbuf, "Frame: {d}", .{frame_counter}) catch "Frame: ?";
    dvui.labelNoFmt(@src(), ftext, .{}, .{});

    var cbuf: [64]u8 = undefined;
    const ctext = std.fmt.bufPrint(&cbuf, "Clicks: {d}", .{click_count}) catch "Clicks: ?";
    dvui.labelNoFmt(@src(), ctext, .{}, .{});

    // Toolbar of icon buttons. Each icon is rasterized at a size derived
    // from the current font height — hover effects can change widget
    // padding which changes the rasterized size and triggers a fresh
    // texture create. This is exactly the "flicker on hover" pattern in
    // graphl.
    {
        var hbox = dvui.box(@src(), .{ .dir = .horizontal }, .{
            .padding = .all(4),
            .margin = .all(4),
        });
        defer hbox.deinit();

        const icon_pairs = [_]struct { name: []const u8, bytes: []const u8 }{
            .{ .name = "menu",        .bytes = entypo.menu },
            .{ .name = "magnify",     .bytes = entypo.magnifying_glass },
            .{ .name = "plus",        .bytes = entypo.plus },
            .{ .name = "minus",       .bytes = entypo.minus },
            .{ .name = "cog",         .bytes = entypo.cog },
            .{ .name = "save",        .bytes = entypo.save },
            .{ .name = "folder",      .bytes = entypo.folder },
            .{ .name = "trash",       .bytes = entypo.trash },
        };
        for (icon_pairs) |ip| {
            if (dvui.buttonIcon(@src(), ip.name, ip.bytes, .{}, .{}, .{
                .id_extra = @intFromPtr(ip.name.ptr),
                .min_size_content = .{ .w = 28, .h = 28 },
            })) {
                std.log.info("[sample] icon '{s}' clicked", .{ip.name});
            }
        }
    }

    if (dvui.button(@src(), "Click Me", .{}, .{
        .min_size_content = .{ .w = 200, .h = 40 },
    })) {
        click_count += 1;
        std.log.info("[sample] button click #{d}", .{click_count});
    }

    // A slider — dragging it requires drag detection to work; a noticeably
    // delayed handle confirms input lag.
    _ = dvui.slider(@src(), .{
        .dir = .horizontal,
        .fraction = &slider_val,
    }, .{
        .expand = .horizontal,
        .min_size_content = .{ .w = 100, .h = 20 },
        .margin = .all(6),
    });

    // Animated color box — color cycles each click, position is fixed.
    var rect_box = dvui.box(@src(), .{}, .{
        .min_size_content = .{ .w = 320, .h = 80 },
        .background = true,
        .color_fill = clickColor(click_count),
    });
    defer rect_box.deinit();
}

fn clickColor(n: u32) dvui.Color {
    const palette = [_]dvui.Color{
        .{ .r = 200, .g = 50,  .b = 50,  .a = 255 },
        .{ .r = 50,  .g = 200, .b = 50,  .a = 255 },
        .{ .r = 50,  .g = 50,  .b = 200, .a = 255 },
        .{ .r = 200, .g = 200, .b = 50,  .a = 255 },
        .{ .r = 50,  .g = 200, .b = 200, .a = 255 },
        .{ .r = 200, .g = 50,  .b = 200, .a = 255 },
    };
    return palette[n % palette.len];
}
