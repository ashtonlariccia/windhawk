// ==WindhawkMod==
// @id              widgets
// @name            Widgets
// @description     Calendar, now-playing and output-box widgets on the desktop, and a top bar
// @version         1.2
// @author          ashtonlariccia
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -luser32 -lgdi32 -ladvapi32 -lole32 -loleaut32 -lruntimeobject -ld2d1 -ldwrite -lwindowscodecs -lshcore -lws2_32 -ldwmapi -lshell32 -lcoremessaging
// ==/WindhawkMod==

// Source code is published under The GNU General Public License v3.0.

// ==WindhawkModReadme==
/*
# Widgets

Small cards that sit on the desktop, above the wallpaper and below every
window. Each widget has its own switch in the settings.

## Calendar

The current month with today marked in your accent colour. The chevrons or the
mouse wheel move between months; clicking the month's name jumps back to today.
The first day of the week follows your regional settings unless overridden.

## Now playing

Whatever Windows' media controls see -- Spotify, a browser tab, the Media
Player: cover art, title, artist, previous / play-pause / next and a progress
bar you can click to seek. By default it fades out when nothing has a media
session and back in when something does.

## Output boxes

A resizable box per configured port that is nothing but a program's output:
it connects to `localhost:<port>` (IPv4, then IPv6), shows what the service
writes there and sends back what you type. Until something answers it shows
only a faint "Nothing listening on port N", or hides if set to.

* Output is plain text, ANSI colours included. A form feed or `ESC[2J` clears
  the box, and the usual cursor-movement codes redraw in place.
* The first output on each new connection replaces the box's contents, so a
  service can answer every connection with a fresh snapshot and close it; the
  box reconnects every *Refresh* seconds. A service that keeps the connection
  open just keeps streaming.
* Click the box to type, as into a terminal: the text appears after the
  output, Enter sends the line and a newline (over a new connection if the
  service had closed it), Up / Down recall earlier lines, Ctrl+V pastes, Esc
  or clicking elsewhere stops typing. Press and drag to move the box instead.
* A program that hides the cursor (`ESC[?25l`) isn't offered input: clicking
  the box doesn't start typing until it shows it again (`ESC[?25h`) or the
  next connection starts.

* Each connection opens with a line from the box, ESC `size COLS ROWS`, sent
  again when a resize changes either. Services use it to fit their output, and
  should drop a connection that doesn't start with it: that's how they tell
  the box from a web page posting to localhost.

`scripts/serve-output.py` in the repo turns any command into such a service;
`scripts/serve-hub.py` puts several in one box, switched by typing `/name`.

## Top bar

A glass bar along the top of the main screen: the cards' colour and border
over a blur, with round ends, placed like the Shell Styler's taskbar pills. Maximized windows stop below it, as they do
above the taskbar, and it hides while an app is fullscreen. It is empty for
now; the desktop pager and the control center will move onto it.

## Fading behind windows

By default the widgets fade out as soon as you switch to a window and come
back when you click the desktop, press Win+D, or minimize or close the last
window on the screen. Opening the taskbar, Start, search or Alt+Tab leaves
them as they are, and so does using a widget (typing into an output box).
The fade starts with the window's minimize or restore and lasts as long as
the Tweaks mod's genie animation, on the same curve.

## Moving widgets

Drag a widget anywhere that isn't a button. It snaps to a grid and to the edges
of the screen's work area, and remembers where it was left. Output boxes also
resize from their edges and corners. Right-click a
widget to **lock** positions (then only Alt+drag moves them) or to put every
widget back in its default spot.

## Notes

The widgets are owned by the desktop window, so they stay on every virtual
desktop, don't show in Alt+Tab or the taskbar, survive Win+D, and never take
focus when clicked. Typing into an output box goes to an invisible window that
takes the keyboard until you're done.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- CalendarEnabled: true
  $name: Calendar
- WeekStart: locale
  $name: Week starts on
  $options:
  - locale: Regional setting
  - monday: Monday
  - sunday: Sunday
- NowPlayingEnabled: true
  $name: Now playing
- NowPlayingHideWhenIdle: true
  $name: Hide now playing when nothing is playing
  $description: >-
    Fade the widget out while no app has a media session. A paused session
    still counts as something playing.
- NowPlayingProgress: true
  $name: Show the progress bar
- OutputBoxesEnabled: true
  $name: Output boxes
  $description: >-
    Off hides every box below without losing the list.
- OutputBoxes:
  - - Port: 7300
      $name: Port
    - Refresh: 5
      $name: Refresh (seconds)
      $description: >-
        How long after the service closes the connection to reconnect for a
        fresh snapshot. 0 reconnects only when you send something.
    - HideOffline: false
      $name: Hide while nothing is listening
      $description: >-
        Off, the box stays up with a placeholder until the service starts.
  $name: Output box ports
  $description: >-
    One box per entry, showing the text a service on localhost:<port> sends.
- OutputFontSize: 12
  $name: Output box font size
- TopBarEnabled: true
  $name: Top bar
  $description: >-
    A glass bar along the top of the main screen, coloured like the cards.
    Maximized windows stop below it.
- TopBarHeight: 32
  $name: Top bar height
  $description: >-
    In DIPs. The bar sits 2 below the screen's top edge and keeps 2 clear
    under it, like the taskbar's pills.
- TopBarOpacity: 0
  $name: Top bar opacity (%)
  $description: >-
    How strongly the card colour covers the bar's blur. 0 follows the cards'
    background opacity below; 1-100 sets the bar's own.
- TopBarBlur: true
  $name: Blur behind the top bar
  $description: >-
    Off, the bar is just the card colour over whatever is behind it, exactly
    like the cards.
- Theme: auto
  $name: Theme
  $options:
  - auto: Follow Windows
  - dark: Dark
  - light: Light
- BackgroundOpacity: 62
  $name: Background opacity (%)
- CornerRadius: 14
  $name: Corner radius
- Scale: 100
  $name: Scale (%)
- Shadow: true
  $name: Drop shadow
- SnapGrid: 8
  $name: Snap grid (px)
  $description: Grid that dragged widgets snap to. 0 turns snapping off.
- Animations: true
  $name: Animations
  $description: Quick fades for showing, hovering, month changes and cover art.
- HideBehindWindows: true
  $name: Fade out while a window is focused
  $description: >-
    The widgets show while the desktop is focused or no window is open on the
    screen (all minimized, or Win+D), and fade out when you switch to a
    window. The taskbar, Start and Alt+Tab leave them as they are.
- FontFamily: Segoe UI Variable Display
  $name: Font
*/
// ==/WindhawkModSettings==

// windhawk_api.h has already included windows.h by now, so this warns that it
// should come first; mingw's headers cope with the order.
#include <winsock2.h>
#include <ws2tcpip.h>

#include <windows.h>
#include <windowsx.h>

#include <d2d1.h>
#include <dwmapi.h>
#include <dwrite.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#include <wincodec.h>

#include <DispatcherQueue.h>

#undef GetCurrentTime

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Composition.Desktop.h>
#include <winrt/Windows.UI.Composition.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using winrt::com_ptr;

// =============================================================================
// Settings
// =============================================================================

struct OutputBoxConfig {
    int port = 0;
    int refresh = 5;  // seconds; 0 = only on input
    bool hideOffline = false;
};

struct Settings {
    bool calendar = true;
    int weekStart = -1;  // -1 = locale, otherwise 0 = Monday ... 6 = Sunday
    bool nowPlaying = true;
    bool hideIdle = true;
    bool progress = true;
    bool outputBoxesEnabled = true;
    std::vector<OutputBoxConfig> outputBoxes;
    float outputFontSize = 12;
    int theme = 0;  // 0 = auto, 1 = dark, 2 = light
    float opacity = 0.62f;
    float cornerRadius = 14;
    float scale = 1;
    float snapGrid = 8;
    bool shadow = true;
    bool animations = true;
    bool hideBehindWindows = true;
    std::wstring font;
    bool topBar = true;
    float topBarHeight = 32;
    float topBarOpacity = 0;  // 0 = the cards' background opacity
    bool topBarBlur = true;
};

Settings g_settings;
bool g_locked = false;

std::wstring GetStringSetting(PCWSTR name) {
    PCWSTR value = Wh_GetStringSetting(name);
    std::wstring result = value ? value : L"";
    Wh_FreeStringSetting(value);
    return result;
}

void LoadSettings() {
    Settings s;
    s.calendar = Wh_GetIntSetting(L"CalendarEnabled");
    std::wstring weekStart = GetStringSetting(L"WeekStart");
    s.weekStart = weekStart == L"monday" ? 0 : weekStart == L"sunday" ? 6 : -1;
    s.nowPlaying = Wh_GetIntSetting(L"NowPlayingEnabled");
    s.hideIdle = Wh_GetIntSetting(L"NowPlayingHideWhenIdle");
    s.progress = Wh_GetIntSetting(L"NowPlayingProgress");
    s.outputBoxesEnabled = Wh_GetIntSetting(L"OutputBoxesEnabled");
    for (int i = 0; i < 32; i++) {
        OutputBoxConfig box;
        box.port = Wh_GetIntSetting(L"OutputBoxes[%d].Port", i);
        if (box.port == 0) {
            break;  // past the end of the list
        }
        if (box.port < 1 || box.port > 65535 ||
            std::any_of(s.outputBoxes.begin(), s.outputBoxes.end(),
                        [&](auto& b) { return b.port == box.port; })) {
            continue;
        }
        box.refresh = std::clamp(Wh_GetIntSetting(L"OutputBoxes[%d].Refresh", i), 0, 3600);
        box.hideOffline = Wh_GetIntSetting(L"OutputBoxes[%d].HideOffline", i);
        s.outputBoxes.push_back(std::move(box));
    }
    // 0 when the setting isn't stored yet, as after a recompile adds it.
    int fontSize = Wh_GetIntSetting(L"OutputFontSize");
    s.outputFontSize = (float)(fontSize ? std::clamp(fontSize, 8, 32) : 12);
    std::wstring theme = GetStringSetting(L"Theme");
    s.theme = theme == L"dark" ? 1 : theme == L"light" ? 2 : 0;
    s.opacity = std::clamp(Wh_GetIntSetting(L"BackgroundOpacity"), 0, 100) / 100.0f;
    s.cornerRadius = (float)std::clamp(Wh_GetIntSetting(L"CornerRadius"), 0, 40);
    s.scale = std::clamp(Wh_GetIntSetting(L"Scale"), 50, 300) / 100.0f;
    s.snapGrid = (float)std::clamp(Wh_GetIntSetting(L"SnapGrid"), 0, 200);
    s.shadow = Wh_GetIntSetting(L"Shadow");
    s.animations = Wh_GetIntSetting(L"Animations");
    s.hideBehindWindows = Wh_GetIntSetting(L"HideBehindWindows");
    s.font = GetStringSetting(L"FontFamily");
    s.topBar = Wh_GetIntSetting(L"TopBarEnabled");
    int topBarHeight = Wh_GetIntSetting(L"TopBarHeight");
    s.topBarHeight = (float)(topBarHeight ? std::clamp(topBarHeight, 16, 80) : 32);
    s.topBarOpacity = std::clamp(Wh_GetIntSetting(L"TopBarOpacity"), 0, 100) / 100.0f;
    s.topBarBlur = Wh_GetIntSetting(L"TopBarBlur");
    g_settings = std::move(s);

    g_locked = Wh_GetIntValue(L"Locked", 0) != 0;
}

// =============================================================================
// Shared state
// =============================================================================

constexpr UINT WM_APP_RELOAD = WM_APP + 1;
constexpr UINT WM_APP_QUIT = WM_APP + 2;
constexpr UINT WM_APP_MEDIA = WM_APP + 3;
constexpr UINT WM_APP_SOCKET = WM_APP + 4;
constexpr UINT WM_APP_APPBAR = WM_APP + 5;

constexpr UINT_PTR kTimerTick = 2;
constexpr UINT_PTR kTimerConnect = 3;
constexpr UINT_PTR kTimerRepaint = 4;
constexpr UINT_PTR kTimerCaret = 5;
constexpr UINT_PTR kTimerProgman = 10;
constexpr UINT_PTR kTimerPalette = 11;
constexpr UINT_PTR kTimerDisplay = 12;
constexpr UINT_PTR kTimerFocus = 13;
constexpr UINT_PTR kTimerTopBar = 14;

constexpr PCWSTR kHostClass = L"WhWidgetsHost";
constexpr PCWSTR kWidgetClass = L"WhWidget";
constexpr PCWSTR kInputClass = L"WhWidgetInput";
constexpr PCWSTR kTopBarClass = L"WhWidgetsTopBar";

// Every fade, hover and slide takes this long -- quick on purpose.
constexpr float kAnimMs = 100.0f;
constexpr float kShadowMargin = 8.0f;   // DIPs around the card for the shadow
constexpr float kScreenMargin = 24.0f;  // default distance from the work area edge
constexpr float kStackGap = 12.0f;      // default gap between stacked widgets

HINSTANCE g_hinst;
std::thread g_uiThread;
HWND g_host;
HWND g_progman;
// False while a window has the user's attention; every widget fades out then.
bool g_desktopShown = true;

// =============================================================================
// Graphics
// =============================================================================

struct Palette {
    D2D1_COLOR_F bg, border, text, text2, text3, hover, track, accent, onAccent;
};

Palette g_pal;

com_ptr<ID2D1Factory> g_d2d;
com_ptr<IDWriteFactory> g_dw;
com_ptr<ID2D1DCRenderTarget> g_rt;
com_ptr<ID2D1SolidColorBrush> g_brush;
// Bumped whenever the render target is recreated, so widgets know their
// target-bound bitmaps are stale.
int g_rtGeneration = 0;

struct Fonts {
    com_ptr<IDWriteTextFormat> title, body, bodyEllipsis, titleEllipsis, small,
        smallRight, weekday, day, dayBold, icon, iconBig, mono, hint;
    std::wstring monoFamily;
} g_fonts;

D2D1_COLOR_F Rgba(float r, float g, float b, float a) {
    return D2D1::ColorF(r, g, b, a);
}

D2D1_COLOR_F WithAlpha(D2D1_COLOR_F c, float a) {
    c.a *= a;
    return c;
}

D2D1_COLOR_F Lerp(D2D1_COLOR_F a, D2D1_COLOR_F b, float t) {
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t,
            a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t};
}

float EaseOut(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return 1 - (1 - t) * (1 - t) * (1 - t);
}

// The genie's curve (smoothstep), for fades that run alongside it.
float SmoothStep(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3 - 2 * t);
}

DWORD ReadDword(PCWSTR path, PCWSTR name, DWORD fallback) {
    DWORD value, size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, path, name, RRF_RT_REG_DWORD, nullptr,
                     &value, &size) == ERROR_SUCCESS) {
        return value;
    }
    return fallback;
}

void LoadPalette() {
    bool light;
    if (g_settings.theme == 0) {
        // The desktop is a shell surface, so it follows the Windows mode, not
        // the app mode.
        light = ReadDword(
                    L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                    L"SystemUsesLightTheme", 0) != 0;
    } else {
        light = g_settings.theme == 2;
    }

    float a = g_settings.opacity;
    Palette p;
    if (light) {
        p.bg = Rgba(0.97f, 0.97f, 0.98f, a);
        p.border = Rgba(0, 0, 0, 0.07f);
        p.text = Rgba(0, 0, 0, 0.90f);
        p.text2 = Rgba(0, 0, 0, 0.58f);
        p.text3 = Rgba(0, 0, 0, 0.30f);
        p.hover = Rgba(0, 0, 0, 0.07f);
        p.track = Rgba(0, 0, 0, 0.12f);
    } else {
        p.bg = Rgba(0.11f, 0.11f, 0.13f, a);
        p.border = Rgba(1, 1, 1, 0.08f);
        p.text = Rgba(1, 1, 1, 0.96f);
        p.text2 = Rgba(1, 1, 1, 0.64f);
        p.text3 = Rgba(1, 1, 1, 0.32f);
        p.hover = Rgba(1, 1, 1, 0.10f);
        p.track = Rgba(1, 1, 1, 0.16f);
    }

    // AccentPalette is eight RGBA entries, lightest first: Light3, Light2,
    // Light1, Accent, Dark1, Dark2, Dark3. Light2 reads well on dark cards,
    // Dark1 on light ones -- the same picks the shell makes.
    p.accent = Rgba(0, 0.47f, 0.84f, 1);
    BYTE palette[32];
    DWORD size = sizeof(palette);
    if (RegGetValueW(HKEY_CURRENT_USER,
                     L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Accent",
                     L"AccentPalette", RRF_RT_REG_BINARY, nullptr, palette,
                     &size) == ERROR_SUCCESS &&
        size >= 28) {
        const BYTE* c = palette + (light ? 4 : 1) * 4;
        p.accent = Rgba(c[0] / 255.0f, c[1] / 255.0f, c[2] / 255.0f, 1);
    }
    float luminance =
        0.2126f * p.accent.r + 0.7152f * p.accent.g + 0.0722f * p.accent.b;
    p.onAccent = luminance > 0.55f ? Rgba(0, 0, 0, 0.92f) : Rgba(1, 1, 1, 1);

    g_pal = p;
}

