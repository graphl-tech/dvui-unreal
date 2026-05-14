// Top-level dvui-unreal Zig package.
//
// Consumers depend on this package and call `addUnrealPlugin` from their
// own `build.zig` to generate a complete UE plugin containing:
//   - the dvui-unreal C++ source tree (renamed/templated for the consumer)
//   - the .uplugin manifest
//   - the dvui Zig backend statically linked against the consumer's app,
//     emitted as `Source/<ModuleName>/lib/lib<slug>_dvui.a`
//
// Mirrors blender-dvui's `buildBlenderAddon` shape.

const std = @import("std");
const dvui_unreal_backend = @import("dvui_unreal_backend");

pub fn build(b: *std.Build) void {
    // Accept (and ignore) the standard target/optimize options so consumers can
    // pass them through `b.dependency("dvui_unreal", .{ .target, .optimize })`.
    _ = b.standardTargetOptions(.{});
    _ = b.standardOptimizeOption(.{});
}

pub const UnrealPluginOptions = struct {
    // FIXME: can this be removed?
    /// Reference back to this package, obtained by the caller via
    /// `b.dependency("dvui_unreal", .{ .target = ..., .optimize = ... })`.
    /// Used to resolve our source files when called from a different
    /// package's build context.
    dvui_unreal_dep: *std.Build.Dependency,

    /// The dvui module the app was built against. Wired into the custom
    /// backend so types match.
    dvui_module: *std.Build.Module,

    /// Module exposing `pub fn frame(*dvui.Window) !void` (and optional
    /// `init` / `deinit`). Already imports `dvui` itself.
    root_module: *std.Build.Module,

    /// Human-readable plugin name. Becomes `FriendlyName` in the .uplugin
    /// manifest. e.g. "Graphl IDE".
    name: []const u8,

    /// PascalCase identifier used as the plugin folder, .uplugin
    /// filename, UE module name, Build.cs class name, and `<MOD>_API`
    /// macro stem. Default = `name` PascalCased.
    /// e.g. "Graphl IDE" → "GraphlIde".
    module_name: ?[]const u8 = null,

    /// UMG widget class name (without the leading `U`). This is what
    /// users see in the UMG palette. Default = "<ModuleName>Widget".
    /// e.g. "GraphlIdeWidget" → registers as `UGraphlIdeWidget`.
    widget_class: ?[]const u8 = null,

    /// Where to place the plugin directory inside the install prefix.
    /// `<install_root>/<module_name>/` will hold all the produced files.
    install_root: []const u8 = "unreal_plugin",

    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    use_llvm: bool = true,
};

pub const UnrealPlugin = struct {
    /// Top-level step that, once depended on, builds the .a and writes
    /// the templated plugin tree.
    step: *std.Build.Step,

    /// The static library compile step, exposed for further tweaking
    /// (extra C sources, system libraries, etc.).
    lib: *std.Build.Step.Compile,

    /// Final relative path inside the install prefix, e.g.
    /// "unreal_plugin/GraphlIde".
    install_subdir: []const u8,

    module_name: []const u8,
    widget_class: []const u8,
    /// Lowercase slug used for the static library filename
    /// (`lib<slug>_dvui.a`).
    slug: []const u8,
};

