// ==WindhawkMod==
// @id              desktops
// @name            Desktops
// @description     Virtual desktop pager pill on the taskbar plus Linux-style Win+number, Win+N and Win+Q keybinds
// @version         1.0
// @author          ashtonlariccia
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -lole32 -loleaut32 -lruntimeobject -luuid -ladvapi32
// ==/WindhawkMod==

// Source code is published under The GNU General Public License v3.0.
//
// The virtual desktop interface IDs and the shape of the undocumented
// IVirtualDesktopManagerInternal vtable were taken from two GPL sources:
//   * the "Virtual Desktop Helper" Windhawk mod by u2x3
//   * MScholtes/VirtualDesktop (VirtualDesktop11-24H2.cs)
//
// The taskbar visual tree helpers and the deferred-layout pattern follow the
// "Vertical Taskbar for Windows 11" mod by m417z.

// ==WindhawkModReadme==
/*
# Desktops

Everything virtual-desktop in one mod. Each feature has its own switch in the
settings, so any of them can be turned off without touching the others.

## Pager

A pill on the far left of the taskbar listing the virtual desktops that exist.
The active desktop's number is drawn in your accent colour, clicking a chip
switches to that desktop, and hovering shows the desktop's name and the windows
on it.

```
[1][2]            [start][search][task view][apps]        [tray] [clock]
 ^^^^ the pager
```

## Keybinds

| Keys | Action |
|---|---|
| `Win+1` ... `Win+9`, `Win+0` | Switch to desktop 1 ... 9, 10. Does nothing if that desktop doesn't exist |
| `Win+N` | Create a new desktop and switch to it |
| `Win+Q` | Close the current desktop; its windows move to the neighbouring one |
| `Win+Shift+1` ... `Win+Shift+0` | Move the focused window to desktop 1 ... 10, staying where you are |

These replace the Windows shortcuts on the same keys (pinned taskbar apps, the
notification centre, search) for as long as they are enabled.

The keys are read with a low-level keyboard hook inside explorer. Windows skips
such hooks while an **elevated** (administrator) window has focus, so the
keybinds do nothing then -- the same limitation PowerToys has when it isn't
itself running elevated.

## Switch animation

On Windows 11 24H2/25H2 (build 26100+) every switch -- a chip click or a keybind
-- slides between desktops with the same animation as Task View. Turn it off
with **Animate desktop switches**. Creating and closing desktops also needs
26100+, because those methods sit in the part of the shell's private interface
whose layout is only known there.

## Styling

This mod draws only the numbers. The pill around them -- background, padding,
corners, position -- comes from the **Shell Styler** mod's taskbar theme, which
targets these stable element names:

| Target | What it is |
|---|---|
| `Grid#WhVdPagerRoot` | outer container, positions the pill |
| `Border#WhVdPagerBackground` | the pill itself: background, corner radius |
| `StackPanel#WhVdPagerItems` | the row of chips |
| `Button#WhVdDesktopButton` | a desktop chip |
| `TextBlock#WhVdDesktopLabel` | the number or name inside a chip |
| `Border#WhVdHoverCard` | the hover card |

## Supported Windows versions

Windows 11 22621 (22H2) and newer; create/close need 26100 (24H2) and newer.
The interface IDs the virtual desktop service uses change between builds. The
mod probes the ones it knows and keeps the one that answers; the **Windows
version** setting is an escape hatch for when a future build needs a new ID.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- PagerEnabled: true
  $name: Show desktop pager on the taskbar
  $description: >-
    The pager pill at the left of the taskbar. Applies live; the keybinds
    below work either way.
- KeySwitch: true
  $name: "Keybind: Win+1 ... Win+0 switches desktop"
- KeyNew: true
  $name: "Keybind: Win+N creates a desktop"
- KeyClose: true
  $name: "Keybind: Win+Q closes the current desktop"
- KeyMove: true
  $name: "Keybind: Win+Shift+1 ... Win+Shift+0 moves the focused window"
  $description: >-
    Sends the focused window to that desktop without switching to it.
- SwitchAnimation: true
  $name: Animate desktop switches
  $description: >-
    Slide between desktops the way Task View does, for chip clicks and
    keybinds alike. Windows 11 build 26100 and newer only.
- UiAnimations: true
  $name: Animate the pager
  $description: >-
    Fade the active number between colours, and fade in new chips and the
    hover card.
- ChipSize: 28
  $name: Chip size
  $description: Width and height of each desktop chip, in pixels.
- ChipSpacing: 4
  $name: Chip spacing
  $description: Gap between chips, in pixels.
- LabelStyle: number
  $name: Chip label
  $options:
  - number: Desktop number
  - name: Desktop name
- HoverCard: true
  $name: Hover card
  $description: Show the desktop's name and windows when hovering a chip.
- HoverDelayMs: 450
  $name: Hover delay (ms)
  $description: >-
    How long to hover a chip before its card appears. Moving between chips
    while a card is already open switches instantly, as Windows tooltips do.
- HideWhenSingleDesktop: false
  $name: Hide the pager when there is only one desktop
- ShowOnSecondaryTaskbars: true
  $name: Show the pager on secondary taskbars
- PollIntervalMs: 400
  $name: Refresh interval (ms)
  $description: >-
    Only used if Windows won't send desktop change notifications, which it
    normally does -- then the pager updates instantly and doesn't poll.
- WindowsVersion: auto
  $name: Windows version
  $description: >-
    Which set of virtual desktop interface IDs to use. Leave on automatic
    unless a Windows update breaks detection.
  $options:
  - auto: Detect automatically
  - win11_26100: Windows 11 24H2 / 25H2 (build 26100 and newer)
  - win11_22621: Windows 11 22H2 / 23H2 (build 22621, 22631)
- DebugLogToFile: false
  $name: Write the log to a file
  $description: >-
    Also append this mod's log to desktops-debug.log in your temp folder, for
    capturing a full trace when reporting a problem.
*/
// ==/WindhawkModSettings==

#include <initguid.h>

#include <windows.h>

#include <objbase.h>
#include <objectarray.h>
#include <servprov.h>
#include <winstring.h>

#include <windhawk_utils.h>

// winbase.h defines GetCurrentTime as a macro, which collides with a method of
// the same name in the XAML animation interfaces.
#undef GetCurrentTime

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Windows.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Media.Animation.h>
#include <winrt/Windows.UI.Xaml.Media.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using namespace winrt::Windows::UI::Xaml;

// Windhawk's log window and any other debug-output viewer compete for the same
// OutputDebugString messages -- DBWIN_DATA_READY is auto-reset, so each message
// wakes exactly one reader and a second viewer sees roughly half the lines.
// Mirroring to a file is the only way to capture a complete trace. Off by
// default; enabled with the DebugLogToFile setting.
static std::atomic<bool> g_debugLogToFile;