bool FontExists(PCWSTR family) {
    com_ptr<IDWriteFontCollection> fonts;
    if (FAILED(g_dw->GetSystemFontCollection(fonts.put()))) {
        return false;
    }
    UINT32 index;
    BOOL exists = FALSE;
    fonts->FindFamilyName(family, &index, &exists);
    return exists;
}

com_ptr<IDWriteTextFormat> MakeFormat(PCWSTR family,
                                      float size,
                                      DWRITE_FONT_WEIGHT weight,
                                      DWRITE_TEXT_ALIGNMENT align,
                                      bool ellipsis = false) {
    wchar_t locale[LOCALE_NAME_MAX_LENGTH] = L"en-us";
    GetUserDefaultLocaleName(locale, ARRAYSIZE(locale));

    com_ptr<IDWriteTextFormat> format;
    if (FAILED(g_dw->CreateTextFormat(family, nullptr, weight,
                                      DWRITE_FONT_STYLE_NORMAL,
                                      DWRITE_FONT_STRETCH_NORMAL, size, locale,
                                      format.put()))) {
        return nullptr;
    }
    format->SetTextAlignment(align);
    format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    if (ellipsis) {
        com_ptr<IDWriteInlineObject> sign;
        if (SUCCEEDED(g_dw->CreateEllipsisTrimmingSign(format.get(), sign.put()))) {
            DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
            format->SetTrimming(&trimming, sign.get());
        }
    }
    return format;
}

void CreateFonts() {
    std::wstring text = g_settings.font;
    if (text.empty() || !FontExists(text.c_str())) {
        text = L"Segoe UI";
    }
    PCWSTR t = text.c_str();
    PCWSTR icons = FontExists(L"Segoe Fluent Icons") ? L"Segoe Fluent Icons"
                                                      : L"Segoe MDL2 Assets";

    auto L = DWRITE_TEXT_ALIGNMENT_LEADING;
    auto C = DWRITE_TEXT_ALIGNMENT_CENTER;
    auto R = DWRITE_TEXT_ALIGNMENT_TRAILING;
    Fonts f;
    f.title = MakeFormat(t, 15, DWRITE_FONT_WEIGHT_SEMI_BOLD, L);
    f.titleEllipsis = MakeFormat(t, 14.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD, L, true);
    f.body = MakeFormat(t, 13, DWRITE_FONT_WEIGHT_NORMAL, L);
    f.bodyEllipsis = MakeFormat(t, 13, DWRITE_FONT_WEIGHT_NORMAL, L, true);
    f.small = MakeFormat(t, 11.5f, DWRITE_FONT_WEIGHT_NORMAL, L);
    f.smallRight = MakeFormat(t, 11.5f, DWRITE_FONT_WEIGHT_NORMAL, R);
    f.weekday = MakeFormat(t, 11.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD, C);
    f.day = MakeFormat(t, 13, DWRITE_FONT_WEIGHT_NORMAL, C);
    f.dayBold = MakeFormat(t, 13, DWRITE_FONT_WEIGHT_SEMI_BOLD, C);
    f.icon = MakeFormat(icons, 13, DWRITE_FONT_WEIGHT_NORMAL, C);
    f.iconBig = MakeFormat(icons, 18, DWRITE_FONT_WEIGHT_NORMAL, C);
    f.hint = MakeFormat(t, 12, DWRITE_FONT_WEIGHT_NORMAL, C, true);
    f.monoFamily = FontExists(L"Cascadia Mono") ? L"Cascadia Mono" : L"Consolas";
    f.mono = MakeFormat(f.monoFamily.c_str(), g_settings.outputFontSize,
                        DWRITE_FONT_WEIGHT_NORMAL, L);
    if (f.mono) {
        f.mono->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    }
    g_fonts = std::move(f);
}

bool EnsureRenderTarget() {
    if (g_rt) {
        return true;
    }
    auto props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96, 96);
    if (FAILED(g_d2d->CreateDCRenderTarget(&props, g_rt.put()))) {
        return false;
    }
    // ClearType needs an opaque background, which a layered card doesn't have.
    g_rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    g_rt->CreateSolidColorBrush(D2D1::ColorF(0), g_brush.put());
    g_rtGeneration++;
    return true;
}

void ReleaseRenderTarget() {
    g_brush = nullptr;
    g_rt = nullptr;
}

void Fill(ID2D1RenderTarget* rt, D2D1_RECT_F r, float radius, D2D1_COLOR_F c) {
    g_brush->SetColor(c);
    rt->FillRoundedRectangle(D2D1::RoundedRect(r, radius, radius), g_brush.get());
}

void DrawString(ID2D1RenderTarget* rt,
                IDWriteTextFormat* format,
                const std::wstring& s,
                D2D1_RECT_F r,
                D2D1_COLOR_F c) {
    if (!format || s.empty()) {
        return;
    }
    g_brush->SetColor(c);
    rt->DrawText(s.c_str(), (UINT32)s.size(), format, r, g_brush.get(),
                 D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

bool Contains(D2D1_RECT_F r, D2D1_POINT_2F p) {
    return p.x >= r.left && p.x < r.right && p.y >= r.top && p.y < r.bottom;
}

bool IsLightPalette() {
    return g_pal.text.r < 0.5f;
}

COLORREF ToColorRef(D2D1_COLOR_F c) {
    return RGB(std::lround(c.r * 255), std::lround(c.g * 255), std::lround(c.b * 255));
}

// =============================================================================
// Frame clock
// =============================================================================

// Animations step at the display's refresh rate (capped at 240 Hz), paced by a
// high-resolution waitable timer the UI thread's message loop waits on. A
// window timer can't go below ~15.6 ms -- 64 steps a second, visibly stepped
// on a fast display -- and GetTickCount64 ticks just as coarsely, so time is
// read from the performance counter instead.
struct Widget;
std::vector<Widget*> g_animating;
HANDLE g_frameTimer;
bool g_frameArmed = false;
double g_frameIntervalMs = 1000.0 / 60;
double g_nextFrameMs = 0;

double NowMs() {
    static LARGE_INTEGER frequency = [] {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return f;
    }();
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return now.QuadPart * 1000.0 / frequency.QuadPart;
}

void UpdateFrameInterval() {
    double hz = 60;
    DWM_TIMING_INFO timing{sizeof(timing)};
    if (SUCCEEDED(DwmGetCompositionTimingInfo(nullptr, &timing)) &&
        timing.rateRefresh.uiDenominator) {
        hz = (double)timing.rateRefresh.uiNumerator / timing.rateRefresh.uiDenominator;
    }
    g_frameIntervalMs = 1000.0 / std::clamp(hz, 30.0, 240.0);
}

void ArmFrameTimer() {
    if (g_frameArmed || !g_frameTimer) {
        return;
    }
    // Frames land on a steady cadence; coming back from idle (or after a
    // stall) starts a fresh one rather than rushing to catch up.
    double now = NowMs();
    if (g_nextFrameMs < now) {
        g_nextFrameMs = now;
    }
    g_nextFrameMs += g_frameIntervalMs;
    LARGE_INTEGER due;
    due.QuadPart = -std::max<LONGLONG>(1, (LONGLONG)((g_nextFrameMs - now) * 10000));
    g_frameArmed = SetWaitableTimer(g_frameTimer, &due, 0, nullptr, nullptr, FALSE);
}

// =============================================================================
// Widget base: a layered, desktop-owned window drawing one card
// =============================================================================

struct Widget {
    std::wstring key;  // prefix for the saved position
    HWND hwnd = nullptr;
    UINT dpi = 96;
    float cardW = 0, cardH = 0;  // DIPs, without the shadow margin

    HDC memDC = nullptr;
    HBITMAP dib = nullptr;
    HGDIOBJ oldBitmap = nullptr;
    SIZE dibSize{};

    float opacity = 0;
    bool shown = false;
    std::vector<float> hover;  // per button, 0..1
    int hot = -1, pressed = -1;
    bool tracking = false, dragging = false;
    POINT grab{};
    POINT pressPoint{};
    // Edges being dragged while resizing: 1 left, 2 right, 4 top, 8 bottom.
    int resizing = 0;
    POINT resizeCursor{}, resizeWindow{};
    float resizeW = 0, resizeH = 0;
    bool animTimer = false;
    double lastAnim = 0;
    // The current show/hide fade: its length, and whether it follows the
    // genie's curve (fading behind windows) or the quick ease-out.
    float fadeMs = kAnimMs;
    bool fadeSmooth = false;

    virtual ~Widget() {
        std::erase(g_animating, this);
        if (hwnd && IsWindow(hwnd)) {
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            DestroyWindow(hwnd);
        }
        FreeDib();
    }

    virtual void Paint(ID2D1RenderTarget* rt) = 0;
    virtual int HitTest(D2D1_POINT_2F) { return -1; }
    virtual bool Enabled(int) { return true; }
    virtual void Click(int, D2D1_POINT_2F) {}
    virtual void Wheel(int) {}
    virtual void Tick() {}
    // Advances the widget's own animations. Returns true while any is running.
    virtual bool AnimateContent(float) { return false; }
    virtual bool WantsVisible() { return true; }
    virtual void OnMedia() {}
    virtual PCWSTR Cursor(int) { return IDC_HAND; }
    // Pressing on this hit target and moving drags the widget instead;
    // releasing without moving still clicks it.
    virtual bool ClickOrDrag(int) { return false; }
    // Resizable widgets can be dragged by their edges and corners.
    virtual bool Resizable() { return false; }
    virtual float MinWidth() { return 120; }
    virtual float MinHeight() { return 80; }
    // After every step of a resize drag.
    virtual void OnResized() {}
    // Messages a widget handles itself; true when handled.
    virtual bool OnMessage(UINT, WPARAM, LPARAM, LRESULT&) { return false; }
    // Once the window exists.
    virtual void OnCreated() {}
    // Extra right-click menu entries, with ids from 100 up.
    virtual void AddMenuItems(HMENU) {}
    virtual void MenuCommand(int) {}

    float Scale() const { return dpi / 96.0f * g_settings.scale; }
    float Margin() const { return g_settings.shadow ? kShadowMargin : 1.0f; }

    SIZE PixelSize() const {
        float s = Scale(), m = Margin();
        return {(LONG)std::ceil((cardW + 2 * m) * s),
                (LONG)std::ceil((cardH + 2 * m) * s)};
    }

    D2D1_POINT_2F ToCard(POINT px) const {
        float s = Scale();
        return {px.x / s - Margin(), px.y / s - Margin()};
    }

    void FreeDib() {
        if (memDC) {
            SelectObject(memDC, oldBitmap);
            DeleteDC(memDC);
            memDC = nullptr;
        }
        if (dib) {
            DeleteObject(dib);
            dib = nullptr;
        }
        dibSize = {};
    }

    bool EnsureDib(SIZE size) {
        if (memDC && size.cx == dibSize.cx && size.cy == dibSize.cy) {
            return true;
        }
        FreeDib();
        BITMAPINFO bi{};
        bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
        bi.bmiHeader.biWidth = size.cx;
        bi.bmiHeader.biHeight = -size.cy;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        void* bits;
        HDC screen = GetDC(nullptr);
        memDC = CreateCompatibleDC(screen);
        dib = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        ReleaseDC(nullptr, screen);
        if (!memDC || !dib) {
            FreeDib();
            return false;
        }
        oldBitmap = SelectObject(memDC, dib);
        dibSize = size;
        return true;
    }

    void DrawCard(ID2D1RenderTarget* rt) {
        float r = g_settings.cornerRadius;
        auto card = D2D1::RoundedRect(D2D1::RectF(0, 0, cardW, cardH), r, r);

        if (g_settings.shadow) {
            // Stacked rings, each with the card cut out of it, so the shadow
            // darkens the wallpaper around the card and not the translucent
            // card itself. Many faint rings under a pixel apart blend into a
            // smooth falloff instead of visible bands.
            com_ptr<ID2D1RoundedRectangleGeometry> cardGeometry;
            g_d2d->CreateRoundedRectangleGeometry(card, cardGeometry.put());
            g_brush->SetColor(Rgba(0, 0, 0, 0.022f));
            for (int i = 1; i <= 8 && cardGeometry; i++) {
                float e = i * 0.75f;
                auto ring = D2D1::RoundedRect(
                    D2D1::RectF(-e, -e + 1.5f, cardW + e, cardH + e + 1.5f),
                    r + e, r + e);
                com_ptr<ID2D1RoundedRectangleGeometry> ringGeometry;
                com_ptr<ID2D1PathGeometry> path;
                com_ptr<ID2D1GeometrySink> sink;
                if (FAILED(g_d2d->CreateRoundedRectangleGeometry(ring, ringGeometry.put())) ||
                    FAILED(g_d2d->CreatePathGeometry(path.put())) ||
                    FAILED(path->Open(sink.put()))) {
                    break;
                }
                ringGeometry->CombineWithGeometry(cardGeometry.get(),
                                                  D2D1_COMBINE_MODE_EXCLUDE,
                                                  nullptr, sink.get());
                sink->Close();
                rt->FillGeometry(path.get(), g_brush.get());
            }
        }

        g_brush->SetColor(g_pal.bg);
        rt->FillRoundedRectangle(card, g_brush.get());

        // A one-pixel hairline, whatever the scale.
        float px = 1 / Scale();
        auto edge = D2D1::RoundedRect(
            D2D1::RectF(px / 2, px / 2, cardW - px / 2, cardH - px / 2), r, r);
        g_brush->SetColor(g_pal.border);
        rt->DrawRoundedRectangle(edge, g_brush.get(), px);
    }

    void Present() {
        if (!hwnd || !memDC) {
            return;
        }
        float alpha = fadeSmooth ? SmoothStep(opacity) : EaseOut(opacity);
        BLENDFUNCTION blend{AC_SRC_OVER, 0, (BYTE)(alpha * 255),
                            AC_SRC_ALPHA};
        POINT src{0, 0};
        UpdateLayeredWindow(hwnd, nullptr, nullptr, &dibSize, memDC, &src, 0,
                            &blend, ULW_ALPHA);
    }

    void Render() {
        if (!hwnd || !EnsureDib(PixelSize())) {
            return;
        }
        RECT rc{0, 0, dibSize.cx, dibSize.cy};
        float s = Scale(), m = Margin();
        for (int attempt = 0; attempt < 2; attempt++) {
            if (!EnsureRenderTarget()) {
                return;
            }
            if (FAILED(g_rt->BindDC(memDC, &rc))) {
                ReleaseRenderTarget();
                continue;
            }
            g_rt->BeginDraw();
            g_rt->SetTransform(D2D1::Matrix3x2F::Identity());
            g_rt->Clear(D2D1::ColorF(0, 0));
            g_rt->SetTransform(D2D1::Matrix3x2F::Translation(m, m) *
                               D2D1::Matrix3x2F::Scale(s, s));
            DrawCard(g_rt.get());
            Paint(g_rt.get());
            if (g_rt->EndDraw() == D2DERR_RECREATE_TARGET) {
                ReleaseRenderTarget();
                continue;
            }
            break;
        }
        Present();
    }

    void StartAnimation() {
        if (!hwnd || animTimer) {
            return;
        }
        animTimer = true;
        lastAnim = NowMs();
        g_animating.push_back(this);
        ArmFrameTimer();
    }

    void OnAnimationFrame() {
        double now = NowMs();
        float dt = (float)(now - lastAnim);
        lastAnim = now;
        float step = g_settings.animations ? dt / kAnimMs : 1.0f;

        bool busy = false, redraw = false, refade = false;

        for (int i = 0; i < (int)hover.size(); i++) {
            float target = (i == hot && Enabled(i)) ? 1.0f : 0.0f;
            if (hover[i] != target) {
                hover[i] = hover[i] < target ? std::min(target, hover[i] + step)
                                             : std::max(target, hover[i] - step);
                redraw = true;
                busy |= hover[i] != target;
            }
        }

        // Also one frame after the content settles, so it lands exactly on
        // its end state.
        bool contentBusy = AnimateContent(g_settings.animations ? dt : 1e6f);
        redraw |= contentBusy || contentWasAnimating;
        contentWasAnimating = contentBusy;
        busy |= contentBusy;

        float target = shown ? 1.0f : 0.0f;
        if (opacity != target) {
            float fadeStep = g_settings.animations ? dt / fadeMs : 1.0f;
            opacity = opacity < target ? std::min(target, opacity + fadeStep)
                                       : std::max(target, opacity - fadeStep);
            refade = true;
            busy |= opacity != target;
            if (opacity == 0 && !shown) {
                ShowWindow(hwnd, SW_HIDE);
            }
        }

        if (redraw) {
            Render();
        } else if (refade) {
            Present();
        }

        if (!busy) {
            animTimer = false;  // RunAnimationFrame drops it from the list
        }
    }
    bool contentWasAnimating = false;

    void SetShown(bool value, float ms = kAnimMs, bool smooth = false) {
        if (value && !IsWindowVisible(hwnd)) {
            Render();
            ShowWindow(hwnd, SW_SHOWNOACTIVATE);
            SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        if (shown != value) {
            shown = value;
            fadeMs = std::max(1.0f, ms);
            fadeSmooth = smooth;
            StartAnimation();
        }
    }

    void UpdateVisibility(float ms = kAnimMs, bool smooth = false) {
        SetShown(WantsVisible() && g_desktopShown, ms, smooth);
    }

    void SavePosition() {
        RECT r;
        GetWindowRect(hwnd, &r);
        Wh_SetIntValue((key + L"X").c_str(), r.left);
        Wh_SetIntValue((key + L"Y").c_str(), r.top);
    }

    // Snaps the card (not the window, whose shadow margin is invisible) to the
    // grid and to the work area's edges, and keeps it inside the work area.
    POINT SnapPosition(POINT windowPos, POINT cursor) {
        MONITORINFO mi{sizeof(mi)};
        GetMonitorInfoW(MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST), &mi);
        RECT work = mi.rcWork;

        float s = Scale();
        int m = (int)std::lround(Margin() * s);
        int w = (int)std::lround(cardW * s), h = (int)std::lround(cardH * s);
        int x = windowPos.x + m, y = windowPos.y + m;

        if (g_settings.snapGrid > 0) {
            float g = g_settings.snapGrid * s;
            x = work.left + (int)std::lround(std::round((x - work.left) / g) * g);
            y = work.top + (int)std::lround(std::round((y - work.top) / g) * g);
        }

        int edge = (int)std::lround(kScreenMargin * s);
        int pull = (int)std::lround(12 * s);
        auto magnet = [&](int& v, int size, int lo, int hi) {
            if (std::abs(v - (lo + edge)) < pull) {
                v = lo + edge;
            } else if (std::abs(v + size - (hi - edge)) < pull) {
                v = hi - edge - size;
            }
            v = std::clamp(v, (int)lo, std::max((int)lo, (int)hi - size));
        };
        magnet(x, w, work.left, work.right);
        magnet(y, h, work.top, work.bottom);

        return {x - m, y - m};
    }

    // The resize edges under a point in card DIPs, as in `resizing`. The
    // shadow margin outside the card counts as edge too.
    int ResizeEdges(D2D1_POINT_2F p) {
        if (!Resizable() || g_locked) {
            return 0;
        }
        constexpr float kEdge = 6, kCorner = 18;
        bool left = p.x < kEdge, right = p.x >= cardW - kEdge;
        bool top = p.y < kEdge, bottom = p.y >= cardH - kEdge;
        // Corners get a longer grab along each side.
        bool nearLeft = p.x < kCorner, nearRight = p.x >= cardW - kCorner;
        bool nearTop = p.y < kCorner, nearBottom = p.y >= cardH - kCorner;
        if (top || bottom) {
            left |= nearLeft;
            right |= nearRight;
        }
        if (left || right) {
            top |= nearTop;
            bottom |= nearBottom;
        }
        return (left ? 1 : 0) | (right ? 2 : 0) | (top ? 4 : 0) | (bottom ? 8 : 0);
    }

    static PCWSTR ResizeCursor(int edges) {
        bool l = edges & 1, r = edges & 2, t = edges & 4, b = edges & 8;
        if ((l && t) || (r && b)) return IDC_SIZENWSE;
        if ((r && t) || (l && b)) return IDC_SIZENESW;
        if (l || r) return IDC_SIZEWE;
        return IDC_SIZENS;
    }

    void ResizeTo(POINT cursor) {
        float s = Scale();
        float dx = (cursor.x - resizeCursor.x) / s, dy = (cursor.y - resizeCursor.y) / s;
        float w = resizeW, h = resizeH;
        if (resizing & 1) w -= dx;
        if (resizing & 2) w += dx;
        if (resizing & 4) h -= dy;
        if (resizing & 8) h += dy;
        if (g_settings.snapGrid > 0) {
            w = std::round(w / g_settings.snapGrid) * g_settings.snapGrid;
            h = std::round(h / g_settings.snapGrid) * g_settings.snapGrid;
        }
        w = std::clamp(w, MinWidth(), 4000.0f);
        h = std::clamp(h, MinHeight(), 4000.0f);
        if (w == cardW && h == cardH) {
            return;
        }
        // Dragging the left or top edge keeps the opposite one in place.
        POINT pos = resizeWindow;
        if (resizing & 1) pos.x += (LONG)std::lround((resizeW - w) * s);
        if (resizing & 4) pos.y += (LONG)std::lround((resizeH - h) * s);
        cardW = w;
        cardH = h;
        SIZE size = PixelSize();
        SetWindowPos(hwnd, nullptr, pos.x, pos.y, size.cx, size.cy,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        Render();
        OnResized();
    }

    void SaveSize() {
        Wh_SetIntValue((key + L"W").c_str(), (int)std::lround(cardW));
        Wh_SetIntValue((key + L"H").c_str(), (int)std::lround(cardH));
    }

    void EndResize() {
        resizing = 0;
        SavePosition();
        SaveSize();
    }

    void ShowMenu(POINT screen);

    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
        LRESULT handled;
        if (OnMessage(msg, wParam, lParam, handled)) {
            return handled;
        }
        switch (msg) {
            case WM_MOUSEACTIVATE:
                return MA_NOACTIVATE;

            case WM_WINDOWPOSCHANGING: {
                // Stay at the bottom of the desktop's owned group: above the
                // wallpaper, below every normal window.
                auto* wp = reinterpret_cast<WINDOWPOS*>(lParam);
                if (!(wp->flags & SWP_NOZORDER)) {
                    wp->hwndInsertAfter = HWND_BOTTOM;
                }
                return 0;
            }

            case WM_SETCURSOR:
                if (LOWORD(lParam) == HTCLIENT) {
                    POINT pt;
                    GetCursorPos(&pt);
                    ScreenToClient(hwnd, &pt);
                    int edges = resizing ? resizing
                                : dragging  ? 0
                                            : ResizeEdges(ToCard(pt));
                    PCWSTR cursor = edges      ? ResizeCursor(edges)
                                    : dragging ? IDC_SIZEALL
                                    : (hot >= 0 && Enabled(hot)) ? Cursor(hot)
                                                                 : IDC_ARROW;
                    SetCursor(LoadCursorW(nullptr, cursor));
                    return TRUE;
                }
                break;

            case WM_MOUSEMOVE: {
                if (!tracking) {
                    TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, hwnd, 0};
                    TrackMouseEvent(&tme);
                    tracking = true;
                }
                if (resizing) {
                    POINT cursor;
                    GetCursorPos(&cursor);
                    ResizeTo(cursor);
                    return 0;
                }
                if (pressed >= 0 && ClickOrDrag(pressed) && !g_locked) {
                    POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                    if (std::abs(pt.x - pressPoint.x) > GetSystemMetrics(SM_CXDRAG) ||
                        std::abs(pt.y - pressPoint.y) > GetSystemMetrics(SM_CYDRAG)) {
                        pressed = -1;
                        dragging = true;
                        grab = pressPoint;
                        SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
                    }
                }
                if (dragging) {
                    POINT cursor;
                    GetCursorPos(&cursor);
                    POINT pos = SnapPosition(
                        {cursor.x - grab.x, cursor.y - grab.y}, cursor);
                    SetWindowPos(hwnd, nullptr, pos.x, pos.y, 0, 0,
                                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                    return 0;
                }
                POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                int id = HitTest(ToCard(pt));
                if (id != hot) {
                    hot = id;
                    StartAnimation();
                }
                return 0;
            }

            case WM_MOUSELEAVE:
                tracking = false;
                if (hot != -1) {
                    hot = -1;
                    StartAnimation();
                }
                return 0;

            case WM_LBUTTONDOWN: {
                POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                bool alt = GetKeyState(VK_MENU) < 0;
                int id = HitTest(ToCard(pt));
                int edges = alt ? 0 : ResizeEdges(ToCard(pt));
                if (edges) {
                    resizing = edges;
                    GetCursorPos(&resizeCursor);
                    RECT r;
                    GetWindowRect(hwnd, &r);
                    resizeWindow = {r.left, r.top};
                    resizeW = cardW;
                    resizeH = cardH;
                    SetCapture(hwnd);
                } else if (id >= 0 && !alt) {
                    if (Enabled(id)) {
                        pressed = id;
                        pressPoint = pt;
                        SetCapture(hwnd);
                    }
                } else if (!g_locked || alt) {
                    dragging = true;
                    grab = pt;
                    SetCapture(hwnd);
                    SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
                }
                return 0;
            }

            case WM_LBUTTONUP: {
                POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                if (resizing) {
                    ReleaseCapture();  // WM_CAPTURECHANGED ends the resize
                } else if (dragging) {
                    dragging = false;
                    ReleaseCapture();
                    SavePosition();
                } else if (pressed >= 0) {
                    int id = pressed;
                    pressed = -1;
                    ReleaseCapture();
                    D2D1_POINT_2F p = ToCard(pt);
                    if (HitTest(p) == id && Enabled(id)) {
                        Click(id, p);
                    }
                }
                return 0;
            }

            case WM_CAPTURECHANGED:
                if (resizing) {
                    EndResize();
                }
                if (dragging) {
                    dragging = false;
                    SavePosition();
                }
                pressed = -1;
                return 0;

            case WM_RBUTTONUP: {
                POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                ClientToScreen(hwnd, &pt);
                ShowMenu(pt);
                return 0;
            }

            case WM_MOUSEWHEEL:
                Wheel(GET_WHEEL_DELTA_WPARAM(wParam));
                return 0;

            case WM_TIMER:
                if (wParam == kTimerTick) {
                    Tick();
                }
                return 0;

            case WM_DPICHANGED:
                dpi = HIWORD(wParam);
                Render();
                return 0;

            case WM_APP_MEDIA:
                OnMedia();
                return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
};

std::vector<std::unique_ptr<Widget>> g_widgets;

void RunAnimationFrame() {
    g_frameArmed = false;
    // A copy: a frame can start or end other widgets' animations.
    std::vector<Widget*> frame = g_animating;
    for (Widget* w : frame) {
        if (std::find(g_animating.begin(), g_animating.end(), w) != g_animating.end()) {
            w->OnAnimationFrame();
        }
    }
    std::erase_if(g_animating, [](Widget* w) { return !w->animTimer; });
    if (!g_animating.empty()) {
        ArmFrameTimer();
    }
}

LRESULT CALLBACK WidgetWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCCREATE) {
        auto* widget = static_cast<Widget*>(
            reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        widget->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)widget);
    }
    auto* widget = reinterpret_cast<Widget*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!widget) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    if (msg == WM_NCDESTROY) {
        // Also reached when the desktop window goes away and takes its owned
        // windows with it; the host notices and rebuilds.
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        widget->hwnd = nullptr;
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return widget->HandleMessage(msg, wParam, lParam);
}

void ResetPositions();

void Widget::ShowMenu(POINT screen) {
    HMENU menu = CreatePopupMenu();
    AddMenuItems(menu);
    if (GetMenuItemCount(menu) > 0) {
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    }
    AppendMenuW(menu, MF_STRING | (g_locked ? MF_CHECKED : 0), 1,
                L"Lock widget positions");
    AppendMenuW(menu, MF_STRING, 2, L"Reset widget positions");

    // The menu needs a foreground window to close properly when clicking
    // elsewhere. The hidden host serves, so the card itself never activates.
    SetForegroundWindow(g_host);
    int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, screen.x,
                             screen.y, 0, g_host, nullptr);
    PostMessageW(g_host, WM_NULL, 0, 0);
    DestroyMenu(menu);

    if (cmd == 1) {
        g_locked = !g_locked;
        Wh_SetIntValue(L"Locked", g_locked);
    } else if (cmd == 2) {
        ResetPositions();
    } else if (cmd >= 100) {
        MenuCommand(cmd);
    }
}