/// Generate a complete UE plugin directory for a DVUI app.
///
/// `<install_root>/<module_name>/`
///   ├── <module_name>.uplugin
///   ├── Shaders/Private/DvuiShader.usf
///   └── Source/<module_name>/
///       ├── <module_name>.Build.cs
///       ├── Public/  ← templated C++ headers
///       ├── Private/ ← templated C++ sources
///       └── lib/lib<slug>_dvui.a
///
/// The C++ tree is generated (not built) — UnrealBuildTool will compile
/// it when the consumer builds their UE project. The .a IS built (it's
/// the part Zig owns).
pub fn addUnrealPlugin(b: *std.Build, opts: UnrealPluginOptions) UnrealPlugin {
    const module_name = opts.module_name orelse pascalCase(b, opts.name);
    const widget_class = opts.widget_class orelse b.fmt("{s}Widget", .{module_name});
    const module_upper = upperOf(b, module_name);
    const slug = lowerOf(b, module_name);
    const subdir = b.fmt("{s}/{s}", .{ opts.install_root, module_name });

    // Build the static library via the backend's helper.
    const dep = opts.dvui_unreal_dep;
    const backend_dep = dep.builder.dependency("dvui_unreal_backend", .{
        .target = opts.target,
        .optimize = opts.optimize,
    });
    const lib = dvui_unreal_backend.buildBackendLib(b, .{
        .lib_name = b.fmt("{s}_dvui", .{slug}),
        .dvui_module = opts.dvui_module,
        .app_module = opts.root_module,
        .target = opts.target,
        .optimize = opts.optimize,
        .backend_zig = backend_dep.path("src/backend.zig"),
        .main_zig = backend_dep.path("src/main.zig"),
        .use_llvm = opts.use_llvm,
    });

    // Install the shared lib into the UE platform-specific Binaries dir
    // — the same directory where UBT puts `libUnrealEditor-<Module>.so`.
    // That way `${ORIGIN}` of the editor module's .so already covers our
    // lib at runtime, with no RPATH plumbing in Build.cs needed (which
    // matters because UBT's `PublicRuntimeLibraryPaths` bakes absolute
    // project paths into the RPATH that don't survive project moves).
    const ue_platform: []const u8 = switch (opts.target.result.os.tag) {
        .linux => "Linux",
        .windows => "Win64",
        .macos => "Mac",
        else => @panic("dvui_unreal: unsupported target OS for UE plugin"),
    };
    const lib_subdir = b.fmt("{s}/Binaries/{s}", .{ subdir, ue_platform });
    const install_lib = b.addInstallArtifact(lib, .{
        .dest_dir = .{ .override = .{ .custom = lib_subdir } },
    });

    // Template every plugin source file into a single WriteFiles, then
    // install it as a directory.
    const wf = b.addWriteFiles();

    // .uplugin (generated from scratch — easier than patching a JSON).
    _ = wf.add(b.fmt("{s}.uplugin", .{module_name}), renderUplugin(b, opts.name, module_name));

    // Build.cs for the C++ module — links against the shared lib.
    _ = wf.add(
        b.fmt("Source/{s}/{s}.Build.cs", .{ module_name, module_name }),
        renderBuildCs(b, module_name, slug),
    );

    // Public/
    const pub_files = [_]struct { src: []const u8, out_basename: []const u8, sub: bool }{
        .{ .src = "unreal-plugin/Source/DVUIUnreal/Public/DVUI.h", .out_basename = "DVUI.h", .sub = true },
        .{ .src = "unreal-plugin/Source/DVUIUnreal/Public/DVUIRenderer.h", .out_basename = b.fmt("{s}Renderer.h", .{module_name}), .sub = true },
        .{ .src = "unreal-plugin/Source/DVUIUnreal/Public/DVUIUnrealModule.h", .out_basename = b.fmt("{s}Module.h", .{module_name}), .sub = true },
        .{ .src = "unreal-plugin/Source/DVUIUnreal/Public/DVUIWidget.h", .out_basename = b.fmt("{s}.h", .{widget_class}), .sub = true },
        .{ .src = "unreal-plugin/Source/DVUIUnreal/Public/SDVUIWidget.h", .out_basename = b.fmt("S{s}.h", .{widget_class}), .sub = true },
    };
    for (pub_files) |f| {
        addTemplated(b, dep, wf, .{
            .src = f.src,
            .out = b.fmt("Source/{s}/Public/{s}", .{ module_name, f.out_basename }),
            .module_name = module_name,
            .module_upper = module_upper,
            .widget_class = widget_class,
            .substitute = f.sub,
        });
    }

    // Private/
    const priv_files = [_]struct { src: []const u8, out_basename: []const u8, sub: bool }{
        .{ .src = "unreal-plugin/Source/DVUIUnreal/Private/CompilerRtStubs.cpp", .out_basename = "CompilerRtStubs.cpp", .sub = false },
        .{ .src = "unreal-plugin/Source/DVUIUnreal/Private/DVUIRenderer.cpp", .out_basename = b.fmt("{s}Renderer.cpp", .{module_name}), .sub = true },
        .{ .src = "unreal-plugin/Source/DVUIUnreal/Private/DVUIUnrealModule.cpp", .out_basename = b.fmt("{s}Module.cpp", .{module_name}), .sub = true },
        .{ .src = "unreal-plugin/Source/DVUIUnreal/Private/DVUIWidget.cpp", .out_basename = b.fmt("{s}.cpp", .{widget_class}), .sub = true },
        .{ .src = "unreal-plugin/Source/DVUIUnreal/Private/DvuiCustomElement.h", .out_basename = "DvuiCustomElement.h", .sub = false },
        .{ .src = "unreal-plugin/Source/DVUIUnreal/Private/DvuiCustomElement.cpp", .out_basename = "DvuiCustomElement.cpp", .sub = false },
        .{ .src = "unreal-plugin/Source/DVUIUnreal/Private/DvuiShader.h", .out_basename = "DvuiShader.h", .sub = false },
        .{ .src = "unreal-plugin/Source/DVUIUnreal/Private/DvuiShader.cpp", .out_basename = "DvuiShader.cpp", .sub = true },
    };
    for (priv_files) |f| {
        addTemplated(b, dep, wf, .{
            .src = f.src,
            .out = b.fmt("Source/{s}/Private/{s}", .{ module_name, f.out_basename }),
            .module_name = module_name,
            .module_upper = module_upper,
            .widget_class = widget_class,
            .substitute = f.sub,
        });
    }

    // Private/Widgets/S<WidgetClass>.cpp
    addTemplated(b, dep, wf, .{
        .src = "unreal-plugin/Source/DVUIUnreal/Private/Widgets/SDVUIWidget.cpp",
        .out = b.fmt("Source/{s}/Private/Widgets/S{s}.cpp", .{ module_name, widget_class }),
        .module_name = module_name,
        .module_upper = module_upper,
        .widget_class = widget_class,
        .substitute = true,
    });

    // Shader (verbatim).
    addTemplated(b, dep, wf, .{
        .src = "unreal-plugin/Shaders/Private/DvuiShader.usf",
        .out = "Shaders/Private/DvuiShader.usf",
        .module_name = module_name,
        .module_upper = module_upper,
        .widget_class = widget_class,
        .substitute = false,
    });

    // Install the entire WriteFiles directory into <subdir>/.
    const install_tree = b.addInstallDirectory(.{
        .source_dir = wf.getDirectory(),
        .install_dir = .prefix,
        .install_subdir = subdir,
    });

    const top = b.allocator.create(std.Build.Step) catch @panic("OOM");
    top.* = std.Build.Step.init(.{
        .id = .custom,
        .name = b.fmt("unreal-plugin ({s})", .{module_name}),
        .owner = b,
    });
    top.dependOn(&install_lib.step);
    top.dependOn(&install_tree.step);

    return .{
        .step = top,
        .lib = lib,
        .install_subdir = subdir,
        .module_name = module_name,
        .widget_class = widget_class,
        .slug = slug,
    };
}