static void DebugFileLog(PCWSTR format, ...) {
    if (!g_debugLogToFile) {
        return;
    }

    wchar_t path[MAX_PATH];
    DWORD pathLength = GetTempPath(ARRAYSIZE(path), path);
    if (!pathLength || pathLength >= ARRAYSIZE(path) - 20) {
        return;
    }
    wcscat_s(path, L"desktops-debug.log");

    wchar_t text[1024];
    va_list args;
    va_start(args, format);
    int length = _vsnwprintf_s(text, ARRAYSIZE(text), _TRUNCATE, format, args);
    va_end(args);
    if (length < 0) {
        return;
    }

    // Stamp every line with the time and the thread. This mod logs from the
    // taskbar UI thread and from its own worker, and every startup question is
    // about which of them got somewhere first. Appends are atomic per write so
    // lines stay whole, but the order two threads reach the file is not the
    // order things happened -- the stamp is what makes a trace readable.
    SYSTEMTIME st;
    GetLocalTime(&st);

    wchar_t line[1200];
    int lineLength =
        _snwprintf_s(line, ARRAYSIZE(line), _TRUNCATE,
                     L"%02u:%02u:%02u.%03u  t%-5lu  %s\r\n", st.wHour,
                     st.wMinute, st.wSecond, st.wMilliseconds,
                     GetCurrentThreadId(), text);
    if (lineLength < 0) {
        return;
    }

    char utf8[4096];
    int utf8Length = WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8,
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

#define VDPLOG(...)                \
    do {                           \
        Wh_Log(__VA_ARGS__);       \
        DebugFileLog(__VA_ARGS__); \
    } while (0)

// =============================================================================
// Undocumented shell virtual desktop API
// =============================================================================

// {C2F03A33-21F5-47FA-B4BB-156362A2F239}
static const CLSID CLSID_ImmersiveShell = {
    0xC2F03A33,
    0x21F5,
    0x47FA,
    {0xB4, 0xBB, 0x15, 0x63, 0x62, 0xA2, 0xF2, 0x39}};

// {C5E0CDCA-7B6E-41B2-9FC4-D93975CC467B}
static const CLSID CLSID_VirtualDesktopManagerInternal = {
    0xC5E0CDCA,
    0x7B6E,
    0x41B2,
    {0x9F, 0xC4, 0xD9, 0x39, 0x75, 0xCC, 0x46, 0x7B}};

struct IVirtualDesktop : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE IsViewVisible(IUnknown*, BOOL*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetId(GUID*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetName(HSTRING*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetWallpaperPath(HSTRING*) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsRemote(BOOL*) = 0;
};

// The prefix through SwitchDesktop (slot 6) has been stable since 22621; the
// tail is where Microsoft keeps adding methods. Reading and plain switching stay
// in the prefix. Everything else this mod does lives in the tail:
// CreateDesktop (8) and RemoveDesktop (10) for the Win+N / Win+Q keybinds, and
// SwitchDesktopWithAnimation (19) with WaitForAnimationToComplete (21) for the
// Task View slide.
//
// Reaching them means declaring every slot up to them, because a vtable index
// is fixed by declaration order. The methods never called are declared only to
// occupy their slots -- their parameter types are approximate, so a wrong
// signature on, say, CopyDesktopState cannot matter. The slot layout below is
// the 26100+ (24H2/25H2) one, taken from MScholtes/VirtualDesktop
// (VirtualDesktop11-24H2). It is NOT the 22621 layout, which is why every tail
// call is gated to the 26100 interface ID: on 22621 slot 19 is some other method
// entirely, and calling it blind could create or destroy a desktop. See
// VersionIIDs::tailKnown.
struct IVirtualDesktopManagerInternal : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetCount(UINT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE MoveViewToDesktop(IUnknown*,
                                                        IVirtualDesktop*) = 0;
    virtual HRESULT STDMETHODCALLTYPE CanViewMoveDesktops(IUnknown*, BOOL*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentDesktop(IVirtualDesktop**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDesktops(IObjectArray**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetAdjacentDesktop(IVirtualDesktop*,
                                                         int,
                                                         IVirtualDesktop**) = 0;
    virtual HRESULT STDMETHODCALLTYPE SwitchDesktop(IVirtualDesktop*) = 0;
    // --- tail: 26100+ layout only, occupied to reach slots 19 and 21 ---
    virtual HRESULT STDMETHODCALLTYPE SwitchDesktopAndMoveForegroundView(
        IVirtualDesktop*) = 0;                                        // 7
    virtual HRESULT STDMETHODCALLTYPE CreateDesktop(IVirtualDesktop**) = 0;  // 8
    virtual HRESULT STDMETHODCALLTYPE MoveDesktop(IVirtualDesktop*, int) = 0;  // 9
    virtual HRESULT STDMETHODCALLTYPE RemoveDesktop(IVirtualDesktop*,
                                                    IVirtualDesktop*) = 0;  // 10
    virtual HRESULT STDMETHODCALLTYPE FindDesktop(GUID*,
                                                  IVirtualDesktop**) = 0;  // 11
    virtual HRESULT STDMETHODCALLTYPE GetDesktopSwitchIncludeExcludeViews(
        IVirtualDesktop*, IObjectArray**, IObjectArray**) = 0;  // 12
    virtual HRESULT STDMETHODCALLTYPE SetDesktopName(IVirtualDesktop*,
                                                     HSTRING) = 0;  // 13
    virtual HRESULT STDMETHODCALLTYPE SetDesktopWallpaper(IVirtualDesktop*,
                                                          HSTRING) = 0;  // 14
    virtual HRESULT STDMETHODCALLTYPE UpdateWallpaperPathForAllDesktops(
        HSTRING) = 0;                                                    // 15
    virtual HRESULT STDMETHODCALLTYPE CopyDesktopState(IUnknown*,
                                                       IUnknown*) = 0;  // 16
    virtual HRESULT STDMETHODCALLTYPE CreateRemoteDesktop(
        HSTRING, IVirtualDesktop**) = 0;                                    // 17
    virtual HRESULT STDMETHODCALLTYPE SwitchRemoteDesktop(IVirtualDesktop*,
                                                          void*) = 0;      // 18
    virtual HRESULT STDMETHODCALLTYPE SwitchDesktopWithAnimation(
        IVirtualDesktop*) = 0;                                              // 19
    virtual HRESULT STDMETHODCALLTYPE GetLastActiveDesktop(
        IVirtualDesktop**) = 0;                                            // 20
    virtual HRESULT STDMETHODCALLTYPE WaitForAnimationToComplete() = 0;    // 21
};

struct VersionIIDs {
    PCWSTR settingValue;
    PCWSTR description;
    IID managerInternal;
    IID virtualDesktop;
    // Whether the tail of the vtable above matches this interface. Only the
    // 26100+ layout is known, so only there may anything past slot 6 be called.
    bool tailKnown;
};

// Probed in order. QueryService fails cleanly with E_NOINTERFACE when the ID
// doesn't match the running build, so probing is safe -- a wrong ID never
// yields a pointer we could call into.
static const VersionIIDs g_versionIIDs[] = {
    // Windows 11 build 26100+ (24H2 / 25H2).
    {L"win11_26100",
     L"Windows 11 26100+ (24H2/25H2)",
     {0x53F5CA0B,
      0x158F,
      0x4124,
      {0x90, 0x0C, 0x05, 0x71, 0x58, 0x06, 0x0B, 0x27}},
     {0x3F07F4BE,
      0xB107,
      0x441A,
      {0xAF, 0x0F, 0x39, 0xD8, 0x25, 0x29, 0x07, 0x2C}},
     /*tailKnown=*/true},

    // Windows 11 builds 22621 / 22631 (22H2 / 23H2). The animated-switch tail
    // is unverified for this interface, so switches here are always instant.
    {L"win11_22621",
     L"Windows 11 22621/22631 (22H2/23H2)",
     {0xA3175F2D,
      0x239C,
      0x4BD2,
      {0x8A, 0xA0, 0xEE, 0xBA, 0x8B, 0x0B, 0x13, 0x8E}},
     {0x3F07F4BE,
      0xB107,
      0x441A,
      {0xAF, 0x0F, 0x39, 0xD8, 0x25, 0x29, 0x07, 0x2C}},
     /*tailKnown=*/false},
};

// =============================================================================
// Settings
// =============================================================================

struct {
    bool pagerEnabled;
    bool keySwitch;
    bool keyNew;
    bool keyClose;
    bool keyMove;
    bool uiAnimations;
    bool hoverCard;
    double chipSize;
    double chipSpacing;
    bool labelIsName;
    int hoverDelayMs;
    bool switchAnimation;
    bool hideWhenSingleDesktop;
    bool showOnSecondaryTaskbars;
    std::wstring windowsVersion;
    int pollIntervalMs;
} g_settings;

static double ClampDouble(int value, int minValue, int maxValue) {
    return static_cast<double>(std::clamp(value, minValue, maxValue));
}

static void LoadSettings() {
    g_settings.pagerEnabled = Wh_GetIntSetting(L"PagerEnabled") != 0;
    g_settings.keySwitch = Wh_GetIntSetting(L"KeySwitch") != 0;
    g_settings.keyNew = Wh_GetIntSetting(L"KeyNew") != 0;
    g_settings.keyClose = Wh_GetIntSetting(L"KeyClose") != 0;
    g_settings.keyMove = Wh_GetIntSetting(L"KeyMove") != 0;
    g_settings.uiAnimations = Wh_GetIntSetting(L"UiAnimations") != 0;
    g_settings.hoverCard = Wh_GetIntSetting(L"HoverCard") != 0;

    g_settings.chipSize = ClampDouble(Wh_GetIntSetting(L"ChipSize"), 8, 200);
    g_settings.chipSpacing =
        ClampDouble(Wh_GetIntSetting(L"ChipSpacing"), 0, 100);
    PCWSTR labelStyle = Wh_GetStringSetting(L"LabelStyle");
    g_settings.labelIsName = labelStyle && wcscmp(labelStyle, L"name") == 0;
    Wh_FreeStringSetting(labelStyle);

    g_settings.hoverDelayMs =
        std::clamp(Wh_GetIntSetting(L"HoverDelayMs"), 0, 5000);
    g_settings.switchAnimation = Wh_GetIntSetting(L"SwitchAnimation") != 0;
    g_settings.hideWhenSingleDesktop =
        Wh_GetIntSetting(L"HideWhenSingleDesktop") != 0;
    g_settings.showOnSecondaryTaskbars =
        Wh_GetIntSetting(L"ShowOnSecondaryTaskbars") != 0;

    PCWSTR version = Wh_GetStringSetting(L"WindowsVersion");
    g_settings.windowsVersion = version ? version : L"auto";
    Wh_FreeStringSetting(version);

    g_debugLogToFile = Wh_GetIntSetting(L"DebugLogToFile") != 0;

    g_settings.pollIntervalMs =
        std::clamp(Wh_GetIntSetting(L"PollIntervalMs"), 100, 5000);
}

// =============================================================================
// Desktop state
// =============================================================================

struct DesktopInfo {
    GUID id;
    std::wstring name;  // may be empty -- Windows only stores renamed desktops
};

struct DesktopSnapshot {
    std::vector<DesktopInfo> desktops;
    int currentIndex = -1;

    bool operator==(const DesktopSnapshot& other) const {
        if (currentIndex != other.currentIndex ||
            desktops.size() != other.desktops.size()) {
            return false;
        }
        for (size_t i = 0; i < desktops.size(); i++) {
            if (!IsEqualGUID(desktops[i].id, other.desktops[i].id) ||
                desktops[i].name != other.desktops[i].name) {
                return false;
            }
        }
        return true;
    }
};

// {AA509086-5CA9-4C25-8F95-589D3C07B48A}
static const CLSID CLSID_VirtualDesktopManagerPublic = {
    0xAA509086,
    0x5CA9,
    0x4C25,
    {0x8F, 0x95, 0x58, 0x9D, 0x3C, 0x07, 0xB4, 0x8A}};

// {A5CD92FF-29BE-454C-8D04-D82879FB3F1B}
static const IID IID_IVirtualDesktopManagerPublic = {
    0xA5CD92FF,
    0x29BE,
    0x454C,
    {0x8D, 0x04, 0xD8, 0x28, 0x79, 0xFB, 0x3F, 0x1B}};

// The documented interface -- unlike the internal one above, this is a stable
// public API, which is why the hover card's window lookup carries no version
// risk at all.
struct IVirtualDesktopManagerPublic : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE IsWindowOnCurrentVirtualDesktop(HWND,
                                                                      BOOL*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetWindowDesktopId(HWND, GUID*) = 0;
    virtual HRESULT STDMETHODCALLTYPE MoveWindowToDesktop(HWND, REFGUID) = 0;
};

// {1841C6D7-4F9D-42C0-AF41-8747538F10E5}
// Stable since Windows 10 1809; only GetViewForHwnd is ever called.
static const IID IID_IApplicationViewCollection = {
    0x1841C6D7,
    0x4F9D,
    0x42C0,
    {0xAF, 0x41, 0x87, 0x47, 0x53, 0x8F, 0x10, 0xE5}};

struct IApplicationViewCollection : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetViews(IObjectArray**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetViewsByZOrder(IObjectArray**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetViewsByAppUserModelId(PCWSTR,
                                                               IObjectArray**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetViewForHwnd(HWND, IUnknown**) = 0;
};

// =============================================================================
// Desktop change notifications
// =============================================================================
//
// Instead of re-reading the desktop list on a timer, the pager asks the shell
// to call it when desktops change. The notification interface's ID and method
// list differ between Windows builds -- which is why every method here does the
// same thing, wake the worker to re-read, and ignores its arguments. Which
// method the shell calls, and with what, then doesn't matter; on x64 the caller
// cleans up the stack, so the arguments can't unbalance anything. The object
// answers to each build's interface ID it knows. If none is accepted, the
// pager falls back to polling.

// {A501FDEC-4A09-464C-AE4E-1B9C21B84918}
static const CLSID CLSID_VirtualDesktopNotificationService = {
    0xA501FDEC,
    0x4A09,
    0x464C,
    {0xAE, 0x4E, 0x1B, 0x9C, 0x21, 0xB8, 0x49, 0x18}};

// {0CD45E71-D927-4F15-8B0A-8FEF525337BF}
static const IID IID_IVirtualDesktopNotificationService = {
    0x0CD45E71,
    0xD927,
    0x4F15,
    {0x8B, 0x0A, 0x8F, 0xEF, 0x52, 0x53, 0x37, 0xBF}};

struct IVirtualDesktopNotificationService : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Register(IUnknown*, DWORD*) = 0;
    virtual HRESULT STDMETHODCALLTYPE Unregister(DWORD) = 0;
};

static const IID kVirtualDesktopNotificationIIDs[] = {
    // Windows 11 26100+ (24H2 / 25H2).
    {0xB9E5E94D, 0x233E, 0x49AB, {0xAF, 0x5C, 0x2B, 0x45, 0x41, 0xC3, 0xAA, 0xDE}},
    // Windows 11 22621 / 22631 (22H2 / 23H2).
    {0xCD403E52, 0xDEED, 0x4C13, {0xB4, 0x37, 0xB9, 0x83, 0x80, 0xF2, 0xB1, 0xE8}},
};

static void WakeWorkerForDesktopChange();

#define VDP_NOTIFY_METHOD(n)                                                  \
    virtual HRESULT STDMETHODCALLTYPE Notify##n(void*, void*, void*, void*) { \
        WakeWorkerForDesktopChange();                                         \
        return S_OK;                                                          \
    }

// A single static instance: the shell's AddRef/Release are no-ops, so no
// lifetime can outlive the registration.
class DesktopChangeNotifier : public IUnknown {
   public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) {
            return E_POINTER;
        }
        bool known = IsEqualIID(riid, IID_IUnknown);
        for (const auto& iid : kVirtualDesktopNotificationIIDs) {
            known = known || IsEqualIID(riid, iid);
        }
        if (!known) {
            *ppv = nullptr;
            return E_NOINTERFACE;
        }
        *ppv = this;
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return 2; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }

    // More slots than any known build's interface has.
    VDP_NOTIFY_METHOD(0)
    VDP_NOTIFY_METHOD(1)
    VDP_NOTIFY_METHOD(2)
    VDP_NOTIFY_METHOD(3)
    VDP_NOTIFY_METHOD(4)
    VDP_NOTIFY_METHOD(5)
    VDP_NOTIFY_METHOD(6)
    VDP_NOTIFY_METHOD(7)
    VDP_NOTIFY_METHOD(8)
    VDP_NOTIFY_METHOD(9)
    VDP_NOTIFY_METHOD(10)
    VDP_NOTIFY_METHOD(11)
    VDP_NOTIFY_METHOD(12)
    VDP_NOTIFY_METHOD(13)
    VDP_NOTIFY_METHOD(14)
    VDP_NOTIFY_METHOD(15)
};

#undef VDP_NOTIFY_METHOD

static DesktopChangeNotifier g_desktopChangeNotifier;

// =============================================================================
// Engine -- owns every COM pointer, lives entirely on the worker thread
// =============================================================================

class VirtualDesktopEngine {
   public:
    bool Connect() {
        Disconnect();

        HRESULT hr = CoCreateInstance(CLSID_ImmersiveShell, nullptr, CLSCTX_ALL,
                                      IID_PPV_ARGS(&m_serviceProvider));
        if (FAILED(hr) || !m_serviceProvider) {
            VDPLOG(L"CoCreateInstance(ImmersiveShell) failed: 0x%08X", hr);
            return false;
        }

        for (const auto& candidate : g_versionIIDs) {
            if (g_settings.windowsVersion != L"auto" &&
                g_settings.windowsVersion != candidate.settingValue) {
                continue;
            }

            IVirtualDesktopManagerInternal* manager = nullptr;
            hr = m_serviceProvider->QueryService(
                CLSID_VirtualDesktopManagerInternal, candidate.managerInternal,
                reinterpret_cast<void**>(&manager));
            if (FAILED(hr) || !manager) {
                VDPLOG(L"Not %s: 0x%08X", candidate.description, hr);
                continue;
            }

            // A matching interface ID isn't proof on its own -- ask it
            // something and check the answer is sane before trusting the
            // vtable.
            UINT count = 0;
            hr = manager->GetCount(&count);
            if (FAILED(hr) || count < 1 || count > 64) {
                VDPLOG(L"%s answered GetCount with 0x%08X, count=%u -- rejected",
                       candidate.description, hr, count);
                manager->Release();
                continue;
            }

            m_manager = manager;
            m_version = &candidate;
            VDPLOG(L"Connected as %s, %u desktop(s)", candidate.description,
                   count);

            // Both optional: without the view collection only explorer's
            // own windows can be moved, and without notifications the
            // pager polls.
            m_serviceProvider->QueryService(
                IID_IApplicationViewCollection, IID_IApplicationViewCollection,
                reinterpret_cast<void**>(&m_viewCollection));
            RegisterNotifications();
            return true;
        }

        VDPLOG(L"No known virtual desktop interface answered");
        Disconnect();
        return false;
    }

    void Disconnect() {
        if (m_notificationService) {
            m_notificationService->Unregister(m_notificationCookie);
            m_notificationService->Release();
            m_notificationService = nullptr;
            m_notificationCookie = 0;
        }
        if (m_viewCollection) {
            m_viewCollection->Release();
            m_viewCollection = nullptr;
        }
        if (m_manager) {
            m_manager->Release();
            m_manager = nullptr;
        }
        if (m_serviceProvider) {
            m_serviceProvider->Release();
            m_serviceProvider = nullptr;
        }
        m_version = nullptr;
    }

    bool IsConnected() const { return m_manager != nullptr; }

    bool HasNotifications() const { return m_notificationService != nullptr; }

    // Zero-based. Moves the window without switching; a desktop that doesn't
    // exist, or the one it's already on, is a no-op.
    bool MoveWindowToIndex(HWND hwnd,
                           int index,
                           IVirtualDesktopManagerPublic* publicManager) {
        if (!m_manager || !hwnd) {
            return false;
        }

        IVirtualDesktop* desktop = GetAt(index);
        if (!desktop) {
            return false;
        }

        bool moved = false;
        DWORD processId = 0;
        GetWindowThreadProcessId(hwnd, &processId);

        if (processId == GetCurrentProcessId()) {
            // The documented call works, but only for the caller's own
            // windows -- here, explorer's.
            GUID id{};
            moved = publicManager && SUCCEEDED(desktop->GetId(&id)) &&
                    SUCCEEDED(publicManager->MoveWindowToDesktop(hwnd, id));
        } else if (m_viewCollection) {
            IUnknown* view = nullptr;
            if (SUCCEEDED(m_viewCollection->GetViewForHwnd(hwnd, &view)) &&
                view) {
                HRESULT hr = m_manager->MoveViewToDesktop(view, desktop);
                if (FAILED(hr)) {
                    VDPLOG(L"MoveViewToDesktop failed: 0x%08X", hr);
                }
                moved = SUCCEEDED(hr);
                view->Release();
            }
        }

        desktop->Release();
        return moved;
    }

    bool ReadSnapshot(DesktopSnapshot* snapshot) {
        if (!m_manager) {
            return false;
        }

        IObjectArray* desktops = nullptr;
        HRESULT hr = m_manager->GetDesktops(&desktops);
        if (FAILED(hr) || !desktops) {
            VDPLOG(L"GetDesktops failed: 0x%08X", hr);
            return false;
        }

        GUID currentId{};
        bool haveCurrentId = false;
        IVirtualDesktop* current = nullptr;
        if (SUCCEEDED(m_manager->GetCurrentDesktop(&current)) && current) {
            haveCurrentId = SUCCEEDED(current->GetId(&currentId));
            current->Release();
        }

        UINT count = 0;
        desktops->GetCount(&count);

        snapshot->desktops.clear();
        snapshot->desktops.reserve(count);
        snapshot->currentIndex = -1;

        for (UINT i = 0; i < count; i++) {
            IVirtualDesktop* desktop = nullptr;
            hr = desktops->GetAt(i, m_version->virtualDesktop,
                                 reinterpret_cast<void**>(&desktop));
            if (FAILED(hr) || !desktop) {
                continue;
            }

            DesktopInfo info{};
            if (SUCCEEDED(desktop->GetId(&info.id))) {
                info.name = ReadDesktopName(desktop);

                if (haveCurrentId && IsEqualGUID(info.id, currentId)) {
                    snapshot->currentIndex =
                        static_cast<int>(snapshot->desktops.size());
                }
                snapshot->desktops.push_back(std::move(info));
            }

            desktop->Release();
        }

        desktops->Release();
        return !snapshot->desktops.empty();
    }

    // Zero-based. An index past the last desktop is a no-op, not an error --
    // Win+5 with three desktops does nothing, by design.
    bool SwitchToIndex(int index) {
        IVirtualDesktop* desktop = GetAt(index);
        if (!desktop) {
            return false;
        }

        bool switched = ActivateDesktop(desktop);
        desktop->Release();
        return switched;
    }

    // Windows appends the new desktop after the last one.
    bool CreateAndSwitch() {
        if (!CanUseTail()) {
            VDPLOG(L"Create: needs the 26100+ interface");
            return false;
        }

        IVirtualDesktop* desktop = nullptr;
        HRESULT hr = m_manager->CreateDesktop(&desktop);
        if (FAILED(hr) || !desktop) {
            VDPLOG(L"CreateDesktop failed: 0x%08X", hr);
            return false;
        }

        bool switched = ActivateDesktop(desktop);
        desktop->Release();
        return switched;
    }

    // Closes the current desktop. Its windows move to the fallback, which is
    // the neighbour on the left, or on the right when closing the first one --
    // the same choice Task View makes. With animation on, the slide to the
    // fallback plays first and the now-background desktop is removed after, so
    // closing looks like a switch rather than a jump cut.
    bool RemoveCurrent() {
        if (!CanUseTail()) {
            VDPLOG(L"Remove: needs the 26100+ interface");
            return false;
        }

        IVirtualDesktop* current = nullptr;
        if (FAILED(m_manager->GetCurrentDesktop(&current)) || !current) {
            return false;
        }

        IVirtualDesktop* fallback = nullptr;
        if (FAILED(m_manager->GetAdjacentDesktop(current, kAdjacentLeft,
                                                 &fallback)) ||
            !fallback) {
            fallback = nullptr;
            if (FAILED(m_manager->GetAdjacentDesktop(current, kAdjacentRight,
                                                     &fallback)) ||
                !fallback) {
                // The only desktop -- there is nowhere for its windows to go.
                current->Release();
                return false;
            }
        }

        if (AnimationEnabled()) {
            ActivateDesktop(fallback);
        }

        HRESULT hr = m_manager->RemoveDesktop(current, fallback);
        if (FAILED(hr)) {
            VDPLOG(L"RemoveDesktop failed: 0x%08X", hr);
        }

        fallback->Release();
        current->Release();
        return SUCCEEDED(hr);
    }

   private:
    void RegisterNotifications() {
        if (FAILED(m_serviceProvider->QueryService(
                CLSID_VirtualDesktopNotificationService,
                IID_IVirtualDesktopNotificationService,
                reinterpret_cast<void**>(&m_notificationService))) ||
            !m_notificationService) {
            m_notificationService = nullptr;
            VDPLOG(L"No notification service, polling");
            return;
        }

        HRESULT hr = m_notificationService->Register(&g_desktopChangeNotifier,
                                                     &m_notificationCookie);
        if (FAILED(hr)) {
            VDPLOG(L"Notification Register failed: 0x%08X, polling", hr);
            m_notificationService->Release();
            m_notificationService = nullptr;
            return;
        }

        VDPLOG(L"Desktop change notifications registered");
    }

    static constexpr int kAdjacentLeft = 3;
    static constexpr int kAdjacentRight = 4;

    bool CanUseTail() const {
        return m_manager && m_version && m_version->tailKnown;
    }

    bool AnimationEnabled() const {
        return CanUseTail() && g_settings.switchAnimation;
    }

    // Returns an owned reference, or null.
    IVirtualDesktop* GetAt(int index) {
        if (!m_manager || index < 0) {
            return nullptr;
        }

        IObjectArray* desktops = nullptr;
        if (FAILED(m_manager->GetDesktops(&desktops)) || !desktops) {
            return nullptr;
        }

        IVirtualDesktop* desktop = nullptr;
        UINT count = 0;
        desktops->GetCount(&count);
        if (static_cast<UINT>(index) < count &&
            FAILED(desktops->GetAt(index, m_version->virtualDesktop,
                                   reinterpret_cast<void**>(&desktop)))) {
            desktop = nullptr;
        }

        desktops->Release();
        return desktop;
    }

    // Switches to the given desktop, sliding the way Task View does when the
    // running build supports it and the user hasn't turned the animation off.
    //
    // The instant path is a bare SwitchDesktop. The animated path mirrors
    // MScholtes/VirtualDesktop's /Switch handler exactly, because that is the
    // command the user confirmed animates flawlessly on this machine:
    // SwitchDesktopWithAnimation only plays its animation when the switch is
    // initiated by the foreground thread, so the taskbar window is forced to
    // the foreground first -- with the AttachThreadInput handshake that lets
    // SetForegroundWindow succeed from a thread that doesn't already own the
    // foreground -- and the switch is bracketed by WaitForAnimationToComplete
    // so back-to-back clicks don't stack half-finished slides.
    bool ActivateDesktop(IVirtualDesktop* desktop) {
        // Pressing the key for the desktop you're already on would otherwise
        // play a pointless bounce, or steal focus for nothing.
        if (IsCurrent(desktop)) {
            return true;
        }

        if (!AnimationEnabled()) {
            HRESULT hr = m_manager->SwitchDesktop(desktop);
            if (FAILED(hr)) {
                VDPLOG(L"SwitchDesktop failed: 0x%08X", hr);
            }
            return SUCCEEDED(hr);
        }

        BringTaskbarToForeground();

        m_manager->WaitForAnimationToComplete();
        HRESULT hr = m_manager->SwitchDesktopWithAnimation(desktop);
        if (FAILED(hr)) {
            VDPLOG(L"SwitchDesktopWithAnimation failed: 0x%08X, "
                   L"falling back to instant",
                   hr);
            hr = m_manager->SwitchDesktop(desktop);
            return SUCCEEDED(hr);
        }
        m_manager->WaitForAnimationToComplete();
        return true;
    }

    bool IsCurrent(IVirtualDesktop* desktop) {
        IVirtualDesktop* current = nullptr;
        if (FAILED(m_manager->GetCurrentDesktop(&current)) || !current) {
            return false;
        }

        GUID a{}, b{};
        bool same = SUCCEEDED(current->GetId(&a)) &&
                    SUCCEEDED(desktop->GetId(&b)) && IsEqualGUID(a, b);
        current->Release();
        return same;
    }

    static void BringTaskbarToForeground() {
        BringToForeground(FindWindow(L"Shell_TrayWnd", nullptr));
    }

   public:
    static void BringToForeground(HWND taskbar) {
        if (!taskbar) {
            return;
        }

        DWORD unused = 0;
        DWORD taskbarThread = GetWindowThreadProcessId(taskbar, &unused);
        DWORD foregroundThread =
            GetWindowThreadProcessId(GetForegroundWindow(), &unused);
        DWORD currentThread = GetCurrentThreadId();

        // Already ours to move, or nothing to attach to -- just take it.
        if (!taskbarThread || !foregroundThread ||
            foregroundThread == currentThread) {
            SetForegroundWindow(taskbar);
            return;
        }

        AttachThreadInput(taskbarThread, currentThread, TRUE);
        AttachThreadInput(foregroundThread, currentThread, TRUE);
        SetForegroundWindow(taskbar);
        AttachThreadInput(foregroundThread, currentThread, FALSE);
        AttachThreadInput(taskbarThread, currentThread, FALSE);
    }

   private:

    static std::wstring ReadDesktopName(IVirtualDesktop* desktop) {
        HSTRING name = nullptr;
        if (FAILED(desktop->GetName(&name)) || !name) {
            return std::wstring();
        }

        UINT32 length = 0;
        PCWSTR buffer = WindowsGetStringRawBuffer(name, &length);
        std::wstring result(buffer ? buffer : L"", length);
        WindowsDeleteString(name);
        return result;
    }

    IServiceProvider* m_serviceProvider = nullptr;
    IVirtualDesktopManagerInternal* m_manager = nullptr;
    IApplicationViewCollection* m_viewCollection = nullptr;
    IVirtualDesktopNotificationService* m_notificationService = nullptr;
    DWORD m_notificationCookie = 0;
    const VersionIIDs* m_version = nullptr;
};

// =============================================================================
// Shared state
// =============================================================================

static std::atomic<bool> g_unloading;

static std::thread g_workerThread;
static HANDLE g_workerWakeEvent;
static std::atomic<bool> g_workerStop;
static std::atomic<bool> g_reconnectRequested;

// Everything that changes desktops goes through one queue, drained by the
// worker thread that owns the COM pointers -- chip clicks from the UI thread
// and keybinds from the hook thread alike.
enum class DesktopActionType {
    SwitchToIndex,
    Create,
    RemoveCurrent,
    MoveWindowToIndex,
};

struct DesktopAction {
    DesktopActionType type;
    int index;            // SwitchToIndex, MoveWindowToIndex; zero-based
    HWND hwnd = nullptr;  // MoveWindowToIndex: captured at the keypress
};

static std::mutex g_stateMutex;
static DesktopSnapshot g_snapshot;               // guarded by g_stateMutex
static std::vector<DesktopAction> g_actionQueue;  // guarded by g_stateMutex

static void OnSnapshotChanged(const DesktopSnapshot& snapshot);
static bool TryHookTaskbarViewDll(PCWSTR reason);

// Declared up here rather than beside the hook code because the worker loop
// reads it: while this is false the loop is doubling as the module poll and
// has to keep a tighter cadence than the shell-service backoff wants.
static std::atomic<bool> g_taskbarViewDllHooked;

// Set by the loader hooks, read only so the worker can say in the log which
// path got it moving. The loader thread must not do the logging itself -- the
// file write is exactly the kind of work that has no business happening inside
// a module load.
static std::atomic<bool> g_loaderNotified;

static void QueueDesktopAction(const DesktopAction& action) {
    {
        std::lock_guard<std::mutex> guard(g_stateMutex);
        g_actionQueue.push_back(action);
    }
    SetEvent(g_workerWakeEvent);
}

// Snapshot of the last known state, safe to call from any thread.
static DesktopSnapshot GetDesktopSnapshot() {
    std::lock_guard<std::mutex> guard(g_stateMutex);
    return g_snapshot;
}

// =============================================================================
// Taskbar visual tree
// =============================================================================

static FrameworkElement EnumChildElements(
    FrameworkElement element,
    std::function<bool(FrameworkElement)> enumCallback) {
    int childrenCount = Media::VisualTreeHelper::GetChildrenCount(element);

    for (int i = 0; i < childrenCount; i++) {
        auto child = Media::VisualTreeHelper::GetChild(element, i)
                         .try_as<FrameworkElement>();
        if (!child) {
            continue;
        }

        if (enumCallback(child)) {
            return child;
        }
    }

    return nullptr;
}

static FrameworkElement FindChildByName(FrameworkElement element,
                                        PCWSTR name) {
    return EnumChildElements(element, [name](FrameworkElement child) {
        return child.Name() == name;
    });
}

static FrameworkElement FindChildByClassName(FrameworkElement element,
                                             PCWSTR className) {
    return EnumChildElements(element, [className](FrameworkElement child) {
        return winrt::get_class_name(child) == className;
    });
}

// On secondary taskbars the element that holds the system icons is empty and
// only a couple of pixels wide.
static bool IsSecondaryTaskbar(XamlRoot xamlRoot) {
    FrameworkElement child = xamlRoot.Content().try_as<FrameworkElement>();
    if (child &&
        (child = FindChildByClassName(child, L"SystemTray.SystemTrayFrame")) &&
        (child = FindChildByName(child, L"SystemTrayFrameGrid")) &&
        (child = FindChildByName(child, L"ControlCenterButton"))) {
        return child.ActualWidth() < 5;
    }

    // Couldn't tell -- treat it as primary, which shows the pager rather than
    // silently hiding it.
    return false;
}

// =============================================================================
// The pager pill
// =============================================================================

// Element names are the styling contract with the taskbar styler. Keep them
// stable; the readme documents them.
static PCWSTR kPagerRootName = L"WhVdPagerRoot";
static PCWSTR kPagerBackgroundName = L"WhVdPagerBackground";
static PCWSTR kPagerItemsName = L"WhVdPagerItems";
static PCWSTR kChipName = L"WhVdDesktopButton";
static PCWSTR kChipLabelName = L"WhVdDesktopLabel";
static PCWSTR kHoverPopupName = L"WhVdHoverPopup";
static PCWSTR kHoverCardName = L"WhVdHoverCard";
static PCWSTR kHoverTitleName = L"WhVdHoverTitle";
static PCWSTR kHoverWindowsName = L"WhVdHoverWindows";

// Gap between the top of the pill and the bottom of the hover card.
static const double kHoverCardGap = 8;

struct PagerInstance {
    winrt::weak_ref<XamlRoot> xamlRoot;
    winrt::weak_ref<Controls::Grid> root;
    winrt::weak_ref<Controls::StackPanel> items;
    winrt::Windows::UI::Core::CoreDispatcher dispatcher{nullptr};
};

// A hover card asks the worker thread for the window list of one desktop. Only
// the newest request matters, so each one carries a generation and a stale
// answer is dropped on arrival.
struct HoverRequest {
    GUID desktopId;
    uint64_t generation;
    winrt::weak_ref<Controls::TextBlock> target;
    winrt::Windows::UI::Core::CoreDispatcher dispatcher{nullptr};
};

static std::atomic<uint64_t> g_hoverGeneration;
static std::mutex g_hoverMutex;
static bool g_hoverPending;          // guarded by g_hoverMutex
static HoverRequest g_hoverRequest;  // guarded by g_hoverMutex

static void RequestWindowList(HoverRequest request) {
    {
        std::lock_guard<std::mutex> guard(g_hoverMutex);
        g_hoverRequest = std::move(request);
        g_hoverPending = true;
    }
    SetEvent(g_workerWakeEvent);
}

// Mutated only on the taskbar UI thread, read from the worker thread.
static std::mutex g_pagersMutex;
static std::vector<PagerInstance> g_pagers;
static std::vector<winrt::weak_ref<XamlRoot>> g_ensureQueued;

static winrt::Windows::UI::Color MakeColor(uint8_t a,
                                           uint8_t r,
                                           uint8_t g,
                                           uint8_t b) {
    return winrt::Windows::UI::Color{a, r, g, b};
}

static Media::SolidColorBrush MakeBrush(uint8_t a,
                                        uint8_t r,
                                        uint8_t g,
                                        uint8_t b) {
    return Media::SolidColorBrush(MakeColor(a, r, g, b));
}

// The active desktop is marked by colouring its number, so the accent has to be
// legible on the taskbar -- the raw accent often isn't. AccentPalette is the
// shade ramp Windows derives from the accent chosen in Settings, eight RGBA
// entries lightest first: Light3, Light2, Light1, Accent, Dark1, Dark2, Dark3.
// Light2 reads well on a dark taskbar and Dark1 on a light one -- the same
// picks the widgets mod makes. Read live rather than through the XAML
// AccentTextFillColorPrimaryBrush resource, which in the taskbar's island
// doesn't reliably track the accent chosen in Settings.
static bool TaskbarUsesLightTheme() {
    DWORD value = 0;
    DWORD size = sizeof(value);
    return RegGetValue(
               HKEY_CURRENT_USER,
               L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
               L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &value,
               &size) == ERROR_SUCCESS &&
           value != 0;
}

static winrt::Windows::UI::Color AccentTextColor() {
    const bool light = TaskbarUsesLightTheme();

    BYTE palette[32];
    DWORD size = sizeof(palette);
    if (RegGetValue(HKEY_CURRENT_USER,
                    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Accent",
                    L"AccentPalette", RRF_RT_REG_BINARY, nullptr, palette,
                    &size) == ERROR_SUCCESS &&
        size >= 28) {
        const BYTE* c = palette + (light ? 4 : 1) * 4;
        return winrt::Windows::UI::Color{0xFF, c[0], c[1], c[2]};
    }

    try {
        winrt::Windows::UI::ViewManagement::UISettings uiSettings;
        return uiSettings.GetColorValue(
            light ? winrt::Windows::UI::ViewManagement::UIColorType::AccentDark1
                  : winrt::Windows::UI::ViewManagement::UIColorType::AccentLight2);
    } catch (...) {
        VDPLOG(L"UISettings accent failed %08X", winrt::to_hresult());
    }

    return winrt::Windows::UI::Color{0xFF, 0xFF, 0xFF, 0xFF};
}

static std::wstring DesktopDisplayName(const DesktopSnapshot& snapshot,
                                       size_t index) {
    const auto& name = snapshot.desktops[index].name;
    if (!name.empty()) {
        return name;
    }
    return L"Desktop " + std::to_wstring(index + 1);
}

static const auto kFadeInDuration = std::chrono::milliseconds(90);

// A plain opacity ramp. The built-in theme transitions carry a slide and run
// for ~300ms, which reads as sluggish on something as small as a chip.
static void FadeIn(UIElement element) {
    element.Opacity(0);

    Media::Animation::DoubleAnimation fade;
    fade.From(0.0);
    fade.To(1.0);
    fade.Duration(DurationHelper::FromTimeSpan(kFadeInDuration));
    Media::Animation::Storyboard::SetTarget(fade, element);
    Media::Animation::Storyboard::SetTargetProperty(fade, L"Opacity");

    Media::Animation::Storyboard storyboard;
    storyboard.Children().Append(fade);
    storyboard.Begin();
}

// The card's parts are reached through the tree rather than cached, so there is
// no second lifetime to reason about: if the pager root is alive, so is its card.
struct HoverCardParts {
    Controls::Primitives::Popup popup{nullptr};
    Controls::Border card{nullptr};
    Controls::TextBlock title{nullptr};
    Controls::TextBlock windows{nullptr};
};

static bool ResolveHoverCard(Controls::Grid root, HoverCardParts* parts) {
    parts->popup = root.Tag().try_as<Controls::Primitives::Popup>();
    if (!parts->popup) {
        VDPLOG(L"  hover card: no popup on root tag");
        return false;
    }

    parts->card = parts->popup.Child().try_as<Controls::Border>();
    if (!parts->card) {
        VDPLOG(L"  hover card: popup child is not a border");
        return false;
    }

    auto stack = parts->card.Child().try_as<Controls::StackPanel>();
    if (!stack || stack.Children().Size() < 2) {
        VDPLOG(L"  hover card: unexpected card content");
        return false;
    }

    parts->title = stack.Children().GetAt(0).try_as<Controls::TextBlock>();
    parts->windows = stack.Children().GetAt(1).try_as<Controls::TextBlock>();
    return parts->title && parts->windows;
}

static void ShowHoverCard(const PagerInstance& pager,
                          Controls::Button chip,
                          const DesktopSnapshot& snapshot,
                          size_t index);

static void ShowHoverCard(const PagerInstance& pager,
                          Controls::Button chip,
                          const DesktopSnapshot& snapshot,
                          size_t index) {
    auto root = pager.root.get();
    if (!root) {
        VDPLOG(L"  hover card: pager root is gone");
        return;
    }

    HoverCardParts hover;
    if (!ResolveHoverCard(root, &hover)) {
        return;
    }

    auto popup = hover.popup;
    auto card = hover.card;
    auto title = hover.title;
    auto windows = hover.windows;

    VDPLOG(L"ShowHoverCard for desktop %d", static_cast<int>(index) + 1);

    title.Text(DesktopDisplayName(snapshot, index));
    windows.Text(L"Loading…");

    // Position the card above the chip. The popup is a child of the pager root,
    // so the chip's offset within that root is the coordinate space we need.
    auto offset =
        chip.TransformToVisual(root).TransformPoint(
            winrt::Windows::Foundation::Point{0, 0});

    // Remember what to sit above; the SizeChanged handler applies it as soon as
    // the card has a height, and again whenever the content changes height.
    popup.Tag(winrt::box_value(static_cast<double>(offset.Y)));

    popup.HorizontalOffset(offset.X);
    if (card.ActualHeight() > 0) {
        popup.VerticalOffset(offset.Y - card.ActualHeight() - kHoverCardGap);
    }
    if (!popup.IsOpen() && g_settings.uiAnimations) {
        FadeIn(card);
    }
    popup.IsOpen(true);

    VDPLOG(L"  chip at (%.1f,%.1f), card height %.1f, offsets (%.1f,%.1f)",
           offset.X, offset.Y, card.ActualHeight(), popup.HorizontalOffset(),
           popup.VerticalOffset());

    HoverRequest request;
    request.desktopId = snapshot.desktops[index].id;
    request.generation = ++g_hoverGeneration;
    request.target = winrt::make_weak(windows);
    request.dispatcher = pager.dispatcher;
    RequestWindowList(std::move(request));
}

// One timer is enough: every taskbar shares the one UI thread, and the pointer
// can only be over a single chip at a time.
static DispatcherTimer g_hoverTimer{nullptr};
static PagerInstance g_hoverPendingPager;
static winrt::weak_ref<Controls::Button> g_hoverPendingChip;
static int g_hoverPendingIndex = -1;

static bool IsHoverCardOpen(const PagerInstance& pager) {
    auto root = pager.root.get();
    if (!root) {
        return false;
    }

    auto popup = root.Tag().try_as<Controls::Primitives::Popup>();
    return popup && popup.IsOpen();
}

static void CancelPendingHoverCard() {
    if (g_hoverTimer) {
        g_hoverTimer.Stop();
    }
    g_hoverPendingChip = nullptr;
    g_hoverPendingIndex = -1;
}

static void ShowPendingHoverCard() {
    auto chip = g_hoverPendingChip.get();
    int index = g_hoverPendingIndex;
    if (!chip || index < 0) {
        return;
    }

    auto snapshot = GetDesktopSnapshot();
    if (index < static_cast<int>(snapshot.desktops.size())) {
        ShowHoverCard(g_hoverPendingPager, chip, snapshot,
                      static_cast<size_t>(index));
    }
}

// Brushing past a chip shouldn't flash a card, so the first one waits. Once a
// card is up, moving along the row switches it immediately -- the same
// behaviour Windows tooltips have.
static void ScheduleHoverCard(const PagerInstance& pager,
                              Controls::Button chip,
                              int index) {
    g_hoverPendingPager = pager;
    g_hoverPendingChip = winrt::make_weak(chip);
    g_hoverPendingIndex = index;

    if (g_settings.hoverDelayMs <= 0 || IsHoverCardOpen(pager)) {
        CancelPendingHoverCard();
        g_hoverPendingPager = pager;
        g_hoverPendingChip = winrt::make_weak(chip);
        g_hoverPendingIndex = index;
        ShowPendingHoverCard();
        return;
    }

    if (!g_hoverTimer) {
        g_hoverTimer = DispatcherTimer();
        g_hoverTimer.Tick([](winrt::Windows::Foundation::IInspectable const&,
                             winrt::Windows::Foundation::IInspectable const&) {
            if (g_hoverTimer) {
                g_hoverTimer.Stop();
            }

            if (g_unloading) {
                return;
            }

            try {
                ShowPendingHoverCard();
            } catch (...) {
                VDPLOG(L"ShowPendingHoverCard error %08X", winrt::to_hresult());
            }
        });
    }

    g_hoverTimer.Stop();
    g_hoverTimer.Interval(
        std::chrono::milliseconds(g_settings.hoverDelayMs));
    g_hoverTimer.Start();
}

static void HideHoverCard(const PagerInstance& pager) {
    CancelPendingHoverCard();

    // Invalidate any in-flight window list so it can't repopulate a closed card.
    ++g_hoverGeneration;

    auto root = pager.root.get();
    if (!root) {
        return;
    }

    if (auto popup = root.Tag().try_as<Controls::Primitives::Popup>()) {
        popup.IsOpen(false);
    }
}

static void UpdateChips(const PagerInstance& pager,
                        const DesktopSnapshot& snapshot);

static const winrt::Windows::UI::Color kInactiveLabelColor{0xB3, 0xFF, 0xFF,
                                                          0xFF};
static const auto kLabelFadeDuration = std::chrono::milliseconds(100);

// Each label owns its brush, so its colour can be animated in place.
static void SetLabelColor(Controls::TextBlock label,
                          winrt::Windows::UI::Color color,
                          bool animate) {
    auto brush = label.Foreground().try_as<Media::SolidColorBrush>();
    if (!brush) {
        label.Foreground(Media::SolidColorBrush(color));
        return;
    }

    if (!animate) {
        brush.Color(color);
        return;
    }

    Media::Animation::ColorAnimation fade;
    fade.To(color);
    fade.Duration(DurationHelper::FromTimeSpan(kLabelFadeDuration));
    fade.EnableDependentAnimation(true);
    Media::Animation::Storyboard::SetTarget(fade, brush);
    Media::Animation::Storyboard::SetTargetProperty(fade, L"Color");

    Media::Animation::Storyboard storyboard;
    storyboard.Children().Append(fade);
    storyboard.Begin();
}

static Controls::Button CreateChip(const PagerInstance& pager, int index) {
    Controls::TextBlock label;
    label.Name(kChipLabelName);
    label.FontSize(12);
    label.Foreground(Media::SolidColorBrush(kInactiveLabelColor));
    label.HorizontalAlignment(HorizontalAlignment::Center);
    label.VerticalAlignment(VerticalAlignment::Center);

    Controls::Button chip;
    chip.Name(kChipName);
    chip.Content(label);
    chip.Height(g_settings.chipSize);
    chip.MinWidth(g_settings.chipSize);
    chip.MinHeight(0);
    chip.Padding(Thickness{6, 0, 6, 0});
    chip.BorderThickness(Thickness{});
    chip.CornerRadius(CornerRadius{
        g_settings.chipSize / 2, g_settings.chipSize / 2,
        g_settings.chipSize / 2, g_settings.chipSize / 2});
    // No fill: the active desktop reads as an accent coloured number.
    // Transparent rather than null so the whole chip still hit-tests for
    // clicks and hover.
    chip.Background(MakeBrush(0x00, 0x00, 0x00, 0x00));
    chip.HorizontalContentAlignment(HorizontalAlignment::Center);
    chip.VerticalContentAlignment(VerticalAlignment::Center);
    chip.Margin(index == 0 ? Thickness{}
                           : Thickness{g_settings.chipSpacing, 0, 0, 0});

    // Chips are reused by position as desktops come and go, so they act on
    // their index, not on the GUID of whichever desktop was there when the
    // chip was made.
    chip.Click([index, pager](winrt::Windows::Foundation::IInspectable const&,
                              RoutedEventArgs const&) {
        if (g_unloading) {
            return;
        }

        // Acknowledge the click right away rather than when the slide ends.
        auto snapshot = GetDesktopSnapshot();
        if (index < static_cast<int>(snapshot.desktops.size())) {
            snapshot.currentIndex = index;
            UpdateChips(pager, snapshot);
        }

        QueueDesktopAction({DesktopActionType::SwitchToIndex, index});
    });

    chip.PointerEntered(
        [index, pager](winrt::Windows::Foundation::IInspectable const& sender,
                       Input::PointerRoutedEventArgs const&) {
            if (g_unloading || !g_settings.hoverCard) {
                return;
            }

            if (auto chipElement = sender.try_as<Controls::Button>()) {
                ScheduleHoverCard(pager, chipElement, index);
            }
        });

    chip.PointerExited([pager](winrt::Windows::Foundation::IInspectable const&,
                               Input::PointerRoutedEventArgs const&) {
        HideHoverCard(pager);
    });

    return chip;
}

// Brings the chip row in line with the snapshot by editing it, not replacing
// it: chips are appended or removed only at the end, and the rest just have
// their label and colour updated. That keeps a switch to a pure colour fade,
// and lets the row's transitions animate only the chips that actually came or
// went.
static void UpdateChips(const PagerInstance& pager,
                        const DesktopSnapshot& snapshot) {
    auto items = pager.items.get();
    if (!items) {
        return;
    }

    auto children = items.Children();
    size_t count = snapshot.desktops.size();

    while (children.Size() > count) {
        children.RemoveAtEnd();
    }
    // Only chips for desktops created after the row first appeared fade in;
    // the initial build just shows up.
    const bool fadeNewChips = g_settings.uiAnimations && children.Size() > 0;
    while (children.Size() < count) {
        auto chip = CreateChip(pager, static_cast<int>(children.Size()));
        children.Append(chip);
        if (fadeNewChips) {
            FadeIn(chip);
        }
    }

    const auto accent = AccentTextColor();

    for (size_t i = 0; i < count; i++) {
        auto chip = children.GetAt(static_cast<uint32_t>(i))
                        .try_as<Controls::Button>();
        auto label = chip ? chip.Content().try_as<Controls::TextBlock>()
                          : nullptr;
        if (!label) {
            continue;
        }

        std::wstring displayName = DesktopDisplayName(snapshot, i);
        std::wstring text =
            g_settings.labelIsName ? displayName : std::to_wstring(i + 1);
        if (label.Text() != text) {
            label.Text(text);
        }
        chip.SetValue(Automation::AutomationProperties::NameProperty(),
                      winrt::box_value(winrt::hstring(displayName)));

        bool isActive = static_cast<int>(i) == snapshot.currentIndex;
        std::optional<bool> wasActive;
        if (auto tag = label.Tag()) {
            wasActive = tag.try_as<bool>();
        }
        if (wasActive && *wasActive == isActive) {
            continue;
        }

        label.Tag(winrt::box_value(isActive));
        label.FontWeight(isActive
                             ? winrt::Windows::UI::Text::FontWeights::SemiBold()
                             : winrt::Windows::UI::Text::FontWeights::Normal());
        // A chip seen for the first time snaps to its colour; only a change of
        // state fades.
        SetLabelColor(label, isActive ? accent : kInactiveLabelColor,
                      g_settings.uiAnimations && wasActive.has_value());
    }
}

static void UpdatePagerUI(const PagerInstance& pager,
                          const DesktopSnapshot& snapshot) {
    auto root = pager.root.get();
    auto items = pager.items.get();
    if (!root || !items) {
        return;
    }

    // A disabled pager is still built, just collapsed: the taskbar measures
    // only while it is being assembled, so a pager that didn't exist then
    // would have no way to appear when the setting is switched back on.
    bool hide = g_unloading || !g_settings.pagerEnabled ||
                snapshot.desktops.empty() ||
                (g_settings.hideWhenSingleDesktop &&
                 snapshot.desktops.size() == 1);
    root.Visibility(hide ? Visibility::Collapsed : Visibility::Visible);
    if (hide) {
        return;
    }

    UpdateChips(pager, snapshot);
}

// Dumps the direct children of an element once per element, so a failure to
// find the insertion point can be diagnosed from the log instead of guessed at.
static void LogChildren(PCWSTR what, FrameworkElement element) {
    if (!element) {
        VDPLOG(L"%s: <null>", what);
        return;
    }

    std::wstring text = std::wstring(winrt::get_class_name(element)) + L" #" +
                        std::wstring(element.Name()) + L" ->";

    EnumChildElements(element, [&text](FrameworkElement child) {
        text += L" [" + std::wstring(winrt::get_class_name(child)) + L" #" +
                std::wstring(child.Name()) + L"]";
        return false;
    });

    VDPLOG(L"%s: %s", what, text.c_str());
}

// =============================================================================
// Carrying the taskbar across a mod reload
// =============================================================================

// The pager is only ever built from TaskbarFrame::MeasureOverride, and a
// taskbar measures while it is being assembled and then essentially stops.
// That is fine at startup -- the hook is in before the frame exists -- but it
// leaves a hole every time the mod is reloaded into a running explorer, which
// is exactly what Windhawk does when you hit Compile. The hook goes in
// instantly, no measure pass ever comes, and the taskbar sits there without a
// pager until something unrelated happens to invalidate layout. Measured gap
// on one reload: 27 seconds, and nothing bounds it.
//
// So the XamlRoot is handed forward instead of being rediscovered. It belongs
// to the taskbar rather than to this mod, so it outlives the DLL; parking a
// reference on the taskbar window lets the next load pick up where the last one
// left off and build the pager immediately, without waiting to be asked.
//
// The reference is deliberately strong. A weak one would be tidier in the
// abstract, but what it would be protecting against is keeping alive an object
// owned by a window that lives as long as explorer does -- so there is nothing
// to protect against, and a strong pointer spares the next load a resolution
// step that could quietly come back empty.
static const wchar_t kHandoffPropName[] = L"WhDesktopsHandoff";

static const UINT kMaxHandoffRoots = 8;

// Publish runs on the UI thread when a pager is created; consume runs on the
// loading thread at after-init. Normally they are ordered -- consume takes the
// prop before any pager exists to publish -- but a measure pass landing at the
// same moment as after-init puts both on the same block, and read-free-write
// against take-free is a double free. The window is narrow and the fix is one
// lock; both racers are in the same load, so a lock in this DLL covers it.
static std::mutex g_handoffMutex;

struct PagerHandoff {
    ::IUnknown* dispatcher;
    UINT rootCount;
    ::IUnknown* roots[kMaxHandoffRoots];
};

static HWND FindCurrentProcessTaskbarWnd() {
    HWND taskbarWnd = nullptr;

    EnumWindows(
        [](HWND wnd, LPARAM param) -> BOOL {
            DWORD processId = 0;
            WCHAR className[32];
            if (GetWindowThreadProcessId(wnd, &processId) &&
                processId == GetCurrentProcessId() &&
                GetClassName(wnd, className, ARRAYSIZE(className)) &&
                _wcsicmp(className, L"Shell_TrayWnd") == 0) {
                *reinterpret_cast<HWND*>(param) = wnd;
                return FALSE;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&taskbarWnd));

    return taskbarWnd;
}

// The block lives on the process heap, not the CRT's: it has to outlive the
// unload of the DLL whose CRT allocated it.
static void FreeHandoff(PagerHandoff* handoff) {
    if (!handoff) {
        return;
    }

    if (handoff->dispatcher) {
        handoff->dispatcher->Release();
    }
    for (UINT i = 0; i < handoff->rootCount; i++) {
        if (handoff->roots[i]) {
            handoff->roots[i]->Release();
        }
    }

    HeapFree(GetProcessHeap(), 0, handoff);
}

// Runs on the taskbar UI thread, once a pager has been added to g_pagers.
// Rewritten wholesale each time rather than appended to, so what is parked is
// always just "the pagers that exist right now".
static void PublishHandoff() {
    HWND taskbarWnd = FindCurrentProcessTaskbarWnd();
    if (!taskbarWnd) {
        return;
    }

    std::vector<PagerInstance> pagers;
    {
        std::lock_guard<std::mutex> guard(g_pagersMutex);
        pagers = g_pagers;
    }

    auto* handoff = static_cast<PagerHandoff*>(
        HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(PagerHandoff)));
    if (!handoff) {
        return;
    }

    for (const auto& pager : pagers) {
        if (handoff->rootCount >= kMaxHandoffRoots) {
            break;
        }

        auto xamlRoot = pager.xamlRoot.get();
        if (!xamlRoot) {
            continue;
        }

        // Every taskbar shares the one UI thread, so one dispatcher covers all
        // of them -- the same assumption Wh_ModSettingsChanged already makes.
        if (!handoff->dispatcher && pager.dispatcher) {
            handoff->dispatcher =
                static_cast<::IUnknown*>(winrt::get_abi(pager.dispatcher));
            handoff->dispatcher->AddRef();
        }

        auto* rootAbi = static_cast<::IUnknown*>(winrt::get_abi(xamlRoot));
        rootAbi->AddRef();
        handoff->roots[handoff->rootCount++] = rootAbi;
    }

    if (!handoff->rootCount || !handoff->dispatcher) {
        FreeHandoff(handoff);
        return;
    }

    std::lock_guard<std::mutex> guard(g_handoffMutex);

    FreeHandoff(
        static_cast<PagerHandoff*>(GetProp(taskbarWnd, kHandoffPropName)));

    if (!SetProp(taskbarWnd, kHandoffPropName, handoff)) {
        VDPLOG(L"SetProp failed: %u", GetLastError());
        FreeHandoff(handoff);
    }
}

// Runs on the taskbar UI thread.
// The active chip keeps whatever colour it was given until its state changes,
// so an accent change has to be pushed to it. Forgetting each label's state
// makes the next update treat every chip as new and give it its colour afresh.
static void RecolorPagers() {
    std::vector<PagerInstance> pagers;
    {
        std::lock_guard<std::mutex> guard(g_pagersMutex);
        pagers = g_pagers;
    }

    for (const auto& pager : pagers) {
        if (!pager.dispatcher) {
            continue;
        }

        try {
            pager.dispatcher.TryRunAsync(
                winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,
                [pager]() {
                    try {
                        auto items = pager.items.get();
                        if (!items || g_unloading) {
                            return;
                        }
                        for (auto child : items.Children()) {
                            auto chip = child.try_as<Controls::Button>();
                            if (auto label =
                                    chip ? chip.Content()
                                               .try_as<Controls::TextBlock>()
                                         : nullptr) {
                                label.Tag(nullptr);
                            }
                        }
                        UpdatePagerUI(pager, GetDesktopSnapshot());
                    } catch (...) {
                        VDPLOG(L"Recolor error %08X", winrt::to_hresult());
                    }
                });
        } catch (...) {
            VDPLOG(L"Dispatch error %08X", winrt::to_hresult());
        }
    }
}

// Subscribed from the taskbar UI thread the first time a pager is built, where
// WinRT is known to be up. ColorValuesChanged covers both a new accent and a
// light/dark switch, and arrives on a background thread.
static winrt::Windows::UI::ViewManagement::UISettings g_uiSettings{nullptr};
static winrt::event_token g_colorValuesChangedToken{};

static void WatchAccentColor() {
    if (g_uiSettings) {
        return;
    }

    try {
        g_uiSettings = winrt::Windows::UI::ViewManagement::UISettings();
        g_colorValuesChangedToken =
            g_uiSettings.ColorValuesChanged([](auto&&, auto&&) {
                if (!g_unloading) {
                    VDPLOG(L"Accent colour changed");
                    RecolorPagers();
                }
            });
    } catch (...) {
        VDPLOG(L"ColorValuesChanged subscribe failed %08X",
               winrt::to_hresult());
        g_uiSettings = nullptr;
    }
}

static void StopWatchingAccentColor() {
    if (!g_uiSettings) {
        return;
    }

    try {
        g_uiSettings.ColorValuesChanged(g_colorValuesChangedToken);
    } catch (...) {
    }
    g_uiSettings = nullptr;
}

static void EnsurePagerForXamlRoot(XamlRoot xamlRoot) {
    if (g_unloading || !xamlRoot) {
        VDPLOG(L"  no xaml root");
        return;
    }

    auto content = xamlRoot.Content().try_as<Controls::Panel>();
    if (!content) {
        LogChildren(L"  content is not a panel",
                    xamlRoot.Content().try_as<FrameworkElement>());
        return;
    }

    // Only the taskbar islands, not any other XAML root explorer may host.
    // Match on class rather than x:Name: on 26200 the frames at this level are
    // unnamed, so a name lookup rejects every taskbar.
    if (!FindChildByClassName(content, L"Taskbar.TaskbarFrame") &&
        !FindChildByClassName(content, L"SystemTray.SystemTrayFrame")) {
        LogChildren(L"  not a taskbar island", content);
        return;
    }

    // Survives mod reloads and any state we may have lost track of.
    if (FindChildByName(content, kPagerRootName)) {
        VDPLOG(L"  pager already present");
        return;
    }

    if (!g_settings.showOnSecondaryTaskbars && IsSecondaryTaskbar(xamlRoot)) {
        VDPLOG(L"  secondary taskbar, skipped");
        return;
    }

    Controls::StackPanel items;
    items.Name(kPagerItemsName);
    items.Orientation(Controls::Orientation::Horizontal);
    items.VerticalAlignment(VerticalAlignment::Center);

    Controls::TextBlock hoverTitle;
    hoverTitle.Name(kHoverTitleName);
    hoverTitle.FontSize(13);
    hoverTitle.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
    hoverTitle.Foreground(MakeBrush(0xFF, 0xFF, 0xFF, 0xFF));

    Controls::TextBlock hoverWindows;
    hoverWindows.Name(kHoverWindowsName);
    hoverWindows.FontSize(12);
    hoverWindows.Foreground(MakeBrush(0xB3, 0xFF, 0xFF, 0xFF));
    hoverWindows.Margin(Thickness{0, 4, 0, 0});

    Controls::StackPanel hoverStack;
    hoverStack.Orientation(Controls::Orientation::Vertical);
    hoverStack.Children().Append(hoverTitle);
    hoverStack.Children().Append(hoverWindows);

    Controls::Border hoverCard;
    hoverCard.Name(kHoverCardName);
    // The card can only be measured once the popup has opened and given it a
    // layout context, so its final position is set when its size is known.
    hoverCard.SizeChanged(
        [](winrt::Windows::Foundation::IInspectable const& sender,
           SizeChangedEventArgs const& args) {
            auto card = sender.try_as<FrameworkElement>();
            if (!card) {
                return;
            }

            auto popup = card.Parent().try_as<Controls::Primitives::Popup>();
            if (!popup) {
                return;
            }

            auto anchor = popup.Tag().try_as<double>();
            if (!anchor) {
                return;
            }

            popup.VerticalOffset(anchor.value() - args.NewSize().Height -
                                 kHoverCardGap);
        });
    hoverCard.Child(hoverStack);
    hoverCard.Padding(Thickness{12, 8, 12, 10});
    hoverCard.CornerRadius(CornerRadius{8, 8, 8, 8});
    hoverCard.Background(MakeBrush(0xF0, 0x20, 0x20, 0x20));
    hoverCard.BorderBrush(MakeBrush(0x33, 0xFF, 0xFF, 0xFF));
    hoverCard.BorderThickness(Thickness{1, 1, 1, 1});
    hoverCard.MaxWidth(420);

    // The taskbar island is only as tall as the taskbar, so a popup that is
    // constrained to it is clipped away entirely when placed above a chip.
    // This is what lets the card render over the desktop instead.
    Controls::Primitives::Popup hoverPopup;
    hoverPopup.Name(kHoverPopupName);
    hoverPopup.Child(hoverCard);
    hoverPopup.ShouldConstrainToRootBounds(false);
    hoverPopup.IsLightDismissEnabled(false);

    Controls::Border background;
    background.Name(kPagerBackgroundName);
    background.Child(items);
    // Deliberately unstyled: the pill's look -- background, padding, corners,
    // margin -- belongs to the Shell Styler theme, which targets this element
    // by name. This mod only draws the numbers.
    background.VerticalAlignment(VerticalAlignment::Center);

    Controls::Grid root;
    root.Name(kPagerRootName);
    root.Children().Append(background);
    root.Children().Append(hoverPopup);
    root.Tag(hoverPopup);
    root.HorizontalAlignment(HorizontalAlignment::Left);
    root.VerticalAlignment(VerticalAlignment::Stretch);
    root.Margin(Thickness{8, 0, 0, 0});

    content.Children().Append(root);

    PagerInstance pager;
    pager.xamlRoot = winrt::make_weak(xamlRoot);
    pager.root = winrt::make_weak(root);
    pager.items = winrt::make_weak(items);
    pager.dispatcher = root.Dispatcher();

    {
        std::lock_guard<std::mutex> guard(g_pagersMutex);
        g_pagers.push_back(pager);
    }

    VDPLOG(L"Pager added to a taskbar (%d total)",
           static_cast<int>(g_pagers.size()));

    WatchAccentColor();
    UpdatePagerUI(pager, GetDesktopSnapshot());
    PublishHandoff();
}

// Runs on whatever thread loads the mod. Claims whatever the previous load
// parked and rebuilds from it on the UI thread. Independent of the taskbar
// hook: this is the path for a taskbar that already exists and has no reason
// left to measure.
static void ConsumeHandoff() {
    HWND taskbarWnd = FindCurrentProcessTaskbarWnd();
    if (!taskbarWnd) {
        return;
    }

    PagerHandoff* handoff = nullptr;
    {
        std::lock_guard<std::mutex> guard(g_handoffMutex);
        handoff =
            static_cast<PagerHandoff*>(RemoveProp(taskbarWnd,
                                                  kHandoffPropName));
    }

    if (!handoff) {
        return;
    }

    VDPLOG(L"Handoff from the previous load: %u taskbar(s)",
           handoff->rootCount);

    winrt::Windows::UI::Core::CoreDispatcher dispatcher{nullptr};
    std::vector<XamlRoot> roots;

    try {
        winrt::copy_from_abi(dispatcher, handoff->dispatcher);
        for (UINT i = 0; i < handoff->rootCount; i++) {
            XamlRoot root{nullptr};
            winrt::copy_from_abi(root, handoff->roots[i]);
            roots.push_back(root);
        }
    } catch (...) {
        VDPLOG(L"Handoff decode error %08X", winrt::to_hresult());
    }

    FreeHandoff(handoff);

    if (!dispatcher || roots.empty()) {
        return;
    }

    // A XamlRoot whose taskbar has been torn down in the meantime needs no
    // detection here: EnsurePagerForXamlRoot finds no content panel and
    // returns.
    try {
        dispatcher.TryRunAsync(
            winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,
            [roots]() {
                for (const auto& root : roots) {
                    try {
                        EnsurePagerForXamlRoot(root);
                    } catch (...) {
                        VDPLOG(L"Handoff rebuild error %08X",
                               winrt::to_hresult());
                    }
                }
            });
    } catch (...) {
        VDPLOG(L"Handoff dispatch error %08X", winrt::to_hresult());
    }
}

static void RemovePagerElements() {
    CancelPendingHoverCard();
    if (g_hoverTimer) {
        g_hoverTimer = nullptr;
    }

    std::vector<PagerInstance> pagers;
    {
        std::lock_guard<std::mutex> guard(g_pagersMutex);
        pagers.swap(g_pagers);
    }

    for (const auto& pager : pagers) {
        auto root = pager.root.get();
        if (!root) {
            continue;
        }

        auto parent = Media::VisualTreeHelper::GetParent(root)
                          .try_as<Controls::Panel>();
        if (!parent) {
            continue;
        }

        uint32_t index = 0;
        if (parent.Children().IndexOf(root, index)) {
            parent.Children().RemoveAt(index);
        }
    }
}

static bool HasPagerForXamlRoot(XamlRoot xamlRoot) {
    std::lock_guard<std::mutex> guard(g_pagersMutex);

    g_pagers.erase(std::remove_if(g_pagers.begin(), g_pagers.end(),
                                  [](const PagerInstance& pager) {
                                      return !pager.root.get();
                                  }),
                   g_pagers.end());

    return std::find_if(g_pagers.begin(), g_pagers.end(),
                        [&xamlRoot](const PagerInstance& pager) {
                            auto other = pager.xamlRoot.get();
                            return other && other == xamlRoot;
                        }) != g_pagers.end();
}

static bool MarkEnsureQueued(XamlRoot xamlRoot) {
    std::lock_guard<std::mutex> guard(g_pagersMutex);

    g_ensureQueued.erase(
        std::remove_if(g_ensureQueued.begin(), g_ensureQueued.end(),
                       [](const winrt::weak_ref<XamlRoot>& ref) {
                           return !ref.get();
                       }),
        g_ensureQueued.end());

    for (const auto& ref : g_ensureQueued) {
        auto other = ref.get();
        if (other && other == xamlRoot) {
            return false;
        }
    }

    g_ensureQueued.push_back(winrt::make_weak(xamlRoot));
    return true;
}

static void ClearEnsureQueued(XamlRoot xamlRoot) {
    std::lock_guard<std::mutex> guard(g_pagersMutex);

    g_ensureQueued.erase(
        std::remove_if(g_ensureQueued.begin(), g_ensureQueued.end(),
                       [&xamlRoot](const winrt::weak_ref<XamlRoot>& ref) {
                           auto other = ref.get();
                           return !other || other == xamlRoot;
                       }),
        g_ensureQueued.end());
}

// Building elements inside a measure pass risks an AG_E_LAYOUT_CYCLE fail-fast,
// so the work is deferred to the dispatcher, at most once per taskbar.
static void QueueEnsurePager(FrameworkElement element) {
    auto xamlRoot = element.XamlRoot();
    if (!xamlRoot || g_unloading || HasPagerForXamlRoot(xamlRoot)) {
        return;
    }

    if (!MarkEnsureQueued(xamlRoot)) {
        return;
    }

    VDPLOG(L"Queueing pager creation");

    auto xamlRootWeak = winrt::make_weak(xamlRoot);

    try {
        element.Dispatcher().TryRunAsync(
            winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,
            [xamlRootWeak]() {
                auto xamlRoot = xamlRootWeak.get();
                if (!xamlRoot) {
                    return;
                }

                ClearEnsureQueued(xamlRoot);

                try {
                    EnsurePagerForXamlRoot(xamlRoot);
                } catch (...) {
                    VDPLOG(L"EnsurePagerForXamlRoot error %08X",
                           winrt::to_hresult());
                }
            });
    } catch (...) {
        ClearEnsureQueued(xamlRoot);
        VDPLOG(L"QueueEnsurePager error %08X", winrt::to_hresult());
    }
}

// Called on the worker thread whenever the desktop list or the active desktop
// changes.
static void OnSnapshotChanged(const DesktopSnapshot& snapshot) {
    std::vector<PagerInstance> pagers;
    (void)snapshot;
    {
        std::lock_guard<std::mutex> guard(g_pagersMutex);
        pagers = g_pagers;
    }

    VDPLOG(L"Desktop state changed: %d desktop(s), current %d, %d pager(s)",
           static_cast<int>(snapshot.desktops.size()), snapshot.currentIndex + 1,
           static_cast<int>(pagers.size()));

    for (const auto& pager : pagers) {
        if (!pager.dispatcher) {
            continue;
        }

        try {
            pager.dispatcher.TryRunAsync(
                winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,
                [pager, snapshot]() {
                    try {
                        UpdatePagerUI(pager, snapshot);
                    } catch (...) {
                        VDPLOG(L"UpdatePagerUI error %08X",
                               winrt::to_hresult());
                    }
                });
        } catch (...) {
            VDPLOG(L"Dispatch error %08X", winrt::to_hresult());
        }
    }
}

// =============================================================================
// Window list for the hover card
// =============================================================================

struct WindowScan {
    IVirtualDesktopManagerPublic* manager;
    GUID desktopId;
    std::vector<std::wstring> titles;
};

static bool IsAltTabWindow(HWND hwnd) {
    if (!IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER)) {
        return false;
    }

    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) {
        return false;
    }

    return GetWindowTextLength(hwnd) > 0;
}

static BOOL CALLBACK CollectWindowProc(HWND hwnd, LPARAM lParam) {
    auto* scan = reinterpret_cast<WindowScan*>(lParam);
    if (scan->titles.size() >= 12 || !IsAltTabWindow(hwnd)) {
        return TRUE;
    }

    GUID desktopId{};
    if (FAILED(scan->manager->GetWindowDesktopId(hwnd, &desktopId)) ||
        !IsEqualGUID(desktopId, scan->desktopId)) {
        return TRUE;
    }

    wchar_t title[256];
    int length = GetWindowText(hwnd, title, ARRAYSIZE(title));
    if (length > 0) {
        scan->titles.emplace_back(title, length);
    }

    return TRUE;
}

// Runs on the worker thread. Windows on other desktops are cloaked, so their
// titles are still readable even though they cannot be captured as thumbnails.
static std::wstring DescribeWindowsOnDesktop(
    IVirtualDesktopManagerPublic* manager,
    const GUID& desktopId) {
    if (!manager) {
        return std::wstring();
    }

    WindowScan scan{manager, desktopId, {}};
    EnumWindows(CollectWindowProc, reinterpret_cast<LPARAM>(&scan));

    if (scan.titles.empty()) {
        return L"No windows";
    }

    std::wstring text;
    for (size_t i = 0; i < scan.titles.size(); i++) {
        if (i) {
            text += L'\n';
        }

        std::wstring title = scan.titles[i];
        if (title.size() > 48) {
            title = title.substr(0, 47) + L"\u2026";
        }
        text += title;
    }

    return text;
}

// =============================================================================
// Worker thread
// =============================================================================

static bool IsSwitchAction(const DesktopAction& action) {
    return action.type == DesktopActionType::SwitchToIndex;
}

// An animated switch blocks the worker for its whole slide, so tapping Win+2
// Win+3 Win+4 quickly would otherwise queue three slides back to back. Only
// where you ended up matters: a run of consecutive switches collapses to its
// last one. Creates and removes are never dropped.
static void CoalesceSwitches(std::vector<DesktopAction>* actions) {
    std::vector<DesktopAction> result;
    for (const auto& action : *actions) {
        if (IsSwitchAction(action) && !result.empty() &&
            IsSwitchAction(result.back())) {
            result.back() = action;
        } else {
            result.push_back(action);
        }
    }
    actions->swap(result);
}

// Moves the pager's highlight to the target the moment a switch is requested,
// instead of after the slide finishes and the next poll notices. lastSnapshot
// is updated too, so the real read that follows the switch finds nothing new
// and doesn't rebuild the row a second time.
static void PublishOptimisticSwitch(const DesktopAction& action,
                                    DesktopSnapshot* lastSnapshot) {
    int target =
        action.type == DesktopActionType::SwitchToIndex ? action.index : -1;

    if (target < 0 ||
        target >= static_cast<int>(lastSnapshot->desktops.size()) ||
        target == lastSnapshot->currentIndex) {
        return;
    }

    lastSnapshot->currentIndex = target;
    {
        std::lock_guard<std::mutex> guard(g_stateMutex);
        g_snapshot = *lastSnapshot;
    }
    OnSnapshotChanged(*lastSnapshot);
}

static const DWORD kNotifiedPollIntervalMs = 5000;

static void WakeWorkerForDesktopChange() {
    if (HANDLE event = g_workerWakeEvent) {
        SetEvent(event);
    }
}

// A moved window leaves focus on something now invisible. Hand it to the next
// window down the z-order that's on this desktop, or to the desktop itself,
// the way closing a window would.
static void FocusNextWindow(HWND moved,
                            IVirtualDesktopManagerPublic* publicManager) {
    HWND next = moved;
    while ((next = GetWindow(next, GW_HWNDNEXT)) != nullptr) {
        if (!IsAltTabWindow(next) || IsIconic(next)) {
            continue;
        }
        BOOL onCurrent = FALSE;
        if (publicManager &&
            SUCCEEDED(publicManager->IsWindowOnCurrentVirtualDesktop(
                next, &onCurrent)) &&
            onCurrent) {
            VirtualDesktopEngine::BringToForeground(next);
            return;
        }
    }

    VirtualDesktopEngine::BringToForeground(GetShellWindow());
}

static void WorkerThreadProc() {
    HRESULT hrInit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hrInit)) {
        VDPLOG(L"CoInitializeEx failed: 0x%08X", hrInit);
        return;
    }

    VirtualDesktopEngine engine;
    DesktopSnapshot lastSnapshot;
    int reconnectBackoffMs = 1000;
    ULONGLONG nextConnectAttemptTick = 0;

    IVirtualDesktopManagerPublic* publicManager = nullptr;
    CoCreateInstance(CLSID_VirtualDesktopManagerPublic, nullptr,
                     CLSCTX_INPROC_SERVER, IID_IVirtualDesktopManagerPublic,
                     reinterpret_cast<void**>(&publicManager));

    while (!g_workerStop) {
        // Cheap: a GetModuleHandle call until it succeeds exactly once.
        if (!g_unloading) {
            TryHookTaskbarViewDll(g_loaderNotified ? L"loader notification"
                                                   : L"worker poll");
        }

        if (g_reconnectRequested.exchange(false)) {
            // A changed version override only takes effect on a fresh probe.
            engine.Disconnect();
            nextConnectAttemptTick = 0;
            reconnectBackoffMs = 1000;
        }

        if (!engine.IsConnected()) {
            // The shell service can be unavailable early in the session or
            // while explorer is restarting, so connecting backs off. The
            // backoff is held as a deadline rather than as the length of this
            // loop's sleep, because the sleep is not only the shell service's
            // to set: until the taskbar hook is in, this loop is also what
            // polls for Taskbar.View.dll. Those two were coupled, and at boot
            // they race -- the 2s backoff after a second failed connect is
            // exactly the delay that pushed one boot's hook past the taskbar's
            // final measure pass, leaving a mod that ran all session and drew
            // nothing.
            ULONGLONG now = GetTickCount64();
            bool connected = false;
            if (now >= nextConnectAttemptTick) {
                connected = engine.Connect();
                if (!connected) {
                    nextConnectAttemptTick = now + reconnectBackoffMs;
                    if (reconnectBackoffMs < 30000) {
                        reconnectBackoffMs *= 2;
                    }
                }
            }

            if (!connected) {
                // Stay responsive to unload, and to the loader hooks' wake.
                WaitForSingleObject(
                    g_workerWakeEvent,
                    g_taskbarViewDllHooked
                        ? static_cast<DWORD>(reconnectBackoffMs)
                        : static_cast<DWORD>(g_settings.pollIntervalMs));
                continue;
            }

            reconnectBackoffMs = 1000;
            nextConnectAttemptTick = 0;
            lastSnapshot = DesktopSnapshot();
        }

        std::vector<DesktopAction> actions;
        {
            std::lock_guard<std::mutex> guard(g_stateMutex);
            actions.swap(g_actionQueue);
        }
        CoalesceSwitches(&actions);

        for (const auto& action : actions) {
            PublishOptimisticSwitch(action, &lastSnapshot);

            switch (action.type) {
                case DesktopActionType::SwitchToIndex:
                    engine.SwitchToIndex(action.index);
                    break;
                case DesktopActionType::Create:
                    engine.CreateAndSwitch();
                    break;
                case DesktopActionType::RemoveCurrent:
                    engine.RemoveCurrent();
                    break;
                case DesktopActionType::MoveWindowToIndex:
                    if (engine.MoveWindowToIndex(action.hwnd, action.index,
                                                 publicManager)) {
                        FocusNextWindow(action.hwnd, publicManager);
                    }
                    break;
            }
        }

        HoverRequest hoverRequest;
        bool haveHoverRequest = false;
        {
            std::lock_guard<std::mutex> guard(g_hoverMutex);
            if (g_hoverPending) {
                hoverRequest = g_hoverRequest;
                g_hoverPending = false;
                haveHoverRequest = true;
            }
        }

        if (haveHoverRequest && hoverRequest.dispatcher) {
            std::wstring text =
                DescribeWindowsOnDesktop(publicManager, hoverRequest.desktopId);

            auto target = hoverRequest.target;
            uint64_t generation = hoverRequest.generation;

            try {
                hoverRequest.dispatcher.TryRunAsync(
                    winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,
                    [target, text, generation]() {
                        if (g_hoverGeneration != generation) {
                            return;  // the pointer has already moved on
                        }
                        if (auto textBlock = target.get()) {
                            textBlock.Text(text);
                        }
                    });
            } catch (...) {
                VDPLOG(L"Hover dispatch error %08X", winrt::to_hresult());
            }
        }

        DesktopSnapshot snapshot;
        if (!engine.ReadSnapshot(&snapshot)) {
            // Most likely explorer's shell service went away underneath us.
            engine.Disconnect();
            continue;
        }

        if (!(snapshot == lastSnapshot)) {
            {
                std::lock_guard<std::mutex> guard(g_stateMutex);
                g_snapshot = snapshot;
            }
            OnSnapshotChanged(snapshot);
            lastSnapshot = std::move(snapshot);
        }

        // With notifications the shell wakes this loop on every change, and
        // the timeout is only a safety net.
        WaitForSingleObject(g_workerWakeEvent,
                            engine.HasNotifications()
                                ? kNotifiedPollIntervalMs
                                : static_cast<DWORD>(g_settings.pollIntervalMs));
    }

    engine.Disconnect();
    if (publicManager) {
        publicManager->Release();
    }
    CoUninitialize();
}

