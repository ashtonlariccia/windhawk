# Windhawk mod fork workspace

Personal forks of [Windhawk](https://windhawk.net) mods, structured so upstream
updates can be merged in rather than lost.

Modelled on what Windhawk's own author does with
[`m417z/my-windhawk-mods`](https://github.com/m417z/my-windhawk-mods).

## Why this exists

Windhawk lets you edit any mod in place, but if you keep the original `@id` the
updater will overwrite your changes the next time that mod ships a new version.
Changing the `@id` makes you immune — but then you stop getting upstream fixes.

This repo resolves that: it keeps a **pristine upstream baseline** next to
**your edited fork**, so at any point you can see exactly what you changed and
replay it onto a newer upstream release.

## Layout

```
upstream/   pristine vendored sources, never edited   <id>@<version>.wh.cpp
mods/       your forked mods (unique @id)             <forkid>.wh.cpp
            ...and original mods, which have no upstream and no patch
patches/    generated diffs vs baseline               <forkid>.patch
scripts/    tooling (see below)
forks.json  maps each fork -> its upstream id + baseline version
build/      scratch: editor-buffer backups (gitignored)
```

## Current forks

| Fork | Upstream | Baseline |
|---|---|---|
| `windows-11-start-menu-styler-fork` | `windows-11-start-menu-styler` | 1.7 |
| `windows-11-taskbar-styler-fork` | `windows-11-taskbar-styler` | 1.8 |

Every "Custom theme" section below belongs to the Start menu fork, except
[WindowGlass Pill](#custom-theme-windowglass-pill-taskbar), which is the
taskbar fork's.

A "Fork feature" section is a fork addition that is not a theme -- it is a mod
setting, so it composes with whichever theme you have selected. There is one:
[the system information panel](#fork-feature-system-information-panel).

## Original mods

Not everything here is a fork. The mods below are written from scratch, so they
have no `upstream/` baseline, no `patches/` entry and no line in `forks.json`
— the fork tooling iterates that file, and an original mod has nothing to
replay onto. `check.ps1` and `stage.ps1` resolve any `*.wh.cpp` under `mods/` by
substring, so they work on them unchanged.

| Mod | What it does |
|---|---|
| `virtual-desktop-pager` | [a third taskbar pill listing your virtual desktops](#original-mod-virtual-desktop-pager) |
| `instant-desktop-switch` | [removes the desktop-switch slide, so the taskbar stops blinking](#original-mod-instant-virtual-desktop-switch) |
| `desktop-drag-select` | [brings the selection rectangle back when desktop icons are hidden](#original-mod-desktop-drag-select-with-icons-hidden) |

## Custom theme: OnlySearch (Minimal) + profile & power

**This is the one to use.** Select **"OnlySearch (Minimal) + profile & power"**
in the mod's Theme setting.

Built directly on OnlySearch (Minimal) -- that base renders correctly, so it is
copied verbatim and changed as little as possible:

```
[avatar]        [ (o) Search for apps, settings... ]        [power]
```

### Why the first attempt showed neither control

Both `StartDocked.UserTileView` (profile picture) and
`StartDocked.PowerOptionsView` (power button) are children of
`StartDocked.NavigationPaneView#NavigationPane`.

OnlySearch (Minimal) collapses that parent. A collapsed parent makes its
children unrenderable no matter what is set on them -- so every rule aimed at
the avatar and power button was dead on arrival.

This theme keeps `NavigationPane` visible and positions the two controls
directly. It also does **not** carry over Minimal's `Grid#NavPanePlaceholder`
collapse, for the same reason.

Vertical placement uses `RenderTransform` rather than `Margin` deliberately:
`RenderTransform` does not participate in layout, so nudging the row cannot
re-expand the bar back into a full-height Start menu.

### Tuning

All positioning is style constants -- override in the mod's **Style constants**
setting, no recompile:

| Constant | Default | Controls |
|---|---|---|
| `RowShiftY` | `0` | vertical position of avatar + power |
| `SearchShiftY` | `6` | **vertical position of the search box.** Positive = down |
| `RowHeight` | `48` | height of the row hosting avatar + power |
| `IconSize` | `32` | hit area of avatar and power button |
| `AvatarSize` | `28` | the avatar image itself |
| `UserTileMargin` | `16,0,0,0` | avatar distance from the left edge |
| `PowerMargin` | `0,0,16,0` | power button distance from the right edge |
| `SearchInset` | `72,-2,72,2` | search box margins, i.e. the gap left for both controls |
| `FlyoutBackground` | `SystemChromeMediumColor` | power menu background |
| `FlyoutBorderBrush` | `SurfaceStrokeColorDefault` | power menu border |

### Three fixes applied after first render

**Power menu opened transparent.** OnlySearch (Minimal) collapses
`Border#AcrylicOverlay` *unscoped* — so it also strips the background from
popups containing an element of that name. Re-scoping that rule is what makes
the bar itself look right, so instead the flyout is painted back explicitly on
`MenuFlyoutPresenter` / `FlyoutPresenter`, the same approach the Everblush theme
uses.

**Avatar was cut in half.** `StartDocked.UserTileView` had both `Width` and
`Height` forced to `IconSize`, which clips the inner image. The Windows10 themes
size the inner `Grid#UserTileIcon` instead and leave the view's width automatic
— that is what this now does, via `AvatarSize`.

**Search box sat higher than the other two.** Nudged with a `RenderTransform`
rather than a margin, so it stays layout-neutral and cannot re-expand the bar.

Negative values are fine and expected -- the official styling guide relocates
the power button with `Margin=-580,-1330,0,0`.

## Custom theme: FrostyGlass Search Line

A merged theme added to the fork. Select **"FrostyGlass Search Line (one-line
search bar)"** in the mod's Theme setting.

Collapses the Start menu to a single ~60px bar holding, on one row:

```
[ avatar ]        [        search box        ]        [ power ]
```

**Taken from FrostyGlass:** all the glass styling (acrylic, gradient borders,
corner radii), the profile picture (`StartDocked.UserTileView`) and the power
options (`StartDocked.PowerOptionsView`).

**Taken from OnlySearch (Minimal):** the window size (60px tall) and the
hide-everything-else rules — pinned list, recommended, all-apps frame, and the
right-hand companion are all collapsed.

### The one thing that could not be copied verbatim

OnlySearch (Minimal) collapses `StartDocked.NavigationPaneView#NavigationPane`
outright — and that element is the **parent of both the profile picture and the
power button**. Taking that rule as-is would have hidden the two things you
asked to keep.

So this theme deliberately keeps `NavigationPane` visible and instead pins it to
the bar height, then re-anchors the avatar left and the power button right. The
`Grid#NavPanePlaceholder` (the empty row the nav pane normally reserves) is
collapsed so it does not add a second row.

### Tuning without recompiling

Every layout number is a style constant. Override any of them in the mod's
**Style constants** setting (Windhawk UI) — no rebuild needed:

| Constant | Default | Controls |
|---|---|---|
| `BarHeight` | `60` | height of the whole bar |
| `BarWidth` | `666` | width of the whole bar |
| `IconSize` | `40` | avatar + power button size |
| `SearchHeight` | `36` | search box height |
| `SearchWidth` | `470` | search box width |
| `UserTileMargin` | `14,0,0,0` | avatar position |
| `PowerMargin` | `0,0,14,0` | power button position |
| `NavPaneMargin` | `0,0,0,0` | nudges avatar+power as a pair |
| `SearchPageMargin` | `0,64,0,0` | where search results appear |

Example — to widen the bar and shrink the search box, add these as two entries
in Style constants:

```
BarWidth=760
SearchWidth=540
```

Note: the search box keeps FrostyGlass's glass styling rather than the plain
default one from OnlySearch, since the bar is otherwise FrostyGlass. Width comes
from FrostyGlass too (OnlySearch Minimal never sets a width, only height).

## Custom theme: LiquidGlass (Legacy) - search, profile & power only

Select **"LiquidGlass (Legacy) - search, profile & power only"** in the mod's
Theme setting. For the **redesigned** Start menu -- like its parent it sets
`DisableNewStartMenuLayout::forceNewLayout`.

LiquidGlass (Legacy) with the app content taken out:

```
[ (o) Search for apps, settings, and documents ]
[ (avatar)                             (power) ]
```

Gone: the pinned list, the category folders, the all-apps grid, the recommended
section, and the phone-link companion panel on the right. Kept: the search box,
the profile bubble and the power bubble -- with every one of LiquidGlass's glass
styles (acrylic, gradient borders, corner radii) copied over verbatim, so the
delta against a future upstream LiquidGlass stays easy to replay.

### How the middle is removed

In the redesigned Start menu, `Grid#MainContent` has three rows:

| Row | Element | Fate |
|---|---|---|
| 0 | search box | kept |
| 1 | `Frame#StartFrame` | **collapsed** -- pinned, folders, all apps, recommended |
| 2 | `Grid#NavPanePlaceholder` | kept -- the tray with avatar + power |

LiquidGlass itself reads the tree the same way: it spans `Frame#StartFrame`
across all three rows at `Canvas.ZIndex=-1` so the search box and tray float
over it.

**Collapsing row 1 is not enough to shrink the window.** That row is
star-sized, so it goes on claiming whatever space is available and you get a
full-height glass rectangle with a search box floating in it. The height is
therefore pinned on every ancestor -- `StartMenu.StartBlendedFlexFrame`,
`Grid#FrameRoot`, `Grid#AnimationRoot`, `Grid#MainMenu`, `Grid#MainContent`,
`StartDocked.StartSizingFrame` -- the same thing FrostyGlass Search Line has to
do.

### Why NavPanePlaceholder stays visible

`Grid#NavPanePlaceholder` is the parent of both `StartDocked.UserTileView` and
`StartDocked.PowerOptionsView`. The OnlySearch themes collapse it, which is
exactly why they show neither bubble: a collapsed parent makes its children
unrenderable no matter what is set on them. This theme leaves it -- and the
`StartDocked.NavigationPaneView#NavigationPane` inside it -- visible.

### Tuning

| Constant | Default | Controls |
|---|---|---|
| `MenuHeight` | `150` | layout box: offset + search row + tray |
| `PanelInset` | `0,0,0,30` | insets the glass. Bottom number should match `-TrayShiftY` |
| `SearchHeight` | `40` | height of the search box |
| `TrayHeight` | `56` | height of the avatar + power tray |
| `PanelBorderThickness` | `0.3,1,0.3,1` | panel border. Bottom raised to match the top |
| `ContentShiftY` | `0` | moves the whole search + tray stack. Negative = up |
| `SearchShiftY` | `0` | nudges the search box. Positive = down |
| `TrayShiftY` | `-30` | nudges the tray. Negative pulls it back up |

`MenuHeight=150` with `TrayShiftY=-30` are the values that came out of tuning
this live; they are the defaults now, so a fresh install needs no overrides.

Raise `MenuHeight` if the tray is clipped, lower it to close a gap underneath.
If there is dead glass *above* the search box, pull the pair up with a negative
`ContentShiftY` and lower `MenuHeight` by the same amount. All three nudges use
`RenderTransform`, not `Margin`, so moving a row cannot push the window back
open to full height.

### The layout box is taller than what you can see

About 30px of the layout box is not on screen. That is the single fact behind
both of the fixes below, and it is what `TrayShiftY=-30` is really doing: the
tray was not mispositioned, it was parked in the invisible strip at the bottom,
and the shift lifts it back into view.

Confirmed by setting `Background=<SolidColorBrush Color="#FFFF00FF" />` in Style
constants: the magenta filled the entire visible window with a square bottom
edge and no glass below it. So `Border#AcrylicBorder` / `Border#AppBorder` are
the layers that paint the panel, and they were running past the cut.

A background cannot be shifted into view the way the tray can -- move it up and
you just expose bare window underneath. It has to be made shorter.

**`MaxHeight` is the wrong tool for that here, and cost two rounds to find
out.** These borders are stretch-sized by their parent, so capping their height
does nothing whatsoever; `PanelHeight` at 120 and at 116 both left the corners
exactly as square as before. The way the rest of the file does it is a
**margin** -- Down Aero insets this very layer with
`Border#AcrylicOverlay { Margin=0,0,0,20 }`, and its main variant uses
`Margin=0,-65,0,0` to pull the same layer the other way.

So the glass is inset with `$PanelInset` (`0,0,0,30`), applied to every layer
that paints part of the panel:

| | drives | value |
|---|---|---|
| `$MenuHeight` | the layout box: `FrameRoot`, `AnimationRoot`, `MainMenu`, `StartSizingFrame` | `150` |
| `$PanelInset` | the glass: `AcrylicBorder`, `AppBorder`, `AcrylicOverlay`, `LayerBorder`, `Grid#Root > Border`, all three drop shadows | `0,0,0,30` |

The bottom number should match `-$TrayShiftY`, since both are correcting for
the same invisible strip.

### Bottom corners

At full height the bottom of the panel sits off the bottom of the screen, so
LiquidGlass never has to say anything about its bottom corners. Once the window
is short they are on screen, and two more things needed fixing:

- **Rounding.** `Grid#Root > Border`, `Border#LayerBorder`,
  `Border#StartDropShadow` and `Border#RootGridDropShadow` all paint a corner
  and all needed the same `$CornerRadius`; whichever layer disagreed drew a
  square bottom over the ones that did not. `MaxHeight=$MenuHeight` keeps the
  panel layers from hanging past the shortened window, where the clip would
  take the rounding off again.
- **Border weight.** LiquidGlass's `$BorderThickness` is `0.3,1,0.3,0.3` -- a
  bright top edge, a faint everything else. The fork uses
  `$PanelBorderThickness` (`0.3,1,0.3,1`) on `Border#AcrylicBorder`,
  `Border#AppBorder` and `Border#AcrylicOverlay` so the bottom edge is drawn as
  heavily as the top and the bottom corners match the top pair. Set it to
  `0.3,1,0.3,0.3` to get LiquidGlass's original asymmetry back.

### Why the tray was clipped on the first build

`Grid#MainContent` was pinned to `$MenuHeight` alongside its ancestors. That is
wrong: MainContent starts some way *below* the frame's top edge -- OnlySearch
(Minimal) compensates for the same offset with `Margin=0,-60,0,0` -- so a box
forced to the full window height hangs that far past the bottom of the window,
taking its last row, the tray, with it. MainContent is now left unpinned and
simply fills what the pinned ancestors give it, so its bottom row lands on the
window's bottom edge. `MenuHeight` has to cover that offset too, which is why
it is larger than the two rows alone would suggest.

Every LiquidGlass constant (`Background`, `BorderBrush`, `CornerRadius`,
`ElementCornerRadius`, ...) is inherited unchanged and can be overridden the
same way.

## Custom theme: TranslucentStartMenu - search, profile & power only

Select **"TranslucentStartMenu - search, profile & power only"** in the mod's
Theme setting.

TranslucentStartMenu with the middle taken out. Gone: the pinned list, the
category folders, the all-apps grid, the recommended section and the phone-link
companion on the right. Kept: the search box, the profile picture and the power
button -- with every TranslucentStartMenu style (the 25-blur glass, the reveal
borders, the corner radii) copied over verbatim, so the delta against a future
upstream TranslucentStartMenu stays easy to replay.

### How the middle is removed

Both Start menu layouts get a variant, since TranslucentStartMenu has one for
each and the mod picks between them at runtime.

**Redesigned layout** -- `Grid#MainContent` has three rows:

| Row | Element | Fate |
|---|---|---|
| 0 | search box | kept |
| 1 | `Frame#StartFrame` | **collapsed** -- pinned, folders, all apps, recommended |
| 2 | `Grid#NavPanePlaceholder` | kept -- the tray with profile picture + power |

**Classic layout** -- the app content lives in two roots rather than one frame:

| Element | Fate |
|---|---|
| `Grid#UndockedRoot` | **collapsed** -- pinned list + recommended |
| `Grid#AllAppsRoot` | **collapsed** -- the all-apps list |
| `Button#ShowAllAppsButton` / `Button#CloseAllAppsButton` | **collapsed** -- nothing left to switch between |
| search box, nav pane, profile picture, power | kept -- all sit outside both roots |

OnlySearch's classic variant collapses `Grid#UndockedRoot` on its own, which is
exactly what makes it a search-only menu; `Grid#AllAppsRoot` is added here so
the all-apps page cannot be reached either.

### Why NavPanePlaceholder stays visible

`Grid#NavPanePlaceholder` is the parent of both `StartDocked.UserTileView` and
`StartDocked.PowerOptionsView`. The OnlySearch themes collapse it, which is
precisely why they show neither control: a collapsed parent makes its children
unrenderable no matter what is set on them. Both it and the
`StartDocked.NavigationPaneView#NavigationPane` inside it are pinned visible.

### Tuning

| Constant | Default | Controls |
|---|---|---|
| `MenuHeight` | `100000` | caps the height of the whole window |
| `CompanionVisibility` | `Collapsed` | the phone-link companion panel on the right |

`CommonBgBrush` is inherited from TranslucentStartMenu unchanged and can be
overridden the same way.

### Shrinking the window

`MenuHeight` is the only knob, and its default is past what any display can
reach -- so leaving it unset caps nothing and the menu is exactly the size
Windows gives it. Set it in the mod's **Style constants** setting; no recompile:

```
MenuHeight=220
```

Start around 220 and work down until it is snug. The usable area inside is
roughly `MenuHeight` minus the ~60px offset above `Grid#MainContent`, so it has
to clear about 165 (≈60 offset + ≈48 search row + ≈56 tray) before both rows fit
at all. **150 is below that floor**, which is why it clips the tray.

### One element is capped, and nothing else is touched

This is the whole design, and it is a correction of an earlier revision.

That revision pinned `Height`, `MinHeight` and `MaxHeight` to the same number on
`Grid#FrameRoot`, `Grid#AnimationRoot`, `Grid#MainMenu` **and**
`StartDocked.StartSizingFrame`, then added `ContentShiftY`, `SearchShiftY`,
`TrayShiftY`, `TrayAlignment`, `PanelInset` and `ShadowInset` to correct what
that did. It did not work, and the failure was symmetric in a telling way:

| `MenuHeight` | symptom |
|---|---|
| 150 | bottom borders and the profile/power tray clipped off the bottom |
| 1000 | search box and top borders clipped off the top |

Those four boxes **do not share a top edge**. Pinning them all to one number
stacks their offsets, so the content appears to slide rather than shrink: too
small and the tray falls off the bottom, too large and the frame overflows a
window the screen has already capped, taking the search box off the top. No
amount of shift constants fixes a stack of conflicting explicit heights --
each nudge just moves the clipping somewhere else.

An explicit `Height` inside a stretch slot is also centred rather than
top-aligned, and the `RenderTransform` nudges were overwriting transforms the
menu's own open animation drives. Both make the "everything shifted down"
reading exact.

So all of it is gone. What is left is what OnlySearch does, which is one rule:

```
Grid#FrameRoot              { MaxHeight=$MenuHeight; MinHeight=0 }   (redesigned)
StartDocked.StartSizingFrame{ MaxHeight=$MenuHeight; MinHeight=0 }   (classic)
```

Both targets are listed in both variants; the one absent from a given build's
tree simply does not match. `MaxHeight` caps without positioning, so nothing
moves -- the star-sized row just has less room to claim.

`Grid#MainContent`, the search box, the tray and the glass layers are all left
alone deliberately. With nothing pinned inside it, MainContent fills whatever
the capped frame gives it and its rows land where they belong; the glass
stretches to the window and keeps its borders and corner radii without help.
This theme also needs no corner-radius block, unlike the LiquidGlass fork: it
already gives every layer that paints a corner a radius of its own, and it
collapses `Border#AcrylicOverlay` and `Border#LayerBorder` outright, so no layer
can drop a square bottom over the rounded ones.

`MinHeight=0` is the one rule that is not a provable no-op. It only ever
permits, never forces, and the menu is full height because the star row
stretches to fill -- not because of any minimum -- so at the default cap it
changes nothing. It is there so an app-set minimum cannot fight the cap.

## Fork feature: system information panel

**Start menu fork.** Not a theme -- a mod setting, so it composes with whichever
search-only theme you are running. Turn on **System information panel** in the
*Windows 11 Start Menu Styler - Fork* mod's settings.

The search-only themes empty the middle of the Start menu and leave it as glass.
This fills it with a Task Manager "Performance" style readout: text only, label
on the left, live value on the right.

```
+--------------------------------------------------------+
|  [avatar]      ( Search for apps, settings... )  [power]|
+--------------------------------------------------------+
|                                                        |
|  CPU  .  Intel Core i7-13700K   Memory  .  31.9 GB     |
|  Utilization           23%      In use   14.2 GB (45%) |
|  Speed            3.79 GHz      Available     17.7 GB  |
|  Processes             312      Committed 18.4/36.7 GB |
|  Threads             4,821      Cached         9.1 GB  |
|  Handles           158,203      Paged pool      812 MB |
|  Up time      3:04:17:22        Non-paged pool  1.1 GB |
|                                                        |
|  Disk                           Network  .  Ethernet   |
|  Active time             4%     Send         220 Kbps  |
|  Read              1.2 MB/s     Receive      4.1 Mbps  |
|  Write             380 KB/s                            |
|  C:      412 GB of 931 GB                              |
|                                                        |
+--------------------------------------------------------+
```

### It takes the slot the app content vacated

The panel is inserted as a **sibling** of the element the theme collapsed, and
copies that element's `Grid.Row`, `Grid.RowSpan`, `Grid.Column` and
`Grid.ColumnSpan`. So it lands exactly where the app content was, with the
search box still above it and the profile picture and power button still below
it -- and no row index is hardcoded anywhere.

| Layout | Anchor | Which is also |
|---|---|---|
| redesigned | `Frame#StartFrame` | what the search-only themes collapse |
| classic | `Grid#UndockedRoot` | what the search-only themes collapse |

Both names can exist in the same tree, so the anchor is chosen from the detected
layout rather than from whichever element the visual tree watcher happens to
report first.

Collapsing the anchor is what makes the room; the panel does not depend on it
and does not do it. That is why this is **off by default** — with a theme that
keeps the app list visible, the panel would be drawn over the top of that list.

### Set a height to close the gap underneath

The panel does not resize the menu. On the search-only themes the middle row is
star-sized, so with `MenuHeight` unset the Start menu is still full height and
the panel sits in the middle of a lot of empty glass.

Set `MenuHeight` in the theme's **Style constants** to bring the bottom edge up
to the content. Roughly, the menu needs the panel's own height plus about 165
for the offset above `Grid#MainContent`, the search row and the tray:

| Sections enabled | Rows of sections at `columns = 2` | Try `MenuHeight` |
|---|---|---|
| CPU + Memory | 1 | `320` |
| CPU + Memory + Disk + Network (default) | 2 | `430` |
| ...plus GPU | 3 | `500` |

Work down from there until it is snug. See
[the TranslucentStartMenu section](#shrinking-the-window) for why `MenuHeight`
caps exactly one element and touches nothing else.

### What each number comes from

Everything here is cheap, needs no elevation, and needs no third-party library.

| Section | Rows | Source |
|---|---|---|
| CPU | Utilization | `GetSystemTimes` deltas |
| | Speed | `CallNtPowerInformation(ProcessorInformation)`, mean current MHz |
| | Processes, Threads, Handles | `GetPerformanceInfo` |
| | Up time | `GetTickCount64` |
| Memory | In use, Available | `GlobalMemoryStatusEx` |
| | Committed, Cached, pools | `GetPerformanceInfo` |
| Disk | Active time, Read, Write | `\PhysicalDisk(_Total)` counters |
| | free space | `GetDiskFreeSpaceEx`, polled every tenth tick |
| Network | Send, Receive | `\Network Interface(*)` counters |
| GPU | Utilization (3D) | `\GPU Engine(*engtype_3D)` counters |

Counters are added with `PdhAddEnglishCounter`, not `PdhAddCounter`: the display
names are localised, the English names are not, so the paths above are safe on a
non-English Windows.

Units follow Task Manager rather than being uniform — disks in bytes per second,
network in **bits** per second. A 1 MB/s download therefore reads as 8 Mbps,
which is the same thing Task Manager shows you.

The **Disk** throughput rows cover all physical disks (`_Total`); only the free
space row is specific to the drive you pick. The **Network** rows describe the
busiest real adapter, which is named in the heading — loopback, tunnelling and
filter pseudo-adapters are excluded so that a machine with Hyper-V or a VPN does
not report traffic nobody meant.

**GPU is off by default.** It is the only figure here that needs a wildcard
counter, and on a machine with many GPU engine instances it costs more to sample
than everything else on the list put together. One checkbox turns it on.

### Idling while the Start menu is closed

Sampling runs on a background thread, not on the UI thread — all the measuring,
arithmetic and string building happens off it, and the dispatcher callback only
assigns `Text`. When the menu is off screen the tick skips both the sampling and
the post, and costs one wait per interval.

Visibility comes from `XamlRoot::IsHostVisible`, and the signal is only honoured
**once it has reported true at least once**. Until then the sampler ignores it
and samples every tick.

That asymmetry is deliberate, and it is the fix for a real bug. The first
version gated on finding a `Windows.UI.Core.CoreWindow` for the process, the way
the rest of this mod locates its UI thread. On build 26200 that window does not
exist — `StartMenuExperienceHost.exe` has **no top-level window at all**, open or
closed:

```
=== Start menu CLOSED : 0 window(s) ===
=== Start menu OPEN   : 0 window(s) ===
```

So the gate never opened, and the panel rendered its labels and never showed a
single number. A visibility check that cannot answer must fall back to sampling,
never to skipping: being wrong the first way wastes a few hundred microseconds a
second, and being wrong the second way costs the entire feature. Only a signal
that has demonstrably gone up can be trusted to bring the sampler back when it
goes down again.

The same finding explains why `Wh_ModUninit` stops the sampler directly rather
than relying on `UninitializeSettingsAndTap` — that path runs only
`if (hCoreWnd)`, which on this build is never.

### Styling it

Every element is named, so the styler can theme the panel with ordinary target
rules like any native element:

| Target | What it is |
|---|---|
| `Grid#WhSysInfoRoot` | the panel, filling the vacated slot |
| `StackPanel#WhSysInfoSection` | one component (CPU, Memory, ...) |
| `TextBlock#WhSysInfoHeader` | that component's heading |
| `Grid#WhSysInfoRow` | one label/value pair |
| `TextBlock#WhSysInfoLabel` | the left-hand label |
| `TextBlock#WhSysInfoValue` | the right-hand value |

Colours are looked up from `TextFillColorPrimaryBrush` and
`TextFillColorSecondaryBrush`, so the panel follows the system light/dark theme
without any rules at all.

### Settings

| Setting | Default | Controls |
|---|---|---|
| System information panel | off | the whole feature |
| CPU / Memory / Disk / Network sections | on | which sections appear |
| GPU section | off | see above |
| columns | `2` | sections placed side by side before wrapping to a new row |
| refresh interval (ms) | `1000` | sampling cadence while the menu is open |
| font size | `12` | labels and values; headings are one point larger |
| drive | `C` | which drive's free space to report |
| padding | `28,8,28,8` | gap between the panel and the edges of its slot |

## Custom theme: WindowGlass Pill (taskbar)

**Taskbar fork.** Select **"WindowGlass Pill (centered apps, detached tray)"**
in the *Windows 11 Taskbar Styler - Fork* mod's Theme setting.

WindowGlass's styling with the pill separation of **LiquidGlass (Legacy -
Alternate)**:

```
[ widgets  apps  apps  apps ]        ...        [ ^ (o) 12:34 ]
      centred app pill                            right tray pill
```

Stock WindowGlass draws the taskbar as **one continuous capsule**. The app
frame is rounded on its left corners only, the tray on its right corners only,
and a four-column root grid (`* Auto Auto *`) parks the two in adjacent columns
so their flat edges meet in the middle of the screen.

LiquidGlass (Legacy - Alternate) does it differently: neither frame is placed
by column at all. Each one sizes itself (`Width=Auto`) and is positioned by
`HorizontalAlignment` — `Center` for the apps, `Right` for the tray — so they
end up as two independent, fully rounded pills with the desktop showing
between them.

This theme is WindowGlass copied verbatim and then given that geometry.

### What changed, and what did not

**Unchanged — all of WindowGlass's styling except the fill of the three
pills.** The gradient `$BorderBrush`, `CornerRadius=25`, the Morganite/vivo
Sans clock fonts, the `Search` placeholder, the tray icon paddings and radii,
the widgets divider, and every flyout, tooltip, snap-bar and
virtual-desktop-bar rule — all still on WindowGlass's `$Glass`. What the pills
are filled with comes from the Start menu fork instead; see [the pill
glass](#the-pill-glass) below.

**Changed — six layout rules**, each swapped for the Alternate's:

| Target | WindowGlass | This theme |
|---|---|---|
| `Taskbar.TaskbarFrame` | `Grid.Column=1` | `HorizontalAlignment=Center` |
| `TaskbarFrame > Grid#RootGrid` | `CornerRadius=25,0,0,25`, border open on the right | `CornerRadius=$CornerRadius` all round, `$BorderBrush` all round |
| `SystemTray.SystemTrayFrame` | `Grid.Column=2`, aligned left | `HorizontalAlignment=Right` |
| `Grid#SystemTrayFrameGrid` | `CornerRadius=0,25,25,0`, border open on the left | `CornerRadius=$CornerRadius` all round, `$BorderBrush` all round |
| `:root > ... > Grid` | 4-column split + `ActualWidth=>containerGridWidth` | rule dropped entirely |
| `Gripper` / `AugmentedEntryPointContentGrid` | — | `HorizontalAlignment=Left` |

`Taskbar.TaskbarFrame` also takes its width cap without that capture, the way
the Alternate writes it, rather than WindowGlass's
`MaxWidth={{min($TaskbarFrameMaxWidth, containerGridWidth)}}`. That is what
retires the root-grid rule: nothing consumes the capture any more. See [why the
width capture had to go](#why-the-width-capture-had-to-go) for why that matters
— it is not cosmetic, and [how wide the app pill
gets](#how-wide-the-app-pill-gets) for what took its place.

### Why the width capture had to go

The first version of this theme kept `ActualWidth=>containerGridWidth` and the
`min(...)` expression, on the reasoning that clamping against the real
container width is strictly better than a fixed number. It is not, and the
symptom was ugly: **clicking an already-open Quick Settings or calendar flyout
dismissed it and then immediately reopened it**, about half a second later.

`=>` is not a one-off read. It registers a live listener, so every change to
the root grid's `ActualWidth` restyles `Taskbar.TaskbarFrame`. Stock
WindowGlass carries the same capture and is fine, because its two frames are
pinned in fixed `Auto` columns and never move. Once placement moves onto
`HorizontalAlignment`, a restyle that lands mid-click re-fires the tray
button — the flyout light-dismisses from the press, then the re-fire opens it
again.

Worth recording how this was found, because three plausible theories fitted
the evidence and all three were wrong: it was not the tray's layout slot
diverging from where it draws, not the legacy `TrayNotifyWnd` rect, and not
proximity to the screen edge. Each was ruled out by measuring geometry with UI
Automation against `DwmGetWindowAttribute(DWMWA_CLOAKED)` on the flyout, which
is the only reliable open/closed signal here — `IsWindowVisible` is useless,
since the calendar's host window (`Windows.UI.Core.CoreWindow`, owned by
`ShellExperienceHost`) stays visible permanently. What settled it was comparing
against LiquidGlass (Legacy - Alternate), which places the tray hard right the
same way and does **not** have the bug: it uses a static `MaxWidth:=1200`.

So: if you ever reintroduce a `{{...}}` expression or an `=>` capture into this
theme, retest the flyout toggle.

### How wide the app pill gets

The pill is `Width=Auto`, so it is only ever as wide as the buttons in it. What
matters is the ceiling, because once that is reached Windows starts shrinking
the task buttons and then pushing them into the overflow chevron.

That ceiling is **not** a fixed number. `Taskbar.TaskbarFrame` gets

```
Margin=$AppPillGutter,0,$AppPillGutter,0
```

which reserves a strip at each end of the bar and leaves the pill
`bar width - 2 × $AppPillGutter` to grow into. Two things follow:

* It is **per monitor**. A fixed cap clamps a 3440-wide bar to the same width
  as a 1920-wide one; a gutter does not. This is what fixed "the icons shrink
  at 20 windows" — on a 1920 bar the ceiling went from the old fixed
  `1200` to `1920 - 540 = 1380`, and every wider display gains proportionally
  more.
* It stays **centred**. The two gutters are equal, so
  `HorizontalAlignment=Center` still puts the pill on the screen's centre line.
  Keep them equal — unequal gutters slide the pill off centre, since the
  frame centres in whatever is left after its margins.

`$TaskbarFrameMaxWidth` survives as an optional hard cap layered on top,
defaulted to `8000` so that it never binds. Lower it if you want the pill to
stop growing before it reaches the gutters.

#### Why a constant gutter rather than measuring the tray

The obvious version — capture the tray's real width and subtract it —
needs an `=>` capture, which is exactly what [the section
above](#why-the-width-capture-had-to-go) says not to reintroduce. A margin is a
static value with no listener behind it, so the gutters buy back the per-monitor
width without reviving the flyout-reopen bug.

The default of `270` is measured rather than guessed. `scripts\measure-pills.ps1`
walks every taskbar with UI Automation and prints how far each end pill reaches
in from its screen edge, in DIPs — the same unit XAML sizes use — so the
number can be re-derived on any display instead of trusted:

```
=== Shell_TrayWnd  0,1032  1920x48 DIP ===
  app buttons    569 ->  1340   width   771
  tray icons    1684 ->  1901   width   217   reaches   236 from its edge
  pager chips     19 ->   111   width    92   reaches   111 from its edge
  => $AppPillGutter 270, leaving the app pill 1380 DIP to grow into
```

UI Automation reports the *buttons*, not the pill drawn around them, so the
suggestion adds one `$TrayPillPadding` (10) to reach the pill's actual border,
then 20 more for a tray icon's worth of headroom. The right-hand end is the
binding one: 236 + 10 + 20 rounds to `270`. The pager end reaches only 111, so
it has ~150 DIP to spare — about four more desktop chips before it becomes
the constraint instead. If either pill ever overlaps the apps, raise
`$AppPillGutter`; if you run a bare tray, lower it and hand the width back to
the apps.

Because the script reports every bar, run it with the mod live to check a
secondary display too: the gutter is a single constant, so it has to clear the
widest tray of the two.

### The pill glass

The three pills are not filled with WindowGlass's `$Glass`. They use
`$PillBackground`, which is the Start menu fork's `$CommonBgBrush` copied
verbatim — the brush the [TranslucentStartMenu - search, profile & power
only](#custom-theme-translucentstartmenu---search-profile--power-only) theme
paints its own surfaces with:

```
PillBackground = <WindhawkBlur BlurAmount="25" TintColor="#25323232"/>
```

The point is that the taskbar and the Start menu are then the same glass,
rather than two different ones sitting on the same wallpaper. It is far more
transparent than what it replaces: WindowGlass's `$Glass` is a 5px blur under
a 70% opaque grey, this is a 25px blur under a tint that is barely there —
alpha `0x25` of `#323232`.

Only the fill changes. The pills keep WindowGlass's `$BorderBrush`,
`$BorderThickness` and `$CornerRadius`, and everything that is not a pill —
flyouts, tooltips, the snap bar, and the pager's hover card on `$Frosted` —
still uses `$Background`, where a milkier surface keeps text readable.

### Tuning

All pill geometry is style constants — override in the mod's **Style
constants** setting, no recompile:

| Constant | Default | Controls |
|---|---|---|
| `AppPillGutter` | `270` | the strip kept clear at each end of the bar; the app pill's ceiling is `bar width - 2 × this`. See [above](#how-wide-the-app-pill-gets) |
| `TaskbarFrameMaxWidth` | `8000` | optional hard cap on the app pill, on top of the gutters; set out of the way so `AppPillGutter` is what binds (WindowGlass uses `1895` clamped against the live container width — see above for why that is not used here) |
| `AppPillMargin` | `0,2,12,2` | app pill inset; the `12` nudges it left of dead centre |
| `AppPillPadding` | `10,0,10,0` | space inside the app pill (WindowGlass pads the left only) |
| `TrayPillMargin` | `0,2,8,2` | tray pill inset; the `8` is its gap from the screen edge |
| `TrayPillPadding` | `10,0,10,0` | space inside the tray pill |
| `PagerPillMargin` | `8,2,0,2` | pager pill inset; mirror of `TrayPillMargin` |
| `PagerPillPadding` | `10,0,10,0` | space inside the pager pill; mirrors `TrayPillPadding` |
| `PagerChipCornerRadius` | `12` | roundness of an individual desktop chip |
| `PillBackground` | the Start menu fork's `$CommonBgBrush` | the glass in all three pills; `$Glass` puts them back on WindowGlass's |

The pager pill is deliberately the mirror image of the tray pill: same
`$CornerRadius`, same `2` vertical margin, and `8` from its screen edge instead
of the tray's `8` from the right. Two rules make that work — `Grid#WhVdPagerRoot`
gets `Margin=0` to cancel the offset the mod applies for its own standalone
look, and the pill gets `VerticalAlignment=Stretch` so it fills the bar's height
rather than just wrapping its chips. Measured against the tray pill, both come
out 66px tall and 12px from their edge at 150% scaling.

### The third pill

The theme also styles the [Virtual Desktop
Pager](#original-mod-virtual-desktop-pager), giving it the same
`$PillBackground`, gradient border and `CornerRadius` as the tray pill it mirrors, with
the active chip filled in `$ElementSysColor`. Those rules are inert if that mod
isn't installed — the styler never sees the targets — so the theme stays
self-contained.

They sit *after* the catch-all `Windows.UI.Xaml.Controls.Button` rule on
purpose. Styles are applied in array order and the last write to a property
wins, so chips placed before it would inherit `BorderThickness=$BorderThickness`
and pick up a border each.

Every WindowGlass constant (`Glass`, `Background`, `BorderBrush`,
`CornerRadius`, `TrayPadding`, ...) is inherited unchanged and can be
overridden the same way — e.g. `CornerRadius=12` for the flatter Alternate
look, or `PillBackground=$Frosted` for heavier pills.

### Heads-up

Disable the stock **Windows 11 Taskbar Styler** before enabling this fork.
Both inject into `explorer.exe` and both rewrite the same XAML tree.

## Original mod: Virtual Desktop Pager

A third pill on the taskbar, at the far left, listing the virtual desktops that
currently exist. It mirrors the tray pill on the right:

```
[1][2]        [start][search][task view][apps]        [tray] [clock]
 ^^^^^                    app pill                    tray pill
 this mod             Taskbar.TaskbarFrame     SystemTray.SystemTrayFrame
```

Click a chip to switch. Hover one for a card naming the desktop and listing the
windows on it. The current desktop is marked by its number taking the accent
colour rather than by a filled bubble — chips carry no background of their own
in either state, so the row reads as bare numbers. Desktops are still created
and closed with `Ctrl+Win+D` and `Ctrl+Win+F4` — the pager reflects and
switches, nothing more.

### Where it attaches

`XamlRoot.Content()` is a `Grid` whose only children are the app frame and the
tray frame. That is the layer the WindowGlass Pill theme uses to align those two
independently, so the pager goes in as a third child with
`HorizontalAlignment=Left` and needs to know nothing about the other two.

Entry point is a symbol hook on `TaskbarFrame::MeasureOverride`, one pager per
`XamlRoot`. The actual element creation is deferred to the dispatcher rather
than done inside the measure pass — building elements during layout risks the
`AG_E_LAYOUT_CYCLE` fail-fast, which is the same reason the vertical-taskbar mod
defers its styling. Because the hook fires on every measure, injection is
self-healing: a taskbar that gets torn down and rebuilt gets its pager back.

### Losing the boot race, and why it looked intermittent

That self-healing has one hard edge, and it is the whole reason the pager would
come up on one boot and not the next. The hook is self-healing only for taskbars
that measure *after* it is installed. A taskbar measures while it is being
built, then settles and essentially stops — switching desktops does not measure
it, and neither does anything else the pager does. Install the hook one second
late and there is no next measure pass to catch: the mod runs all session, polls
happily, logs desktop changes, and draws nothing.

The trace of a good boot and a bad one differ by less than a second:

```
14:02:34.996  Hooking the taskbar view module (worker poll)
14:02:35.301  TaskbarFrame::MeasureOverride hook is live     <- made it

16:43:50.037  Hooking the taskbar view module (worker poll)
              (nothing -- the taskbar had already laid out)
```

Two things put the hook on the wrong side of that line.

**The loader hook never fired.** `LoadLibraryExW` was hooked precisely so the
module would be caught the moment it loads, which is always early enough. In
every trace taken, the hook that actually landed came from the worker's poll
instead — the shell brings `Taskbar.View.dll` in by some route that does not
pass through `LoadLibraryExW`. The notification now hangs off `LdrLoadDll`,
which sits under every route into the loader.

**The module poll inherited an unrelated backoff.** The worker polls for the
module at the top of its loop, but when the virtual desktop service is not up
yet — normal for the first seconds of a session — the loop sleeps on the
*connect* backoff: 1s, then 2s, then 4s. So the poll cadence silently became the
backoff cadence, and the 2s gap after a second failed connect was the entire
difference between the two traces above. The backoff is now a deadline the
connect attempt checks, not the length of the loop's sleep, so the poll keeps
its own cadence while the service is still coming up.

Both hooks now only `SetEvent` the worker rather than hooking symbols inline:
they run on a thread that is inside a module load, and `HookSymbols` can load
modules of its own and reach for the symbol cache on disk. The worker owns every
hook attempt, and wakes immediately.

The reason string in the log says which path got there, so a future trace
answers the question directly:

```
Hooking the taskbar view module (loader notification)
```

**Still true:** loading the mod into an already-settled taskbar — which is what
Windhawk does when you hit Compile — leaves no pager until something makes the
taskbar measure again. Restart explorer to test a build; a reboot also works.

### Two things specific to this build that cost time

**The frames under the island content grid are unnamed.** `FindChildByName(content,
L"TaskbarFrame")` — lifted from the vertical-taskbar mod — rejected every
taskbar on 26200, so nothing was ever inserted. Matching on *class* name
(`Taskbar.TaskbarFrame`, `SystemTray.SystemTrayFrame`) is both correct here and
consistent with how the styler targets things.

**Tooltips are clipped, so the hover card is a popup.** `ToolTipService` hosts
its tooltip in a Popup constrained to the XAML island, and the island is exactly
taskbar-height — a tooltip placed above a chip falls entirely outside it and
never appears. The taskbar's own tooltips dodge this because they are real
HWND-backed flyouts. The card is therefore a `Popup` with
`ShouldConstrainToRootBounds(false)`, which is the API added for islands
precisely so a popup can paint outside its root.

### The virtual desktop API

Switching uses the undocumented `IVirtualDesktopManagerInternal`, reached through
`CLSID_ImmersiveShell`. Only the first seven methods are declared and nothing
past `SwitchDesktop` is ever called: that prefix has been stable since 22621,
while the tail of the vtable is where Microsoft keeps adding methods.

Microsoft rotates the interface ID between builds. The mod probes the IDs it
knows and keeps the one that answers — `QueryService` fails cleanly with
`E_NOINTERFACE` on a mismatch, so a wrong ID never yields a pointer to call
into, and the winner still has to return a sane `GetCount` before it is trusted.

| Build | IVirtualDesktopManagerInternal | IVirtualDesktop |
|---|---|---|
| 26100+ (24H2/25H2) | `53F5CA0B-158F-4124-900C-057158060B27` | `3F07F4BE-B107-441A-AF0F-39D82529072C` |
| 22621 / 22631 | `A3175F2D-239C-4BD2-8AA0-EEBA8B0B138E` | `3F07F4BE-…` (same) |

Verified working on **26200**, which shares 26100's IDs. If a future build breaks
detection, add its IDs to `g_versionIIDs` — they are usually published quickly in
[MScholtes/VirtualDesktop](https://github.com/MScholtes/VirtualDesktop) — and use
the **Windows version** setting to pin it meanwhile.

Build 22000 is deliberately unsupported: every method there took an extra
`HMONITOR`, which is a different vtable, not a different ID.

The hover card's window list is the opposite: it uses the fully documented
`IVirtualDesktopManager::GetWindowDesktopId` over `EnumWindows`, so it carries no
version risk at all. Live thumbnails are not possible — windows on other desktops
are shell-cloaked and DWM will not render a thumbnail of a cloaked window, which
is why Task View's previews are out of reach from a mod.

### Styling

Everything is plain XAML with stable names, so the taskbar styler fork themes it
like any native element:

| Target | What it is |
|---|---|
| `Grid#WhVdPagerRoot` | outer container, positions the pill |
| `Border#WhVdPagerBackground` | the pill: background, corner radius |
| `StackPanel#WhVdPagerItems` | the row of chips |
| `Button#WhVdDesktopButton` | an inactive chip |
| `Button#WhVdDesktopButtonActive` | the active chip (transparent; the accent lives on its label) |
| `TextBlock#WhVdDesktopLabel` | the number or name inside a chip |
| `Border#WhVdHoverCard` | the hover card |
| `TextBlock#WhVdHoverTitle` / `#WhVdHoverWindows` | its two lines |

The active chip gets its own *name* rather than a visual state, because the
styler evaluates a target's conditions once when it first sees an element — a
state change on a live element would not re-trigger it. The chip row is rebuilt
whenever the desktop state changes, so both targets keep matching.

### Tuning

| Setting | Default | Controls |
|---|---|---|
| `ChipSize` | `28` | chip height and minimum width |
| `ChipSpacing` | `4` | gap between chips |
| `PillPadding` | `4` | chips to pill edge |
| `PillMargin` | `8,4,0,4` | pill offset from the screen's left edge |
| `PillCornerRadius` | `16` | matches the tray pill |
| `LabelStyle` | `number` | `number` or the desktop's `name` |
| `HoverDelayMs` | `450` | how long to hover a chip before its card appears; `0` for instant. Only the first card waits -- once one is open, moving along the row swaps it immediately, as Windows tooltips do |
| `HideWhenSingleDesktop` | `false` | hide the pill until a second desktop exists |
| `ShowOnSecondaryTaskbars` | `true` | |
| `WindowsVersion` | `auto` | interface-ID override, see above |
| `PollIntervalMs` | `400` | how often the desktop list is re-read |
| `DebugLogToFile` | `false` | also append the log to `vdp-debug.log` in `%TEMP%` |

`DebugLogToFile` exists because Windhawk's log window and any other debug-output
viewer compete for the same messages: `DBWIN_DATA_READY` is auto-reset, so each
message wakes exactly one reader and a second viewer silently sees about half
the lines. A file is the only reliable full trace.

### Threading

All COM lives on one MTA worker thread that never touches XAML: it polls, and
only when the count or the active desktop actually changes does it marshal a
snapshot to each taskbar's dispatcher. Clicks post switch requests back to it.
A shell service that hangs therefore cannot stall the taskbar.

### Credits

The interface IDs and the vtable layout come from two GPL sources — the
**Virtual Desktop Helper** mod by u2x1 and **MScholtes/VirtualDesktop**. The
visual tree helpers and the deferred-layout pattern follow m417z's **Vertical
Taskbar for Windows 11**.

## Original mod: Instant Virtual Desktop Switch

`Ctrl+Win+Left/Right` plays a slide transition, and during it the taskbar looks
like it blinks out and comes back. This mod removes the transition for desktop
switches only, so the taskbar, the tray and the pager stay put while the icons
update in place.

### The taskbar was never the problem

The obvious theory — something hides the taskbar during the switch — is
wrong, and it is worth recording how that was settled, because the measurement
is cheap and the wrong theory leads straight to a pointless hooking mod.

Tracking `Shell_TrayWnd`, its XAML island and all eight descendants at 8ms
across a switch produces *no* state change at all:

```
1.009s  >>> Ctrl+Win+RIGHT
1.086s  === CURRENT DESKTOP -> 92474a97
1.139s  ReBarWindow32   vis=1 wsvis=1 cloak=0  rect=485,1019 704x41
```

`IsWindowVisible` stays 1, `DWMWA_CLOAKED` stays 0, the rect never moves. The
only event is the icon strip re-laying out ~130ms after the switch, which is the
wanted behaviour, not the bug. Rendering the taskbar's own content with
`PrintWindow(PW_RENDERFULLCONTENT)` every 20ms agrees: 91 frames, every one a
fully painted taskbar, ink coverage moving only 63.2% → 67.1% as the window
count changes.

So nothing in explorer takes the taskbar away, and there is nothing there to
hook. What blinks is DWM compositing the slide over the whole screen.

One measurement trap on the way: `Graphics.CopyFromScreen` BitBlts the screen DC
and does **not** include DWM-composited content, so it captures the windows
*behind* the taskbar and appears to show it vanishing. `PrintWindow` with
`PW_RENDERFULLCONTENT` is the one that sees the taskbar.

### The lever

The only thing that reaches the transition is `SPI_SETCLIENTAREAANIMATION`, the
system's "animation effects" flag. `MinAnimate` is a different setting and is
never touched, so minimize and maximize animations are unaffected either way.

The mod has two modes, and the difference between them is entirely about *when*
the flag is cleared.

**Always** (default) holds the flag off for as long as the mod is enabled. It is
unconditional, so it works regardless of what has focus and covers every way of
switching -- keyboard, Task View, a pager chip, a touchpad swipe. The cost is
that the same flag animates elements inside windows, so the Start menu loses its
open animation.

**Only around the switch shortcut** clears the flag for `RestoreDelayMs` around
`Ctrl+Win+Left/Right` via a `WH_KEYBOARD_LL` hook, keeping the Start menu
animation the rest of the time. The key is never swallowed -- the shell still
performs the switch itself, which keeps the mod out of the undocumented virtual
desktop COM API and removes any chance of switching twice. Clearing the flag
costs ~17us against the 300ms `LowLevelHooksTimeout` budget, so doing it inline
in the hook is safe.

### Why per-switch mode is not the default

A low-level keyboard hook inside medium-integrity `explorer.exe` is silently
skipped by UIPI whenever an **elevated window has focus**. Measured here, 3 of 6
switches did nothing for exactly that reason:

```
#0 LEFT  on D1  MISS      shaman     elevated=1   <- focused on every MISS
#1 RIGHT on D1  MISS      VSCodium   elevated=1
#2 RIGHT on D2  HIT       chrome     elevated=0   <- focused on every HIT
#3 LEFT  on D2  HIT       explorer   elevated=0   <- where the hook lives
```

The first reading of that data was that it depended on which arrow was pressed,
and the second that it depended on which desktop you were on -- both fitted, and
both were wrong. Direction and desktop were confounded with focus, because the
elevated apps happened to live on one desktop and the browser on the other.
Holding the desktop fixed and varying only focus is what settled it.

It fails safe: the flag stays on and you get the stock slide. But if you run
elevated apps, this mode does nothing a lot of the time, which is why Always is
the default.

### Restoring

The flag is put back when the mod is disabled or unloaded, but only if the mod
is the one that turned it off. If it was already off at load, the mod leaves it
alone and restores nothing -- it never switches on an animation you had
deliberately off. It is also re-asserted every few seconds, so anything else
that turns animations back on cannot quietly bring the blink back.

### Tuning

| Setting | Default | Controls |
|---|---|---|
| `Mode` | `always` | `always` or `perSwitch` |
| `RestoreDelayMs` | `500` | per-switch only; the slide is ~300ms |
| `HandleNewDesktop` | `true` | per-switch only; `Ctrl+Win+D` |
| `HandleCloseDesktop` | `true` | per-switch only; `Ctrl+Win+F4` |
| `DebugLogToFile` | `false` | also append the log to `ids-debug.log` in `%TEMP%` |

## Original mod: Desktop Drag-Select With Icons Hidden

Turning off **Show desktop icons** also takes the drag-select rectangle away.
This mod draws the rectangle itself, so it comes back without the icons coming
back with it.

### Why the rectangle goes away with the icons

"Show desktop icons" is not a draw flag. The desktop is three nested windows,
and unchecking the option hides the innermost one:

```
Progman
  SHELLDLL_DefView          stays visible, full screen
    SysListView32           ShowWindow(SW_HIDE) when icons are off
```

The marquee is a *list view* feature. With `SysListView32` hidden there is
nothing left to draw it, and the click lands on `SHELLDLL_DefView`, which has no
selection behaviour of its own. Same reason Ctrl+A, arrow-key navigation,
type-to-select and dropping files on the desktop all stop at once — one hidden
window takes them all out together.

Measured here with icons off, which is also what settled where to attach:

```
Progman            hwnd=0x000302F0  visible=True
  SHELLDLL_DefView hwnd=0x0003030E  visible=True    <- click at centre lands here
    SysListView32  hwnd=0x00020312  visible=False
  WorkerW          hwnd=0x0002030A  visible=True
    WPEDesktopDX11Window            visible=True    <- Wallpaper Engine, below DefView
```

`ChildWindowFromPointEx(Progman, centre, CWP_SKIPINVISIBLE | CWP_SKIPTRANSPARENT)`
returns `SHELLDLL_DefView`, so that is the window that is subclassed.

### Why the rectangle is its own window

It is a layered, click-through `WS_POPUP` rather than paint on the desktop's own
DC. With Wallpaper Engine running, anything drawn straight onto the desktop is
wiped by the next wallpaper frame; a layered window is composited by DWM and
stays put. It also keeps the classic `DrawFocusRect` XOR artefacts out of it.

One DIB the size of the virtual screen is allocated per drag and released at the
end. Each frame fills only its top-left corner and hands `UpdateLayeredWindow`
that sub-rectangle, so no frame allocates.

### Two things that would otherwise bite

**Every second drag would go missing.** A fast run of drags — which is the whole
point of the mod — makes Windows deliver alternate presses as
`WM_LBUTTONDBLCLK` rather than `WM_LBUTTONDOWN`. Both start a rectangle.

**`SetCapture` on a window that already holds capture** can bounce a
`WM_CAPTURECHANGED` straight back, and that handler clears the drag state before
the first `WM_MOUSEMOVE` arrives. It is guarded with `GetCapture() != hWnd`.

### Following the accent colour

The rectangle is always the system accent, fill and border alike, re-read at the
start of every drag. Change the accent and the next drag already has it, with
nothing to reload and no change notification to subscribe to — one registry read
per drag, not per frame.

The value read is `Explorer\Accent\AccentColorMenu`, which is the accent shown
in Settings, stored as `0xAABBGGRR`. A `COLORREF` is `0x00BBGGRR`, so the low
three bytes drop straight in with no shuffling.

`DwmGetColorizationColor` is only the fallback, because what it returns is the
*colorization* colour, which can be a blend rather than the accent. Both were
checked here and agree — registry `0xFF6F6FD5` and DWM `0xC4D56F6F` are both
RGB(213, 111, 111) — but the registry value is the one that is definitionally
right.

### Rounded corners without GDI+

The corners come from the signed distance to a rounded rectangle, which buys
three things at once:

- anti-aliasing falls out as `clamp(0.5 - d, 0, 1)`
- the border is the *same* field offset by its own width, because insetting a
  rounded rect by `w` shifts its distance field by exactly `w`
- no GDI+, no extra library, no `GdiplusStartup` on the shell thread

Fill and border coverage are disjoint, and both colours are already
premultiplied, so compositing them is a straight weighted sum — including the
alpha channel, which is what keeps the anti-aliased edge translucent rather than
haloed.

Only the four corner boxes evaluate the field. Every other pixel is on an
axis-aligned edge that is already pixel-exact and takes a straight run instead,
which matters when one frame can cover the whole screen.

Verified by rendering `PaintMarquee` standalone and dumping the alpha plane:
border 230 (90%), fill 64 (25%), with a ramp of 31, 69, 104, 139, 172, 184, 225
along the arc, symmetric about the diagonal. Degenerate sizes were run too —
1x1, 2x2, 13x5, 1x200, and radius 64 / border 32 on a 40x28 box, where both
clamp to `half` and give a clean stadium rather than garbage.

### Fade-out

Releasing fades the rectangle over 180ms on a quadratic ease-out, so it holds
near full opacity for a moment and then drops away; a linear ramp reads as an
abrupt cut right at the end.

Each frame is one `UpdateLayeredWindow` that changes only `SourceConstantAlpha`
and passes null for the source DC and geometry, which keeps the surface,
position and size the window already has — so the fade re-blits nothing. The
virtual-screen-sized DIB is released the moment the fade starts rather than held
for its duration, since the layered window keeps its own copy of the surface.

Two ordering traps here, both of which show up only as stranded rectangles:

**`ReleaseCapture` re-enters the teardown.** It sends a `WM_CAPTURECHANGED`
which lands back in the same handler, so the drag flags are cleared *before* the
fade starts and before capture is released — otherwise that second pass tears
down the fade the first one just started.

**A press that never becomes a drag.** Pressing during a fade has to cancel it,
but if that press never crosses the drag threshold nothing would ever hide the
rectangle again. So a press hides the overlay outright rather than just
cancelling the fade.

### Standing down

The click is only taken when `SysListView32` is hidden, checked live on each
press. Turn icons back on and the mod does nothing at all — the real marquee,
icon selection and file drops are stock. Right-click is never intercepted unless
a drag is in progress, so the desktop context menu keeps working.

The overlay's window class is registered against `explorer.exe`'s `hInstance`
with `DefWindowProc` as its window procedure, so nothing in it points into the
mod's DLL and an unload cannot leave a dangling proc behind.

That also leaves the overlay unable to run a handler of its own, which is why
the fade timer lives on the subclassed desktop window instead, under an id far
outside the small integers the shell uses for its own timers.

`KillTimer` and `DestroyWindow` both have to run on the thread that owns the
window — the shell thread, not the one unloading the mod. So uninit sends a
registered message to the desktop window, handled inside the subclass while it
is still installed, and removes the subclass only afterwards. It goes through
`SendMessageTimeout` with `SMTO_ABORTIFHUNG` so a wedged shell thread cannot
hang the uninstall.

### Tuning

| Setting | Default | Controls |
|---|---|---|
| `FillOpacity` | `25` | 0 gives a hollow outline |
| `BorderOpacity` | `90` | |
| `BorderWidth` | `1` | 0 gives a plain translucent block |
| `CornerRadius` | `6` | 0 gives square corners and skips the distance field |
| `FadeOutMs` | `180` | 0 makes the rectangle vanish instantly |
| `DebugLogToFile` | `false` | also append the log to `dds-debug.log` in `%TEMP%` |

There is deliberately no colour setting — see below.

## Workflow

**1. Edit** `mods/<forkid>.wh.cpp` in your editor of choice.

**2. Check it compiles** — fast, no Windhawk UI needed:

```powershell
.\scripts\check.ps1                 # all mods
.\scripts\check.ps1 start-menu      # one mod, substring match
.\scripts\check.ps1 -Warnings       # add -Wall -Wextra
```

Runs Windhawk's own bundled clang with the editor's exact flags. Exits non-zero
on error, so it drops straight into a pre-commit hook or watch loop.

**It is `-fsyntax-only`, so it cannot catch a missing `@compilerOptions -l`.**
A forgotten library compiles clean here and only fails at Compile in the UI with
`ld.lld: error: undefined symbol`. If you hit that, look up which library owns
the symbol and add it — the window-subclass helpers in `windhawk_utils.h`
(`SetWindowSubclass`, `DefSubclassProc`) need `-lcomctl32`, which is easy to
miss because the header comes for free.

**3. Build + install** — this part goes through the Windhawk UI:

```powershell
.\scripts\stage.ps1 start-menu            # copy into Windhawk's editor buffer
.\scripts\stage.ps1 start-menu -Clipboard # or just copy to clipboard and paste
```

Then hit **Compile** in Windhawk.

**4. Review your delta:**

```powershell
.\scripts\diff.ps1 -Stat     # "1 hunk(s), +2 / -2 lines vs upstream baseline"
.\scripts\diff.ps1           # full diff
.\scripts\diff.ps1 -Save     # also write patches\<forkid>.patch
```

**5. When upstream releases a new version:**

```powershell
.\scripts\sync-upstream.ps1              # check + vendor the new source
.\scripts\sync-upstream.ps1 -ShowDiff    # see what upstream changed
# ... merge those changes into mods\<forkid>.wh.cpp by hand ...
.\scripts\check.ps1                      # confirm it still compiles
.\scripts\sync-upstream.ps1 -UpdateBaseline   # move the baseline pointer
```

`sync-upstream.ps1` never edits anything in `mods/`. It only vendors new
upstream baselines and reports the gap.

### Diagnostics: measure-pills.ps1

```powershell
.\scripts\measure-pills.ps1          # where each pill starts and ends
.\scripts\measure-pills.ps1 -All     # plus every element it walked
```

Prints the geometry of the WindowGlass Pill theme's three pills, per taskbar,
and suggests an `$AppPillGutter` from it. See [how wide the app pill
gets](#how-wide-the-app-pill-gets) for what that constant does.

Coordinates come out in **DIPs, not physical pixels**, because the script is
deliberately left DPI-unaware — UI Automation then hands back rects already
divided by the scale factor. DIPs are what XAML's `MaxWidth`, `Margin` and
`Padding` are in, so the numbers paste straight into a style constant. At 150%
scaling a 2880-px-wide screen reads as `1920`.

The one thing to keep in mind reading it: UI Automation sees *buttons*, not the
pill the theme draws around them. The pill's border sits one padding further
out on each side, which is why the suggested gutter is the measured reach plus
`$TrayPillPadding` plus headroom.

### Diagnostics: trace-flyout.ps1

```powershell
.\scripts\trace-flyout.ps1 -Seconds 45
```

For the tray flyout toggle bug — clicking an open Quick Settings or calendar
dismisses it and it comes straight back. It reports every close that was
followed by an open of the same window **with no click in between**. That is the
bug's exact signature, so a run either catches it or clears the configuration
under test — which is the point, because the symptom is intermittent and
"seemed fine" is not evidence.

Open/close comes from `SetWinEventHook` (SHOW/HIDE, CLOAKED/UNCLOAKED, DESTROY,
foreground), not from polling. Mouse transitions and the `Shell_TrayWnd` /
`TrayNotifyWnd` rects are sampled on a tight loop interleaved with the message
pump those out-of-context hooks require.

**Why not polling.** The first version discovered windows once a second and
checked their cloaked state. It recorded 36 clicks and *zero* opens: under fast
clicking the flyouts opened and closed inside the polling gap, and its
`120x120` size filter had discarded the hosts anyway — the tray overflow window
is `234x74`. Two independent blind spots, both invisible in the output until the
foreground-window column proved flyouts had been opening the whole time. Events
have no gap and need no size guesswork.

What survived from that version, because it is still true:

* **Cloaked, not visible.** `IsWindowVisible` is useless here: the calendar's
  host (`Windows.UI.Core.CoreWindow`, owned by `ShellExperienceHost`) stays
  visible permanently. Cloak transitions are what count, so they are hooked
  alongside show/hide.
* **No process allowlist.** The flyout hosts start on demand — `ShellExperienceHost`
  is usually not even running until the first Quick Settings open — and which
  process owns which flyout has moved between builds.
* **Windows come and go.** A flyout host can be destroyed rather than cloaked,
  so open/close pairs are matched on process+class+title, not on `HWND`.

Sanity-check the `HOOK` line at the top of a run: it should read `3 of 3`. If
`opens: 0` comes back with clicks recorded, the trace proved nothing — do not
read it as a clean result.

Run it once per configuration when A/B-ing (theme swapped, a co-installed mod
disabled, the styler off entirely) and compare the reopen counts.

## Adding another fork

1. `.\scripts\sync-upstream.ps1` after adding the entry, or fetch by hand from
   `https://raw.githubusercontent.com/ramensoftware/windhawk-mods/main/mods/<id>.wh.cpp`
   into `upstream/<id>@<version>.wh.cpp` (LF endings).
2. Copy it to `mods/<id>-lariccia.wh.cpp` and change `@id` (and ideally `@name`)
   in the `// ==WindhawkMod==` header.
3. Add it to `forks.json`.
4. **Disable the original mod in Windhawk** — running both against the same
   process is asking for trouble.

## Why builds go through the UI

Windhawk has no command-line compile. Its documented flags
(`Program Files\Windhawk\command-line.txt`) are only `-tray-only`, `-exit`,
`-restart`, `-safe-mode`.

A standalone link was attempted and is **not** viable: mod DLLs reference
`InternalWh_*` symbols (`InternalWh_SetFunctionHook`, `InternalWh_GetUrlContent`,
…) which are not exported from the shipped `windhawk-mod-shim.dll`. Windhawk's
own compile step supplies that plumbing privately.

What *is* viable — and what `check.ps1` does — is `clang -fsyntax-only` with
`-DWH_EDITING`, which catches every compile error in seconds without touching
Windhawk at all. That covers essentially all iteration; the UI Compile is then a
one-shot at the end.

## Line endings

Windhawk's `ModsSource` files on disk are CRLF; GitHub raw is LF. Everything
here is normalized to LF (see `.gitattributes`) and every diff normalizes both
sides first — otherwise a 16,000-line mod shows up as 16,000 changed lines.

## Environment (verified 2026-08-27)

- Windhawk **1.7.3**, `C:\Program Files\Windhawk`
- Bundled toolchain: **clang 20** / MinGW-w64 (x86_64, i686, aarch64)
- Mod sources: `C:\ProgramData\Windhawk\ModsSource`
- Editor workspace: `C:\ProgramData\Windhawk\EditorWorkspace`
- Installed mods live in the registry under
  `HKLM\SOFTWARE\Windhawk\Engine\Mods\<id>` (`LibraryFileName`, `Disabled`,
  `Include`, `Exclude`, `Architecture`, `Version`), with settings in the
  `Settings` subkey — useful to know when debugging, but let the UI write them.

## Licensing

Upstream mods are GPL-3.0. Forks stay GPL-3.0, and the original `@author`
attribution is deliberately left intact in the headers.