// =============================================================================
// Internals
// =============================================================================

const TemplateOpts = struct {
    src: []const u8, // path relative to dvui_unreal package root
    out: []const u8, // path inside the WriteFiles output
    module_name: []const u8,
    module_upper: []const u8,
    widget_class: []const u8,
    substitute: bool,
};

/// Read a source file from the dvui_unreal dep, optionally apply the
/// per-app substitutions, and add it to the WriteFiles step under `out`.
fn addTemplated(
    b: *std.Build,
    dep: *std.Build.Dependency,
    wf: *std.Build.Step.WriteFile,
    t: TemplateOpts,
) void {
    const max_size: usize = 4 * 1024 * 1024; // 4 MiB safety cap
    const bytes = dep.builder.build_root.handle.readFileAlloc(b.allocator, t.src, max_size) catch |err| std.debug.panic("dvui_unreal: failed to read template '{s}': {s}", .{ t.src, @errorName(err) });

    const out_text: []const u8 = if (t.substitute) blk: {
        // Apply in this order — longest-first so longer identifiers don't
        // get clobbered by shorter ones.
        var s = bytes;
        s = std.mem.replaceOwned(u8, b.allocator, s, "DVUIUNREAL_API", b.fmt("{s}_API", .{t.module_upper})) catch @panic("OOM");
        s = std.mem.replaceOwned(u8, b.allocator, s, "DVUIUnreal", t.module_name) catch @panic("OOM");
        s = std.mem.replaceOwned(u8, b.allocator, s, "DVUIRenderer", b.fmt("{s}Renderer", .{t.module_name})) catch @panic("OOM");
        s = std.mem.replaceOwned(u8, b.allocator, s, "DVUIWidget", t.widget_class) catch @panic("OOM");
        break :blk s;
    } else bytes;

    _ = wf.add(t.out, out_text);
}

