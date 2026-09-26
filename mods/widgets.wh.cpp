// ==WindhawkMod==
// @id              widgets
// @name            Widgets
// @description     Calendar and now-playing widgets on the desktop
// @version         1.0
// @author          ashtonlariccia
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -luser32 -lgdi32 -ladvapi32 -lole32 -loleaut32 -lruntimeobject -ld2d1 -ldwrite -lwindowscodecs -lshcore
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

## Moving widgets

Drag a widget anywhere that isn't a button. It snaps to a grid and to the edges
of the screen's work area, and remembers where it was left. Right-click a
widget to **lock** positions (then only Alt+drag moves them) or to put every
widget back in its default spot.

## Notes

The widgets are owned by the desktop window, so they stay on every virtual
desktop, don't show in Alt+Tab or the taskbar, survive Win+D, and never take
focus when clicked.
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
- FontFamily: Segoe UI Variable Display
  $name: Font
*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <windowsx.h>

#include <d2d1.h>
#include <dwrite.h>
#include <shellscalingapi.h>
#include <wincodec.h>

#undef GetCurrentTime

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using winrt::com_ptr;

// =============================================================================
// Settings
// =============================================================================

struct Settings {
    bool calendar = true;
    int weekStart = -1;  // -1 = locale, otherwise 0 = Monday ... 6 = Sunday
    bool nowPlaying = true;
    bool hideIdle = true;
    bool progress = true;
    int theme = 0;  // 0 = auto, 1 = dark, 2 = light
    float opacity = 0.62f;
    float cornerRadius = 14;
    float scale = 1;
    float snapGrid = 8;
    bool shadow = true;
    bool animations = true;
    std::wstring font;
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
    std::wstring theme = GetStringSetting(L"Theme");
    s.theme = theme == L"dark" ? 1 : theme == L"light" ? 2 : 0;
    s.opacity = std::clamp(Wh_GetIntSetting(L"BackgroundOpacity"), 0, 100) / 100.0f;
    s.cornerRadius = (float)std::clamp(Wh_GetIntSetting(L"CornerRadius"), 0, 40);
    s.scale = std::clamp(Wh_GetIntSetting(L"Scale"), 50, 300) / 100.0f;
    s.snapGrid = (float)std::clamp(Wh_GetIntSetting(L"SnapGrid"), 0, 200);
    s.shadow = Wh_GetIntSetting(L"Shadow");
    s.animations = Wh_GetIntSetting(L"Animations");
    s.font = GetStringSetting(L"FontFamily");
    g_settings = std::move(s);

    g_locked = Wh_GetIntValue(L"Locked", 0) != 0;
}

// =============================================================================
// Shared state
// =============================================================================

constexpr UINT WM_APP_RELOAD = WM_APP + 1;
constexpr UINT WM_APP_QUIT = WM_APP + 2;
constexpr UINT WM_APP_MEDIA = WM_APP + 3;

constexpr UINT_PTR kTimerAnim = 1;
constexpr UINT_PTR kTimerTick = 2;
constexpr UINT_PTR kTimerProgman = 10;
constexpr UINT_PTR kTimerPalette = 11;
constexpr UINT_PTR kTimerDisplay = 12;

constexpr PCWSTR kHostClass = L"WhWidgetsHost";
constexpr PCWSTR kWidgetClass = L"WhWidget";

// Every fade, hover and slide takes this long -- quick on purpose.
constexpr float kAnimMs = 100.0f;
constexpr float kShadowMargin = 8.0f;   // DIPs around the card for the shadow
constexpr float kScreenMargin = 24.0f;  // default distance from the work area edge
constexpr float kStackGap = 12.0f;      // default gap between stacked widgets

HINSTANCE g_hinst;
std::thread g_uiThread;
HWND g_host;
HWND g_progman;

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
        smallRight, weekday, day, dayBold, icon, iconBig;
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
    bool animTimer = false;
    ULONGLONG lastAnim = 0;

    virtual ~Widget() {
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
        BLENDFUNCTION blend{AC_SRC_OVER, 0, (BYTE)(EaseOut(opacity) * 255),
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
        lastAnim = GetTickCount64();
        SetTimer(hwnd, kTimerAnim, 15, nullptr);
    }

    void OnAnimationFrame() {
        ULONGLONG now = GetTickCount64();
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
            opacity = opacity < target ? std::min(target, opacity + step)
                                       : std::max(target, opacity - step);
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
            KillTimer(hwnd, kTimerAnim);
            animTimer = false;
        }
    }
    bool contentWasAnimating = false;

    void SetShown(bool value) {
        if (value && !IsWindowVisible(hwnd)) {
            Render();
            ShowWindow(hwnd, SW_SHOWNOACTIVATE);
            SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        if (shown != value) {
            shown = value;
            StartAnimation();
        }
    }

    void UpdateVisibility() { SetShown(WantsVisible()); }

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

    void ShowMenu(POINT screen);

    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
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
                    PCWSTR cursor = dragging ? IDC_SIZEALL
                                    : (hot >= 0 && Enabled(hot)) ? IDC_HAND
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
                if (id >= 0 && !alt) {
                    if (Enabled(id)) {
                        pressed = id;
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
                if (dragging) {
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
                if (wParam == kTimerAnim) {
                    OnAnimationFrame();
                } else if (wParam == kTimerTick) {
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
            } else if (wParam == kTimerDisplay) {
                KillTimer(hwnd, kTimerDisplay);
                CreateWidgets();
            }
            return 0;

        case WM_APP_RELOAD:
            Reload();
            return 0;

        case WM_APP_QUIT:
            KillTimer(hwnd, kTimerProgman);
            media::Stop();
            DestroyWidgets();
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

    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = HostWndProc;
    wc.hInstance = g_hinst;
    wc.lpszClassName = kHostClass;
    RegisterClassExW(&wc);

    wc.lpfnWndProc = WidgetWndProc;
    wc.lpszClassName = kWidgetClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
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
        SetTimer(g_host, kTimerProgman, 1000, nullptr);

        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    } else if (g_host) {
        DestroyWindow(g_host);
    }

    media::Stop();
    DestroyWidgets();
    g_fonts = Fonts{};
    ReleaseRenderTarget();
    g_dw = nullptr;
    g_d2d = nullptr;
    g_host = nullptr;
    UnregisterClassW(kWidgetClass, g_hinst);
    UnregisterClassW(kHostClass, g_hinst);
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