// =============================================================================
// Hooks
// =============================================================================

using TaskbarFrame_MeasureOverride_t =
    int(WINAPI*)(void* pThis,
                 winrt::Windows::Foundation::Size size,
                 winrt::Windows::Foundation::Size* resultSize);
TaskbarFrame_MeasureOverride_t TaskbarFrame_MeasureOverride_Original;
int WINAPI
TaskbarFrame_MeasureOverride_Hook(void* pThis,
                                  winrt::Windows::Foundation::Size size,
                                  winrt::Windows::Foundation::Size* resultSize) {
    static std::atomic<bool> logged;
    if (!logged.exchange(true)) {
        VDPLOG(L"TaskbarFrame::MeasureOverride hook is live");
    }

    FrameworkElement taskbarFrame = nullptr;
    ((IUnknown*)pThis)
        ->QueryInterface(winrt::guid_of<FrameworkElement>(),
                         winrt::put_abi(taskbarFrame));
    if (taskbarFrame) {
        try {
            QueueEnsurePager(taskbarFrame);
        } catch (...) {
            VDPLOG(L"Error %08X", winrt::to_hresult());
        }
    }

    return TaskbarFrame_MeasureOverride_Original(pThis, size, resultSize);
}

static HMODULE GetTaskbarViewModuleHandle() {
    HMODULE module = GetModuleHandle(L"Taskbar.View.dll");
    if (!module) {
        module = GetModuleHandle(L"ExplorerExtensions.dll");
    }

    return module;
}

