// ==WindhawkMod==
// @id              desktop-drag-select
// @name            Desktop Drag-Select With Icons Hidden
// @description     Brings the selection rectangle back to the desktop when "Show desktop icons" is off
// @version         2.0
// @author          ashtonlariccia
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -luser32 -lgdi32 -lcomctl32 -ladvapi32 -ldwmapi
// ==/WindhawkMod==

// Source code is published under The GNU General Public License v3.0.

// ==WindhawkModReadme==
/*
# Desktop Drag-Select With Icons Hidden

Turning off **Show desktop icons** also takes away the drag-select rectangle.
This mod gives the rectangle back, in whatever colour you want, without turning
the icons back on.

## Why the rectangle disappears

"Show desktop icons" is not a draw flag. The desktop is three nested windows:

```
Progman
  SHELLDLL_DefView          <- stays visible, full screen
    SysListView32           <- ShowWindow(SW_HIDE) when icons are off
```

Unchecking the option hides `SysListView32`, and the marquee is a *list view*
feature. With the list view gone there is nothing left to draw it, so the click
falls through to `SHELLDLL_DefView`, which has no selection behaviour of its
own. That is also why Ctrl+A, arrow-key navigation, type-to-select and dropping
files on the desktop all stop working at the same time -- one hidden window
takes them all out together.

Measured on this machine with icons off:

```
Progman            hwnd=0x000302F0  visible=True
  SHELLDLL_DefView hwnd=0x0003030E  visible=True   <- a click at the centre lands here
    SysListView32  hwnd=0x00020312  visible=False
```

## What the mod does

It subclasses `SHELLDLL_DefView` and draws the rectangle itself: press, drag,
release. There are no icons to hit, so nothing gets selected -- this is the
rectangle, and only the rectangle.

The rectangle is a separate layered, click-through popup window rather than
paint on the desktop's own DC. That matters if you run an animated wallpaper
(Wallpaper Engine's `WPEDesktopDX11Window` sits under `SHELLDLL_DefView` here):
anything drawn straight onto the desktop gets wiped by the next wallpaper frame,
whereas a layered window is composited by DWM and stays put.

The mod only takes the click when `SysListView32` is hidden. Turn icons back on
and it stands down completely, so the real marquee, icon selection and file
drops behave exactly as stock. Nothing is disabled and nothing needs undoing.

Right-click is untouched, so the desktop context menu still works. Right-click
or `Esc` during a drag cancels it.

## Rapid repeated drags

Dragging over and over quickly makes Windows deliver every second press as
`WM_LBUTTONDBLCLK` instead of `WM_LBUTTONDOWN`. Both start a rectangle here, so
a fast back-and-forth does not drop every other drag.

## Colour

The rectangle is always the system accent colour, fill and border alike, and the
accent is re-read at the start of every drag. Change it in Settings and the very
next drag is already the new colour -- nothing to reload, no change notification
to subscribe to. It is one registry read per drag, not per frame.

The value read is `Explorer\Accent\AccentColorMenu`, which is the accent shown
in Settings. `DwmGetColorizationColor` is only a fallback, because what it
reports is the *colorization* colour and that can be a blend rather than the
accent itself. On this machine the two agree: registry `0xFF6F6FD5` and DWM
`0xC4D56F6F` are both RGB(213, 111, 111).

## Rounded corners

The corner radius defaults to 6px, KDE-style, and the corners are anti-aliased,
so a small radius stays smooth instead of stair-stepping.

They are drawn from the signed distance to a rounded rectangle, which buys three
things at once: anti-aliasing falls out as `clamp(0.5 - d)`, the border is the
same field offset by its own width -- insetting a rounded rect by `w` shifts its
distance field by exactly `w` -- and it needs no GDI+ or extra library.

Only the four corner boxes evaluate that field. Every other pixel sits on an
axis-aligned edge that is already pixel-exact and takes a straight run instead,
which matters when a drag can cover the whole screen every frame.

## Fade-out

Letting go fades the rectangle rather than cutting it, over 180ms by default, on
a quadratic ease-out -- it holds near full opacity for a moment and then drops
away. A linear ramp reads as an abrupt cut right at the end.

The fade costs nothing to draw. The layered window keeps its own copy of the
surface, so each frame is one `UpdateLayeredWindow` that changes only
`SourceConstantAlpha` and passes null for the source and geometry, re-blitting
nothing. The virtual-screen-sized DIB is released the moment the fade starts
rather than being held for its duration.

Pressing again cuts a running fade short, which is what you want mid-drag-run:
the old rectangle goes at once and the new one starts clean.

## Tuning

Setting **fill opacity to 0** gives a hollow outline. **Border width 0** gives a
plain translucent block. **Corner radius 0** gives square corners and skips the
distance field entirely. **Fade-out 0** makes the rectangle vanish instantly.

## Where it attaches

A watcher thread re-scans every 2 seconds for `Progman` and any `WorkerW` that
owns a `SHELLDLL_DefView`, and subclasses the DefView. A poll rather than a
`CreateWindowEx` hook because the desktop windows get torn down and rebuilt by
more than just explorer restarts -- wallpaper apps reparent DefView into a
`WorkerW` -- and a 2-second pair of `FindWindowEx` calls costs nothing.

If DefView is somehow *not* visible, the parent gets subclassed instead, since
that is then what receives the click.

## Unload safety

The rectangle's window class is registered against `explorer.exe`'s own
`hInstance` with `DefWindowProc` as its window procedure, so no part of the
class points into this mod's DLL.

That also leaves the overlay unable to run a handler of its own, which is why
the fade timer lives on the subclassed desktop window instead, under an id far
outside the small integers the shell uses for its own timers.

`KillTimer` and `DestroyWindow` both have to run on the thread that owns the
window -- the shell thread, not the one unloading the mod. So uninit sends a
registered message to the desktop window, handled inside the subclass while it
is still installed, and only removes the subclass afterwards. It goes through
`SendMessageTimeout` with `SMTO_ABORTIFHUNG` so a wedged shell thread cannot
hang the uninstall.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- FillOpacity: 25
  $name: Fill opacity (%)
  $description: 0 leaves the inside empty, so you get a hollow outline.
- BorderOpacity: 90
  $name: Border opacity (%)
- BorderWidth: 1
  $name: Border width (px)
  $description: 0 removes the border, leaving a plain translucent block.
- CornerRadius: 6
  $name: Corner radius (px)
  $description: >-
    Rounded like KDE's rubber band. 0 gives square corners. The corners are
    anti-aliased, so small values stay smooth rather than stair-stepping.
- FadeOutMs: 180
  $name: Fade-out (ms)
  $description: >-
    How long the rectangle takes to fade after you let go. 0 makes it vanish
    instantly. Starting a new drag cuts any fade still running short.
- DebugLogToFile: false
  $name: Write the log to a file
  $description: >-
    Also append this mod's log to dds-debug.log in your temp folder. Windhawk's
    log window and any other debug-output viewer compete for the same messages,
    so a file is the reliable way to capture a full trace.
*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <math.h>

#include <windhawk_utils.h>

#include <atomic>
#include <unordered_set>
#include <vector>

// =============================================================================
// Logging
// =============================================================================

static std::atomic<bool> g_debugLogToFile;

// Windhawk's log window and any other debug-output viewer compete for the same
// DBWIN messages, so a file is the only reliable full trace. Same reasoning as
// the Instant Desktop Switch mod.
static void DebugFileLog(PCWSTR format, ...) {
    if (!g_debugLogToFile) {
        return;
    }

    wchar_t path[MAX_PATH];
    DWORD pathLength = GetTempPath(ARRAYSIZE(path), path);
    if (!pathLength || pathLength >= ARRAYSIZE(path) - 20) {
        return;
    }
    wcscat_s(path, L"dds-debug.log");

    wchar_t text[1024];
    va_list args;
    va_start(args, format);
    int length =
        _vsnwprintf_s(text, ARRAYSIZE(text) - 4, _TRUNCATE, format, args);
    va_end(args);
    if (length < 0) {
        return;
    }

    wcscat_s(text, L"\r\n");

    char utf8[2048];
    int utf8Length = WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8,
                                         ARRAYSIZE(utf8), nullptr, nullptr);
    if (utf8Length <= 1) {
        return;
    }

    HANDLE file =
        CreateFile(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                   nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD written = 0;
    WriteFile(file, utf8, utf8Length - 1, &written, nullptr);
    CloseHandle(file);
}

#define DDSLOG(...)                \
    do {                           \
        Wh_Log(__VA_ARGS__);       \
        DebugFileLog(__VA_ARGS__); \
    } while (0)

// =============================================================================
// Settings
// =============================================================================

static std::atomic<int> g_fillOpacity;
static std::atomic<int> g_borderOpacity;
static std::atomic<int> g_borderWidth;
static std::atomic<int> g_cornerRadius;
static std::atomic<int> g_fadeOutMs;

// 32bpp premultiplied BGRA, ready to drop straight into the DIB. Resolved from
// the accent colour at the start of each drag, on the desktop thread, so they
// need no synchronisation of their own.
static DWORD g_fillPixel;
static DWORD g_borderPixel;

static DWORD Premultiply(COLORREF color, int opacityPercent) {
    if (opacityPercent < 0) {
        opacityPercent = 0;
    } else if (opacityPercent > 100) {
        opacityPercent = 100;
    }

    DWORD alpha = (DWORD)((opacityPercent * 255 + 50) / 100);
    DWORD red = (GetRValue(color) * alpha + 127) / 255;
    DWORD green = (GetGValue(color) * alpha + 127) / 255;
    DWORD blue = (GetBValue(color) * alpha + 127) / 255;

    return (alpha << 24) | (red << 16) | (green << 8) | blue;
}

// The accent colour proper, as shown in Settings. `AccentColorMenu` is
// 0xAABBGGRR and a COLORREF is 0x00BBGGRR, so the low three bytes drop straight
// in. DwmGetColorizationColor is only the fallback: it reports the colorization
// colour, which can be a blend rather than the accent. On this machine both
// agree -- registry 0xFF6F6FD5 and DWM 0xC4D56F6F are both RGB(213,111,111).
static COLORREF SystemAccentColor() {
    DWORD value = 0;
    DWORD size = sizeof(value);
    if (RegGetValue(
            HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Accent",
            L"AccentColorMenu", RRF_RT_REG_DWORD, nullptr, &value,
            &size) == ERROR_SUCCESS) {
        return (COLORREF)(value & 0x00FFFFFF);
    }

    DWORD argb = 0;
    BOOL opaqueBlend = FALSE;
    if (SUCCEEDED(DwmGetColorizationColor(&argb, &opaqueBlend))) {
        return RGB((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF);
    }

    return RGB(0, 120, 212);
}

// Re-read at the start of every drag rather than cached at load, so changing
// the accent colour is picked up on the very next drag with nothing to reload
// and no change notification to subscribe to. It is one registry read per drag,
// not per frame.
static void RefreshAccentColor() {
    COLORREF accent = SystemAccentColor();
    g_fillPixel = Premultiply(accent, g_fillOpacity);
    g_borderPixel = Premultiply(accent, g_borderOpacity);
}

static int ClampSetting(PCWSTR name, int low, int high) {
    int value = Wh_GetIntSetting(name);
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

static void LoadSettings() {
    g_debugLogToFile = Wh_GetIntSetting(L"DebugLogToFile") != 0;

    g_fillOpacity = ClampSetting(L"FillOpacity", 0, 100);
    g_borderOpacity = ClampSetting(L"BorderOpacity", 0, 100);
    g_borderWidth = ClampSetting(L"BorderWidth", 0, 32);
    g_cornerRadius = ClampSetting(L"CornerRadius", 0, 64);
    g_fadeOutMs = ClampSetting(L"FadeOutMs", 0, 2000);
}

// =============================================================================
// The rectangle
// =============================================================================
//
// A layered, click-through popup rather than paint on the desktop's own DC: an
// animated wallpaper repaints over anything drawn directly on the desktop,
// while a layered window is composited by DWM instead.
//
// The class is registered against explorer.exe's hInstance with DefWindowProcW
// as the window procedure, so nothing about it points into this DLL and it
// survives an unload harmlessly. Re-registering after a reload fails with
// ERROR_CLASS_ALREADY_EXISTS, which is fine.

static PCWSTR const kOverlayClass = L"WhDesktopDragSelectOverlay";

// Registered at init; see the top of the subclass procedure.
static UINT g_cleanupMessage;

// Everything below is touched only on the desktop's UI thread, from inside the
// subclass procedure.
static HWND g_overlay;
static HDC g_overlayDC;
static HBITMAP g_overlayBitmap;
static HGDIOBJ g_overlayOldBitmap;
static void* g_overlayBits;
static int g_overlayBitmapWidth;
static int g_overlayBitmapHeight;

static bool g_pressed;   // button is down, drag threshold not crossed yet
static bool g_dragging;  // rectangle is on screen
static POINT g_anchor;   // screen coordinates

static void RegisterOverlayClass() {
    WNDCLASSEX wc = {sizeof(wc)};
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = kOverlayClass;

    if (!RegisterClassEx(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        DDSLOG(L"RegisterClassEx failed: %u", GetLastError());
    }
}

static void VirtualScreenSize(int* width, int* height) {
    *width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    *height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (*width <= 0) {
        *width = GetSystemMetrics(SM_CXSCREEN);
    }
    if (*height <= 0) {
        *height = GetSystemMetrics(SM_CYSCREEN);
    }
}

static void ReleaseOverlayBitmap() {
    if (g_overlayDC) {
        if (g_overlayOldBitmap) {
            SelectObject(g_overlayDC, g_overlayOldBitmap);
            g_overlayOldBitmap = nullptr;
        }
        DeleteDC(g_overlayDC);
        g_overlayDC = nullptr;
    }
    if (g_overlayBitmap) {
        DeleteObject(g_overlayBitmap);
        g_overlayBitmap = nullptr;
    }
    g_overlayBits = nullptr;
    g_overlayBitmapWidth = 0;
    g_overlayBitmapHeight = 0;
}

// One DIB the size of the whole virtual screen, allocated once per drag. Every
// frame then fills only its top-left width x height corner and hands
// UpdateLayeredWindow that sub-rectangle, so there is no per-frame allocation.
static bool EnsureOverlayBitmap() {
    int width, height;
    VirtualScreenSize(&width, &height);

    if (g_overlayBits && g_overlayBitmapWidth == width &&
        g_overlayBitmapHeight == height) {
        return true;
    }

    ReleaseOverlayBitmap();

    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;  // top-down
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    g_overlayBitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS,
                                       &g_overlayBits, nullptr, 0);
    if (!g_overlayBitmap) {
        DDSLOG(L"CreateDIBSection failed: %u", GetLastError());
        g_overlayBits = nullptr;
        return false;
    }

    g_overlayDC = CreateCompatibleDC(nullptr);
    if (!g_overlayDC) {
        DDSLOG(L"CreateCompatibleDC failed: %u", GetLastError());
        ReleaseOverlayBitmap();
        return false;
    }

    g_overlayOldBitmap = SelectObject(g_overlayDC, g_overlayBitmap);
    g_overlayBitmapWidth = width;
    g_overlayBitmapHeight = height;
    return true;
}

static bool EnsureOverlayWindow() {
    if (g_overlay) {
        return true;
    }

    g_overlay = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE |
            WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        kOverlayClass, L"", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr,
        GetModuleHandle(nullptr), nullptr);

    if (!g_overlay) {
        DDSLOG(L"CreateWindowEx failed: %u", GetLastError());
        return false;
    }
    return true;
}

// Both inputs are already premultiplied and the two coverages are disjoint, so
// a straight weighted sum is the correct composite -- including the alpha
// channel, which is what keeps the anti-aliased edge translucent instead of
// haloed.
static DWORD BlendCoverage(DWORD fill,
                           float fillCoverage,
                           DWORD edge,
                           float edgeCoverage) {
    DWORD result = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        float value = (float)((fill >> shift) & 0xFF) * fillCoverage +
                      (float)((edge >> shift) & 0xFF) * edgeCoverage;
        int rounded = (int)(value + 0.5f);
        if (rounded < 0) {
            rounded = 0;
        } else if (rounded > 255) {
            rounded = 255;
        }
        result |= (DWORD)rounded << shift;
    }
    return result;
}

// Rounded corners come from the signed distance to a rounded rectangle, which
// buys three things at once: anti-aliasing falls out as clamp(0.5 - d), the
// border is the same field offset by its own width (insetting a rounded rect by
// w shifts its distance field by exactly w), and no extra library is needed.
//
// Only the four corner boxes evaluate it. Every other pixel is on an
// axis-aligned edge that is already pixel-exact, and takes the straight run
// below -- which matters because this fills up to a full screen per frame.
static void PaintMarquee(int width, int height) {
    const int half = (width < height ? width : height) / 2;

    int border = g_borderWidth;
    if (border > half) {
        border = half;
    }
    int radius = g_cornerRadius;
    if (radius > half) {
        radius = half;
    }

    const DWORD fill = g_fillPixel;
    const DWORD edge = g_borderPixel;
    const size_t stride = (size_t)g_overlayBitmapWidth * 4;
    BYTE* base = (BYTE*)g_overlayBits;

    const float halfWidth = width * 0.5f;
    const float halfHeight = height * 0.5f;
    const float straightX = halfWidth - radius;
    const float straightY = halfHeight - radius;
    const float borderF = (float)border;
    const float radiusF = (float)radius;

    for (int y = 0; y < height; y++) {
        DWORD* row = (DWORD*)(base + (size_t)y * stride);
        const bool inCornerBand = radius > 0 && (y < radius || y >= height - radius);

        if (!inCornerBand) {
            int x = 0;
            for (; x < border; x++) {
                row[x] = edge;
            }
            for (; x < width - border; x++) {
                row[x] = fill;
            }
            for (; x < width; x++) {
                row[x] = edge;
            }
            continue;
        }

        const float offsetY = fabsf((float)y + 0.5f - halfHeight) - straightY;
        const DWORD betweenCorners = (y < border || y >= height - border) ? edge : fill;

        for (int x = 0; x < width; x++) {
            // The top and bottom edges are straight between the two corners.
            if (x >= radius && x < width - radius) {
                row[x] = betweenCorners;
                continue;
            }

            const float offsetX = fabsf((float)x + 0.5f - halfWidth) - straightX;
            const float clampedX = offsetX > 0 ? offsetX : 0;
            const float clampedY = offsetY > 0 ? offsetY : 0;
            const float inside = offsetX > offsetY ? offsetX : offsetY;
            const float distance =
                sqrtf(clampedX * clampedX + clampedY * clampedY) +
                (inside < 0 ? inside : 0) - radiusF;

            float outer = 0.5f - distance;
            if (outer < 0) {
                outer = 0;
            } else if (outer > 1) {
                outer = 1;
            }

            float inner = 0.5f - (distance + borderF);
            if (inner < 0) {
                inner = 0;
            } else if (inner > 1) {
                inner = 1;
            }

            row[x] = BlendCoverage(fill, inner, edge, outer - inner);
        }
    }
}

static void ShowMarquee(POINT cursor) {
    RECT rect;
    rect.left = g_anchor.x < cursor.x ? g_anchor.x : cursor.x;
    rect.top = g_anchor.y < cursor.y ? g_anchor.y : cursor.y;
    rect.right = g_anchor.x > cursor.x ? g_anchor.x : cursor.x;
    rect.bottom = g_anchor.y > cursor.y ? g_anchor.y : cursor.y;

    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    if (width < 1) {
        width = 1;
    }
    if (height < 1) {
        height = 1;
    }

    if (!EnsureOverlayWindow() || !EnsureOverlayBitmap()) {
        return;
    }
    if (width > g_overlayBitmapWidth) {
        width = g_overlayBitmapWidth;
    }
    if (height > g_overlayBitmapHeight) {
        height = g_overlayBitmapHeight;
    }

    PaintMarquee(width, height);

    POINT source = {0, 0};
    POINT destination = {rect.left, rect.top};
    SIZE size = {width, height};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};

    UpdateLayeredWindow(g_overlay, nullptr, &destination, &size, g_overlayDC,
                        &source, 0, &blend, ULW_ALPHA);

    if (!IsWindowVisible(g_overlay)) {
        ShowWindow(g_overlay, SW_SHOWNOACTIVATE);
    }
}

// =============================================================================
// Fade-out
// =============================================================================
//
// The timer lives on the subclassed desktop window, not on the overlay: the
// overlay deliberately uses DefWindowProc so that nothing in its class points
// into this DLL, which also leaves it unable to run a handler of its own. The
// id is far outside the small integers the shell uses for its own timers.

static const UINT_PTR kFadeTimerId = 0x57444453;  // 'WDDS'

static HWND g_fadeHost;
static ULONGLONG g_fadeStart;
static bool g_fading;

static void SetOverlayAlpha(int alpha) {
    if (!g_overlay) {
        return;
    }
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, (BYTE)alpha, AC_SRC_ALPHA};
    // A null source and null geometry keep the surface, position and size the
    // window already has, so a fade frame costs one call and re-blits nothing.
    UpdateLayeredWindow(g_overlay, nullptr, nullptr, nullptr, nullptr, nullptr,
                        0, &blend, ULW_ALPHA);
}

static void StopFade() {
    if (!g_fading) {
        return;
    }
    g_fading = false;
    if (g_fadeHost) {
        KillTimer(g_fadeHost, kFadeTimerId);
        g_fadeHost = nullptr;
    }
}

static void HideMarquee() {
    StopFade();
    if (g_overlay) {
        ShowWindow(g_overlay, SW_HIDE);
    }
    // The DIB is the size of the virtual screen; do not hold it between drags.
    ReleaseOverlayBitmap();
}

static void BeginFadeOut(HWND host) {
    int duration = g_fadeOutMs;
    if (duration <= 0 || !host || !g_overlay || !IsWindowVisible(g_overlay)) {
        HideMarquee();
        return;
    }

    StopFade();

    // The layered window holds its own copy of the surface, so the DIB -- which
    // is the size of the whole virtual screen -- goes back now rather than
    // being held for the length of the fade.
    ReleaseOverlayBitmap();

    g_fadeHost = host;
    g_fadeStart = GetTickCount64();
    g_fading = true;

    if (!SetTimer(host, kFadeTimerId, 8, nullptr)) {
        DDSLOG(L"SetTimer failed: %u", GetLastError());
        g_fading = false;
        g_fadeHost = nullptr;
        HideMarquee();
    }
}

static void OnFadeTick() {
    if (!g_fading) {
        return;
    }

    int duration = g_fadeOutMs;
    if (duration < 1) {
        duration = 1;
    }

    ULONGLONG elapsed = GetTickCount64() - g_fadeStart;
    if (elapsed >= (ULONGLONG)duration) {
        HideMarquee();
        return;
    }

    // Quadratic ease-out: holds near full opacity for a moment, then drops
    // away. A linear ramp reads as an abrupt cut at the end.
    float progress = (float)elapsed / (float)duration;
    float opacity = 1.0f - progress * progress;

    int alpha = (int)(opacity * 255.0f + 0.5f);
    if (alpha < 0) {
        alpha = 0;
    } else if (alpha > 255) {
        alpha = 255;
    }
    SetOverlayAlpha(alpha);
}

// =============================================================================
// Drag handling
// =============================================================================

static bool DesktopIconsHidden(HWND hWnd) {
    HWND defView = hWnd;

    WCHAR className[64];
    if (!GetClassName(hWnd, className, ARRAYSIZE(className)) ||
        wcscmp(className, L"SHELLDLL_DefView") != 0) {
        defView = FindWindowEx(hWnd, nullptr, L"SHELLDLL_DefView", nullptr);
    }
    if (!defView) {
        return true;
    }

    HWND listView = FindWindowEx(defView, nullptr, L"SysListView32", nullptr);
    return !listView || !IsWindowVisible(listView);
}

// The flags are cleared before anything else, because ReleaseCapture sends a
// WM_CAPTURECHANGED that re-enters here -- and that pass must not tear down the
// fade this one just started.
static void EndDrag(bool releaseCapture, bool fadeOut, HWND host) {
    bool wasActive = g_pressed || g_dragging;
    bool wasDragging = g_dragging;

    g_pressed = false;
    g_dragging = false;

    if (wasDragging) {
        if (fadeOut) {
            BeginFadeOut(host);
        } else {
            HideMarquee();
        }
    }

    if (releaseCapture && wasActive) {
        ReleaseCapture();
    }
}

static void OnMouseMove(HWND hWnd, POINT cursor) {
    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
        EndDrag(true, true, hWnd);
        return;
    }

    if (g_pressed) {
        int dx = cursor.x - g_anchor.x;
        int dy = cursor.y - g_anchor.y;
        if (dx < 0) {
            dx = -dx;
        }
        if (dy < 0) {
            dy = -dy;
        }
        if (dx < GetSystemMetrics(SM_CXDRAG) &&
            dy < GetSystemMetrics(SM_CYDRAG)) {
            return;
        }
        g_pressed = false;
        g_dragging = true;
    }

    if (g_dragging) {
        ShowMarquee(cursor);
    }
}

static LRESULT CALLBACK DesktopSubclassProc(HWND hWnd,
                                            UINT uMsg,
                                            WPARAM wParam,
                                            LPARAM lParam,
                                            DWORD_PTR dwRefData) {
    // Sent from Wh_ModBeforeUninit. KillTimer and DestroyWindow both have to
    // run on the thread that owns the window, which is this one, not the one
    // unloading the mod.
    if (g_cleanupMessage && uMsg == g_cleanupMessage) {
        EndDrag(false, false, hWnd);
        HideMarquee();
        if (g_overlay) {
            DestroyWindow(g_overlay);
            g_overlay = nullptr;
        }
        return 0;
    }

    switch (uMsg) {
        // A fast run of drags turns every second press into WM_LBUTTONDBLCLK,
        // so both have to start a rectangle or half the drags go missing.
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK: {
            if (!DesktopIconsHidden(hWnd)) {
                break;
            }

            POINT cursor = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ClientToScreen(hWnd, &cursor);

            // Takes down a rectangle still fading from the last drag. Without
            // this, a press that never crosses the drag threshold would cancel
            // the fade and leave that rectangle stranded on screen.
            EndDrag(false, false, hWnd);
            HideMarquee();

            RefreshAccentColor();
            g_anchor = cursor;
            g_pressed = true;

            // Only when the capture actually moves. Re-taking capture we
            // already hold can bounce a WM_CAPTURECHANGED straight back at us,
            // and that handler would clear the state set on the two lines
            // above before the first WM_MOUSEMOVE ever arrived.
            if (GetCapture() != hWnd) {
                SetCapture(hWnd);
            }
            return 0;
        }

        case WM_MOUSEMOVE:
            if (g_pressed || g_dragging) {
                POINT cursor = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                ClientToScreen(hWnd, &cursor);
                OnMouseMove(hWnd, cursor);
                return 0;
            }
            break;

        case WM_LBUTTONUP:
            if (g_pressed || g_dragging) {
                EndDrag(true, true, hWnd);
                return 0;
            }
            break;

        case WM_RBUTTONDOWN:
            // Cancel the drag and swallow the click, so the context menu does
            // not open on the press that ended the rectangle. A right-click
            // with no drag in progress falls through untouched.
            if (g_pressed || g_dragging) {
                EndDrag(true, true, hWnd);
                return 0;
            }
            break;

        case WM_CAPTURECHANGED:
            // Capture is already gone; releasing it again would take it from
            // whoever just acquired it. No fade either: this is the re-entrant
            // pass from our own ReleaseCapture as often as it is a real loss.
            EndDrag(false, false, hWnd);
            break;

        case WM_TIMER:
            if (wParam == kFadeTimerId) {
                OnFadeTick();
                return 0;
            }
            break;

        case WM_NCDESTROY:
            EndDrag(false, false, hWnd);
            HideMarquee();
            if (g_overlay) {
                DestroyWindow(g_overlay);
                g_overlay = nullptr;
            }
            break;
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

// =============================================================================
// Attaching to the desktop
// =============================================================================

static CRITICAL_SECTION g_attachedMutex;
static std::unordered_set<HWND> g_attached;

static void Attach(HWND hWnd) {
    EnterCriticalSection(&g_attachedMutex);
    bool isNew = g_attached.insert(hWnd).second;
    LeaveCriticalSection(&g_attachedMutex);

    if (!isNew) {
        return;
    }

    if (WindhawkUtils::SetWindowSubclassFromAnyThread(hWnd, DesktopSubclassProc,
                                                      0)) {
        DDSLOG(L"Attached to 0x%08X", (DWORD)(DWORD_PTR)hWnd);
        return;
    }

    DDSLOG(L"Failed to attach to 0x%08X", (DWORD)(DWORD_PTR)hWnd);
    EnterCriticalSection(&g_attachedMutex);
    g_attached.erase(hWnd);
    LeaveCriticalSection(&g_attachedMutex);
}

static BOOL CALLBACK EnumDesktopCandidates(HWND hWnd, LPARAM lParam) {
    WCHAR className[64];
    if (!GetClassName(hWnd, className, ARRAYSIZE(className))) {
        return TRUE;
    }
    if (wcscmp(className, L"Progman") != 0 &&
        wcscmp(className, L"WorkerW") != 0) {
        return TRUE;
    }

    HWND defView = FindWindowEx(hWnd, nullptr, L"SHELLDLL_DefView", nullptr);
    if (!defView) {
        return TRUE;
    }

    // DefView covers the desktop and sits above the wallpaper, so it is what
    // receives the click. Only if it is not visible does the parent get it.
    Attach(IsWindowVisible(defView) ? defView : hWnd);
    return TRUE;
}

static void PruneDeadWindows() {
    EnterCriticalSection(&g_attachedMutex);
    for (auto it = g_attached.begin(); it != g_attached.end();) {
        if (IsWindow(*it)) {
            ++it;
        } else {
            it = g_attached.erase(it);
        }
    }
    LeaveCriticalSection(&g_attachedMutex);
}

static std::atomic<bool> g_stopping;
static HANDLE g_wakeEvent;
static HANDLE g_watcherThread;

static DWORD WINAPI WatcherThreadProc(LPVOID) {
    while (!g_stopping) {
        PruneDeadWindows();
        EnumWindows(EnumDesktopCandidates, 0);
        WaitForSingleObject(g_wakeEvent, 2000);
    }
    return 0;
}

// =============================================================================
// Mod lifecycle
// =============================================================================

BOOL Wh_ModInit() {
    DDSLOG(L"Init");

    LoadSettings();
    RegisterOverlayClass();

    g_cleanupMessage = RegisterWindowMessage(L"WhDesktopDragSelect_Cleanup");
    if (!g_cleanupMessage) {
        DDSLOG(L"RegisterWindowMessage failed: %u", GetLastError());
    }

    InitializeCriticalSection(&g_attachedMutex);

    g_wakeEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!g_wakeEvent) {
        DDSLOG(L"CreateEvent failed: %u", GetLastError());
        DeleteCriticalSection(&g_attachedMutex);
        return FALSE;
    }

    g_stopping = false;
    g_watcherThread =
        CreateThread(nullptr, 0, WatcherThreadProc, nullptr, 0, nullptr);
    if (!g_watcherThread) {
        DDSLOG(L"CreateThread failed: %u", GetLastError());
        CloseHandle(g_wakeEvent);
        g_wakeEvent = nullptr;
        DeleteCriticalSection(&g_attachedMutex);
        return FALSE;
    }

    return TRUE;
}

void Wh_ModSettingsChanged() {
    DDSLOG(L"Settings changed");
    LoadSettings();
}

void Wh_ModBeforeUninit() {
    DDSLOG(L"Before uninit");

    g_stopping = true;
    if (g_wakeEvent) {
        SetEvent(g_wakeEvent);
    }
    if (g_watcherThread) {
        WaitForSingleObject(g_watcherThread, 5000);
        CloseHandle(g_watcherThread);
        g_watcherThread = nullptr;
    }

    EnterCriticalSection(&g_attachedMutex);
    std::vector<HWND> attached(g_attached.begin(), g_attached.end());
    g_attached.clear();
    LeaveCriticalSection(&g_attachedMutex);

    for (HWND hWnd : attached) {
        if (!IsWindow(hWnd)) {
            continue;
        }

        // Tear the timer and the overlay down on the desktop thread first,
        // while the subclass that handles this message is still installed.
        // Timed rather than a bare SendMessage so a wedged shell thread cannot
        // hang the uninstall.
        if (g_cleanupMessage) {
            DWORD_PTR result = 0;
            SendMessageTimeout(hWnd, g_cleanupMessage, 0, 0,
                               SMTO_ABORTIFHUNG | SMTO_NORMAL, 2000, &result);
        }

        WindhawkUtils::RemoveWindowSubclassFromAnyThread(hWnd,
                                                         DesktopSubclassProc);
    }
}

void Wh_ModUninit() {
    DDSLOG(L"Uninit");

    ReleaseOverlayBitmap();

    if (g_wakeEvent) {
        CloseHandle(g_wakeEvent);
        g_wakeEvent = nullptr;
    }

    DeleteCriticalSection(&g_attachedMutex);
}
