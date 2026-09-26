# Windhawk config

The whole Windhawk setup: the shell's look, its virtual desktops and a handful
of behaviour tweaks. Four mods, each feature inside them on its own switch.

| Mod | Runs in | Features |
|---|---|---|
| [`desktops`](mods/desktops.wh.cpp) | explorer | Desktop pager on the taskbar · Win+1…0 / Win+N / Win+Q keybinds · Task View slide on switch |
| [`shell-styler`](mods/shell-styler.wh.cpp) | explorer, Start menu, notification center | Taskbar, Start menu and notification center themes · Start menu system info panel · taskbar dock animation |
| [`tweaks`](mods/tweaks.wh.cpp) | every app | Genie minimize · auto-hide taskbar when maximized · Alt+Tab delay · desktop drag-select · MB/GB file sizes |
| [`widgets`](mods/widgets.wh.cpp) | explorer | Desktop calendar · now-playing card |

This replaced, on 2026-09-25, ten separately installed mods plus PowerToys
Keyboard Manager driving `VirtualDesktop11-24H2.exe` for the desktop keybinds.
Everything that was installed before the switch is kept verbatim in
[`backup/2026-09-25/`](backup/2026-09-25/): mod sources, their live registry
settings, and the PowerToys config.

## What is in each mod

### Desktops

The pager is a strip of desktop numbers at the left of the taskbar; the active
one is in the accent colour, clicking one switches, hovering shows the desktop's
windows. The pill *around* the numbers comes from the Shell Styler's taskbar
theme -- the Desktops mod draws only the numbers.

| Keys | Action |
|---|---|
| `Win+1` … `Win+9`, `Win+0` | Switch to desktop 1 … 9, 10. No-op if it doesn't exist |
| `Win+N` | New desktop, and switch to it |
| `Win+Q` | Close the current desktop (slides to its neighbour first) |

The keys are caught by a low-level keyboard hook in explorer, the same
mechanism PowerToys used. Windows skips such hooks while an **elevated** window
has focus, so the keybinds don't fire then -- same as before.

Creating, closing and animated switching call the undocumented
`IVirtualDesktopManagerInternal` past its stable prefix. Its layout is only
known for build 26100+ (24H2/25H2), so those are gated to that interface ID.

### Shell Styler

One styling engine serves all three surfaces. It is the Notification Center
Styler's engine -- the newest of m417z's three styler engines -- with the
taskbar's and Start menu's host-specific workarounds folded in, and each host's
own start-up path. The themes are compiled in; there is no theme picker.

| Surface | Theme |
|---|---|
| Taskbar | WindowGlass Pill: centred app pill, detached tray pill, pager pill |
| Start menu | Translucent, search + profile + power only, height set in settings |
| Notification center | TranslucentShell |

Stripped relative to the originals: the theme libraries (~20k lines), the
custom control-style / resource-variable settings, the upstream usage-stats
ping, remote-image caching, web-content (SearchHost) styling, the Start menu
layout override and its process restarts, and click-through taskbar.

The dock animation was changed to time frames in fractional milliseconds and
to stop applying a second easing step every 120 ms -- both showed up as
jitter.

### Tweaks

Runs in every process because the genie and the file-size fix need to; the
explorer-only features stay dormant elsewhere. A feature that fails to hook
turns itself off without taking the others down.

Stripped relative to the originals: the genie's Modern (Direct2D) engine and
launch animation, the file-size mod's Everything integration and folder-size
calculation (which also made its "mix folders when sorting" option a no-op),
auto-hide's ExplorerPatcher / Windows 10 / old-taskbar support and its
per-event window logging (which opened a process handle for every window event
explorer saw, logging on or not).

### Widgets

Cards on the desktop, drawn with Direct2D into layered windows that the
desktop window (Progman) owns. Being owned by it is what keeps them above the
wallpaper and below every app, on every virtual desktop, out of Alt+Tab, and
there after Win+D. They never activate when clicked.

* **Calendar** -- month grid, today in the accent colour; chevrons or the wheel
  change month, clicking the title returns to today.
* **Now playing** -- the Windows media session (Spotify, browsers, ...): cover,
  title, artist, prev / play-pause / next, click-to-seek progress bar. Fades
  out when nothing has a session. The WinRT calls run on their own MTA thread;
  the UI thread only draws.

Drag a card to move it (grid + screen-edge snapping, position saved in the
mod's storage). Right-click for **Lock positions** (then only Alt+drag moves)
and **Reset positions**.

## Workflow

**Check** -- Windhawk's own clang, the editor's exact flags, no UI:

```powershell
.\scripts\check.ps1                 # all mods
.\scripts\check.ps1 tweaks          # one mod, substring match
.\scripts\check.ps1 -Warnings
```

It is `-fsyntax-only`, so a missing `@compilerOptions -l` library only shows up
at the real Compile as `ld.lld: error: undefined symbol`.

**Build and install** -- Windhawk has no command-line compile (its mod DLLs
link against private `InternalWh_*` plumbing), so the last step is the UI:

```powershell
.\scripts\stage.ps1 tweaks              # into the open editor buffer
.\scripts\stage.ps1 tweaks -Clipboard   # or paste it yourself
```

then **Compile** in Windhawk.

**Live settings** are in the registry, not the source:
`HKLM\SOFTWARE\Windhawk\Engine\Mods\local@<id>\Settings`.

### Diagnostics

* `measure-pills.ps1` -- where each taskbar pill starts and ends, in DIPs, for
  tuning the taskbar theme's constants.
* `trace-flyout.ps1` -- tray flyout open/close traced against clicks.

## History

Before the consolidation this repo tracked forks of the taskbar and Start menu
stylers against pristine upstream baselines, plus three standalone mods, with a
long write-up of every theme and fix. That README, the baselines and the patch
tooling are in git history at `57fc834`.

## Licensing

GPL-3.0, as the mods these are built from. The genie code is MIT
(Potassiumuncher / Abdullah Masood).