// Recursive, plus an explicit in-progress flag: HookSymbols can itself load a
// module, and the loader hooks below run on whatever thread is doing the
// loading -- including this one. They no longer call in here (they only wake
// the worker), but init, after-init and the worker still race for this, and a
// plain mutex would deadlock on the reentrant path while a plain retry would
// recurse.
static std::recursive_mutex g_hookMutex;
static bool g_hookInProgress;            // guarded by g_hookMutex
static ULONGLONG g_nextHookAttemptTick;  // guarded by g_hookMutex

// A failed hook is retried, but not at the polling rate -- symbol resolution
// can go out to the symbol server, which is not a thing to do 2.5x a second.
static const ULONGLONG kHookRetryIntervalMs = 2000;

static bool HookTaskbarViewDllSymbols(HMODULE module);
static bool TryHookTaskbarViewDll(PCWSTR reason);

static bool HookTaskbarViewDllSymbols(HMODULE module) {
    WindhawkUtils::SYMBOL_HOOK symbolHooks[] = {
        {
            {LR"(public: virtual int __cdecl winrt::impl::produce<struct winrt::Taskbar::implementation::TaskbarFrame,struct winrt::Windows::UI::Xaml::IFrameworkElementOverrides>::MeasureOverride(struct winrt::Windows::Foundation::Size,struct winrt::Windows::Foundation::Size *))"},
            &TaskbarFrame_MeasureOverride_Original,
            TaskbarFrame_MeasureOverride_Hook,
        },
    };

    if (!WindhawkUtils::HookSymbols(module, symbolHooks,
                                    ARRAYSIZE(symbolHooks))) {
        VDPLOG(L"HookSymbols failed");
        return false;
    }

    return true;
}