/// Build.cs for the C++ module. Links against the shared library and
/// registers it as a runtime dependency so UAT copies it into the staged
/// build alongside the editor / packaged game.
fn renderBuildCs(b: *std.Build, module_name: []const u8, slug: []const u8) []const u8 {
    return b.fmt(
        \\// Generated by dvui_unreal.addUnrealPlugin — do not edit.
        \\
        \\using UnrealBuildTool;
        \\using System.IO;
        \\using System.Collections.Generic;
        \\
        \\public class {[mod]s} : ModuleRules
        \\{{
        \\    public {[mod]s}(ReadOnlyTargetRules Target) : base(Target)
        \\    {{
        \\        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        \\
        \\        PublicDependencyModuleNames.AddRange(new string[]
        \\        {{
        \\            "Core", "CoreUObject", "Engine", "InputCore",
        \\            "ApplicationCore", "RHI", "RenderCore", "Renderer",
        \\            "Slate", "SlateCore", "UMG"
        \\        }});
        \\
        \\        PrivateDependencyModuleNames.AddRange(new string[] {{ "Projects" }});
        \\
        \\        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
        \\
        \\        // Link against the Zig-built shared library. Shared (not
        \\        // static): the .so/.dll is loaded once at runtime so every
        \\        // UE module that depends on this plugin sees the same copy
        \\        // of every Zig global. A static .a would be re-linked into
        \\        // each dependent's .so via UBT's PublicAdditionalLibraries
        \\        // propagation, giving each its own copy and breaking
        \\        // cross-module wiring (e.g. registration callbacks).
        \\        //
        \\        // The .so/.dll lives in `<plugin>/Binaries/<Platform>/`,
        \\        // i.e. the same directory as the editor module's own .so.
        \\        // No PublicRuntimeLibraryPaths entry needed — `${{ORIGIN}}`
        \\        // of the editor .so already covers the lib at runtime,
        \\        // and that path is portable across project moves (unlike
        \\        // PublicRuntimeLibraryPaths, which UBT bakes as an
        \\        // absolute path into the RPATH and breaks on relocation).
        \\        if (Target.Platform == UnrealTargetPlatform.Linux)
        \\        {{
        \\            string LibDir = Path.Combine(PluginDirectory, "Binaries", "Linux");
        \\            string SoPath = Path.Combine(LibDir, "lib{[slug]s}_dvui.so");
        \\            PublicAdditionalLibraries.Add(SoPath);
        \\            RuntimeDependencies.Add(SoPath);
        \\        }}
        \\        else if (Target.Platform == UnrealTargetPlatform.Win64)
        \\        {{
        \\            string LibDir = Path.Combine(PluginDirectory, "Binaries", "Win64");
        \\            // Link against the import library (.lib) Zig emits
        \\            // alongside the .dll; the .dll itself is the runtime dep.
        \\            PublicAdditionalLibraries.Add(Path.Combine(LibDir, "{[slug]s}_dvui.lib"));
        \\            string DllPath = Path.Combine(LibDir, "{[slug]s}_dvui.dll");
        \\            RuntimeDependencies.Add(DllPath);
        \\            PublicDelayLoadDLLs.Add("{[slug]s}_dvui.dll");
        \\
        \\            var win10KitBase = "C:/Program Files (x86)/Windows Kits/10/Lib";
        \\            var win10KitVersions = new List<string>(Directory.EnumerateDirectories(win10KitBase));
        \\            if (win10KitVersions.Count >= 1)
        \\            {{
        \\                PublicAdditionalLibraries.Add(Path.Combine(win10KitBase, win10KitVersions[0], "um/x64/ntdll.lib"));
        \\            }}
        \\            else
        \\            {{
        \\                throw new System.NotSupportedException("No Windows Kit with ntdll.lib found");
        \\            }}
        \\        }}
        \\    }}
        \\}}
        \\
    , .{ .mod = module_name, .slug = slug });
}

