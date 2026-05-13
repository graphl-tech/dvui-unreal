# DVUI for Unreal Engine

An Unreal Engine 5 plugin that hosts a [dvui](https://github.com/david-vanderson/dvui)
immediate-mode UI inside any UMG widget. The dvui frame is rendered through
a custom Slate element using a dedicated render-thread shader, so text,
icons, and PMA blending look identical to dvui's reference SDL/web backends.

The dvui app itself is a Zig package; this plugin links a static `.a`
produced by Zig and forwards Slate input/cursor/clipboard/texture lifecycle
events to it through a stable C ABI.

Tested with **UE 5.6 on Linux**. Win64 paths exist in `Build.cs` but have
not been verified on this branch.

## Repo layout

```
libs/dvui-unreal/
├── DVUIUnreal.uplugin            # plugin manifest (LoadingPhase: PostConfigInit)
├── Source/DVUIUnreal/            # the UE plugin module (C++)
│   ├── Public/                   # DVUI.h (C ABI), UMG/Slate widget, renderer
│   └── Private/                  # custom Slate element, shader, GC stubs
├── Shaders/Private/DvuiShader.usf
├── ThirdParty/
│   ├── dvui-unreal-backend/      # Zig package — dvui.Backend implementation
│   ├── dvui-unreal-sample-app/   # the default app linked into the .a
│   └── TestProject/              # bare UE5 project that hosts the plugin
└── scripts/                      # build / screenshot / bench harnesses
```

## Quick start (test project)

```bash
# Build Zig backend (sample app) + UE editor module + run a headless
# screenshot. The PNG lands in ThirdParty/TestProject/Saved/Screenshots/.
./scripts/build-test.sh
./scripts/run-screenshot.sh
```

For an interactive window:

```bash
./scripts/run-graphl-windowed.sh   # builds the graphl-flavoured .a + opens window
# or, for the sample app:
./scripts/build-test.sh && \
  ~/.local/share/unreal/Engine/Binaries/Linux/UnrealEditor-Cmd \
    ThirdParty/TestProject/DVUITest.uproject -game -windowed -preferNvidia
```

`UNREAL_ENGINE` overrides the engine path (default `~/.local/share/unreal/Engine`).

## Using the plugin in your own UE project

The recommended path is to generate a per-app plugin from your own
`build.zig` — same pattern as `buildBlenderAddon` in blender-dvui. The
generator templates the C++ tree (Build.cs, module/widget/renderer class
names, .uplugin manifest, shader directory mapping) so multiple
dvui-unreal plugins coexist cleanly and the UMG widget shows up in the
palette with your chosen class name.

In your DVUI app's `build.zig.zon`:

```zig
.dependencies = .{
    .dvui_unreal = .{ .path = "path/to/libs/dvui-unreal", .lazy = true },
    .dvui = ...,                     // your dvui dep, with .backend = .custom
},
```

In your DVUI app's `build.zig`:

```zig
if (b.lazyDependency("dvui_unreal", .{ .target = target, .optimize = optimize })) |unreal_dep| {
    const dvui_unreal = @import("dvui_unreal");

    const plugin = dvui_unreal.addUnrealPlugin(b, .{
        .dvui_unreal_dep = unreal_dep,
        .dvui_module = dvui_mod,
        .root_module = my_dvui_app_mod,   // exposes pub fn frame(*dvui.Window) !void
        .name = "My Widget",              // FriendlyName in the .uplugin
        // .module_name  = "MyWidget",        // optional; default = PascalCase(name)
        // .widget_class = "MyWidget",        // optional; default = "<ModuleName>Widget"
        .target = target,
        .optimize = optimize,
    });

    const step = b.step("unreal-plugin", "Generate the UE plugin");
    step.dependOn(plugin.step);
}
```

Run `zig build unreal-plugin` and the result lands in
`zig-out/unreal_plugin/MyWidget/`:

```
MyWidget/
├── MyWidget.uplugin
├── Shaders/Private/DvuiShader.usf
└── Source/MyWidget/
    ├── MyWidget.Build.cs
    ├── Public/{MyWidgetModule.h, MyWidgetRenderer.h, MyWidgetWidget.h, ...}
    ├── Private/{MyWidgetModule.cpp, ...Widgets/SMyWidgetWidget.cpp, ...}
    └── lib/libmy_widget_dvui.a    ← Zig-built static library
```

Drop that directory into your UE project's `Plugins/`, regenerate project
files, rebuild the editor — `UMyWidgetWidget` (or whatever you set
`widget_class` to) will appear in the UMG palette under "DVUI".

For graphl specifically, `scripts/install-graphl-flavor.sh /path/to/YourUEProject`
runs the build and rsyncs the resulting tree into `<project>/Plugins/GraphlIde/`.

> Note: an individual app may declare its own `extern "C"` symbols that
> the consuming UE project must define (e.g. `graphl_unreal_run_js` for
> the graphl IDE). Those are the app's contract, not dvui-unreal's —
> consult the app's own docs.

### Bundled test project

`ThirdParty/TestProject/` is a bare UE5 C++ project that consumes
`libs/dvui-unreal/` directly via a `Plugins/DVUIUnreal` symlink (skipping
`addUnrealPlugin` since it already IS the plugin tree). Use the scripts
in `scripts/` (`build-test.sh`, `run-screenshot.sh`,
`run-graphl-windowed.sh`) for development.

## Recommended CVars

dvui repaints every frame and relies on input arriving with low latency.
Default UE rendering settings fight both; the test GameMode applies the
ones below by default. If you're embedding into a project that doesn't run
through `ADVUITestGameMode`, replicate the important ones yourself:

| CVar / setting                            | Recommended | Why |
| ----------------------------------------- | ----------- | --- |
| `r.AntiAliasingMethod`                    | `0` or `1`  | TSR (default in UE5) is the single biggest cost on a near-empty scene — ~10× slowdown vs. off. |
| `r.OneFrameThreadLag`                     | `0`         | Cuts one frame of input → display latency. |
| `t.IdleWhenNotForeground`                 | `0`         | Otherwise UE hard-sleeps 100 ms/frame when not focused (caps you at 10 FPS). |
| `t.MaxFPS`                                | `0`         | Don't cap. dvui paces itself. |
| `GEngine->bSmoothFrameRate`               | `false`     | UE's frame smoother slows tick rate when it sees no UE-side activity. |
| `Viewport->bDisableWorldRendering`        | `true`      | Skip the entire 3D pass when you only want UI. |

The bundled `ADVUITestGameMode` exposes flags for all of these (see
`-Dvui*` command-line options in `DVUITestGameMode.cpp`).

## Limitations

- **Single dvui window per process.** dvui isn't designed for multiple
  concurrent windows; the renderer / backend / dvui.Window are singletons.
  Multiple `UDVUIWidget` instances share the same dvui state — they will
  interleave events and overwrite each other's draw lists. Place at most
  one per app.
- **`CanvasWidth` / `CanvasHeight` are hints, not the actual dvui canvas.**
  dvui always renders at the widget's on-screen pixel size so clicks land
  on the visible pixels. The properties only affect Slate's preferred-size
  layout when nothing else constrains the widget.
- **UE 5.6 on Linux is the only verified target.** Win64 link rules exist
  in `Build.cs` but have not been retested. Other UE versions may need
  adjustments to RHI/RDG calls in `DvuiCustomElement.cpp`.
- **Shaders must be recompiled per engine version.** `DvuiShader.usf`
  is a `TGlobalShader`; engine-version-locked.
- **Plugin loads at `PostConfigInit`** so the global shader registers
  before the engine's shader compile pass. If you need to depend on this
  module from another module, declare a hard dependency.
- **One dvui-unreal plugin per UE process.** `addUnrealPlugin` renames
  the module, UMG widget, renderer, and `_API` macro — but the global
  shader classes (`FDvuiVS`, `FDvuiPS`) and Slate custom-element class
  are kept verbatim. Loading two generated plugins into the same UE
  process clashes at `IMPLEMENT_GLOBAL_SHADER` registration. Multi-plugin
  support would need those classes templated too.
- **f128 ops are stubbed via `CompilerRtStubs.cpp`.** Zig's bundled
  `compiler_rt` doesn't include all `__divtf3` / `__multf3` / etc.
  symbols in static `.a` form. The stubs delegate to `long double` —
  precision-equivalent on x86_64 Linux but wrong elsewhere.
- **Viewport input mode matters.** Game-only input absorbs mouse-move
  events. Use `FInputModeUIOnly` (or the default editor mode) so Slate
  routes hover events to the widget.

## Scripts

| Script                          | What it does |
| ------------------------------- | ------------ |
| `scripts/build-test.sh`         | Build Zig backend (sample app) + UE editor module |
| `scripts/build-graphl.sh`       | Build the graphl-flavoured `.a`, copy it in, relink editor |
| `scripts/install-graphl-flavor.sh` | Stage the graphl `.a` for an *external* UE project |
| `scripts/run-screenshot.sh`     | Headless run, dumps before/after PNGs of a synthetic click |
| `scripts/run-graphl-windowed.sh`| Build graphl, open the test project in a window |
| `scripts/bench-fps.sh`          | Headless N-second FPS / frame-time bench |
| `scripts/bench-windowed.sh`     | Same but with a real on-screen window |
| `scripts/bisect-render-cvars.sh`<br>`scripts/bisect-render-via-gamemode.sh` | Per-CVar FPS bisection (helped find TSR as the cost driver) |

## C ABI

Stable codes only — see `Source/DVUIUnreal/Public/DVUI.h`. dvui's own
enums are translated to/from these inside `ThirdParty/dvui-unreal-backend/src/main.zig`,
so a dvui dependency bump can't silently change the wire format.

The Zig side exports:
- Lifecycle: `dvui_unreal_backend_create / _destroy / _render_frame`
- Inputs: `dvui_event_mouse_motion / _mouse_button / _mouse_wheel / _key / _text / _window_close`
- Queries: `dvui_text_input_active`, `dvui_cursor_requested`

The host (UE) provides callbacks for: render triangles, get time, get/set
clipboard, get DPI scale, get pixel size, texture create/destroy.