// Taskbar.View.dll can appear at three different times: already loaded when the
// mod starts (enabling the mod on a running desktop), loaded between Wh_ModInit
// and Wh_ModAfterInit, or loaded later still (a fresh explorer, which is the
// case an explorer restart produces). Missing it means no hook, and no hook
// means the pager is never injected -- so every path funnels through here and
// the worker thread retries until it succeeds.
static bool TryHookTaskbarViewDll(PCWSTR reason) {
    if (g_taskbarViewDllHooked) {
        return true;
    }

    HMODULE module = GetTaskbarViewModuleHandle();
    if (!module) {
        return false;
    }

    std::lock_guard<std::recursive_mutex> guard(g_hookMutex);

    if (g_taskbarViewDllHooked || g_hookInProgress) {
        return g_taskbarViewDllHooked;
    }

    ULONGLONG now = GetTickCount64();
    if (now < g_nextHookAttemptTick) {
        return false;
    }

    VDPLOG(L"Hooking the taskbar view module (%s)", reason);

    g_hookInProgress = true;
    bool hooked = HookTaskbarViewDllSymbols(module);
    g_hookInProgress = false;

    if (!hooked) {
        g_nextHookAttemptTick = now + kHookRetryIntervalMs;
        VDPLOG(L"Hook attempt failed, retrying in %llums",
               kHookRetryIntervalMs);
        return false;
    }

    Wh_ApplyHookOperations();

    // Set only here, once the hook is actually in place. This used to be set
    // before the attempt, so a single failure latched the mod into a state
    // where every retry path -- the worker poll, the LoadLibraryExW hook,
    // Wh_ModAfterInit -- reported "already hooked" while no hook had ever
    // been installed. At boot it is the worker poll that lands this hook, so
    // that one failure was the whole difference between a working pager and a
    // mod that loads, runs its worker, and draws nothing.
    g_taskbarViewDllHooked = true;
    return true;
}