// =============================================================================
// Calendar
// =============================================================================

int DaysInMonth(int y, int m) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    return m == 2 && leap ? 29 : days[m - 1];
}

// 0 = Monday ... 6 = Sunday.
int DayOfWeek(int y, int m, int d) {
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) {
        y -= 1;
    }
    int sundayBased = (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
    return (sundayBased + 6) % 7;
}

int LocaleWeekStart() {
    DWORD value = 0;
    if (GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT,
                        LOCALE_IFIRSTDAYOFWEEK | LOCALE_RETURN_NUMBER,
                        reinterpret_cast<LPWSTR>(&value),
                        sizeof(value) / sizeof(WCHAR))) {
        return (int)value % 7;
    }
    return 0;
}

struct CalendarWidget : Widget {
    static constexpr float kPad = 16, kHeader = 32, kWeekRow = 26, kRow = 32;
    enum { kPrev, kNext, kTitle };

    SYSTEMTIME today{};
    int viewYear, viewMonth;
    int weekStart;
    std::wstring weekdays[7];
    float gridIn = 1;  // 0..1, the month-change fade
    int slide = 0;     // direction of the last month change
    int wheelAccum = 0;

    CalendarWidget() {
        key = L"Calendar";
        cardW = 284;
        cardH = kPad + kHeader + 6 + kWeekRow + 6 * kRow + kPad - 4;
        hover.assign(3, 0);
        GetLocalTime(&today);
        viewYear = today.wYear;
        viewMonth = today.wMonth;
        weekStart = g_settings.weekStart >= 0 ? g_settings.weekStart
                                              : LocaleWeekStart();
        for (int i = 0; i < 7; i++) {
            wchar_t name[32] = L"";
            GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT,
                            LOCALE_SSHORTESTDAYNAME1 + i, name, ARRAYSIZE(name));
            weekdays[i] = name;
        }
    }

    bool ViewingToday() const {
        return viewYear == today.wYear && viewMonth == today.wMonth;
    }

    D2D1_RECT_F ButtonRect(int id) const {
        switch (id) {
            case kPrev:
                return {cardW - kPad - 64, kPad + 1, cardW - kPad - 34, kPad + 31};
            case kNext:
                return {cardW - kPad - 30, kPad + 1, cardW - kPad, kPad + 31};
            default:
                return {kPad - 8, kPad + 2, kPad + 172, kPad + 30};
        }
    }

    int HitTest(D2D1_POINT_2F p) override {
        for (int id : {kPrev, kNext, kTitle}) {
            if (Contains(ButtonRect(id), p)) {
                return id;
            }
        }
        return -1;
    }

    bool Enabled(int id) override { return id != kTitle || !ViewingToday(); }

    void Shift(int months) {
        int index = viewYear * 12 + (viewMonth - 1) + months;
        viewYear = index / 12;
        viewMonth = index % 12 + 1;
        slide = months > 0 ? 1 : months < 0 ? -1 : 0;
        gridIn = 0;
        StartAnimation();
        Render();
    }

    void Click(int id, D2D1_POINT_2F) override {
        if (id == kPrev) {
            Shift(-1);
        } else if (id == kNext) {
            Shift(1);
        } else {
            Shift((today.wYear * 12 + today.wMonth) - (viewYear * 12 + viewMonth));
        }
    }

    void Wheel(int delta) override {
        wheelAccum += delta;
        while (std::abs(wheelAccum) >= WHEEL_DELTA) {
            int dir = wheelAccum > 0 ? -1 : 1;  // up = earlier
            wheelAccum += dir * WHEEL_DELTA;
            Shift(dir);
        }
    }

    void Tick() override {
        SYSTEMTIME now;
        GetLocalTime(&now);
        if (now.wDay == today.wDay && now.wMonth == today.wMonth &&
            now.wYear == today.wYear) {
            return;
        }
        // Follow the date across midnight, unless the user is browsing
        // another month.
        bool follow = ViewingToday();
        today = now;
        if (follow) {
            viewYear = today.wYear;
            viewMonth = today.wMonth;
        }
        Render();
    }

    bool AnimateContent(float dt) override {
        if (gridIn >= 1) {
            return false;
        }
        gridIn = std::min(1.0f, gridIn + dt / (kAnimMs * 1.3f));
        return gridIn < 1;
    }

    void Paint(ID2D1RenderTarget* rt) override {
        // Header.
        SYSTEMTIME st{};
        st.wYear = (WORD)viewYear;
        st.wMonth = (WORD)viewMonth;
        st.wDay = 1;
        wchar_t title[64] = L"";
        GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, DATE_YEARMONTH, &st, nullptr,
                        title, ARRAYSIZE(title), nullptr);

        D2D1_RECT_F titleRect = ButtonRect(kTitle);
        if (hover[kTitle] > 0) {
            Fill(rt, titleRect, 6, WithAlpha(g_pal.hover, hover[kTitle]));
        }
        DrawString(rt, g_fonts.title.get(), title,
                   {titleRect.left + 8, titleRect.top, titleRect.right, titleRect.bottom},
                   g_pal.text);

        const wchar_t* glyphs[] = {L"", L""};
        for (int id : {kPrev, kNext}) {
            D2D1_RECT_F r = ButtonRect(id);
            if (hover[id] > 0) {
                Fill(rt, r, 15, WithAlpha(g_pal.hover, hover[id]));
            }
            DrawString(rt, g_fonts.icon.get(), glyphs[id], r, g_pal.text2);
        }

        // Grid.
        float t = EaseOut(gridIn);
        float dx = (1 - t) * 10 * slide;
        float left = kPad, cw = (cardW - 2 * kPad) / 7;
        float top = kPad + kHeader + 6;

        for (int i = 0; i < 7; i++) {
            D2D1_RECT_F r{left + i * cw, top, left + (i + 1) * cw, top + kWeekRow};
            DrawString(rt, g_fonts.weekday.get(), weekdays[(weekStart + i) % 7], r,
                       g_pal.text3);
        }

        int lead = (DayOfWeek(viewYear, viewMonth, 1) - weekStart + 7) % 7;
        int days = DaysInMonth(viewYear, viewMonth);
        int prevYear = viewMonth == 1 ? viewYear - 1 : viewYear;
        int prevMonth = viewMonth == 1 ? 12 : viewMonth - 1;
        int prevDays = DaysInMonth(prevYear, prevMonth);
        float gridTop = top + kWeekRow;

        for (int cell = 0; cell < 42; cell++) {
            int n = cell - lead + 1;
            bool inMonth = n >= 1 && n <= days;
            int label = inMonth ? n : n < 1 ? prevDays + n : n - days;
            bool isToday = inMonth && ViewingToday() && n == today.wDay;

            int row = cell / 7, col = cell % 7;
            D2D1_RECT_F r{left + col * cw + dx, gridTop + row * kRow,
                          left + (col + 1) * cw + dx, gridTop + (row + 1) * kRow};

            wchar_t text[4];
            swprintf_s(text, L"%d", label);
            if (isToday) {
                float cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2;
                g_brush->SetColor(WithAlpha(g_pal.accent, t));
                rt->FillEllipse(D2D1::Ellipse({cx, cy}, 14, 14), g_brush.get());
                DrawString(rt, g_fonts.dayBold.get(), text, r,
                           Lerp(g_pal.text, g_pal.onAccent, t));
            } else {
                D2D1_COLOR_F c = inMonth ? g_pal.text : g_pal.text3;
                DrawString(rt, g_fonts.day.get(), text, r, WithAlpha(c, t));
            }
        }
    }
};

