const std = @import("std");
const builtin = @import("builtin");
const dvui_build = @import("dvui");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{
        .default_target = switch (builtin.os.tag) {
            .windows => std.Target.Query.parse(.{ .arch_os_abi = "x86_64-windows-msvc" }) catch unreachable,
            else => .{},
        },
    });
    const optimize = b.standardOptimizeOption(.{});

    const dvui_dep = b.dependency("dvui", .{
        .target = target,
        .optimize = optimize,
        .backend = .custom,
        .libc = true,
        .@"stb-image" = true,
        .@"tree-sitter" = false,
    });
    const dvui_mod = dvui_dep.module("dvui");

    // Standalone build links against the bundled sample app. Downstream
    // callers (e.g. the graphl IDE) call `buildUnrealPlugin()` below with
    // their own app_module instead.
    const app_dep = b.dependency("dvui_unreal_sample_app", .{
        .target = target,
        .optimize = optimize,
    });
    const app_mod = app_dep.module("sample_app");
    app_mod.addImport("dvui", dvui_mod);

    const lib = buildBackendLib(b, .{
        .lib_name = "dvui_unreal",
        .dvui_module = dvui_mod,
        .app_module = app_mod,
        .target = target,
        .optimize = optimize,
        .backend_zig = b.path("src/backend.zig"),
        .main_zig = b.path("src/main.zig"),
    });

    b.installArtifact(lib);
}

/// Mirrors `dvui.linkBackend` so external callers don't have to import
/// dvui's build.zig just to wire the custom backend.
pub fn linkBackend(dvui_module: *std.Build.Module, backend_module: *std.Build.Module) void {
    backend_module.addImport("dvui", dvui_module);
    dvui_module.addImport("backend", backend_module);
}

pub const BackendLibOptions = struct {
    lib_name: []const u8,
    dvui_module: *std.Build.Module,
    app_module: *std.Build.Module,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    /// LazyPath to this package's src/backend.zig. Callers from a
    /// different package context use `dep.path("src/backend.zig")` so the
    /// path resolves against this package, not the caller's.
    backend_zig: std.Build.LazyPath,
    /// LazyPath to this package's src/main.zig (same caveat).
    main_zig: std.Build.LazyPath,
    // TODO: inherit zig build default
    use_llvm: bool = false,
    /// you should generally prefer dynamic, static doesn't work (due to UBT quirks)
    /// if you e.g. need another code module to communicate across the C ABI
    linkage: std.builtin.LinkMode = .dynamic,
};

/// Build the dvui-unreal-backend library wired to the given app module.
/// Used both by this package's standalone `zig build` and by
/// the top-level `dvui_unreal.addUnrealPlugin`
pub fn buildBackendLib(b: *std.Build, opts: BackendLibOptions) *std.Build.Step.Compile {
    // Backend module (implements dvui.Backend interface and calls into the
    // user app's frame()). Wired bidirectionally with dvui via linkBackend
    // and gets the user app via the "app" import.
    const backend_mod = b.createModule(.{
        .root_source_file = opts.backend_zig,
        .target = opts.target,
        .optimize = opts.optimize,
    });
    linkBackend(opts.dvui_module, backend_mod);
    backend_mod.addImport("app", opts.app_module);

    // Library root holds the C-ABI exports and links to the backend.
    const root_mod = b.createModule(.{
        .root_source_file = opts.main_zig,
        .target = opts.target,
        .optimize = opts.optimize,
    });
    root_mod.addImport("backend", backend_mod);
    // main.zig references dvui types directly to map the stable C-ABI input
    // codes onto dvui's enums.
    root_mod.addImport("dvui", opts.dvui_module);

    const lib = b.addLibrary(.{
        .name = opts.lib_name,
        .root_module = root_mod,
        .use_llvm = opts.use_llvm,
        .linkage = opts.linkage,
    });
    lib.linkLibC();
    // Bundle compiler_rt / ubsan: the consumer (Unreal) doesn't provide
    // f128 ops (`__divtf3` etc.) or ubsan helpers; without these we get
    // undefined references at link time.
    lib.bundle_compiler_rt = true;
    lib.bundle_ubsan_rt = true;
    return lib;
}

// The high-level `addUnrealPlugin` helper that templates a full UE plugin
// directory lives in the parent package (`libs/dvui-unreal/build.zig`).
// This package only exposes the static-library build (`buildBackendLib`)
// so the standalone `zig build` and the top-level helper can both reuse it.