// Both loader hooks below only wake the worker; neither hooks anything itself.
// They run on a thread that is in the middle of a module load, and HookSymbols
// can load modules of its own and go out to the symbol cache on disk. Doing
// that from inside a load -- under the loader lock, when the DLL is being
// brought in from another module's DllMain -- trades a missing pager for a hung
// explorer. The worker owns every hook attempt; these two only tell it to stop
// waiting.
static void WakeWorkerToHook() {
    if (g_taskbarViewDllHooked || g_unloading || !g_workerWakeEvent) {
        return;
    }

    g_loaderNotified = true;
    SetEvent(g_workerWakeEvent);
}

using LoadLibraryExW_t = decltype(&LoadLibraryExW);
LoadLibraryExW_t LoadLibraryExW_Original;
HMODULE WINAPI LoadLibraryExW_Hook(LPCWSTR lpLibFileName,
                                   HANDLE hFile,
                                   DWORD dwFlags) {
    HMODULE module = LoadLibraryExW_Original(lpLibFileName, hFile, dwFlags);
    if (module && !g_taskbarViewDllHooked &&
        GetTaskbarViewModuleHandle() == module) {
        WakeWorkerToHook();
    }

    return module;
}

// The LoadLibraryExW hook above has never once been the one that catches
// Taskbar.View.dll -- in every trace taken so far the hook that lands is the
// worker's poll, seconds later. Whatever path the shell uses to bring the
// module in reaches the loader without passing through LoadLibraryExW (a
// delay-load thunk calls the ANSI entry point, and COM activation has its own
// route). LdrLoadDll sits underneath all of them, so this is the notification
// that actually fires, at the one moment that matters: before the taskbar has
// built its frame, and therefore before the first measure pass.
struct VDP_UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
};