// =============================================================================
// Media session (runs on its own MTA thread)
// =============================================================================

namespace media {

using namespace winrt::Windows::Media::Control;
using namespace winrt::Windows::Storage::Streams;

struct Art {
    UINT width = 0, height = 0;
    std::vector<BYTE> pixels;  // 32bpp premultiplied BGRA, tightly packed
};

struct State {
    bool hasSession = false;
    std::wstring title, artist;
    bool playing = false;
    bool canPrev = false, canNext = false, canToggle = false, canSeek = false;
    int64_t start = 0, end = 0, position = 0;  // 100 ns ticks
    uint64_t updatedAt = 0;                    // FILETIME of `position`
    double rate = 1;
    std::shared_ptr<const Art> art;
    int artGeneration = 0;
};

enum : int {
    kFlagSession = 1,
    kFlagProperties = 2,
    kFlagPlayback = 4,
    kFlagTimeline = 8,
    kFlagCommand = 16,
};

enum Command : int { kCmdPrev = 1, kCmdToggle = 2, kCmdNext = 4, kCmdSeek = 8 };

std::thread g_thread;
HANDLE g_stop, g_wake;
std::atomic<int> g_flags;
std::atomic<int> g_commands;
std::atomic<int64_t> g_seekTo;
std::atomic<HWND> g_target;

std::mutex g_mutex;
State g_state;

void Wake(int flags) {
    g_flags |= flags;
    SetEvent(g_wake);
}

uint64_t NowFileTime() {
    FILETIME ft;
    GetSystemTimePreciseAsFileTime(&ft);
    return (uint64_t)ft.dwHighDateTime << 32 | ft.dwLowDateTime;
}

std::shared_ptr<const Art> ReadArt(IRandomAccessStreamReference const& ref) {
    if (!ref) {
        return nullptr;
    }
    auto stream = ref.OpenReadAsync().get();
    uint64_t size = stream.Size();
    if (size == 0 || size > 32 * 1024 * 1024) {
        return nullptr;
    }
    std::vector<uint8_t> bytes((size_t)size);
    DataReader reader(stream);
    reader.LoadAsync((uint32_t)size).get();
    reader.ReadBytes(bytes);

    com_ptr<IWICImagingFactory> wic;
    com_ptr<IWICStream> wicStream;
    com_ptr<IWICBitmapDecoder> decoder;
    com_ptr<IWICBitmapFrameDecode> frame;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                CLSCTX_INPROC_SERVER, __uuidof(IWICImagingFactory),
                                wic.put_void())) ||
        FAILED(wic->CreateStream(wicStream.put())) ||
        FAILED(wicStream->InitializeFromMemory(bytes.data(), (DWORD)bytes.size())) ||
        FAILED(wic->CreateDecoderFromStream(wicStream.get(), nullptr,
                                            WICDecodeMetadataCacheOnDemand,
                                            decoder.put())) ||
        FAILED(decoder->GetFrame(0, frame.put()))) {
        return nullptr;
    }

    UINT w, h;
    frame->GetSize(&w, &h);
    if (!w || !h) {
        return nullptr;
    }

    // Shown at under 200 px even on a 200% screen; no need to keep a
    // 1000-px cover around.
    com_ptr<IWICBitmapSource> source = frame;
    constexpr UINT kMax = 256;
    if (std::max(w, h) > kMax) {
        float k = (float)kMax / std::max(w, h);
        UINT nw = std::max(1u, (UINT)std::lround(w * k));
        UINT nh = std::max(1u, (UINT)std::lround(h * k));
        com_ptr<IWICBitmapScaler> scaler;
        if (SUCCEEDED(wic->CreateBitmapScaler(scaler.put())) &&
            SUCCEEDED(scaler->Initialize(frame.get(), nw, nh,
                                         WICBitmapInterpolationModeFant))) {
            source = scaler;
            w = nw;
            h = nh;
        }
    }

    com_ptr<IWICFormatConverter> converter;
    if (FAILED(wic->CreateFormatConverter(converter.put())) ||
        FAILED(converter->Initialize(source.get(), GUID_WICPixelFormat32bppPBGRA,
                                     WICBitmapDitherTypeNone, nullptr, 0,
                                     WICBitmapPaletteTypeCustom))) {
        return nullptr;
    }

    auto art = std::make_shared<Art>();
    art->width = w;
    art->height = h;
    art->pixels.resize((size_t)w * h * 4);
    if (FAILED(converter->CopyPixels(nullptr, w * 4, (UINT)art->pixels.size(),
                                     art->pixels.data()))) {
        return nullptr;
    }
    return art;
}

void Publish(State const& state) {
    {
        std::lock_guard lock(g_mutex);
        g_state = state;
    }
    if (HWND target = g_target) {
        PostMessageW(target, WM_APP_MEDIA, 0, 0);
    }
}

void ThreadProc() {
    winrt::init_apartment(winrt::apartment_type::multi_threaded);

    try {
        auto manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
        GlobalSystemMediaTransportControlsSession session{nullptr};
        winrt::event_token propertiesToken{}, playbackToken{}, timelineToken{};
        State state;

        // Event handlers only raise flags; all the real work happens here, on
        // one thread, one change at a time.
        auto sessionToken = manager.CurrentSessionChanged(
            [](auto&&, auto&&) { Wake(kFlagSession); });

        auto unsubscribe = [&] {
            if (session) {
                session.MediaPropertiesChanged(propertiesToken);
                session.PlaybackInfoChanged(playbackToken);
                session.TimelinePropertiesChanged(timelineToken);
                session = nullptr;
            }
        };

        g_flags = kFlagSession;
        HANDLE handles[] = {g_stop, g_wake};
        bool first = true;
        while (true) {
            DWORD wait = first ? WAIT_OBJECT_0 + 1
                               : WaitForMultipleObjects(2, handles, FALSE,
                                                        session ? 2000 : INFINITE);
            first = false;
            if (wait == WAIT_OBJECT_0) {
                break;
            }
            // Some players never raise timeline events; a slow poll keeps the
            // progress bar honest for them.
            int flags = g_flags.exchange(0) |
                        (wait == WAIT_TIMEOUT ? kFlagTimeline | kFlagPlayback : 0);

            try {
                if (flags & kFlagSession) {
                    unsubscribe();
                    session = manager.GetCurrentSession();
                    if (session) {
                        propertiesToken = session.MediaPropertiesChanged(
                            [](auto&&, auto&&) { Wake(kFlagProperties); });
                        playbackToken = session.PlaybackInfoChanged(
                            [](auto&&, auto&&) { Wake(kFlagPlayback); });
                        timelineToken = session.TimelinePropertiesChanged(
                            [](auto&&, auto&&) { Wake(kFlagTimeline); });
                        flags |= kFlagProperties | kFlagPlayback | kFlagTimeline;
                    } else {
                        int generation = state.artGeneration;
                        state = State{};
                        state.artGeneration = generation + 1;
                    }
                }

                int commands = g_commands.exchange(0);
                if (session && commands) {
                    if (commands & kCmdPrev) session.TrySkipPreviousAsync().get();
                    if (commands & kCmdToggle) session.TryTogglePlayPauseAsync().get();
                    if (commands & kCmdNext) session.TrySkipNextAsync().get();
                    if (commands & kCmdSeek) {
                        session.TryChangePlaybackPositionAsync(g_seekTo.load()).get();
                    }
                }

                if (session) {
                    state.hasSession = true;
                    if (flags & kFlagProperties) {
                        auto props = session.TryGetMediaPropertiesAsync().get();
                        std::wstring title = props.Title().c_str();
                        std::wstring artist = props.Artist().c_str();
                        if (artist.empty()) {
                            artist = props.AlbumArtist().c_str();
                        }
                        std::shared_ptr<const Art> art;
                        try {
                            art = ReadArt(props.Thumbnail());
                        } catch (...) {
                        }
                        // Players often raise the change twice: text first,
                        // cover a moment later. Only a new cover or a new
                        // track counts as a change of art.
                        if (art || title != state.title || artist != state.artist) {
                            state.art = art;
                            state.artGeneration++;
                        }
                        state.title = std::move(title);
                        state.artist = std::move(artist);
                    }
                    if (flags & kFlagPlayback) {
                        auto info = session.GetPlaybackInfo();
                        state.playing =
                            info.PlaybackStatus() ==
                            GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;
                        auto controls = info.Controls();
                        state.canPrev = controls.IsPreviousEnabled();
                        state.canNext = controls.IsNextEnabled();
                        state.canToggle = controls.IsPlayPauseToggleEnabled();
                        state.canSeek = controls.IsPlaybackPositionEnabled();
                        auto rate = info.PlaybackRate();
                        state.rate = rate ? rate.Value() : 1.0;
                    }
                    if (flags & (kFlagTimeline | kFlagPlayback)) {
                        auto timeline = session.GetTimelineProperties();
                        state.start = timeline.StartTime().count();
                        state.end = timeline.EndTime().count();
                        state.position = timeline.Position().count();
                        uint64_t updated =
                            winrt::clock::to_file_time(timeline.LastUpdatedTime()).value;
                        // A zero or future stamp would make extrapolation run
                        // away; treat it as "just now".
                        uint64_t now = NowFileTime();
                        state.updatedAt = updated && updated <= now ? updated : now;
                    }
                }
            } catch (...) {
                // A session can vanish mid-call (the player quit). The next
                // CurrentSessionChanged sorts it out.
                Wh_Log(L"Media refresh failed: %08X", (unsigned)winrt::to_hresult());
            }

            Publish(state);
        }

        unsubscribe();
        manager.CurrentSessionChanged(sessionToken);
    } catch (...) {
        Wh_Log(L"Media session manager unavailable: %08X",
               (unsigned)winrt::to_hresult());
    }

    winrt::uninit_apartment();
}

void Start() {
    if (g_thread.joinable()) {
        return;
    }
    g_stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    g_flags = 0;
    g_commands = 0;
    g_thread = std::thread(ThreadProc);
}

void Stop() {
    if (!g_thread.joinable()) {
        return;
    }
    SetEvent(g_stop);
    g_thread.join();
    CloseHandle(g_stop);
    CloseHandle(g_wake);
    g_stop = g_wake = nullptr;
    std::lock_guard lock(g_mutex);
    g_state = State{};
}

void Send(int command, int64_t seekTo = 0) {
    if (!g_thread.joinable()) {
        return;
    }
    if (command & kCmdSeek) {
        g_seekTo = seekTo;
    }
    g_commands |= command;
    Wake(kFlagCommand);
}

}  // namespace media

// =============================================================================
// Now playing
// =============================================================================

std::wstring FormatTime(int64_t ticks) {
    int64_t total = std::max<int64_t>(0, ticks / 10'000'000);
    wchar_t buffer[32];
    if (total >= 3600) {
        swprintf_s(buffer, L"%lld:%02lld:%02lld", total / 3600, total / 60 % 60,
                   total % 60);
    } else {
        swprintf_s(buffer, L"%lld:%02lld", total / 60, total % 60);
    }
    return buffer;
}

struct NowPlayingWidget : Widget {
    static constexpr float kPad = 14, kArt = 84;
    enum { kPrev, kToggle, kNext, kSeek };

    media::State state;
    int artGeneration = -1;
    com_ptr<ID2D1Bitmap> art, previousArt;
    int bitmapGeneration = -1;  // g_rtGeneration the bitmaps belong to
    float artIn = 1;

    NowPlayingWidget() {
        key = L"NowPlaying";
        cardW = 340;
        cardH = kPad * 2 + kArt;
        hover.assign(4, 0);
    }

    float TextLeft() const { return kPad + kArt + 14; }

    D2D1_RECT_F ButtonRect(int id) const {
        float x = TextLeft() - 7, y = kPad + 42;
        switch (id) {
            case kPrev:
                return {x, y, x + 34, y + 34};
            case kToggle:
                return {x + 36, y, x + 70, y + 34};
            case kNext:
                return {x + 72, y, x + 106, y + 34};
            default:
                return {TextLeft(), kPad + kArt - 8, cardW - kPad, kPad + kArt + 6};
        }
    }

    D2D1_RECT_F BarRect() const {
        float grow = hover[kSeek] * 2;
        float y = kPad + kArt - 2;
        return {TextLeft(), y - grow / 2 - 1.5f, cardW - kPad, y + grow / 2 + 1.5f};
    }

    bool HasTimeline() const {
        return g_settings.progress && state.hasSession && state.end > state.start;
    }

    int64_t Position() const {
        int64_t pos = state.position;
        if (state.playing) {
            uint64_t now = media::NowFileTime();
            if (now > state.updatedAt) {
                pos += (int64_t)((now - state.updatedAt) * state.rate);
            }
        }
        return std::clamp(pos, state.start, state.end);
    }

    int HitTest(D2D1_POINT_2F p) override {
        if (!state.hasSession) {
            return -1;
        }
        for (int id : {kPrev, kToggle, kNext}) {
            if (Contains(ButtonRect(id), p)) {
                return id;
            }
        }
        if (HasTimeline() && state.canSeek && Contains(ButtonRect(kSeek), p)) {
            return kSeek;
        }
        return -1;
    }

    bool Enabled(int id) override {
        switch (id) {
            case kPrev:
                return state.canPrev;
            case kToggle:
                return state.canToggle;
            case kNext:
                return state.canNext;
            default:
                return state.canSeek;
        }
    }

    void Click(int id, D2D1_POINT_2F p) override {
        switch (id) {
            case kPrev:
                media::Send(media::kCmdPrev);
                break;
            case kToggle:
                // Flip the icon now rather than when the player confirms,
                // which can take a noticeable moment.
                state.position = Position();
                state.updatedAt = media::NowFileTime();
                state.playing = !state.playing;
                media::Send(media::kCmdToggle);
                Render();
                break;
            case kNext:
                media::Send(media::kCmdNext);
                break;
            case kSeek: {
                D2D1_RECT_F bar = ButtonRect(kSeek);
                float f = std::clamp((p.x - bar.left) / (bar.right - bar.left), 0.0f, 1.0f);
                int64_t target = state.start + (int64_t)((state.end - state.start) * (double)f);
                state.position = target;
                state.updatedAt = media::NowFileTime();
                media::Send(media::kCmdSeek, target);
                Render();
                break;
            }
        }
    }

    void Tick() override {
        if (state.playing && HasTimeline() && shown) {
            Render();
        }
    }

    bool WantsVisible() override { return state.hasSession || !g_settings.hideIdle; }

    void OnMedia() override { Pull(); }

    void Pull() {
        {
            std::lock_guard lock(media::g_mutex);
            state = media::g_state;
        }
        if (state.artGeneration != artGeneration) {
            artGeneration = state.artGeneration;
            previousArt = art;
            art = nullptr;
            // Built lazily in Paint, against whatever the render target is by
            // then.
            artIn = previousArt || state.art ? 0 : 1;
            StartAnimation();
        }
        UpdateVisibility();
        Render();
    }

    bool AnimateContent(float dt) override {
        if (artIn >= 1) {
            return false;
        }
        artIn = std::min(1.0f, artIn + dt / (kAnimMs * 1.5f));
        if (artIn >= 1) {
            previousArt = nullptr;
        }
        return artIn < 1;
    }

    void DrawArt(ID2D1RenderTarget* rt, ID2D1Bitmap* bitmap, D2D1_RECT_F r, float alpha) {
        com_ptr<ID2D1BitmapBrush> brush;
        if (FAILED(rt->CreateBitmapBrush(
                bitmap,
                D2D1::BitmapBrushProperties(D2D1_EXTEND_MODE_CLAMP, D2D1_EXTEND_MODE_CLAMP,
                                            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR),
                D2D1::BrushProperties(alpha), brush.put()))) {
            return;
        }
        // Cover-fit: fill the square, crop the long side.
        D2D1_SIZE_F size = bitmap->GetSize();
        float w = r.right - r.left, h = r.bottom - r.top;
        float k = std::max(w / size.width, h / size.height);
        brush->SetTransform(D2D1::Matrix3x2F::Scale(k, k) *
                            D2D1::Matrix3x2F::Translation(
                                r.left + (w - size.width * k) / 2,
                                r.top + (h - size.height * k) / 2));
        rt->FillRoundedRectangle(D2D1::RoundedRect(r, 8, 8), brush.get());
    }