/// Build the .uplugin manifest from scratch.
fn renderUplugin(b: *std.Build, friendly_name: []const u8, module_name: []const u8) []const u8 {
    return b.fmt(
        \\{{
        \\    "FileVersion": 3,
        \\    "Version": 1,
        \\    "VersionName": "0.1.0",
        \\    "FriendlyName": "{[name]s}",
        \\    "Description": "DVUI app rendered into Unreal via a Zig-built static backend",
        \\    "Category": "UI",
        \\    "CreatedBy": "dvui-unreal",
        \\    "CreatedByURL": "",
        \\    "DocsURL": "",
        \\    "MarketplaceURL": "",
        \\    "SupportURL": "",
        \\    "CanContainContent": true,
        \\    "IsBetaVersion": true,
        \\    "IsExperimentalVersion": true,
        \\    "Installed": false,
        \\    "Modules": [
        \\        {{
        \\            "Name": "{[mod]s}",
        \\            "Type": "Runtime",
        \\            "LoadingPhase": "PostConfigInit",
        \\            "PlatformAllowList": [
        \\                "Linux",
        \\                "Win64"
        \\            ]
        \\        }}
        \\    ],
        \\    "Plugins": []
        \\}}
        \\
    , .{ .name = friendly_name, .mod = module_name });
}

// PascalCase, lowerCase, UPPERCASE helpers ------------------------------------

/// "Graphl IDE" -> "GraphlIde". Strips non-alphanumeric, capitalizes the
/// first letter of each word.
fn pascalCase(b: *std.Build, name: []const u8) []const u8 {
    const buf = b.allocator.alloc(u8, name.len + 1) catch @panic("OOM");
    var len: usize = 0;
    var capitalize_next = true;
    for (name) |c| {
        if (std.ascii.isAlphanumeric(c)) {
            buf[len] = if (capitalize_next) std.ascii.toUpper(c) else std.ascii.toLower(c);
            len += 1;
            capitalize_next = false;
        } else {
            capitalize_next = true;
        }
    }
    if (len == 0) {
        const fallback = "DvuiApp";
        @memcpy(buf[0..fallback.len], fallback);
        len = fallback.len;
    }
    if (!std.ascii.isAlphabetic(buf[0])) {
        return b.fmt("App{s}", .{buf[0..len]});
    }
    return buf[0..len];
}

/// "GraphlIde" -> "graphl_ide".
fn lowerOf(b: *std.Build, pascal: []const u8) []const u8 {
    var list = std.ArrayList(u8).initCapacity(b.allocator, pascal.len + 4) catch @panic("OOM");
    for (pascal, 0..) |c, i| {
        if (i > 0 and std.ascii.isUpper(c)) {
            list.append(b.allocator, '_') catch @panic("OOM");
        }
        list.append(b.allocator, std.ascii.toLower(c)) catch @panic("OOM");
    }
    return list.toOwnedSlice(b.allocator) catch @panic("OOM");
}

/// "GraphlIde" -> "GRAPHLIDE".
fn upperOf(b: *std.Build, pascal: []const u8) []const u8 {
    const buf = b.allocator.alloc(u8, pascal.len) catch @panic("OOM");
    for (pascal, 0..) |c, i| buf[i] = std.ascii.toUpper(c);
    return buf;
}