// Matched on the name the caller handed the loader rather than by asking for a
// module handle: this runs on a thread that is still inside a load, and
// GetModuleHandle would take the loader lock again to answer. A string compare
// needs nothing.
static bool UnicodeStringNamesTaskbarView(const VDP_UNICODE_STRING* name) {
    if (!name || !name->Buffer || !name->Length) {
        return false;
    }

    const wchar_t* leaf = name->Buffer;
    size_t length = name->Length / sizeof(wchar_t);

    // The loader gets whatever the caller passed -- a bare name or a full path.
    for (size_t i = length; i > 0; --i) {
        wchar_t c = leaf[i - 1];
        if (c == L'\\' || c == L'/') {
            leaf += i;
            length -= i;
            break;
        }
    }

    static const wchar_t* const kNames[] = {L"taskbar.view.dll",
                                            L"explorerextensions.dll"};
    for (const wchar_t* candidate : kNames) {
        size_t candidateLength = wcslen(candidate);
        if (length == candidateLength &&
            _wcsnicmp(leaf, candidate, candidateLength) == 0) {
            return true;
        }
    }

    return false;
}

using LdrLoadDll_t = LONG(NTAPI*)(PWSTR searchPath,
                                  PULONG dllCharacteristics,
                                  VDP_UNICODE_STRING* dllName,
                                  PVOID* baseAddress);
LdrLoadDll_t LdrLoadDll_Original;
LONG NTAPI LdrLoadDll_Hook(PWSTR searchPath,
                           PULONG dllCharacteristics,
                           VDP_UNICODE_STRING* dllName,
                           PVOID* baseAddress) {
    LONG status = LdrLoadDll_Original(searchPath, dllCharacteristics, dllName,
                                      baseAddress);
    if (status >= 0 && !g_taskbarViewDllHooked &&
        UnicodeStringNamesTaskbarView(dllName)) {
        WakeWorkerToHook();
    }

    return status;
}

// =============================================================================
// Keybinds
// =============================================================================

// A low-level keyboard hook sees keys before explorer's own hotkey handling,
// so returning nonzero takes Win+1, Win+N and Win+Q away from the shell -- the
// same mechanism PowerToys Keyboard Manager uses. The hook only decides and
// queues; every desktop operation happens on the worker, well inside the
// LowLevelHooksTimeout budget.
//
// The hook needs a thread that pumps messages, which the worker (blocked in
// WaitForSingleObject, and for whole slides in WaitForAnimationToComplete)
// is not. So it gets its own.

static std::thread g_keyThread;
static std::atomic<DWORD> g_keyThreadId;

// Keys whose keydown was swallowed, so their auto-repeats and keyup are
// swallowed too instead of leaking a stray '1' or 'q' to the focused app.
// Touched only on the key thread.
static bool g_swallowed[256];

// Any unassigned virtual key works; this is the one PowerToys uses.
static const WORD kDummyKey = 0xFF;

// Modifier state as the hook itself has seen it. GetAsyncKeyState can't be
// used here: called from explorer while another process owns the foreground,
// it reports Win as up, so every bind fell through to Windows unless the
// desktop or taskbar was focused. Touched only on the key thread.
static bool g_keyDown[256];

static bool IsKeyDown(int vk) {
    switch (vk) {
        case VK_SHIFT:
            return g_keyDown[VK_LSHIFT] || g_keyDown[VK_RSHIFT];
        case VK_CONTROL:
            return g_keyDown[VK_LCONTROL] || g_keyDown[VK_RCONTROL];
        case VK_MENU:
            return g_keyDown[VK_LMENU] || g_keyDown[VK_RMENU];
    }
    return g_keyDown[vk];
}

// Keyups that happen on another desktop (lock screen, UAC, Ctrl+Alt+Del)
// never reach the hook -- Win+L would otherwise leave Win stuck down and turn
// a plain '1' into a desktop switch after unlocking.
//
// EVENT_SYSTEM_DESKTOPSWITCH also fires on every *virtual* desktop switch,
// where Win is usually still held for the next Win+N, so the state is only
// dropped when input has really left the Default desktop.
static bool IsInputOnDefaultDesktop() {
    HDESK desk = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    if (!desk) {
        return false;  // Winlogon / secure desktop: access denied
    }
    WCHAR name[64] = L"";
    bool isDefault =
        GetUserObjectInformation(desk, UOI_NAME, name, sizeof(name), nullptr) &&
        _wcsicmp(name, L"Default") == 0;
    CloseDesktop(desk);
    return isDefault;
}

static bool g_inputLeftDefault;  // key thread only

static void CALLBACK DesktopSwitchProc(HWINEVENTHOOK, DWORD, HWND, LONG, LONG,
                                       DWORD, DWORD) {
    bool onDefault = IsInputOnDefaultDesktop();
    // Clear on the way out and again on the way back, so nothing seen in
    // between survives either.
    if (!onDefault || g_inputLeftDefault) {
        memset(g_keyDown, 0, sizeof(g_keyDown));
    }
    g_inputLeftDefault = !onDefault;
}

// The taskbar and the desktop aren't on any one desktop, so there's nothing to
// move.
static bool IsMovableWindow(HWND hwnd) {
    if (!hwnd) {
        return false;
    }

    WCHAR className[32];
    if (!GetClassName(hwnd, className, ARRAYSIZE(className))) {
        return false;
    }

    return _wcsicmp(className, L"Shell_TrayWnd") != 0 &&
           _wcsicmp(className, L"Shell_SecondaryTrayWnd") != 0 &&
           _wcsicmp(className, L"Progman") != 0 &&
           _wcsicmp(className, L"WorkerW") != 0;
}

// Returns true and fills *action when this keydown is one of ours.
static bool MatchKeybind(DWORD vk, DesktopAction* action) {
    if (!IsKeyDown(VK_LWIN) && !IsKeyDown(VK_RWIN)) {
        return false;
    }
    // Ctrl+Win+... and Alt+Win+... stay Windows'.
    if (IsKeyDown(VK_CONTROL) || IsKeyDown(VK_MENU)) {
        return false;
    }

    if (IsKeyDown(VK_SHIFT)) {
        if (g_settings.keyMove && vk >= '0' && vk <= '9') {
            // Always ours, even with nothing movable focused (right after an
            // animated switch the taskbar holds the foreground): a null hwnd
            // makes the move a no-op instead of handing the chord to Windows.
            HWND hwnd = GetForegroundWindow();
            if (!IsMovableWindow(hwnd)) {
                hwnd = nullptr;
            }
            int index = vk == '0' ? 9 : static_cast<int>(vk - '1');
            *action = {DesktopActionType::MoveWindowToIndex, index, hwnd};
            return true;
        }
        // Every other Win+Shift combination stays Windows'.
        return false;
    }

    if (g_settings.keySwitch && vk >= '0' && vk <= '9') {
        // The number row reads 1..9 then 0, so 0 is the tenth desktop.
        int index = vk == '0' ? 9 : static_cast<int>(vk - '1');
        *action = {DesktopActionType::SwitchToIndex, index};
        return true;
    }
    if (g_settings.keyNew && vk == 'N') {
        *action = {DesktopActionType::Create, -1};
        return true;
    }
    if (g_settings.keyClose && vk == 'Q') {
        *action = {DesktopActionType::RemoveCurrent, -1};
        return true;
    }
    return false;
}