    void Paint(ID2D1RenderTarget* rt) override {
        if (bitmapGeneration != g_rtGeneration) {
            bitmapGeneration = g_rtGeneration;
            art = previousArt = nullptr;
        }
        if (!art && state.art) {
            rt->CreateBitmap(
                D2D1::SizeU(state.art->width, state.art->height),
                state.art->pixels.data(), state.art->width * 4,
                D2D1::BitmapProperties(D2D1::PixelFormat(
                    DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
                art.put());
        }

        D2D1_RECT_F artRect{kPad, kPad, kPad + kArt, kPad + kArt};
        float t = EaseOut(artIn);
        if (previousArt && t < 1) {
            DrawArt(rt, previousArt.get(), artRect, 1);
        }
        if (art) {
            DrawArt(rt, art.get(), artRect, t);
        } else {
            Fill(rt, artRect, 8, WithAlpha(g_pal.hover, previousArt ? t : 1));
            DrawString(rt, g_fonts.iconBig.get(), L"", artRect,
                       WithAlpha(g_pal.text3, previousArt ? t : 1));
        }

        float x = TextLeft(), right = cardW - kPad;
        if (!state.hasSession) {
            DrawString(rt, g_fonts.titleEllipsis.get(), L"Nothing playing",
                       {x, kPad, right, kPad + kArt}, g_pal.text2);
            return;
        }

        DrawString(rt, g_fonts.titleEllipsis.get(),
                   state.title.empty() ? L"Unknown title" : state.title,
                   {x, kPad, right, kPad + 21}, g_pal.text);
        DrawString(rt, g_fonts.bodyEllipsis.get(), state.artist,
                   {x, kPad + 21, right, kPad + 40}, g_pal.text2);

        const wchar_t* glyphs[] = {L"", state.playing ? L"" : L"",
                                   L""};
        for (int id : {kPrev, kToggle, kNext}) {
            D2D1_RECT_F r = ButtonRect(id);
            bool enabled = Enabled(id);
            float h = hover[id] * (pressed == id ? 1.6f : 1.0f);
            if (h > 0) {
                Fill(rt, r, 17, WithAlpha(g_pal.hover, h));
            }
            DrawString(rt, (id == kToggle ? g_fonts.iconBig : g_fonts.icon).get(),
                       glyphs[id], r, enabled ? g_pal.text : g_pal.text3);
        }

        if (HasTimeline()) {
            int64_t pos = Position();
            float f = (float)((double)(pos - state.start) / (state.end - state.start));
            D2D1_RECT_F bar = BarRect();
            float radius = (bar.bottom - bar.top) / 2;
            Fill(rt, bar, radius, g_pal.track);
            D2D1_RECT_F done = bar;
            done.right = bar.left + (bar.right - bar.left) * std::clamp(f, 0.0f, 1.0f);
            if (done.right - done.left >= radius * 2) {
                Fill(rt, done, radius, g_pal.accent);
            }

            D2D1_RECT_F times = ButtonRect(kToggle);
            times.left = ButtonRect(kNext).right + 4;
            times.right = right;
            DrawString(rt, g_fonts.smallRight.get(),
                       FormatTime(pos - state.start) + L" / " +
                           FormatTime(state.end - state.start),
                       times, g_pal.text2);
        }
    }
};

// =============================================================================
// Terminal model: just enough of a VT parser for CLI output
// =============================================================================

namespace term {

constexpr uint8_t kBold = 1, kDim = 2;

struct Attr {
    // 0 = default, 1..16 = ANSI palette index + 1 (resolved when drawn, so a
    // theme switch recolours it), otherwise 0xFFRRGGBB.
    uint32_t fg = 0;
    uint8_t flags = 0;
    bool operator==(const Attr&) const = default;
};

struct Line {
    std::wstring text;
    std::vector<Attr> attrs;  // one per character of `text`
};

uint32_t Rgb(int r, int g, int b) {
    return 0xFF000000u | (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b;
}

// Windows Terminal's Campbell scheme, which Windows CLIs pick their colours
// against, with the entries that vanish on the card's background swapped for
// readable ones.
uint32_t Ansi16(int i, bool light) {
    static const uint32_t campbell[16] = {
        0x0C0C0C, 0xC50F1F, 0x13A10E, 0xC19C00, 0x0037DA, 0x881798,
        0x3A96DD, 0xCCCCCC, 0x767676, 0xE74856, 0x16C60C, 0xF9F1A5,
        0x3B78FF, 0xB4009E, 0x61D6D6, 0xF2F2F2};
    uint32_t c = campbell[i & 15];
    if (light) {
        if (i == 7) c = 0x505050;
        if (i == 15) c = 0x202020;
        if (i == 11) c = 0x9A7B00;
    } else {
        if (i == 0) c = 0x767676;
        if (i == 4) c = 0x3B78FF;
    }
    return 0xFF000000u | c;
}

uint32_t Ansi256(int n) {
    if (n < 16) {
        return (uint32_t)n + 1;
    }
    if (n < 232) {
        static const int level[] = {0, 95, 135, 175, 215, 255};
        n -= 16;
        return Rgb(level[n / 36], level[n / 6 % 6], level[n % 6]);
    }
    int v = 8 + (n - 232) * 10;
    return Rgb(v, v, v);
}

class Screen {
   public:
    static constexpr size_t kMaxLines = 2000;
    static constexpr size_t kMaxColumns = 4096;

    std::deque<Line> lines{Line{}};
    // ESC[?25l / ESC[?25h. A program that hides the cursor isn't reading
    // input, so the box doesn't offer to type into it.
    bool cursorVisible = true;

    void Clear() {
        lines.assign(1, Line{});
        row = col = 0;
    }

    // Lines worth showing: a trailing empty line (output ending in a newline)
    // isn't.
    int Count() const {
        int n = (int)lines.size();
        return n > 1 && lines.back().text.empty() ? n - 1 : n;
    }

    bool Empty() const { return lines.size() == 1 && lines[0].text.empty(); }

    void Feed(const char* data, size_t size) {
        pending.append(data, size);
        // Hold back a UTF-8 sequence cut off at the end of the chunk.
        size_t n = pending.size(), cut = n;
        for (size_t i = n; i > 0 && n - i < 4; i--) {
            unsigned char c = pending[i - 1];
            if ((c & 0xC0) == 0x80) {
                continue;
            }
            size_t need = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
            if (n - (i - 1) < need) {
                cut = i - 1;
            }
            break;
        }
        if (cut == 0) {
            return;
        }
        int wide = MultiByteToWideChar(CP_UTF8, 0, pending.data(), (int)cut, nullptr, 0);
        std::wstring text(wide, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, pending.data(), (int)cut, text.data(), wide);
        pending.erase(0, cut);
        for (wchar_t c : text) {
            Step(c);
        }
    }

   private:
    enum class State { Text, Esc, EscSkip, Csi, Osc, OscEsc };

    State state = State::Text;
    std::string pending;  // an incomplete UTF-8 sequence
    std::string csi;
    size_t row = 0, col = 0;
    Attr attr;

    Line& Current() {
        while (row >= lines.size()) {
            lines.push_back(Line{});
        }
        return lines[row];
    }

    void Trim() {
        while (lines.size() > kMaxLines) {
            lines.pop_front();
            row = row ? row - 1 : 0;
        }
    }

    void Put(wchar_t c) {
        Line& line = Current();
        if (col >= kMaxColumns) {
            return;
        }
        if (col > line.text.size()) {
            line.attrs.resize(col);
            line.text.resize(col, L' ');
        }
        if (col < line.text.size()) {
            line.text[col] = c;
            line.attrs[col] = attr;
        } else {
            line.text.push_back(c);
            line.attrs.push_back(attr);
        }
        col++;
    }

    void Truncate(Line& line, size_t at) {
        if (at < line.text.size()) {
            line.text.resize(at);
            line.attrs.resize(at);
        }
    }

    void Step(wchar_t c) {
        switch (state) {
            case State::Text:
                switch (c) {
                    case 0x1B:
                        state = State::Esc;
                        break;
                    case L'\n':
                        // Services write bare LFs; treat them as CR LF.
                        row++;
                        col = 0;
                        Current();
                        Trim();
                        break;
                    case L'\r':
                        col = 0;
                        break;
                    case L'\t':
                        do {
                            Put(L' ');
                        } while (col % 8 && col < kMaxColumns);
                        break;
                    case L'\b':
                        col = col ? col - 1 : 0;
                        break;
                    case L'\f':
                        Clear();
                        break;
                    default:
                        if (c >= 0x20 && c != 0x7F) {
                            Put(c);
                        }
                }
                break;

            case State::Esc:
                state = State::Text;
                if (c == L'[') {
                    csi.clear();
                    state = State::Csi;
                } else if (c == L']') {
                    state = State::Osc;
                } else if (c == L'(' || c == L')') {
                    state = State::EscSkip;  // charset designation
                } else if (c == L'c') {
                    Clear();
                    attr = {};
                    cursorVisible = true;
                }
                break;

            case State::EscSkip:
                state = State::Text;
                break;

            case State::Csi:
                if (c >= 0x40 && c <= 0x7E) {
                    state = State::Text;
                    Command((char)c);
                } else if (csi.size() < 64) {
                    csi.push_back((char)c);
                }
                break;

            case State::Osc:  // window titles and the like: skipped
                if (c == 0x07) {
                    state = State::Text;
                } else if (c == 0x1B) {
                    state = State::OscEsc;
                }
                break;

            case State::OscEsc:
                state = c == L'\\' ? State::Text : State::Osc;
                break;
        }
    }

    void Command(char final) {
        if (csi == "?25" && (final == 'l' || final == 'h')) {
            cursorVisible = final == 'h';
            return;
        }
        if (!csi.empty() && (csi[0] == '?' || csi[0] == '>' || csi[0] == '=')) {
            return;  // other private modes
        }
        std::vector<int> p;
        int value = -1;
        for (char ch : csi) {
            if (ch >= '0' && ch <= '9') {
                value = (value < 0 ? 0 : value) * 10 + (ch - '0');
                value = std::min(value, 99999);
            } else if (ch == ';' || ch == ':') {
                p.push_back(value);
                value = -1;
            }
        }
        p.push_back(value);
        auto arg = [&](size_t i, int fallback) {
            return i < p.size() && p[i] >= 0 ? p[i] : fallback;
        };
        int n = std::max(1, arg(0, 1));

        switch (final) {
            case 'm':
                Sgr(p);
                break;
            case 'J':
                if (arg(0, 0) >= 2) {
                    Clear();
                } else if (arg(0, 0) == 0) {
                    Truncate(Current(), col);
                    lines.resize(row + 1);
                }
                break;
            case 'K': {
                Line& line = Current();
                int mode = arg(0, 0);
                if (mode == 0) {
                    Truncate(line, col);
                } else if (mode == 2) {
                    line = Line{};
                } else {
                    for (size_t i = 0; i < std::min(col + 1, line.text.size()); i++) {
                        line.text[i] = L' ';
                        line.attrs[i] = {};
                    }
                }
                break;
            }
            case 'H':
            case 'f':
                row = (size_t)std::max(1, arg(0, 1)) - 1;
                col = (size_t)std::max(1, arg(1, 1)) - 1;
                Current();
                Trim();
                break;
            case 'A':
                row = row > (size_t)n ? row - n : 0;
                break;
            case 'B':
                row += n;
                Current();
                Trim();
                break;
            case 'C':
                col = std::min(col + n, kMaxColumns);
                break;
            case 'D':
                col = col > (size_t)n ? col - n : 0;
                break;
            case 'G':
                col = (size_t)n - 1;
                break;
            case 'd':
                row = (size_t)n - 1;
                Current();
                Trim();
                break;
        }
    }

    void Sgr(const std::vector<int>& p) {
        for (size_t i = 0; i < p.size(); i++) {
            int v = p[i] < 0 ? 0 : p[i];
            auto at = [&](size_t k) { return k < p.size() && p[k] >= 0 ? p[k] : 0; };
            if (v == 0) {
                attr = {};
            } else if (v == 1) {
                attr.flags |= kBold;
            } else if (v == 2) {
                attr.flags |= kDim;
            } else if (v == 22) {
                attr.flags &= ~(kBold | kDim);
            } else if (v >= 30 && v <= 37) {
                attr.fg = (uint32_t)(v - 30) + 1;
            } else if (v >= 90 && v <= 97) {
                attr.fg = (uint32_t)(v - 90 + 8) + 1;
            } else if (v == 39) {
                attr.fg = 0;
            } else if (v == 38 || v == 48) {
                // Extended colours. Backgrounds aren't drawn, but their
                // arguments still have to be stepped over.
                uint32_t color = 0;
                if (at(i + 1) == 5) {
                    color = Ansi256(std::clamp(at(i + 2), 0, 255));
                    i += 2;
                } else if (at(i + 1) == 2) {
                    color = Rgb(std::clamp(at(i + 2), 0, 255), std::clamp(at(i + 3), 0, 255),
                                std::clamp(at(i + 4), 0, 255));
                    i += 4;
                }
                if (v == 38 && color) {
                    attr.fg = color;
                }
            }
        }
    }
};

}  // namespace term

// =============================================================================
// Output box: shows what a service on a localhost port writes, sends it input
// =============================================================================

struct OutputWidget;
OutputWidget* g_inputOwner = nullptr;
void OpenInput(OutputWidget* widget);
void CloseInput();

struct OutputWidget : Widget {
    static constexpr float kPad = 12;
    enum { kContent };
    enum class Conn { Idle, Connecting, Connected };

    OutputBoxConfig cfg;
    term::Screen screen;

    SOCKET sock = INVALID_SOCKET;
    Conn conn = Conn::Idle;
    bool v6 = false, triedOther = false;
    bool live = false;  // the last connection attempt got an answer
    int failures = 0;
    bool clearOnData = false;
    std::string outbox;  // UTF-8, waiting to be sent
    int sentCols = 0, sentRows = 0;

    int scroll = 0;  // lines scrolled up from the bottom
    int wheelAccum = 0;
    bool repaintPending = false;

    // Typing: the line being edited is drawn inline after the output, the way
    // a terminal echoes it, and only sent on Enter.
    bool typing = false;
    std::wstring input;
    size_t caret = 0;
    bool caretOn = true;
    std::vector<std::wstring> history;
    int historyIndex = 0;
    std::wstring draft;

    std::map<uint32_t, com_ptr<ID2D1SolidColorBrush>> brushes;
    int brushGeneration = -1;

    explicit OutputWidget(OutputBoxConfig config) : cfg(std::move(config)) {
        key = L"Output" + std::to_wstring(cfg.port);
        cardW = 380;
        cardH = 240;
        hover.assign(1, 0);
        int w = Wh_GetIntValue((key + L"W").c_str(), 0);
        int h = Wh_GetIntValue((key + L"H").c_str(), 0);
        if (w > 0 && h > 0) {
            cardW = std::clamp((float)w, MinWidth(), 4000.0f);
            cardH = std::clamp((float)h, MinHeight(), 4000.0f);
        }
    }

    ~OutputWidget() override {
        if (g_inputOwner == this) {
            g_inputOwner = nullptr;
            CloseInput();
        }
        CloseSocket();
    }

    bool Resizable() override { return true; }
    float MinWidth() override { return 160; }
    float MinHeight() override { return 60; }
    PCWSTR Cursor(int) override { return IDC_IBEAM; }
    bool WantsVisible() override { return live || typing || !cfg.hideOffline; }

    D2D1_RECT_F TextRect() const { return {kPad, kPad, cardW - kPad, cardH - kPad}; }
    float LineHeight() const { return std::ceil(g_settings.outputFontSize * 1.34f); }
    int VisibleRows() const {
        D2D1_RECT_F r = TextRect();
        return std::max(1, (int)((r.bottom - r.top) / LineHeight()));
    }
    int Columns() {
        float cell = TextWidth(L"0");
        D2D1_RECT_F r = TextRect();
        return cell > 0 ? std::max(1, (int)((r.right - r.left) / cell)) : 80;
    }
    // While typing, the last line is where the input goes, empty or not.
    int LineCount() const { return typing ? (int)screen.lines.size() : screen.Count(); }
    int MaxScroll() const { return std::max(0, LineCount() - VisibleRows()); }

    // While something answers, the whole box is the place to click to type;
    // pressing and moving still drags it (see ClickOrDrag).
    int HitTest(D2D1_POINT_2F p) override {
        return live && screen.cursorVisible && Contains(D2D1::RectF(0, 0, cardW, cardH), p)
                   ? kContent
                   : -1;
    }
    bool ClickOrDrag(int) override { return true; }
    void Click(int, D2D1_POINT_2F) override { OpenInput(this); }

    void Wheel(int delta) override {
        // One line per 40 units, so precision touchpads scroll smoothly too.
        wheelAccum += delta;
        int lines = wheelAccum / 40;
        wheelAccum %= 40;
        ScrollBy(lines);
    }

    void ScrollBy(int lines) {
        int value = std::clamp(scroll + lines, 0, MaxScroll());
        if (value != scroll) {
            scroll = value;
            Render();
        }
    }

    void AddMenuItems(HMENU menu) override {
        AppendMenuW(menu, MF_STRING, 100, L"Reconnect");
        AppendMenuW(menu, MF_STRING, 101, L"Clear");
    }

    void MenuCommand(int cmd) override {
        if (cmd == 100) {
            CloseSocket();
            failures = 0;
            Connect();
        } else if (cmd == 101) {
            screen.Clear();
            scroll = 0;
            Render();
        }
    }

    void OnCreated() override {
        UpdateVisibility();
        Connect();
    }

    // --- Connection ---------------------------------------------------------

    void CloseSocket() {
        if (sock != INVALID_SOCKET) {
            closesocket(sock);
            sock = INVALID_SOCKET;
        }
        conn = Conn::Idle;
    }

    void ScheduleConnect(UINT ms) {
        if (hwnd) {
            SetTimer(hwnd, kTimerConnect, ms, nullptr);
        }
    }

    void Connect() {
        if (conn != Conn::Idle || !hwnd) {
            return;
        }
        KillTimer(hwnd, kTimerConnect);
        sock = socket(v6 ? AF_INET6 : AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) {
            ScheduleConnect(3000);
            return;
        }
        // Also makes the socket non-blocking.
        WSAAsyncSelect(sock, hwnd, WM_APP_SOCKET,
                       FD_CONNECT | FD_READ | FD_WRITE | FD_CLOSE);
        conn = Conn::Connecting;
        int result;
        if (v6) {
            sockaddr_in6 addr{};
            addr.sin6_family = AF_INET6;
            addr.sin6_port = htons((u_short)cfg.port);
            addr.sin6_addr = in6addr_loopback;
            result = connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        } else {
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons((u_short)cfg.port);
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            result = connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        }
        if (result == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) {
            OnConnect(WSAGetLastError());
        }
    }

    void OnConnect(int error) {
        if (error) {
            CloseSocket();
            // localhost may be served on either family; try the other before
            // calling the service gone. Whichever answers is kept.
            if (!triedOther) {
                triedOther = true;
                v6 = !v6;
                Connect();
                return;
            }
            triedOther = false;
            outbox.clear();
            // One miss is allowed, so a service restarting doesn't make the
            // box blink to its placeholder and back.
            if (++failures >= 2 && live) {
                live = false;
                // An offline box is only a placeholder: nothing to type into,
                // and the old output goes with it.
                if (g_inputOwner == this) {
                    CloseInput();
                }
                screen.Clear();
                scroll = 0;
                UpdateVisibility();
                Render();
            }
            ScheduleConnect(3000);
            return;
        }
        triedOther = false;
        failures = 0;
        conn = Conn::Connected;
        clearOnData = true;
        // Every connection opens with the box's size: it tells services how
        // much fits, and it's how they know a box is calling and not, say, a
        // web page posting to localhost (see scripts/boxlib.py).
        outbox.insert(0, SizeLine());
        if (!live) {
            live = true;
            UpdateVisibility();
        }
        RepaintSoon();
        Flush();
    }

    void Flush() {
        while (!outbox.empty() && conn == Conn::Connected) {
            int n = send(sock, outbox.data(), (int)std::min<size_t>(outbox.size(), 65536), 0);
            if (n == SOCKET_ERROR) {
                if (WSAGetLastError() != WSAEWOULDBLOCK) {
                    OnClosed();
                }
                return;  // FD_WRITE resumes
            }
            outbox.erase(0, (size_t)n);
        }
    }

    void Receive() {
        int before = LineCount();
        char buffer[16384];
        // Bounded, so a firehose can't starve the UI thread; FD_READ is posted
        // again while data remains.
        for (int i = 0; i < 64 && sock != INVALID_SOCKET; i++) {
            int n = recv(sock, buffer, sizeof(buffer), 0);
            if (n <= 0) {
                break;  // would block, or closed (FD_CLOSE follows)
            }
            if (clearOnData) {
                // Each connection's output replaces the last: a snapshot
                // service answers every connection afresh.
                clearOnData = false;
                screen.Clear();
                screen.cursorVisible = true;  // a new connection's program
                before = 1;
            }
            screen.Feed(buffer, (size_t)n);
        }
        if (!screen.cursorVisible && g_inputOwner == this) {
            CloseInput();
        }
        // Scrolled back, the view stays on the same lines as output arrives.
        if (scroll > 0) {
            scroll += LineCount() - before;
        }
        scroll = std::clamp(scroll, 0, MaxScroll());
        RepaintSoon();
    }

    void OnClosed() {
        if (sock != INVALID_SOCKET) {
            Receive();
        }
        CloseSocket();
        RepaintSoon();
        if (!outbox.empty()) {
            Connect();  // input reached a connection that was already closing
        } else if (cfg.refresh > 0) {
            ScheduleConnect((UINT)cfg.refresh * 1000);
        }
    }

    static std::string ToUtf8(const std::wstring& text) {
        int n = WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), nullptr,
                                    0, nullptr, nullptr);
        std::string utf8(n, '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), utf8.data(), n,
                            nullptr, nullptr);
        return utf8;
    }

    // ESC "size COLS ROWS": lines from the box itself start with ESC, which
    // nothing typed can.
    std::string SizeLine() {
        sentCols = Columns();
        sentRows = VisibleRows();
        return "\x1bsize " + std::to_string(sentCols) + " " + std::to_string(sentRows) + "\n";
    }

    void OnResized() override {
        if (conn != Conn::Connected) {
            return;  // the next connection opens with the new size
        }
        if (Columns() != sentCols || VisibleRows() != sentRows) {
            outbox += SizeLine();
            Flush();
        }
    }

    void SendLine(const std::wstring& text) {
        outbox += ToUtf8(text);
        outbox += '\n';
        if (conn == Conn::Connected) {
            Flush();
        } else if (conn == Conn::Idle) {
            Connect();
        }
    }

    void RepaintSoon() {
        if (hwnd && !repaintPending) {
            repaintPending = true;
            SetTimer(hwnd, kTimerRepaint, 16, nullptr);
        }
    }

    // --- Typing -------------------------------------------------------------

    void BeginTyping() {
        typing = true;
        historyIndex = (int)history.size();
        draft.clear();
        RestartCaret();
        scroll = 0;
        UpdateVisibility();
        Render();
    }

    void EndTyping() {
        typing = false;
        if (hwnd) {
            KillTimer(hwnd, kTimerCaret);
        }
        scroll = std::clamp(scroll, 0, MaxScroll());
        UpdateVisibility();
        Render();
    }

    void RestartCaret() {
        caretOn = true;
        if (hwnd) {
            SetTimer(hwnd, kTimerCaret, 530, nullptr);
        }
    }

    void Edited() {
        scroll = 0;  // typing snaps back to the bottom, as in a terminal
        RestartCaret();
        Render();
    }

    void Submit() {
        std::wstring text = std::move(input);
        input.clear();
        caret = 0;
        // Programs reading a pipe don't echo, so the line stays where it was
        // typed, followed by the newline Enter stands for.
        std::string echo = ToUtf8(text) + "\n";
        screen.Feed(echo.data(), echo.size());
        // An empty line is sent too: plenty of programs wait for Enter.
        SendLine(text);
        if (!text.empty() && (history.empty() || history.back() != text)) {
            history.push_back(text);
            if (history.size() > 100) {
                history.erase(history.begin());
            }
        }
        historyIndex = (int)history.size();
        draft.clear();
        Edited();
    }

    void Recall(int direction) {
        int n = (int)history.size();
        if (historyIndex == n) {
            draft = input;
        }
        int index = std::clamp(historyIndex + direction, 0, n);
        if (index == historyIndex) {
            return;
        }
        historyIndex = index;
        input = index == n ? draft : history[index];
        caret = input.size();
        Edited();
    }

    // Every complete line of a multi-line paste is entered; the unfinished
    // last one stays typed.
    void Paste(HWND clipboardOwner) {
        std::wstring clip;
        if (OpenClipboard(clipboardOwner)) {
            if (HANDLE data = GetClipboardData(CF_UNICODETEXT)) {
                if (auto* p = static_cast<PCWSTR>(GlobalLock(data))) {
                    clip = p;
                    GlobalUnlock(data);
                }
            }
            CloseClipboard();
        }
        for (size_t i = 0; i < clip.size(); i++) {
            wchar_t c = clip[i];
            if (c == L'\r' || c == L'\n') {
                if (c == L'\r' && i + 1 < clip.size() && clip[i + 1] == L'\n') {
                    i++;
                }
                Submit();
            } else if (c == L'\t' || c >= 0x20) {
                input.insert(caret++, 1, c == L'\t' ? L' ' : c);
            }
        }
        Edited();
    }

    void OnKey(WPARAM vk, HWND focusWindow) {
        bool ctrl = GetKeyState(VK_CONTROL) < 0;
        switch (vk) {
            case VK_RETURN:
                Submit();
                break;
            case VK_ESCAPE:
                CloseInput();
                break;
            case VK_BACK:
                if (caret > 0) {
                    size_t from = caret - 1;
                    if (ctrl) {
                        while (from > 0 && iswspace(input[from])) from--;
                        while (from > 0 && !iswspace(input[from - 1])) from--;
                    }
                    input.erase(from, caret - from);
                    caret = from;
                    Edited();
                }
                break;
            case VK_DELETE:
                if (caret < input.size()) {
                    input.erase(caret, 1);
                    Edited();
                }
                break;
            case VK_LEFT:
                if (caret > 0) {
                    caret--;
                    Edited();
                }
                break;
            case VK_RIGHT:
                if (caret < input.size()) {
                    caret++;
                    Edited();
                }
                break;
            case VK_HOME:
                caret = 0;
                Edited();
                break;
            case VK_END:
                caret = input.size();
                Edited();
                break;
            case VK_UP:
            case VK_DOWN:
                Recall(vk == VK_UP ? -1 : 1);
                break;
            case VK_PRIOR:
            case VK_NEXT: {
                int page = std::max(1, VisibleRows() - 1);
                ScrollBy(vk == VK_PRIOR ? page : -page);
                break;
            }
            case 'V':
                if (ctrl) {
                    Paste(focusWindow);
                }
                break;
        }
    }

    void OnChar(wchar_t c) {
        // Control characters arrive here too (Backspace as 8, Enter as 13,
        // Ctrl+V as 22); those are handled as keys.
        if (c < 0x20 || c == 0x7F) {
            return;
        }
        input.insert(caret++, 1, c);
        Edited();
    }

    bool OnMessage(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT& result) override {
        result = 0;
        if (msg == WM_APP_SOCKET) {
            if ((SOCKET)wParam != sock) {
                return true;  // left over from a socket already closed
            }
            switch (WSAGETSELECTEVENT(lParam)) {
                case FD_CONNECT:
                    OnConnect(WSAGETSELECTERROR(lParam));
                    break;
                case FD_READ:
                    Receive();
                    break;
                case FD_WRITE:
                    Flush();
                    break;
                case FD_CLOSE:
                    OnClosed();
                    break;
            }
            return true;
        }
        if (msg == WM_TIMER && wParam == kTimerConnect) {
            KillTimer(hwnd, kTimerConnect);
            Connect();
            return true;
        }
        if (msg == WM_TIMER && wParam == kTimerRepaint) {
            KillTimer(hwnd, kTimerRepaint);
            repaintPending = false;
            Render();
            return true;
        }
        if (msg == WM_TIMER && wParam == kTimerCaret) {
            caretOn = !caretOn;
            Render();
            return true;
        }
        return false;
    }

    // --- Drawing ------------------------------------------------------------

    ID2D1Brush* Brush(ID2D1RenderTarget* rt, D2D1_COLOR_F c) {
        if (brushGeneration != g_rtGeneration) {
            brushes.clear();
            brushGeneration = g_rtGeneration;
        }
        uint32_t k = (uint32_t)std::lround(c.a * 255) << 24 |
                     (uint32_t)std::lround(c.r * 255) << 16 |
                     (uint32_t)std::lround(c.g * 255) << 8 | (uint32_t)std::lround(c.b * 255);
        auto& brush = brushes[k];
        if (!brush) {
            rt->CreateSolidColorBrush(c, brush.put());
        }
        return brush.get();
    }

    D2D1_COLOR_F ColorOf(const term::Attr& a) {
        D2D1_COLOR_F c = g_pal.text;
        if (a.fg) {
            uint32_t v = a.fg <= 16 ? term::Ansi16((int)a.fg - 1, IsLightPalette()) : a.fg;
            c = Rgba((v >> 16 & 255) / 255.0f, (v >> 8 & 255) / 255.0f, (v & 255) / 255.0f, 1);
        }
        if (a.flags & term::kDim) {
            c.a *= 0.6f;
        }
        return c;
    }

    // Width of a run of monospace text, trailing spaces included.
    float TextWidth(const std::wstring& text) {
        if (text.empty() || !g_fonts.mono) {
            return 0;
        }
        com_ptr<IDWriteTextLayout> layout;
        DWRITE_TEXT_METRICS metrics{};
        if (FAILED(g_dw->CreateTextLayout(text.c_str(), (UINT32)text.size(),
                                          g_fonts.mono.get(), 100000, 100,
                                          layout.put())) ||
            FAILED(layout->GetMetrics(&metrics))) {
            return 0;
        }
        return metrics.widthIncludingTrailingWhitespace;
    }

    void DrawLine(ID2D1RenderTarget* rt, const term::Line& line, D2D1_POINT_2F at,
                  float width, float height) {
        if (line.text.empty() || !g_fonts.mono) {
            return;
        }
        com_ptr<IDWriteTextLayout> layout;
        if (FAILED(g_dw->CreateTextLayout(line.text.c_str(), (UINT32)line.text.size(),
                                          g_fonts.mono.get(), width, height,
                                          layout.put()))) {
            return;
        }
        size_t start = 0;
        for (size_t i = 1; i <= line.text.size(); i++) {
            if (i < line.text.size() && line.attrs[i] == line.attrs[start]) {
                continue;
            }
            const term::Attr& a = line.attrs[start];
            DWRITE_TEXT_RANGE range{(UINT32)start, (UINT32)(i - start)};
            if (a.fg || (a.flags & term::kDim)) {
                layout->SetDrawingEffect(Brush(rt, ColorOf(a)), range);
            }
            if (a.flags & term::kBold) {
                layout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, range);
            }
            start = i;
        }
        g_brush->SetColor(g_pal.text);
        rt->DrawTextLayout(at, layout.get(), g_brush.get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    void Paint(ID2D1RenderTarget* rt) override {
        // Nothing but the program's output. Offline, a placeholder that says
        // so; live and silent, an empty box.
        if (!live) {
            DrawString(rt, g_fonts.hint.get(),
                       L"Nothing listening on port " + std::to_wstring(cfg.port),
                       D2D1::RectF(kPad, 0, cardW - kPad, cardH), g_pal.text3);
            return;
        }

        D2D1_RECT_F text = TextRect();
        float lh = LineHeight();
        float width = text.right - text.left;
        int count = LineCount(), rows = VisibleRows();
        scroll = std::clamp(scroll, 0, MaxScroll());
        int first = std::max(0, count - rows - scroll);
        int last = std::min(count, first + rows);
        int inputRow = typing ? (int)screen.lines.size() - 1 : -1;

        rt->PushAxisAlignedClip(text, D2D1_ANTIALIAS_MODE_ALIASED);
        for (int i = first; i < last; i++) {
            const term::Line& line = screen.lines[i];
            float y = text.top + (i - first) * lh;
            if (i != inputRow) {
                DrawLine(rt, line, {text.left, y}, width, lh);
                continue;
            }
            // The input follows whatever the program left on its last line
            // (a prompt, usually), shifted left when it would run past the
            // edge so the caret stays in view.
            float prompt = TextWidth(line.text);
            float caretX = prompt + TextWidth(input.substr(0, caret));
            float shift = std::max(0.0f, caretX - (width - 4));
            DrawLine(rt, line, {text.left - shift, y}, width + shift, lh);
            if (!input.empty() && g_fonts.mono) {
                com_ptr<IDWriteTextLayout> layout;
                if (SUCCEEDED(g_dw->CreateTextLayout(input.c_str(), (UINT32)input.size(),
                                                     g_fonts.mono.get(), 100000, lh,
                                                     layout.put()))) {
                    g_brush->SetColor(g_pal.text);
                    rt->DrawTextLayout({text.left + prompt - shift, y}, layout.get(),
                                       g_brush.get());
                }
            }
            if (caretOn) {
                float x = text.left + caretX - shift;
                Fill(rt, {x, y + 1, x + 1.5f, y + lh - 1}, 0, g_pal.accent);
            }
        }
        rt->PopAxisAlignedClip();

        // A thin scroll thumb, only while scrolled back.
        int max = MaxScroll();
        if (scroll > 0 && max > 0) {
            float trackH = text.bottom - text.top;
            float thumbH = std::max(16.0f, trackH * rows / count);
            float y = text.top + (trackH - thumbH) * (1 - (float)scroll / max);
            Fill(rt, {cardW - 6, y, cardW - 3, y + thumbH}, 1.5f, g_pal.track);
        }
    }
};

// =============================================================================
// Output box keyboard input
// =============================================================================

// The cards never activate, so they can't take keyboard focus. While typing,
// the keys go to this window instead: one invisible pixel at the caret, which
// takes the foreground on the click that started typing (explorer may hand it
// over then) and hands every key to the box, which draws the line itself.
// Clicking the box again keeps it -- the box doesn't activate -- and anything
// else being activated ends typing. Not owned by the desktop window, since
// activating an owned window would raise the desktop with it.
HWND g_inputWnd;

LRESULT CALLBACK InputWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE) {
                PostMessageW(hwnd, WM_CLOSE, 0, 0);
            }
            return 0;

