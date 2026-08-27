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
patches/    generated diffs vs baseline               <forkid>.patch
scripts/    tooling (see below)
forks.json  maps each fork -> its upstream id + baseline version
build/      scratch: editor-buffer backups (gitignored)
```

## Current forks

| Fork | Upstream | Baseline |
|---|---|---|
| `windows-11-start-menu-styler-fork` | `windows-11-start-menu-styler` | 1.7 |

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
| `RowShiftY` | `0` | **vertical position of avatar + power.** If they sit too high or too low, change only this |
| `RowHeight` | `48` | height of the row hosting them |
| `IconSize` | `32` | avatar and power button size |
| `UserTileMargin` | `16,0,0,0` | avatar distance from the left edge |
| `PowerMargin` | `0,0,16,0` | power button distance from the right edge |
| `SearchInset` | `72,-2,72,2` | search box margins, i.e. the gap left for both controls |

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