// Explorer opens Start when it sees Win go down and up with nothing in
// between. Swallowing the '1' leaves exactly that, so a harmless key is
// injected while Win is still held to make it a chord again.
static void SendDummyKey() {
    INPUT inputs[2]{};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = kDummyKey;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = kDummyKey;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(ARRAYSIZE(inputs), inputs, sizeof(INPUT));
}

static LRESULT CALLBACK LowLevelKeyboardProc(int code,
                                             WPARAM wParam,
                                             LPARAM lParam) {
    if (code != HC_ACTION || g_unloading) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    const auto* info = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
    if (!info || info->vkCode > 0xFF) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    DWORD vk = info->vkCode;
    bool isDown = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;

    // Track everything, injected included (remappers send modifiers that
    // way), but never act on injected input -- including our own dummy key.
    g_keyDown[vk] = isDown;
    if (info->flags & LLKHF_INJECTED) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    if (!isDown) {
        if (g_swallowed[vk]) {
            g_swallowed[vk] = false;
            return 1;
        }
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    if (g_swallowed[vk]) {
        return 1;  // auto-repeat of a key we already acted on
    }

    DesktopAction action;
    if (!MatchKeybind(vk, &action)) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    g_swallowed[vk] = true;
    SendDummyKey();
    QueueDesktopAction(action);
    return 1;
}

// Low-level hooks run newest-first, and one that doesn't call CallNextHookEx
// ends the chain. VMware installs such a hook when a VM grabs the keyboard, so
// ours -- installed at explorer startup -- never saw Win+N while a VM had
// focus. Re-installing puts ours back at the head of the chain: on every
// foreground change, and on a short timer while an input-grabbing app is in
// front, since the VM can grab after it already has the foreground.
static HHOOK g_keyHook;          // key thread only
static HMODULE g_keyHookModule;  // key thread only
static UINT_PTR g_rehookTimer;   // key thread only
static const UINT kRehookIntervalMs = 250;

static void InstallKeyHook() {
    HHOOK hook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc,
                                  g_keyHookModule, 0);
    if (!hook) {
        VDPLOG(L"SetWindowsHookEx failed: %u", GetLastError());
        return;
    }
    // New one first, then drop the old, so no key slips through a gap.
    if (g_keyHook) {
        UnhookWindowsHookEx(g_keyHook);
    }
    g_keyHook = hook;
}

static bool IsInputGrabbingApp(HWND hwnd) {
    DWORD processId = 0;
    if (!hwnd || !GetWindowThreadProcessId(hwnd, &processId)) {
        return false;
    }
    HANDLE process =
        OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!process) {
        return false;
    }
    WCHAR path[MAX_PATH];
    DWORD size = ARRAYSIZE(path);
    bool ok = QueryFullProcessImageName(process, 0, path, &size);
    CloseHandle(process);
    if (!ok) {
        return false;
    }

    PCWSTR leaf = wcsrchr(path, L'\\');
    leaf = leaf ? leaf + 1 : path;
    static const wchar_t* const kNames[] = {L"vmware.exe", L"vmplayer.exe",
                                            L"VirtualBoxVM.exe"};
    for (PCWSTR name : kNames) {
        if (_wcsicmp(leaf, name) == 0) {
            return true;
        }
    }
    return false;
}

static void CALLBACK RehookTimerProc(HWND, UINT, UINT_PTR, DWORD) {
    InstallKeyHook();
}

static void CALLBACK ForegroundProc(HWINEVENTHOOK, DWORD, HWND hwnd, LONG,
                                    LONG, DWORD, DWORD) {
    InstallKeyHook();

    bool grabbing = IsInputGrabbingApp(hwnd);
    if (grabbing && !g_rehookTimer) {
        g_rehookTimer =
            SetTimer(nullptr, 0, kRehookIntervalMs, RehookTimerProc);
    } else if (!grabbing && g_rehookTimer) {
        KillTimer(nullptr, g_rehookTimer);
        g_rehookTimer = 0;
    }
}

static void KeyThreadProc(HANDLE readyEvent) {
    // Make sure the thread has a message queue before anyone can post WM_QUIT
    // to it; a quit posted to a queue-less thread is silently lost.
    MSG msg;
    PeekMessage(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    g_keyThreadId = GetCurrentThreadId();
    SetEvent(readyEvent);

    // Our own module, not explorer.exe: the hook procedure lives in this DLL.
    GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                          GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                      reinterpret_cast<LPCWSTR>(&LowLevelKeyboardProc),
                      &g_keyHookModule);

    InstallKeyHook();
    if (g_keyHook) {
        VDPLOG(L"Keybind hook installed");
    }

    HWINEVENTHOOK switchHook = SetWinEventHook(
        EVENT_SYSTEM_DESKTOPSWITCH, EVENT_SYSTEM_DESKTOPSWITCH, nullptr,
        DesktopSwitchProc, 0, 0, WINEVENT_OUTOFCONTEXT);
    HWINEVENTHOOK foregroundHook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr,
        ForegroundProc, 0, 0, WINEVENT_OUTOFCONTEXT);
    // A VM that already has focus when the mod loads gets no foreground event.
    ForegroundProc(nullptr, 0, GetForegroundWindow(), 0, 0, 0, 0);

    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (g_rehookTimer) {
        KillTimer(nullptr, g_rehookTimer);
        g_rehookTimer = 0;
    }
    if (foregroundHook) {
        UnhookWinEvent(foregroundHook);
    }
    if (switchHook) {
        UnhookWinEvent(switchHook);
    }
    if (g_keyHook) {
        UnhookWindowsHookEx(g_keyHook);
        g_keyHook = nullptr;
    }
    g_keyThreadId = 0;
}

static void StopKeybinds() {
    if (!g_keyThread.joinable()) {
        return;
    }

    if (DWORD id = g_keyThreadId) {
        PostThreadMessage(id, WM_QUIT, 0, 0);
    }
    g_keyThread.join();
    memset(g_swallowed, 0, sizeof(g_swallowed));
    memset(g_keyDown, 0, sizeof(g_keyDown));
}

static void StartKeybinds() {
    if (g_keyThread.joinable() ||
        !(g_settings.keySwitch || g_settings.keyNew || g_settings.keyClose ||
          g_settings.keyMove)) {
        return;
    }

    HANDLE readyEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!readyEvent) {
        return;
    }

    g_keyThread = std::thread(KeyThreadProc, readyEvent);
    WaitForSingleObject(readyEvent, INFINITE);
    CloseHandle(readyEvent);
}

// =============================================================================
// Command window
// =============================================================================

// Lets other mods in explorer drive the desktops through the same queue as the
// pager and the keybinds. The Widgets mod's top bar pager posts
// kCommandSwitch, with the zero-based desktop index in wParam, to this
// message-only window, found by its class name. Runs whatever the settings, so
// the top bar works with the taskbar pager and every keybind switched off.

static constexpr PCWSTR kCommandClass = L"WhDesktopsCommand";
static constexpr UINT kCommandSwitch = WM_APP + 1;

static std::thread g_commandThread;
static std::atomic<DWORD> g_commandThreadId;

static LRESULT CALLBACK CommandWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                       LPARAM lParam) {
    if (msg == kCommandSwitch) {
        int index = static_cast<int>(wParam);
        if (index >= 0 && index < 1000) {
            QueueDesktopAction({DesktopActionType::SwitchToIndex, index});
        }
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static void CommandThreadProc(HANDLE readyEvent) {
    MSG msg;
    PeekMessage(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    g_commandThreadId = GetCurrentThreadId();

    // Our own module: the window procedure lives in this DLL.
    HMODULE module = nullptr;
    GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                          GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                      reinterpret_cast<LPCWSTR>(&CommandWndProc), &module);

    WNDCLASSEX wc{sizeof(wc)};
    wc.lpfnWndProc = CommandWndProc;
    wc.hInstance = module;
    wc.lpszClassName = kCommandClass;
    RegisterClassEx(&wc);
    HWND hwnd = CreateWindowEx(0, kCommandClass, L"", 0, 0, 0, 0, 0,
                               HWND_MESSAGE, nullptr, module, nullptr);
    if (!hwnd) {
        VDPLOG(L"Command window failed: %u", GetLastError());
    }
    SetEvent(readyEvent);

    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        DispatchMessage(&msg);
    }

    if (hwnd) {
        DestroyWindow(hwnd);
    }
    UnregisterClass(kCommandClass, module);
    g_commandThreadId = 0;
}

static void StartCommandWindow() {
    if (g_commandThread.joinable()) {
        return;
    }
    HANDLE readyEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!readyEvent) {
        return;
    }
    g_commandThread = std::thread(CommandThreadProc, readyEvent);
    WaitForSingleObject(readyEvent, INFINITE);
    CloseHandle(readyEvent);
}

static void StopCommandWindow() {
    if (!g_commandThread.joinable()) {
        return;
    }
    if (DWORD id = g_commandThreadId) {
        PostThreadMessage(id, WM_QUIT, 0, 0);
    }
    g_commandThread.join();
}

// =============================================================================
// Mod entry points
// =============================================================================

BOOL Wh_ModInit() {
    // After LoadSettings, because that is what enables the file log. This
    // line used to run before it and was therefore the one event missing from
    // every trace -- precisely the event you want when diagnosing startup.
    LoadSettings();

    VDPLOG(L"Init");

    g_workerWakeEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!g_workerWakeEvent) {
        VDPLOG(L"CreateEvent failed: %u", GetLastError());
        return FALSE;
    }

    // Not fatal if this fails -- the module may simply not be loaded yet, and
    // both the LoadLibraryExW hook and the worker thread will pick it up.
    if (!TryHookTaskbarViewDll(L"mod init")) {
        VDPLOG(L"Taskbar view module not loaded yet, will retry");
    }

    WindhawkUtils::SetFunctionHook(LoadLibraryExW, LoadLibraryExW_Hook,
                                   &LoadLibraryExW_Original);

    if (HMODULE ntdll = GetModuleHandle(L"ntdll.dll")) {
        auto ldrLoadDll =
            reinterpret_cast<LdrLoadDll_t>(GetProcAddress(ntdll, "LdrLoadDll"));
        if (ldrLoadDll) {
            WindhawkUtils::SetFunctionHook(ldrLoadDll, LdrLoadDll_Hook,
                                           &LdrLoadDll_Original);
        } else {
            VDPLOG(L"LdrLoadDll not found, falling back to the worker poll");
        }
    }

    g_workerStop = false;
    g_workerThread = std::thread(WorkerThreadProc);

    StartKeybinds();
    StartCommandWindow();

    return TRUE;
}

void Wh_ModAfterInit() {
    TryHookTaskbarViewDll(L"mod after init");

    // After the hook, so a taskbar that is about to measure anyway is served by
    // the normal path and this one finds nothing left to do.
    ConsumeHandoff();
}

void Wh_ModSettingsChanged() {
    VDPLOG(L"Settings changed");

    LoadSettings();

    // Rebuild the pills so size, margin and label changes take effect.
    std::vector<PagerInstance> pagers;
    {
        std::lock_guard<std::mutex> guard(g_pagersMutex);
        pagers = g_pagers;
    }

    // Every taskbar shares the one UI thread, so a single dispatch covers all
    // of them. The XamlRoots have to be captured before the removal, which
    // clears the list they came from.
    std::vector<winrt::weak_ref<XamlRoot>> roots;
    winrt::Windows::UI::Core::CoreDispatcher dispatcher{nullptr};
    for (const auto& pager : pagers) {
        roots.push_back(pager.xamlRoot);
        if (!dispatcher && pager.dispatcher) {
            dispatcher = pager.dispatcher;
        }
    }

    if (dispatcher) {
        try {
            dispatcher.TryRunAsync(
                winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,
                [roots]() {
                    try {
                        RemovePagerElements();

                        // Recreate immediately. Waiting for the taskbar's next
                        // measure pass leaves the pill missing until something
                        // happens to trigger layout, which can be a long time
                        // -- changing a setting would make it vanish.
                        for (const auto& rootRef : roots) {
                            if (auto xamlRoot = rootRef.get()) {
                                EnsurePagerForXamlRoot(xamlRoot);
                            }
                        }
                    } catch (...) {
                        VDPLOG(L"Settings rebuild error %08X",
                               winrt::to_hresult());
                    }
                });
        } catch (...) {
            VDPLOG(L"Dispatch error %08X", winrt::to_hresult());
        }
    }

    StopKeybinds();
    StartKeybinds();

    g_reconnectRequested = true;
    SetEvent(g_workerWakeEvent);
}

void Wh_ModBeforeUninit() {
    VDPLOG(L"Before uninit");

    g_unloading = true;

    StopWatchingAccentColor();
    StopKeybinds();
    StopCommandWindow();

    g_workerStop = true;
    if (g_workerWakeEvent) {
        SetEvent(g_workerWakeEvent);
    }
    if (g_workerThread.joinable()) {
        g_workerThread.join();
    }

    // The elements have to go away on the thread that owns them, and before the
    // DLL is unloaded -- so dispatch the removal and wait for it.
    std::vector<PagerInstance> pagers;
    {
        std::lock_guard<std::mutex> guard(g_pagersMutex);
        pagers = g_pagers;
    }

    if (!pagers.empty() && pagers[0].dispatcher) {
        HANDLE removedEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

        try {
            pagers[0].dispatcher.TryRunAsync(
                winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,
                [removedEvent]() {
                    try {
                        RemovePagerElements();
                    } catch (...) {
                        VDPLOG(L"RemovePagerElements error %08X",
                               winrt::to_hresult());
                    }
                    SetEvent(removedEvent);
                });

            WaitForSingleObject(removedEvent, 2000);
        } catch (...) {
            VDPLOG(L"Dispatch error %08X", winrt::to_hresult());
        }

        CloseHandle(removedEvent);
    }
}

void Wh_ModUninit() {
    VDPLOG(L"Uninit");

    if (g_workerWakeEvent) {
        CloseHandle(g_workerWakeEvent);
        g_workerWakeEvent = nullptr;
    }
}