        case WM_KEYDOWN:
            if (g_inputOwner) {
                g_inputOwner->OnKey(wParam, hwnd);
            }
            return 0;

        case WM_CHAR:
            if (g_inputOwner) {
                g_inputOwner->OnChar((wchar_t)wParam);
            }
            return 0;

        case WM_MOUSEWHEEL:
            if (g_inputOwner) {
                g_inputOwner->Wheel(GET_WHEEL_DELTA_WPARAM(wParam));
            }
            return 0;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            if (OutputWidget* owner = g_inputOwner) {
                g_inputOwner = nullptr;
                owner->EndTyping();
            }
            g_inputWnd = nullptr;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void CloseInput() {
    if (g_inputWnd) {
        DestroyWindow(g_inputWnd);  // WM_DESTROY does the rest
    }
}

void OpenInput(OutputWidget* widget) {
    if (g_inputOwner == widget && g_inputWnd) {
        return;  // already typing here
    }
    CloseInput();
    if (!widget->hwnd) {
        return;
    }

    RECT window;
    GetWindowRect(widget->hwnd, &window);
    float s = widget->Scale(), m = widget->Margin();
    g_inputWnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_LAYERED, kInputClass, L"", WS_POPUP,
        window.left + (int)std::lround((m + OutputWidget::kPad) * s),
        window.bottom - (int)std::lround((m + OutputWidget::kPad) * s), 1, 1, nullptr,
        nullptr, g_hinst, nullptr);
    if (!g_inputWnd) {
        return;
    }
    // Alpha 1, not 0: a fully transparent window can't be activated.
    SetLayeredWindowAttributes(g_inputWnd, 0, 1, LWA_ALPHA);

    g_inputOwner = widget;
    widget->BeginTyping();
    ShowWindow(g_inputWnd, SW_SHOW);
    if (!SetForegroundWindow(g_inputWnd)) {
        // Invisible, so it couldn't be clicked into instead: rather than show
        // a caret that goes nowhere, don't start typing.
        Wh_Log(L"Input window didn't get the foreground");
        CloseInput();
        return;
    }
    SetFocus(g_inputWnd);
}

// =============================================================================
// Top bar
// =============================================================================

// A glass pill along the top of the primary monitor, registered as an app bar
// so the work area -- and with it every maximized window -- starts below it.
// Empty for now; the pager and the control center move onto it later.
//
// Unlike the cards it isn't a layered window drawn with Direct2D: it is
// composed with Windows.UI.Composition, because only that can sample what is
// behind the window. The host backdrop brush is DWM's own blur of whatever
// lies under the bar; over it goes the cards' colour, at their opacity unless
// the bar has its own, and the cards' hairline border.
//
// It is not owned by the desktop window either: it must stay above windows,
// and it doesn't take part in fading behind them.

namespace topbar {

namespace wuc = winrt::Windows::UI::Composition;
using winrt::Windows::Foundation::Numerics::float2;
using winrt::Windows::Foundation::Numerics::float3;

// Placed like the Shell Styler's taskbar pills. The tray pill keeps 8 DIPs from
// the screen's side and 2 from its edge ($TrayPillMargin=0,2,8,2); here that
// is the top edge, with the same 2 left clear below. $CornerRadius=25 is wider
// than half of any sensible height, so the ends come out fully round.
constexpr float kSideMargin = 8;
constexpr float kEdgeMargin = 2;
constexpr float kMaxCornerRadius = 25;
constexpr float kBorder = 1;  // the cards' border is a 1 DIP hairline too

// Not declared by the toolchain's windows.ui.composition.interop.h.
// {29E691FA-4567-4DCA-B319-D0F207EB6807}
constexpr GUID IID_ICompositorDesktopInterop = {
    0x29E691FA, 0x4567, 0x4DCA, {0xB3, 0x19, 0xD0, 0xF2, 0x07, 0xEB, 0x68, 0x07}};

struct ICompositorDesktopInterop : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE CreateDesktopWindowTarget(
        HWND hwndTarget, BOOL isTopmost, void** result) = 0;  // IDesktopWindowTarget**
    virtual HRESULT STDMETHODCALLTYPE EnsureOnThread(DWORD threadId) = 0;
};

HWND g_hwnd;
bool g_registered;  // the taskbar accepted ABM_NEW
bool g_fullscreen;  // hidden for a fullscreen app
RECT g_rect;        // the rectangle the bar last settled on
UINT g_taskbarCreated;

// A compositor needs a DispatcherQueue on its thread. Created with the first
// bar and kept until the UI thread ends, so toggling the bar doesn't stack
// queues; it also makes the thread a COM STA, which nothing else here minds.
com_ptr<ABI::Windows::System::IDispatcherQueueController> g_queue;

wuc::Compositor g_compositor{nullptr};
wuc::Desktop::DesktopWindowTarget g_target{nullptr};
wuc::ContainerVisual g_root{nullptr};
wuc::ContainerVisual g_pill{nullptr};  // clipped to the pill's outline
wuc::CompositionRoundedRectangleGeometry g_pillShape{nullptr};
wuc::SpriteVisual g_backdrop{nullptr};
wuc::SpriteVisual g_tint{nullptr};
wuc::ShapeVisual g_border{nullptr};
wuc::CompositionRoundedRectangleGeometry g_borderShape{nullptr};
wuc::CompositionSpriteShape g_borderStroke{nullptr};

winrt::Windows::UI::Color Argb(uint32_t argb) {
    return {(uint8_t)(argb >> 24), (uint8_t)(argb >> 16), (uint8_t)(argb >> 8),
            (uint8_t)argb};
}

winrt::Windows::UI::Color ToColor(D2D1_COLOR_F c) {
    auto byte = [](float v) { return (uint8_t)std::lround(std::clamp(v, 0.0f, 1.0f) * 255); };
    return {byte(c.a), byte(c.r), byte(c.g), byte(c.b)};
}

bool TransparencyOn() {
    return ReadDword(L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"EnableTransparency", 1) != 0;
}

HMONITOR PrimaryMonitor() {
    return MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
}

bool EnsureQueue() {
    if (g_queue) {
        return true;
    }
    DispatcherQueueOptions options{sizeof(options), DQTYPE_THREAD_CURRENT, DQTAT_COM_STA};
    HRESULT hr = CreateDispatcherQueueController(options, g_queue.put());
    if (FAILED(hr)) {
        Wh_Log(L"CreateDispatcherQueueController failed: %08X", hr);
        return false;
    }
    return true;
}

// The cards' colours, from the palette, so the bar follows the theme with
// them. With transparency effects off -- no blur to show -- the bar is solid.
void UpdateBrushes() {
    if (!g_compositor) {
        return;
    }
    try {
        g_borderStroke.StrokeBrush(g_compositor.CreateColorBrush(ToColor(g_pal.border)));
        if (TransparencyOn()) {
            D2D1_COLOR_F tint = g_pal.bg;
            if (g_settings.topBarOpacity > 0) {
                tint.a = g_settings.topBarOpacity;
            }
            // No backdrop brush leaves the pill clear under the tint.
            g_backdrop.Brush(g_settings.topBarBlur
                                 ? wuc::CompositionBrush(g_compositor.CreateHostBackdropBrush())
                                 : nullptr);
            g_tint.Brush(g_compositor.CreateColorBrush(ToColor(tint)));
        } else {
            g_backdrop.Brush(g_compositor.CreateColorBrush(
                Argb(IsLightPalette() ? 0xFFF3F3F3 : 0xFF202020)));
            g_tint.Brush(nullptr);
        }
    } catch (...) {
        Wh_Log(L"Top bar brushes failed: %08X", winrt::to_hresult());
    }
}

// Sizes the pill to the window, in physical pixels, which is what a desktop
// window target works in.
void Layout() {
    if (!g_compositor) {
        return;
    }
    RECT client;
    GetClientRect(g_hwnd, &client);
    float s = GetDpiForWindow(g_hwnd) / 96.0f;
    float x = std::round(kSideMargin * s);
    float y = std::round(kEdgeMargin * s);
    float w = std::max(0.0f, client.right - 2 * x);
    float h = std::round(g_settings.topBarHeight * s);
    float r = std::min(std::round(kMaxCornerRadius * s), h / 2);
    float b = std::max(1.0f, std::round(kBorder * s));
    try {
        g_pill.Offset(float3{x, y, 0});
        g_pill.Size(float2{w, h});
        g_pillShape.Size(float2{w, h});
        g_pillShape.CornerRadius(float2{r, r});
        g_border.Offset(float3{x, y, 0});
        g_border.Size(float2{w, h});
        // A stroke straddles its outline; inset it so all of it lands inside.
        g_borderShape.Offset(float2{b / 2, b / 2});
        g_borderShape.Size(float2{std::max(0.0f, w - b), std::max(0.0f, h - b)});
        g_borderShape.CornerRadius(float2{r - b / 2, r - b / 2});
        g_borderStroke.StrokeThickness(b);
    } catch (...) {
        Wh_Log(L"Top bar layout failed: %08X", winrt::to_hresult());
    }
}

bool BuildVisuals() {
    try {
        g_compositor = wuc::Compositor();
        com_ptr<ICompositorDesktopInterop> interop;
        winrt::check_hresult(reinterpret_cast<::IUnknown*>(winrt::get_abi(g_compositor))
                                 ->QueryInterface(IID_ICompositorDesktopInterop,
                                                  interop.put_void()));
        winrt::check_hresult(
            interop->CreateDesktopWindowTarget(g_hwnd, TRUE, winrt::put_abi(g_target)));

        g_root = g_compositor.CreateContainerVisual();
        g_root.RelativeSizeAdjustment(float2{1, 1});

        g_pillShape = g_compositor.CreateRoundedRectangleGeometry();
        g_pill = g_compositor.CreateContainerVisual();
        g_pill.Clip(g_compositor.CreateGeometricClip(g_pillShape));
        g_backdrop = g_compositor.CreateSpriteVisual();
        g_backdrop.RelativeSizeAdjustment(float2{1, 1});
        g_tint = g_compositor.CreateSpriteVisual();
        g_tint.RelativeSizeAdjustment(float2{1, 1});
        g_pill.Children().InsertAtTop(g_backdrop);
        g_pill.Children().InsertAtTop(g_tint);

        g_borderShape = g_compositor.CreateRoundedRectangleGeometry();
        g_borderStroke = g_compositor.CreateSpriteShape(g_borderShape);
        g_border = g_compositor.CreateShapeVisual();
        g_border.Shapes().Append(g_borderStroke);

        g_root.Children().InsertAtTop(g_pill);
        g_root.Children().InsertAtTop(g_border);
        g_target.Root(g_root);
        return true;
    } catch (...) {
        Wh_Log(L"Top bar composition failed: %08X", winrt::to_hresult());
        return false;
    }
}

void ReleaseVisuals() {
    g_borderStroke = nullptr;
    g_borderShape = nullptr;
    g_border = nullptr;
    g_tint = nullptr;
    g_backdrop = nullptr;
    g_pillShape = nullptr;
    g_pill = nullptr;
    g_root = nullptr;
    g_target = nullptr;
    g_compositor = nullptr;
}

// Claims the strip along the primary monitor's top edge. With force false it
// only re-applies when the shell would now put the bar somewhere else, so
// answering ABN_POSCHANGED can't feed back into another one.
void Place(bool force) {
    HMONITOR monitor = PrimaryMonitor();
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(monitor, &mi);
    UINT dpiX = 96, dpiY = 96;
    GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
    int thickness = (int)std::lround((2 * kEdgeMargin + g_settings.topBarHeight) * dpiX / 96.0f);

    APPBARDATA abd{sizeof(abd)};
    abd.hWnd = g_hwnd;
    abd.uEdge = ABE_TOP;
    abd.rc = mi.rcMonitor;
    abd.rc.bottom = abd.rc.top + thickness;
    // The shell may move the strip down, past another top app bar; the
    // thickness is ours to keep either way.
    SHAppBarMessage(ABM_QUERYPOS, &abd);
    abd.rc.bottom = abd.rc.top + thickness;
    if (!force && EqualRect(&abd.rc, &g_rect)) {
        return;
    }
    SHAppBarMessage(ABM_SETPOS, &abd);
    g_rect = abd.rc;

    SetWindowPos(g_hwnd, HWND_TOPMOST, g_rect.left, g_rect.top,
                 g_rect.right - g_rect.left, g_rect.bottom - g_rect.top,
                 SWP_NOACTIVATE | (g_fullscreen ? 0 : SWP_SHOWWINDOW));
    Layout();
}

// Before the taskbar exists -- explorer starting up -- ABM_NEW has no one to
// answer it, so it's retried every second until it takes.
void Register() {
    if (!g_registered) {
        APPBARDATA abd{sizeof(abd)};
        abd.hWnd = g_hwnd;
        abd.uCallbackMessage = WM_APP_APPBAR;
        g_registered = SHAppBarMessage(ABM_NEW, &abd);
        if (!g_registered) {
            SetTimer(g_hwnd, kTimerTopBar, 1000, nullptr);
            return;
        }
    }
    Place(true);
}

void SetFullscreen(bool fullscreen) {
    g_fullscreen = fullscreen;
    if (fullscreen) {
        ShowWindow(g_hwnd, SW_HIDE);
    } else if (g_registered) {
        SetWindowPos(g_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
}

void Destroy() {
    if (!g_hwnd) {
        return;
    }
    KillTimer(g_hwnd, kTimerTopBar);
    // Without this the shell keeps the strip reserved, empty, until sign-out.
    if (g_registered) {
        APPBARDATA abd{sizeof(abd)};
        abd.hWnd = g_hwnd;
        SHAppBarMessage(ABM_REMOVE, &abd);
        g_registered = false;
    }
    ReleaseVisuals();
    DestroyWindow(g_hwnd);
    g_hwnd = nullptr;
    g_rect = {};
    g_fullscreen = false;
}

void Create() {
    if (!EnsureQueue()) {
        return;
    }
    g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    g_hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_NOREDIRECTIONBITMAP,
        kTopBarClass, L"", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, g_hinst, nullptr);
    if (!g_hwnd) {
        Wh_Log(L"Top bar CreateWindowEx failed: %u", GetLastError());
        return;
    }
    BOOL on = TRUE;
    // Lets the host backdrop brush see what's behind the window.
    DwmSetWindowAttribute(g_hwnd, DWMWA_USE_HOSTBACKDROPBRUSH, &on, sizeof(on));
    // Stays up during Aero Peek, like the taskbar.
    DwmSetWindowAttribute(g_hwnd, DWMWA_EXCLUDED_FROM_PEEK, &on, sizeof(on));
    // The pill draws its own round ends; DWM's corners would clip the strip.
    DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_DONOTROUND;
    DwmSetWindowAttribute(g_hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof(corners));

    if (!BuildVisuals()) {
        Destroy();
        return;
    }
    UpdateBrushes();
    Register();

    if (g_settings.animations) {
        try {
            auto fade = g_compositor.CreateScalarKeyFrameAnimation();
            fade.InsertKeyFrame(0, 0);
            fade.InsertKeyFrame(1, 1);
            fade.Duration(std::chrono::milliseconds((int)kAnimMs));
            g_root.StartAnimation(L"Opacity", fade);
        } catch (...) {
        }
    }
}

// Brings the bar in line with the settings: made, removed, or re-fitted (the
// height may have changed, or the monitors).
void Sync() {
    if (!g_settings.topBar) {
        Destroy();
        return;
    }
    if (!g_hwnd) {
        Create();
        return;
    }
    if (g_registered) {
        Place(true);
    }
    UpdateBrushes();
}

// Called when the UI thread ends: the queue goes last, after everything
// composed on it.
void Shutdown() {
    Destroy();
    g_queue = nullptr;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // A new taskbar knows nothing of the old one's app bars.
    if (g_taskbarCreated && msg == g_taskbarCreated) {
        g_registered = false;
        g_rect = {};
        Register();
        return 0;
    }

    switch (msg) {
        case WM_APP_APPBAR:
            if (wParam == ABN_POSCHANGED) {
                // Settle after the shell is done moving things, not inside
                // its notification.
                SetTimer(hwnd, kTimerTopBar, 100, nullptr);
            } else if (wParam == ABN_FULLSCREENAPP) {
                // Sent for any monitor; only one on the primary hides the bar.
                HWND fg = GetForegroundWindow();
                if (!lParam) {
                    SetFullscreen(false);
                } else if (fg && MonitorFromWindow(fg, MONITOR_DEFAULTTONULL) == PrimaryMonitor()) {
                    SetFullscreen(true);
                }
            }
            return 0;

        case WM_TIMER:
            if (wParam == kTimerTopBar) {
                KillTimer(hwnd, kTimerTopBar);
                if (g_registered) {
                    Place(false);
                } else {
                    Register();
                }
            }
            return 0;

        case WM_WINDOWPOSCHANGED:
            if (g_registered) {
                APPBARDATA abd{sizeof(abd)};
                abd.hWnd = hwnd;
                SHAppBarMessage(ABM_WINDOWPOSCHANGED, &abd);
            }
            break;

        case WM_DPICHANGED:
            // Place already sized the window for the monitor's DPI; the
            // suggested rectangle would undo that.
            Layout();
            return 0;

        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace topbar

// =============================================================================
// Widget management (UI thread)
// =============================================================================

HWND FindOwnDesktop() {
    HWND shell = GetShellWindow();
    if (!shell) {
        return nullptr;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(shell, &pid);
    return pid == GetCurrentProcessId() ? shell : nullptr;
}

void DestroyWidgets() {
    media::g_target = nullptr;
    CloseInput();
    g_widgets.clear();
}

void CreateWidgets() {
    DestroyWidgets();
    if (!g_progman) {
        return;
    }

    if (g_settings.calendar) {
        g_widgets.push_back(std::make_unique<CalendarWidget>());
    }
    if (g_settings.nowPlaying) {
        g_widgets.push_back(std::make_unique<NowPlayingWidget>());
    }
    if (g_settings.outputBoxesEnabled) {
        for (const auto& box : g_settings.outputBoxes) {
            g_widgets.push_back(std::make_unique<OutputWidget>(box));
        }
    }

    // Defaults stack down the right side of the primary work area.
    MONITORINFO primary{sizeof(primary)};
    HMONITOR primaryMonitor = MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
    GetMonitorInfoW(primaryMonitor, &primary);
    UINT primaryDpiX = 96, primaryDpiY = 96;
    GetDpiForMonitor(primaryMonitor, MDT_EFFECTIVE_DPI, &primaryDpiX, &primaryDpiY);
    float stackY = kScreenMargin;

    for (auto& w : g_widgets) {
        w->dpi = primaryDpiX;
        float s = w->Scale(), m = w->Margin();
        POINT pos{
            primary.rcWork.right - (LONG)std::lround((kScreenMargin + w->cardW + m) * s),
            primary.rcWork.top + (LONG)std::lround((stackY - m) * s)};
        stackY += w->cardH + kStackGap;

        int savedX = Wh_GetIntValue((w->key + L"X").c_str(), INT_MIN);
        int savedY = Wh_GetIntValue((w->key + L"Y").c_str(), INT_MIN);
        if (savedX != INT_MIN && savedY != INT_MIN) {
            // Only if it still lands on a screen; monitors come and go.
            HMONITOR monitor = MonitorFromPoint({savedX, savedY}, MONITOR_DEFAULTTONULL);
            if (monitor) {
                UINT dx, dy;
                GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dx, &dy);
                w->dpi = dx;
                pos = {savedX, savedY};
            }
        }

        SIZE size = w->PixelSize();
        CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                        kWidgetClass, L"", WS_POPUP, pos.x, pos.y, size.cx,
                        size.cy, g_progman, nullptr, g_hinst, w.get());
        if (!w->hwnd) {
            Wh_Log(L"CreateWindowEx failed: %u", GetLastError());
            continue;
        }
        w->dpi = GetDpiForWindow(w->hwnd);
        SetTimer(w->hwnd, kTimerTick, 1000, nullptr);

        if (w->key == L"NowPlaying") {
            media::g_target = w->hwnd;
            w->OnMedia();
        } else {
            w->UpdateVisibility();
        }
        w->OnCreated();
    }

    if (g_settings.nowPlaying) {
        media::Start();
    } else {
        media::Stop();
    }
}

void RenderAll() {
    for (auto& w : g_widgets) {
        w->Render();
    }
}

void ResetPositions() {
    for (auto& w : g_widgets) {
        Wh_DeleteValue((w->key + L"X").c_str());
        Wh_DeleteValue((w->key + L"Y").c_str());
        if (w->Resizable()) {
            Wh_DeleteValue((w->key + L"W").c_str());
            Wh_DeleteValue((w->key + L"H").c_str());
        }
    }
    // Not from inside a widget's own message handler, which is where the menu
    // that calls this runs.
    PostMessageW(g_host, WM_APP_RELOAD, 0, 0);
}

void CheckDesktop() {
    if (g_progman && IsWindow(g_progman)) {
        return;
    }
    if (g_progman) {
        // Desktop window gone; its owned widgets went with it.
        DestroyWidgets();
        g_progman = nullptr;
    }
    g_progman = FindOwnDesktop();
    if (g_progman) {
        CreateWidgets();
    }
}

void Reload() {
    LoadSettings();
    LoadPalette();
    CreateFonts();
    CreateWidgets();
    topbar::Sync();
}

// =============================================================================
// Fading behind windows
// =============================================================================

// What the foreground window means for the widgets.
enum class FocusKind {
    Desktop,  // the wallpaper or icons: show
    Ours,     // a widget, its input window or its menu: show
    Shell,    // taskbar, Start, search, Alt+Tab...: leave things as they are
    App,      // anything else: hide
};

std::wstring ClassOf(HWND hwnd) {
    wchar_t name[128] = L"";
    GetClassNameW(hwnd, name, ARRAYSIZE(name));
    return name;
}

bool IsShellClass(const std::wstring& name) {
    static const PCWSTR shell[] = {
        L"Shell_TrayWnd",
        L"Shell_SecondaryTrayWnd",
        L"Windows.UI.Core.CoreWindow",    // Start, search, notification center
        L"XamlExplorerHostIslandWindow",  // Alt+Tab, Task View, snap layouts
        L"MultitaskingViewFrame",
        L"ForegroundStaging",
        L"TaskListThumbnailWnd",
        L"NotifyIconOverflowWindow",
        L"TopLevelWindowForOverflowXamlIsland",
        L"#32768",  // menus
    };
    for (PCWSTR c : shell) {
        if (name == c) {
            return true;
        }
    }
    return false;
}

bool IsOwnClass(const std::wstring& name) {
    return name == kWidgetClass || name == kInputClass || name == kHostClass ||
           name == kTopBarClass;
}

// The window whose minimize just started: it counts as gone already, so the
// fade can start with the genie rather than after the window is down.
HWND g_minimizing;

FocusKind ClassifyForeground(HWND hwnd) {
    if (!hwnd) {
        return FocusKind::Shell;
    }
    std::wstring name = ClassOf(hwnd);
    if (name == L"Progman" || name == L"WorkerW") {
        return FocusKind::Desktop;
    }
    if (IsOwnClass(name)) {
        return FocusKind::Ours;
    }
    // A minimized window -- or one on its way down -- can keep the foreground
    // for a moment; it isn't in the way. Nor is an invisible one, which some
    // apps' helper windows get when everything else is minimized.
    if (IsShellClass(name) || IsIconic(hwnd) || hwnd == g_minimizing ||
        !IsWindowVisible(hwnd)) {
        return FocusKind::Shell;
    }
    // Cloaked or not: the genie keeps a restoring window cloaked for its whole
    // rise, and the fade has to start with it, not after.
    return FocusKind::App;
}

// A window someone would call open on the screen: visible, not minimized,
// not on another virtual desktop (those are cloaked), and an ordinary
// top-level app window rather than a tool window, overlay or shell surface.
bool IsOpenAppWindow(HWND hwnd) {
    if (hwnd == g_minimizing || !IsWindowVisible(hwnd) || IsIconic(hwnd) || GetWindow(hwnd, GW_OWNER)) {
        return false;
    }
    LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if ((ex & WS_EX_TOOLWINDOW) || (ex & WS_EX_NOACTIVATE)) {
        return false;
    }
    DWORD cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) &&
        cloaked) {
        return false;
    }
    RECT r;
    if (!GetWindowRect(hwnd, &r) || r.right - r.left < 2 || r.bottom - r.top < 2 ||
        !MonitorFromRect(&r, MONITOR_DEFAULTTONULL)) {
        return false;
    }
    std::wstring name = ClassOf(hwnd);
    return name != L"Progman" && name != L"WorkerW" && !IsOwnClass(name) &&
           !IsShellClass(name);
}

bool AnyOpenAppWindow() {
    bool found = false;
    EnumWindows(
        [](HWND hwnd, LPARAM lParam) -> BOOL {
            if (IsOpenAppWindow(hwnd)) {
                *reinterpret_cast<bool*>(lParam) = true;
                return FALSE;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&found));
    return found;
}

// The tweaks mod's genie length, read when a fade starts so a change there
// carries over. Its default when tweaks isn't installed.
float GenieDurationMs() {
    DWORD value = 0, size = sizeof(value);
    if (RegGetValueW(HKEY_LOCAL_MACHINE,
                     L"SOFTWARE\\Windhawk\\Engine\\Mods\\local@tweaks\\Settings",
                     L"genieDurationMs", RRF_RT_REG_DWORD, nullptr, &value,
                     &size) == ERROR_SUCCESS &&
        value) {
        return (float)std::clamp<DWORD>(value, 50, 2000);
    }
    return 450;
}

void UpdateDesktopShown() {
    bool shown = g_desktopShown;
    if (!g_settings.hideBehindWindows) {
        shown = true;
    } else {
        FocusKind kind = ClassifyForeground(GetForegroundWindow());
        if (kind == FocusKind::Desktop || kind == FocusKind::Ours) {
            shown = true;
        } else if (kind == FocusKind::App) {
            shown = false;
        } else if (!AnyOpenAppWindow()) {
            shown = true;  // shell focused, nothing open on the screen
        }
        // Otherwise (shell focused, windows open): unchanged.
    }
    if (shown != g_desktopShown) {
        g_desktopShown = shown;
        float ms = GenieDurationMs();
        for (auto& w : g_widgets) {
            w->UpdateVisibility(ms, true);
        }
    }
}

HWINEVENTHOOK g_focusHooks[4];

// Runs on the UI thread: out-of-context hooks are delivered through the
// message loop of the thread that set them. Each event is acted on at once, so
// the fade starts together with the genie (which minimizes or restores the
// window as its first frame goes up); a check shortly after settles bursts --
// a window closing and the next one activating.
void CALLBACK FocusEventProc(HWINEVENTHOOK,
                             DWORD event,
                             HWND hwnd,
                             LONG idObject,
                             LONG idChild,
                             DWORD,
                             DWORD) {
    if (event != EVENT_SYSTEM_FOREGROUND) {
        if (idObject != OBJID_WINDOW || idChild != CHILDID_SELF || !hwnd) {
            return;  // something inside a window, not a window itself
        }
        if (!IsWindow(hwnd)) {
            // Destroyed before this arrived, so whether it was a top-level
            // window can't be told any more; the settling check covers it.
            if (g_host) {
                SetTimer(g_host, kTimerFocus, 40, nullptr);
            }
            return;
        }
        if (GetAncestor(hwnd, GA_ROOT) != hwnd) {
            return;
        }
    }
    if (event == EVENT_SYSTEM_MINIMIZESTART) {
        g_minimizing = hwnd;
    }
    UpdateDesktopShown();
    g_minimizing = nullptr;
    if (g_host) {
        SetTimer(g_host, kTimerFocus, 40, nullptr);
    }
}

void StartFocusTracking() {
    const DWORD ranges[][2] = {
        {EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND},
        {EVENT_SYSTEM_MINIMIZESTART, EVENT_SYSTEM_MINIMIZEEND},
        {EVENT_OBJECT_DESTROY, EVENT_OBJECT_HIDE},       // windows closing
        {EVENT_OBJECT_CLOAKED, EVENT_OBJECT_UNCLOAKED},  // desktop switches
    };
    for (int i = 0; i < 4; i++) {
        g_focusHooks[i] = SetWinEventHook(ranges[i][0], ranges[i][1], nullptr,
                                          FocusEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
    }
    UpdateDesktopShown();
}

void StopFocusTracking() {
    for (auto& hook : g_focusHooks) {
        if (hook) {
            UnhookWinEvent(hook);
            hook = nullptr;
        }
    }
}

LRESULT CALLBACK HostWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_TIMER:
            if (wParam == kTimerProgman) {
                CheckDesktop();
            } else if (wParam == kTimerPalette) {
                KillTimer(hwnd, kTimerPalette);
                LoadPalette();
                RenderAll();
                topbar::UpdateBrushes();  // transparency may have been switched
            } else if (wParam == kTimerDisplay) {
                KillTimer(hwnd, kTimerDisplay);
                CreateWidgets();
                topbar::Sync();
            } else if (wParam == kTimerFocus) {
                KillTimer(hwnd, kTimerFocus);
                UpdateDesktopShown();
            }
            return 0;

        case WM_APP_RELOAD:
            Reload();
            UpdateDesktopShown();  // the setting may have been switched
            return 0;

        case WM_APP_QUIT:
            KillTimer(hwnd, kTimerProgman);
            StopFocusTracking();
            media::Stop();
            DestroyWidgets();
            topbar::Destroy();
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_SETTINGCHANGE:
            if (lParam && wcscmp(reinterpret_cast<PCWSTR>(lParam), L"ImmersiveColorSet") == 0) {
                SetTimer(hwnd, kTimerPalette, 250, nullptr);
            }
            break;

        case WM_DWMCOLORIZATIONCOLORCHANGED:
            SetTimer(hwnd, kTimerPalette, 250, nullptr);
            break;

        case WM_DISPLAYCHANGE:
            UpdateFrameInterval();
            // Monitors added, removed or rescaled: re-place everything.
            SetTimer(hwnd, kTimerDisplay, 750, nullptr);
            break;

        case WM_TIMECHANGE:
            for (auto& w : g_widgets) {
                w->Tick();
            }
            break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void UiThreadProc(HANDLE ready) {
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    WSADATA wsa;
    bool winsock = WSAStartup(MAKEWORD(2, 2), &wsa) == 0;

    // CREATE_WAITABLE_TIMER_HIGH_RESOLUTION; without it (before Windows 10
    // 1803) a plain timer, which paces coarser.
    g_frameTimer = CreateWaitableTimerExW(nullptr, nullptr, 0x00000002, TIMER_ALL_ACCESS);
    if (!g_frameTimer) {
        g_frameTimer = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
    }
    UpdateFrameInterval();

    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = HostWndProc;
    wc.hInstance = g_hinst;
    wc.lpszClassName = kHostClass;
    RegisterClassExW(&wc);

    wc.lpfnWndProc = WidgetWndProc;
    wc.lpszClassName = kWidgetClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    wc.lpfnWndProc = InputWndProc;
    wc.lpszClassName = kInputClass;
    RegisterClassExW(&wc);

    wc.lpfnWndProc = topbar::WndProc;
    wc.lpszClassName = kTopBarClass;
    RegisterClassExW(&wc);

    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory),
                      nullptr, g_d2d.put_void());
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                        reinterpret_cast<IUnknown**>(g_dw.put()));

    // A hidden top-level window rather than a message-only one, because only
    // top-level windows get the colour, display and time broadcasts.
    g_host = CreateWindowExW(WS_EX_TOOLWINDOW, kHostClass, L"", WS_POPUP, 0, 0,
                             0, 0, nullptr, nullptr, g_hinst, nullptr);
    SetEvent(ready);

    if (g_host && g_d2d && g_dw) {
        LoadSettings();
        LoadPalette();
        CreateFonts();
        CheckDesktop();
        topbar::Sync();
        SetTimer(g_host, kTimerProgman, 1000, nullptr);
        StartFocusTracking();

        // Waits on the frame timer as well as messages while anything is
        // animating. When both are ready the timer comes first, so a burst of
        // messages can't hold frames back.
        MSG msg;
        bool quit = false;
        while (!quit) {
            DWORD count = g_frameArmed ? 1 : 0;
            DWORD wait = MsgWaitForMultipleObjectsEx(count, &g_frameTimer, INFINITE,
                                                     QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            if (count && wait == WAIT_OBJECT_0) {
                RunAnimationFrame();
            }
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) {
                    quit = true;
                    break;
                }
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
    } else if (g_host) {
        DestroyWindow(g_host);
    }

    StopFocusTracking();
    media::Stop();
    DestroyWidgets();
    topbar::Shutdown();
    g_fonts = Fonts{};
    ReleaseRenderTarget();
    g_dw = nullptr;
    g_d2d = nullptr;
    g_host = nullptr;
    UnregisterClassW(kTopBarClass, g_hinst);
    UnregisterClassW(kInputClass, g_hinst);
    UnregisterClassW(kWidgetClass, g_hinst);
    UnregisterClassW(kHostClass, g_hinst);
    if (winsock) {
        WSACleanup();
    }
    g_animating.clear();
    g_frameArmed = false;
    if (g_frameTimer) {
        CloseHandle(g_frameTimer);
        g_frameTimer = nullptr;
    }
}

// =============================================================================
// Mod entry points
// =============================================================================

BOOL Wh_ModInit() {
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&Wh_ModInit), &g_hinst);

    HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_uiThread = std::thread(UiThreadProc, ready);
    WaitForSingleObject(ready, INFINITE);
    CloseHandle(ready);
    return TRUE;
}

void Wh_ModSettingsChanged() {
    if (g_host) {
        PostMessageW(g_host, WM_APP_RELOAD, 0, 0);
    }
}

void Wh_ModUninit() {
    if (g_host) {
        PostMessageW(g_host, WM_APP_QUIT, 0, 0);
    }
    if (g_uiThread.joinable()) {
        g_uiThread.join();
    }
}
