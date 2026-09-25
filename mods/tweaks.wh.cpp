// ==WindhawkMod==
// @id              tweaks
// @name            Tweaks
// @description     Genie minimize, auto-hide taskbar when maximized, Alt+Tab delay, desktop drag-select, MB/GB file sizes
// @version         1.0
// @author          ashtonlariccia
// @include         *
// @exclude         msedgewebview2.exe
// @exclude         backgroundTaskHost.exe
// @exclude         LockApp.exe
// @exclude         SearchHost.exe
// @exclude         ShellExperienceHost.exe
// @exclude         ShellHost.exe
// @exclude         StartMenuExperienceHost.exe
// @architecture    x86-64
// @compilerOptions -luser32 -lgdi32 -lcomctl32 -ladvapi32 -ldwmapi -lole32 -loleaut32 -luuid -lpropsys -lversion -lruntimeobject -lcoremessaging
// ==/WindhawkMod==

// Source code is published under The GNU General Public License v3.0.
//
// Built from, and stripped down to what this setup uses:
//   * MacOS Minimize Animation by Abdullah Masood, with Potassiumuncher's
//     capture, targeting and auto-hide work (MIT) -- the Classic engine only
//   * Taskbar auto-hide when maximized by m417z
//   * Alt+Tab window delayer by L3r0y
//   * Better file sizes in Explorer details by m417z -- the MB/GB fix only
//   * Desktop Drag-Select With Icons Hidden (ours)

// ==WindhawkModReadme==
/*
# Tweaks

Behaviour changes that don't style anything, one switch each.

| Feature | Runs in | What it does |
|---|---|---|
| Genie minimize | every app | Windows flow into their taskbar button when minimized, and back out when restored |
| Auto-hide when maximized | explorer | The taskbar hides only while a window is maximized or overlaps it |
| Alt+Tab delay | explorer | A quick Alt+Tab switches without flashing the switcher |
| Desktop drag-select | explorer | The selection rectangle works with desktop icons hidden, in your accent colour |
| MB/GB file sizes | every app | Explorer's details view shows large files in MB/GB instead of always KB |

The mod is injected into every process for the two features that need it; the
explorer-only ones stay dormant everywhere else.

Settings apply live. The one exception is switching auto-hide, Alt+Tab delay
or drag-select on or off, which reloads the mod in explorer only. Compiling a
new build is the slow case: Windhawk then re-injects the mod into every running
app one at a time, and an app has no genie until its turn comes.

The processes excluded above have no minimizable windows and no file views,
so injecting there would only cost memory.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- genie: true
  $name: "Genie minimize"
- genieDurationMs: 450
  $name: "Genie: duration (ms)"
- genieRestore: true
  $name: "Genie: animate restore"
  $description: Play the genie in reverse when a window is restored.
- genieUnhideTaskbar: true
  $name: "Genie: reveal an auto-hidden taskbar"
  $description: >-
    Briefly show the taskbar during a minimize so the window has somewhere
    visible to flow into.
- genieUnhideDurationMs: 450
  $name: "Genie: taskbar reveal (ms)"
- autoHide: true
  $name: "Auto-hide when maximized"
  $description: >-
    Turns on the taskbar's auto-hide, but only lets it hide while a window
    needs the space.
- autoHideMode: intersected
  $name: "Auto-hide: hide when"
  $options:
  - intersected: A window is maximized or overlaps the taskbar
  - maximized: A window is maximized
- altTabDelay: true
  $name: "Alt+Tab delay"
- altTabDelayMs: 150
  $name: "Alt+Tab: delay (ms)"
- dragSelect: true
  $name: "Desktop drag-select"
- dragSelectFillOpacity: 25
  $name: "Drag-select: fill opacity (%)"
- dragSelectBorderOpacity: 90
  $name: "Drag-select: border opacity (%)"
- dragSelectBorderWidth: 1
  $name: "Drag-select: border width (px)"
- dragSelectCornerRadius: 6
  $name: "Drag-select: corner radius (px)"
- dragSelectFadeOutMs: 180
  $name: "Drag-select: fade-out (ms)"
- fileSizesMb: true
  $name: "MB/GB file sizes"
  $description: Show large files in MB/GB in Explorer's details view.
*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <windowsx.h>

#include <dwmapi.h>
#include <math.h>
#include <propsys.h>
#include <shellapi.h>
#include <uiautomation.h>

#include <windhawk_utils.h>

#include <DispatcherQueue.h>

#undef GetCurrentTime

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Composition.Desktop.h>
#include <winrt/Windows.UI.Composition.h>

#include <algorithm>
#include <atomic>
#include <cwctype>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifndef DWMWA_EXTENDED_FRAME_BOUNDS
#define DWMWA_EXTENDED_FRAME_BOUNDS 9
#endif

#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 2
#endif

// High-resolution waitable timer flag (Win10 1803+).
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

// Missing in older MinGW headers.
#ifndef EVENT_OBJECT_CLOAKED
#define EVENT_OBJECT_CLOAKED 0x8017
#endif
#ifndef EVENT_OBJECT_UNCLOAKED
#define EVENT_OBJECT_UNCLOAKED 0x8018
#endif

// Which explorer-only features are running, fixed for the life of this load
// (switching one reloads the mod in explorer).
struct {
    bool autoHide;
    bool altTabDelay;
    bool dragSelect;
} g_active;

bool IsExplorerProcess() {
    WCHAR path[MAX_PATH];
    DWORD length = GetModuleFileName(nullptr, path, ARRAYSIZE(path));
    if (!length || length == ARRAYSIZE(path)) {
        return false;
    }

    PCWSTR name = wcsrchr(path, L'\\');
    return name && _wcsicmp(name + 1, L"explorer.exe") == 0;
}

// ShowWindow is hooked once and shared: Alt+Tab delay needs it in explorer,
// the genie everywhere.
using ShowWindow_t = decltype(&ShowWindow);
ShowWindow_t ShowWindow_Original;

////////////////////////////////////////////////////////////////////////////////
// Genie minimize, from MacOS Minimize Animation (Classic engine).

namespace genie {

typedef LRESULT (WINAPI *DefWindowProcW_t)(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
DefWindowProcW_t DefWindowProcW_Original;

typedef BOOL (WINAPI *ShowWindowAsync_t)(HWND hWnd, int nCmdShow);
ShowWindowAsync_t ShowWindowAsync_Original;

typedef BOOL (WINAPI *SetWindowPlacement_t)(HWND hWnd, const WINDOWPLACEMENT* lpwndpl);
SetWindowPlacement_t SetWindowPlacement_Original;

typedef BOOL (WINAPI *CloseWindow_t)(HWND hWnd);
CloseWindow_t CloseWindow_Original;


struct MacGenieAnimData {
    HWND hRealWnd;
    HBITMAP hBitmap;      // 32-bit top-down premultiplied DIB snapshot (w x h)
    void* pBits;          // its bits (fed straight to D2D CreateBitmap)
    RECT targetRect;      // the window's DWM extended-frame rect when anim started
    HMONITOR hMon;        // monitor the genie targets (window's when multi-monitor
                          // support is on, else the primary)
    int width;
    int height;
    int targetDockX;      // taskbar button X located via UI Automation
    BOOL isRising;        // FALSE = minimize (flow in), TRUE = restore (flow out)
    LONG_PTR originalExStyle;
    BOOL hiddenByCloak;   // rising only: TRUE = hidden via DWM cloak (restore path),
                          // FALSE = via WS_EX_LAYERED + alpha 0 (launch path)
    HANDLE hFirstFrameShown; // falling only: event signaled once the ghost's first
                             // frame is composed; NULL when unused
    int durationMs;
    // Auto-hide "unhide" (deferred minimize) - Potassiumuncher's engine. Only used
    // on the DefWindowProcW SC_MINIMIZE path when the taskbar is set to auto-hide.
    BOOL requestedUnhide;
    HWND hNextApp;
    int  unhideDurationMs;
    BOOL deferredMinimize;
};

// Cached snapshot for the restore animation. The DIB section owns pBits, so
// DeleteObject(hBmp) frees both. Snapshot model from Potassiumuncher's engine.
struct SnapCache { HBITMAP hBmp; void* pBits; int w; int h; };

// --- THE VAULTS ---
std::unordered_map<HWND, SnapCache> g_SnapshotCache;
std::unordered_map<HWND, int> g_IconPositions;            // per-window learned icon X
std::unordered_map<std::wstring, int> g_ProcessIconPositions; // per-process fallback
std::unordered_set<HWND> g_AnimActive;   // windows with a genie currently in flight
std::mutex g_CacheMutex;

// --- SETTINGS ---
std::atomic<int> g_durationMs{450};
std::atomic<bool> g_openAnimation{true};
std::atomic<int> g_unhideDurationMs{450};
std::atomic<bool> g_unhideEnabled{true};
// The hooks stay installed for the life of the load and consult this, so
// switching the genie on or off never has to reload the mod in every process.
std::atomic<bool> g_enabled{false};

// --- UNLOAD COORDINATION ---
// Windhawk unmaps the mod DLL right after uninit, so any worker thread still
// running mod code at that point would crash its host process. Workers register
// here and abort promptly once g_unloading is set; Wh_ModBeforeUninit waits for
// the count to drain before the DLL goes away. (MINE's drain model is kept in
// preference to HIS's join-and-pump StopAndJoinAllThreads: pumping messages while
// unloading can re-enter our six hooks under @include *.)
std::atomic<bool> g_unloading{false};
std::atomic<int>  g_workerCount{0};

void LoadSettings() {
    g_enabled = Wh_GetIntSetting(L"genie") != 0;
    g_durationMs = std::clamp(Wh_GetIntSetting(L"genieDurationMs"), 50, 2000);
    g_openAnimation = Wh_GetIntSetting(L"genieRestore") != 0;
    g_unhideEnabled = Wh_GetIntSetting(L"genieUnhideTaskbar") != 0;
    g_unhideDurationMs =
        std::clamp(Wh_GetIntSetting(L"genieUnhideDurationMs"), 0, 5000);
}

// Resolved on first use rather than imported: the one shell32 call the genie
// makes happens mid-minimize, where shell32 is already loaded, and importing it
// would load it into every process the mod is injected into.
UINT_PTR AppBarGetState(APPBARDATA* abd) {
    using SHAppBarMessage_t = UINT_PTR(WINAPI*)(DWORD, PAPPBARDATA);
    static SHAppBarMessage_t pSHAppBarMessage = []() -> SHAppBarMessage_t {
        HMODULE shell32 = LoadLibraryEx(L"shell32.dll", nullptr,
                                        LOAD_LIBRARY_SEARCH_SYSTEM32);
        return shell32 ? (SHAppBarMessage_t)GetProcAddress(shell32,
                                                           "SHAppBarMessage")
                       : nullptr;
    }();
    return pSHAppBarMessage ? pSHAppBarMessage(ABM_GETSTATE, abd) : 0;
}

void MacGenieSetDwmTransitions(HWND hWnd, BOOL enable) {
    BOOL disable = !enable;
    DwmSetWindowAttribute(hWnd, DWMWA_TRANSITIONS_FORCEDISABLED, &disable, sizeof(disable));
}

// Hide / show a window at the DWM level. A cloaked window is simply not rendered
// (frame included) while staying "visible" to Win32 and painting normally
// underneath, so a restore can happen invisibly under the genie with no
// layered-surface rebuild flash. Kept from MINE in preference to HIS's
// WS_EX_LAYERED + alpha 0 restore hide, which flashes the bare frame.
void MacGenieSetCloak(HWND hWnd, BOOL cloak) {
    DwmSetWindowAttribute(hWnd, DWMWA_CLOAK, &cloak, sizeof(cloak));
}

// Undo a rising caller's pre-animation hide (used whenever the animation can't run
// or is refused): uncloak or un-hide depending on how the window was hidden, and
// re-enable its DWM transitions.
static void MacGenieUndoRisingHide(HWND hWnd, LONG_PTR originalExStyle, BOOL cloakHidden) {
    if (cloakHidden) {
        MacGenieSetCloak(hWnd, FALSE);
    } else {
        SetLayeredWindowAttributes(hWnd, 0, 255, LWA_ALPHA);
        if (!(originalExStyle & WS_EX_LAYERED)) {
            SetWindowLongPtrW(hWnd, GWL_EXSTYLE, originalExStyle);
        }
    }
    MacGenieSetDwmTransitions(hWnd, TRUE);
}

// Should we animate this window at all? Skip child / tiny windows so the effect
// only fires on real top-level windows. The size check stays on the placement /
// window rect; the animation geometry (below) uses the DWM extended frame bounds.
static bool MacGenieShouldAnimate(HWND hWnd) {
    // Every minimize and restore path funnels through here.
    if (!g_enabled.load(std::memory_order_relaxed)) return false;
    if (!hWnd) return false;
    LONG_PTR style = GetWindowLongPtrW(hWnd, GWL_STYLE);
    if (style & WS_CHILD) return false;

    RECT r;
    if (IsIconic(hWnd)) {
        WINDOWPLACEMENT wp;
        wp.length = sizeof(wp);
        if (!GetWindowPlacement(hWnd, &wp)) return false;
        r = wp.rcNormalPosition;
    } else if (!GetWindowRect(hWnd, &r)) {
        return false;
    }
    if ((r.right - r.left) < 40 || (r.bottom - r.top) < 40) return false;
    return true;
}

// Ported from Potassiumuncher's genie engine (github.com/Potassiumuncher).
HWND FindTaskbarForMonitor(HMONITOR hMon) {
    HWND hMainTray = FindWindowW(L"Shell_TrayWnd", NULL);
    HMONITOR mainMon = MonitorFromWindow(hMainTray, MONITOR_DEFAULTTOPRIMARY);
    if (hMon == mainMon || !hMon) return hMainTray;

    HWND hSecTray = NULL;
    while ((hSecTray = FindWindowExW(NULL, hSecTray, L"Shell_SecondaryTrayWnd", NULL)) != NULL) {
        if (MonitorFromWindow(hSecTray, MONITOR_DEFAULTTONULL) == hMon) {
            return hSecTray;
        }
    }
    return hMainTray;
}

// Ported from Potassiumuncher's genie engine (github.com/Potassiumuncher).
// Locates the app's taskbar button via UI Automation and returns its center X
// (falls back to fallbackX, then to the per-window / per-process learned cache).
// Runs on the hook (app UI) thread: CoInitializeEx(APARTMENTTHREADED) matches the
// typical GUI-thread apartment (S_FALSE) or reports RPC_E_CHANGED_MODE on an MTA
// thread; either way CoUninitialize is balanced only when we actually initialized.
int GetTaskbarButtonX(HWND hWndApp, int fallbackX, HMONITOR hMon) {
    int targetX = fallbackX;
    bool uiaFound = false;

    std::wstring procNameLower = L"";
    DWORD ownerPid = 0;
    GetWindowThreadProcessId(hWndApp, &ownerPid);
    if (ownerPid) {
        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, ownerPid);
        if (hProc) {
            WCHAR exePath[MAX_PATH] = {0};
            DWORD exePathLen = MAX_PATH;
            if (QueryFullProcessImageNameW(hProc, 0, exePath, &exePathLen)) {
                WCHAR* name = wcsrchr(exePath, L'\\');
                if (name) {
                    procNameLower = (name + 1);
                    size_t dotPos = procNameLower.find(L'.');
                    if (dotPos != std::wstring::npos) procNameLower = procNameLower.substr(0, dotPos);
                    std::transform(procNameLower.begin(), procNameLower.end(), procNameLower.begin(), ::towlower);
                }
            }
            CloseHandle(hProc);
        }
    }

    std::wstring processKey = procNameLower;
    if (!processKey.empty() && hMon) {
        processKey += L"_" + std::to_wstring(reinterpret_cast<size_t>(hMon));
    }

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    bool coInit = (hr == S_OK || hr == S_FALSE);

    if (hr == S_OK || hr == S_FALSE || hr == RPC_E_CHANGED_MODE) {
        IUIAutomation* pAutomation = nullptr;
        HRESULT hrUia = CoCreateInstance(__uuidof(CUIAutomation8), NULL, CLSCTX_INPROC_SERVER, __uuidof(IUIAutomation), (void**)&pAutomation);
        if (FAILED(hrUia)) {
            hrUia = CoCreateInstance(__uuidof(CUIAutomation), NULL, CLSCTX_INPROC_SERVER, __uuidof(IUIAutomation), (void**)&pAutomation);
        }

        if (SUCCEEDED(hrUia) && pAutomation) {
            HWND hTray = FindTaskbarForMonitor(hMon);
            if (hTray) {
                IUIAutomationElement* pTrayElement = nullptr;
                if (SUCCEEDED(pAutomation->ElementFromHandle(hTray, &pTrayElement)) && pTrayElement) {

                    WCHAR titleW[512] = {0};
                    GetWindowTextW(hWndApp, titleW, 512);
                    std::wstring titleLower = titleW;
                    std::transform(titleLower.begin(), titleLower.end(), titleLower.begin(), ::towlower);

                    std::wstring procHintLower = procNameLower;
                    if (procNameLower == L"chrome") procHintLower = L"google chrome";
                    else if (procNameLower == L"msedge") procHintLower = L"microsoft edge";
                    else if (procNameLower == L"firefox") procHintLower = L"firefox";
                    else if (procNameLower == L"brave") procHintLower = L"brave";
                    else if (procNameLower == L"opera") procHintLower = L"opera";
                    else if (procNameLower == L"vivaldi") procHintLower = L"vivaldi";

                    IUIAutomationCondition* pButtonCond = nullptr;
                    IUIAutomationCondition* pListItemCond = nullptr;
                    IUIAutomationCondition* pOrCond = nullptr;

                    VARIANT varBtn; varBtn.vt = VT_I4; varBtn.lVal = UIA_ButtonControlTypeId;
                    pAutomation->CreatePropertyCondition(UIA_ControlTypePropertyId, varBtn, &pButtonCond);

                    VARIANT varList; varList.vt = VT_I4; varList.lVal = UIA_ListItemControlTypeId;
                    pAutomation->CreatePropertyCondition(UIA_ControlTypePropertyId, varList, &pListItemCond);

                    if (pButtonCond && pListItemCond) {
                        pAutomation->CreateOrCondition(pButtonCond, pListItemCond, &pOrCond);
                    }

                    IUIAutomationElementArray* pArray = nullptr;
                    if (pOrCond && SUCCEEDED(pTrayElement->FindAll(TreeScope_Descendants, pOrCond, &pArray)) && pArray) {
                        int length = 0;
                        pArray->get_Length(&length);

                        MONITORINFO mi = {0};
                        mi.cbSize = sizeof(MONITORINFO);
                        GetMonitorInfoW(hMon, &mi);
                        int monRight = mi.rcMonitor.right;

                        int bestScore = 0;

                        for (int i = 0; i < length; i++) {
                            IUIAutomationElement* pItem = nullptr;
                            if (SUCCEEDED(pArray->GetElement(i, &pItem)) && pItem) {
                                BSTR name;
                                if (SUCCEEDED(pItem->get_CurrentName(&name)) && name) {
                                    std::wstring uiaNameLower = name;
                                    std::transform(uiaNameLower.begin(), uiaNameLower.end(), uiaNameLower.begin(), ::towlower);

                                    if (!uiaNameLower.empty()) {
                                        int score = 0;

                                        if (titleLower == uiaNameLower) score += 1000;
                                        if (!titleLower.empty() && titleLower.find(uiaNameLower) != std::wstring::npos) score += 500;
                                        if (!uiaNameLower.empty() && uiaNameLower.find(titleLower) != std::wstring::npos) score += 500;

                                        if (!procNameLower.empty() && uiaNameLower.find(procNameLower) != std::wstring::npos) score += 400;
                                        if (!procHintLower.empty() && procHintLower != procNameLower &&
                                            uiaNameLower.find(procHintLower) != std::wstring::npos) score += 900;

                                        std::wstring currentWord;
                                        for (wchar_t c : titleLower) {
                                            if (iswalnum(c)) {
                                                currentWord += c;
                                            } else {
                                                if (currentWord.length() >= 4 && uiaNameLower.find(currentWord) != std::wstring::npos) score += 50;
                                                currentWord.clear();
                                            }
                                        }
                                        if (currentWord.length() >= 4 && uiaNameLower.find(currentWord) != std::wstring::npos) score += 50;

                                        if (uiaNameLower.find(L"start") != std::wstring::npos) score -= 500;
                                        if (uiaNameLower.find(L"search") != std::wstring::npos) score -= 500;
                                        if (uiaNameLower.find(L"task view") != std::wstring::npos) score -= 500;
                                        if (uiaNameLower.find(L"widgets") != std::wstring::npos) score -= 500;

                                        if (score > bestScore) {
                                            RECT bRect;
                                            if (SUCCEEDED(pItem->get_CurrentBoundingRectangle(&bRect))) {
                                                if (bRect.right > bRect.left && bRect.left < monRight - 50) {
                                                    bestScore = score;
                                                    targetX = bRect.left + (bRect.right - bRect.left) / 2;
                                                    uiaFound = true;
                                                }
                                            }
                                        }
                                    }
                                    SysFreeString(name);
                                }
                                pItem->Release();
                            }
                        }
                        pArray->Release();
                    }
                    if (pButtonCond) pButtonCond->Release();
                    if (pListItemCond) pListItemCond->Release();
                    if (pOrCond) pOrCond->Release();
                    pTrayElement->Release();
                }
            }
            pAutomation->Release();
        }
        if (coInit) CoUninitialize();
    }

    std::lock_guard<std::mutex> lock(g_CacheMutex);
    if (uiaFound) {
        g_IconPositions[hWndApp] = targetX;
        if (!processKey.empty()) {
            g_ProcessIconPositions[processKey] = targetX;
        }
    } else if (g_IconPositions.count(hWndApp)) {
        targetX = g_IconPositions[hWndApp];
    } else if (!processKey.empty() && g_ProcessIconPositions.count(processKey)) {
        targetX = g_ProcessIconPositions[processKey];
    }

    return targetX;
}

// Shared by both engines: union of the monitors involved (the window's own
// monitor and the monitor the genie funnels to). With Multi-monitor support off
// (the default) data->hMon is the primary monitor, so a window sitting on a
// secondary display still needs its own monitor in the union or the canvas would
// collapse onto the primary and clip the first half of the animation.
static RECT GetGenieMonitorUnion(const RECT& winRect, HMONITOR hDockMon) {
    RECT winMon;
    HMONITOR hWinMon = MonitorFromRect(&winRect, MONITOR_DEFAULTTONEAREST);
    MONITORINFO wmi = {0};
    wmi.cbSize = sizeof(wmi);
    if (hWinMon && GetMonitorInfoW(hWinMon, &wmi)) {
        winMon = wmi.rcMonitor;
    } else {
        winMon = winRect;
    }

    RECT dockMon;
    MONITORINFO dmi = {0};
    dmi.cbSize = sizeof(dmi);
    if (hDockMon && GetMonitorInfoW(hDockMon, &dmi)) {
        dockMon = dmi.rcMonitor;
    } else {
        dockMon = winMon;
    }

    RECT u;
    u.left   = (winMon.left   < dockMon.left)   ? winMon.left   : dockMon.left;
    u.top    = (winMon.top    < dockMon.top)    ? winMon.top    : dockMon.top;
    u.right  = (winMon.right  > dockMon.right)  ? winMon.right  : dockMon.right;
    u.bottom = (winMon.bottom > dockMon.bottom) ? winMon.bottom : dockMon.bottom;
    return u;
}

// Shared by both engines: clamp a proposed canvas rect to the union of the
// monitors involved, then re-expand it so it can never clip the window rect or
// the dock target column away. The monitor clamp only trims the box to the
// screens actually used; it must never cut geometry the animation draws.
static void ClampGenieCanvas(int& left, int& top, int& right, int& bottom,
                             const RECT& winRect, int dockX, const RECT& monUnion) {
    if (left   < monUnion.left)   left   = monUnion.left;
    if (right  > monUnion.right)  right  = monUnion.right;
    if (top    < monUnion.top)    top    = monUnion.top;
    if (bottom > monUnion.bottom) bottom = monUnion.bottom;

    if (left  > winRect.left)  left  = winRect.left;
    if (right < winRect.right) right = winRect.right;
    if (top   > winRect.top)   top   = winRect.top;
    if (bottom < winRect.bottom) bottom = winRect.bottom;
    if (left  > dockX - 16)    left  = dockX - 16;   // iGeom is dockX ± 11
    if (right < dockX + 16)    right = dockX + 16;
}

// -------------------------------------------------------------------------
// CLASSIC engine: the mod's original (v2.2.0) per-output-scanline GDI strip warp,
// kept as an opt-in alternative to Potassiumuncher's Direct2D engine (some users
// preferred it). It consumes the SAME MacGenieAnimData contract the modern engine
// does - snapshot bitmap, extended-frame rect, dock target, first-frame event - and
// runs the SAME teardown (reveal, DWM transitions, auto-hide deferred minimize,
// worker drain), so it drops in behind the identical setup/capture path; only the
// per-frame rendering differs. Selected by the "Animation style" setting.
// -------------------------------------------------------------------------
DWORD WINAPI MacGenieAnimThreadClassic(LPVOID lpParam) {
    MacGenieAnimData* data = (MacGenieAnimData*)lpParam;

    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

    const int W = data->width;
    const int H = data->height;
    // The monitor the genie plays on, in virtual-screen coords (from the shared
    // setup: the primary when multi-monitor support is off, else the window's).
    RECT mon;
    MONITORINFO mmi; mmi.cbSize = sizeof(mmi);
    if (data->hMon && GetMonitorInfoW(data->hMon, &mmi)) {
        mon = mmi.rcMonitor;
    } else {
        mon.left = 0; mon.top = 0;
        mon.right = GetSystemMetrics(SM_CXSCREEN);
        mon.bottom = GetSystemMetrics(SM_CYSCREEN);
    }

    const int origLeft = data->targetRect.left;
    const int origTop  = data->targetRect.top;
    const float origCenterX = (float)origLeft + W * 0.5f;

    // Where the genie funnels to: the learned taskbar icon X. The dock Y is taken
    // from the real per-monitor taskbar window (the same signal the modern engine
    // uses), so a taskbar pinned to the top of the screen is honored too -
    // previously the classic style always assumed the taskbar was at the bottom of
    // the monitor. A single global shell query would get the wrong edge on setups
    // where each monitor's taskbar can be on a different edge.
    int dockX = data->targetDockX;
    if (dockX < mon.left) dockX = mon.left;
    if (dockX > mon.right) dockX = mon.right;
    const float dockXf = (float)dockX;
    // The window funnels into an icon-sized square centred on the taskbar
    // button, rather than into a line on the screen edge below it. The centre
    // comes from the taskbar's height measured from the monitor edge it's
    // docked to, not from where its window currently is, so an auto-hidden
    // taskbar that's still sliding in is aimed at where it will settle.
    float iconSize = 24.0f;
    float dockY = (float)mon.bottom - iconSize;
    if (HWND hTray = FindTaskbarForMonitor(data->hMon)) {
        RECT tr;
        if (GetWindowRect(hTray, &tr) && tr.bottom > tr.top) {
            float barH = (float)(tr.bottom - tr.top);
            iconSize = std::clamp(barH * 0.5f, 16.0f, 64.0f);
            dockY = (tr.top + tr.bottom) < (mon.top + mon.bottom)  // upper half
                        ? (float)mon.top + barH * 0.5f
                        : (float)mon.bottom - barH * 0.5f;
        }
    }
    // Which end leads the morph. Taken from the window's mid-line so a dock that
    // lands exactly on the window edge still picks a sane direction, and kept
    // outside the window's own vertical span so the yb[] map below stays
    // monotonic (a hard requirement of the scanline walk).
    const float origBottomF = (float)(origTop + H);
    const float halfIcon = iconSize * 0.5f;
    const bool dockAbove = dockY < ((float)origTop + origBottomF) * 0.5f;
    if (dockAbove) { if (dockY > (float)origTop - halfIcon) dockY = (float)origTop - halfIcon; }
    else           { if (dockY < origBottomF + halfIcon)    dockY = origBottomF + halfIcon; }
    const float neckW = iconSize;

    // Bounding box the funnel can occupy. Both the window's own monitor and the
    // monitor it funnels to are unioned; the box is trimmed to them but never
    // allowed to clip the window rect or the dock column away (shared helper -
    // the same clipping bug the modern engine had, when multi-monitor support is
    // off and the window sits on a secondary display).
    int boundLeft   = (origLeft < dockX ? origLeft : dockX) - W / 2;
    int boundRight  = ((origLeft + W) > dockX ? (origLeft + W) : dockX) + W / 2;
    // Span from the dock edge up/down to the window so a top taskbar (dock above
    // the window) is inside the canvas instead of below the box.
    int boundTop    = std::min<int>(origTop, (int)(dockY - halfIcon));
    int boundBottom = std::max<int>(origTop + H, (int)(dockY + halfIcon) + 1);
    RECT monUnion = GetGenieMonitorUnion(data->targetRect, data->hMon);
    ClampGenieCanvas(boundLeft, boundTop, boundRight, boundBottom,
                     data->targetRect, dockX, monUnion);
    int boundW = boundRight - boundLeft;
    int boundH = boundBottom - boundTop;
    if (boundW < 1) boundW = 1;
    if (boundH < 1) boundH = 1;

    // Layered "ghost" window (plain STATIC class - no custom WndProc). Created
    // hidden, shown after frame 1.
    //
    // WS_EX_NOACTIVATE matters beyond focus: the canvas stretches from the
    // window down to the taskbar, and auto-hide treats any visible window over
    // the taskbar as a reason to hide it -- except no-activate ones. Without
    // it, the taskbar dropped out for a moment at the end of every genie.
    HWND hGhost = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_TRANSPARENT |
            WS_EX_NOACTIVATE,
        L"STATIC", NULL, WS_POPUP,
        boundLeft, boundTop, boundW, boundH,
        NULL, NULL, NULL, NULL);

    HDC hScreenDC = GetDC(NULL);

    // Source pixels as a readable 32-bit top-down DIB.
    HDC hSrcDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hOldSrc = (HBITMAP)SelectObject(hSrcDC, data->hBitmap);

    BITMAPINFO sbmi;
    ZeroMemory(&sbmi, sizeof(sbmi));
    sbmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    sbmi.bmiHeader.biWidth = W;
    sbmi.bmiHeader.biHeight = -H;       // negative height = top-down
    sbmi.bmiHeader.biPlanes = 1;
    sbmi.bmiHeader.biBitCount = 32;
    sbmi.bmiHeader.biCompression = BI_RGB;
    BYTE* srcBits = NULL;
    HBITMAP hSrcDib = CreateDIBSection(hScreenDC, &sbmi, DIB_RGB_COLORS, (void**)&srcBits, NULL, 0);
    HDC hSrcDibDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hOldSrcDib = (HBITMAP)SelectObject(hSrcDibDC, hSrcDib);
    BitBlt(hSrcDibDC, 0, 0, W, H, hSrcDC, 0, 0, SRCCOPY);
    GdiFlush();
    const int srcStride = W * 4;

    // Canvas: 32-bit top-down DIB for genuine per-pixel alpha.
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = boundW;
    bmi.bmiHeader.biHeight = -boundH;   // negative height = top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    BYTE* pBits = NULL;
    HBITMAP hCanvas = CreateDIBSection(hScreenDC, &bmi, DIB_RGB_COLORS, (void**)&pBits, NULL, 0);
    HDC hCanvasDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hOldCanvas = (HBITMAP)SelectObject(hCanvasDC, hCanvas);
    const int canvasStride = boundW * 4;

    const float SPREAD = 0.65f;   // rows nearest the dock lead the morph (the
                                  // spread v is flipped when the dock is above)
    const size_t canvasBytes = (size_t)boundW * 4 * boundH;

    const double totalMs = (double)data->durationMs;

    auto morphAt = [&](float v, float tt) -> float {
        float m = tt * (1.0f + SPREAD) - (1.0f - v) * SPREAD;
        if (m < 0.0f) m = 0.0f;
        if (m > 1.0f) m = 1.0f;
        return m * m * (3.0f - 2.0f * m);
    };

    float* yb = new float[H + 1];

    // Time-based progress; frames paced at 2x refresh below (no per-frame
    // DwmFlush, same as the modern engine, so the classic warp hits the same
    // half-rate cliff only if its per-frame work ever exceeds the interval).
    LARGE_INTEGER qpcFreq, qpcStart, qpcNow;
    QueryPerformanceFrequency(&qpcFreq);
    QueryPerformanceCounter(&qpcStart);

    // Frame pacing, 2x the display's refresh rate (fallback 240 fps).
    double frameIntervalMs = 1000.0 / 240.0;
    DWM_TIMING_INFO ti = { sizeof(ti) };
    if (SUCCEEDED(DwmGetCompositionTimingInfo(NULL, &ti)) && ti.rateRefresh.uiDenominator) {
        double hz = (double)ti.rateRefresh.uiNumerator / ti.rateRefresh.uiDenominator;
        frameIntervalMs = 1000.0 / (hz * 2.0);
    }
    HANDLE hFrameTimer = CreateWaitableTimerExW(
        nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
        TIMER_MODIFY_STATE | SYNCHRONIZE);
    if (!hFrameTimer) {
        // Pre-1803: the high-resolution flag isn't recognized. A plain waitable
        // timer paces coarser, but it still isn't a spin.
        hFrameTimer = CreateWaitableTimerExW(nullptr, nullptr, 0,
                                             TIMER_MODIFY_STATE | SYNCHRONIZE);
    }

    BOOL firstFrame = TRUE;
    for (;;) {
        QueryPerformanceCounter(&qpcNow);
        const LARGE_INTEGER qpcFrameStart = qpcNow;
        double elapsedMs = (qpcNow.QuadPart - qpcStart.QuadPart) * 1000.0 / qpcFreq.QuadPart;
        BOOL lastFrame = (elapsedMs >= totalMs);
        float progress = lastFrame ? 1.0f : (float)(elapsedMs / totalMs);

        // 0 = full window at its place, 1 = collapsed at the dock; restore reverses.
        float tt = data->isRising ? (1.0f - progress) : progress;

        // Fade over the last stretch towards the icon, eased at both ends, so
        // a restore grows out of the icon rather than popping in, and a
        // minimize melts into it. The old linear 20% ramp was short enough
        // that the small copy read as appearing out of nowhere.
        constexpr float kFadeSpan = 0.35f;
        float fade = (1.0f - tt) / kFadeSpan;
        if (fade < 0.0f) fade = 0.0f;
        if (fade > 1.0f) fade = 1.0f;
        fade = fade * fade * (3.0f - 2.0f * fade);

        memset(pBits, 0, canvasBytes);

        // Vertical map: where each source row lands on screen this frame. The rows
        // nearest the dock lead the morph (bottom-first neck when the dock is
        // below the window, top-first when it's above), so the spread v is
        // flipped accordingly.
        for (int k = 0; k <= H; ++k) {
            float v = (float)k / (float)H;
            float e = morphAt(dockAbove ? (1.0f - v) : v, tt);
            float idY = (float)origTop + (float)H * v;
            // Each row lands at its place within the icon square, so the
            // window ends as a scaled-down copy on the icon, not a sliver.
            float iconY = dockY - halfIcon + iconSize * v;
            yb[k] = idY + (iconY - idY) * e;
        }

        int kSeg = 0;
        for (int yC = 0; yC < boundH; ++yC) {
            float screenY = (float)(yC + boundTop) + 0.5f;
            if (screenY < yb[0] || screenY >= yb[H]) continue;

            while (kSeg < H - 1 && yb[kSeg + 1] <= screenY) kSeg++;
            float segH = yb[kSeg + 1] - yb[kSeg];
            float frac = segH > 1e-4f ? (screenY - yb[kSeg]) / segH : 0.0f;
            float v = ((float)kSeg + frac) / (float)H;

            float em = morphAt(dockAbove ? (1.0f - v) : v, tt);
            float width = (float)W + (neckW - (float)W) * em;
            if (width < 1.0f) width = 1.0f;
            float cx = origCenterX + (dockXf - origCenterX) * em;
            float leftCanvas = (cx - width * 0.5f) - (float)boundLeft;

            int srcRow = (int)(v * (float)H);
            if (srcRow < 0) srcRow = 0;
            if (srcRow > H - 1) srcRow = H - 1;
            const BYTE* srcRowPtr = srcBits + (size_t)srcRow * srcStride;
            BYTE* dstRowPtr = pBits + (size_t)yC * canvasStride;

            int xStart = (int)leftCanvas;
            int xEnd   = (int)(leftCanvas + width) + 1;
            if (xStart < 0) xStart = 0;
            if (xEnd > boundW) xEnd = boundW;

            float invW = 1.0f / width;
            for (int xC = xStart; xC < xEnd; ++xC) {
                float u = ((float)xC + 0.5f - leftCanvas) * invW;
                if (u < 0.0f || u >= 1.0f) continue;
                int srcX = (int)(u * (float)W);
                if (srcX < 0) srcX = 0;
                if (srcX > W - 1) srcX = W - 1;
                const BYTE* sp = srcRowPtr + (size_t)srcX * 4;
                BYTE* dp = dstRowPtr + (size_t)xC * 4;
                // Snapshots carry premultiplied per-pixel alpha since the v1.5
                // capture fix (translucent regions have a<255); propagate it so
                // acrylic areas stay see-through in the classic style too. The
                // constant alpha below still applies the global fade on top.
                dp[0] = sp[0];
                dp[1] = sp[1];
                dp[2] = sp[2];
                dp[3] = sp[3];
            }
        }

        POINT ptDst = { boundLeft, boundTop };
        SIZE  sz    = { boundW, boundH };
        POINT ptSrc = { 0, 0 };
        BLENDFUNCTION bf;
        bf.BlendOp = AC_SRC_OVER;
        bf.BlendFlags = 0;
        bf.SourceConstantAlpha = (BYTE)(255.0f * fade);
        bf.AlphaFormat = AC_SRC_ALPHA;
        UpdateLayeredWindow(hGhost, hScreenDC, &ptDst, &sz, hCanvasDC, &ptSrc, 0, &bf, ULW_ALPHA);

        if (firstFrame) {
            ShowWindow(hGhost, SW_SHOWNOACTIVATE);
        }

        if (lastFrame) break;
        if (g_unloading.load(std::memory_order_relaxed)) break;

        // First frame is on screen; release the minimize hook it was holding.
        // One DwmFlush here matches the modern engine's handoff sync; subsequent
        // frames are paced by the timer below instead of a per-frame DwmFlush.
        if (firstFrame) {
            DwmFlush();
            firstFrame = FALSE;
            if (data->hFirstFrameShown) SetEvent(data->hFirstFrameShown);
        }

        // Rate-limit to ~2x refresh so we never busy-spin. The deadline is
        // measured from the START of the frame, so the period is
        // max(render, interval), not render + interval (a render faster than
        // the interval just sleeps the difference; a slower one runs as-is).
        // Capped wait keeps the g_unloading check responsive. When a frame
        // overruns its budget, SwitchToThread still yields the quantum.
        if (hFrameTimer) {
            LARGE_INTEGER qpcAfter;
            QueryPerformanceCounter(&qpcAfter);
            double spentMs = (qpcAfter.QuadPart - qpcFrameStart.QuadPart) * 1000.0 / qpcFreq.QuadPart;
            double waitMs = frameIntervalMs - spentMs;
            if (waitMs > 0.1) {
                LARGE_INTEGER due;
                due.QuadPart = -(LONGLONG)(waitMs * 10000.0);   // 100ns units
                SetWaitableTimer(hFrameTimer, &due, 0, nullptr, nullptr, FALSE);
                WaitForSingleObject(hFrameTimer, 100);
            } else {
                SwitchToThread();   // overran the budget - yield the quantum anyway
            }
        } else {
            Sleep(1);   // last resort: never spin
        }
    }

    if (hFrameTimer) { CloseHandle(hFrameTimer); hFrameTimer = nullptr; }

    // -------------------- COMMON TEARDOWN (identical to the modern engine) -----
    if (data->isRising) {
        // Reveal the real window the same way it was hidden.
        if (data->hiddenByCloak) {
            MacGenieSetCloak(data->hRealWnd, FALSE);
        } else {
            SetLayeredWindowAttributes(data->hRealWnd, 0, 255, LWA_ALPHA);
            if (!(data->originalExStyle & WS_EX_LAYERED)) {
                SetWindowLongPtrW(data->hRealWnd, GWL_EXSTYLE, data->originalExStyle);
            }
        }
    }

    // The auto-hide deferred path keeps transitions disabled through the real
    // (deferred) minimize below; every other path re-enables them now.
    if (!data->deferredMinimize) {
        MacGenieSetDwmTransitions(data->hRealWnd, TRUE);
    }

    delete[] yb;
    SelectObject(hCanvasDC, hOldCanvas);
    SelectObject(hSrcDibDC, hOldSrcDib);
    SelectObject(hSrcDC, hOldSrc);
    DeleteObject(hCanvas);
    DeleteObject(hSrcDib);
    DeleteObject(data->hBitmap);
    DeleteDC(hCanvasDC);
    DeleteDC(hSrcDibDC);
    DeleteDC(hSrcDC);
    ReleaseDC(NULL, hScreenDC);
    DestroyWindow(hGhost);

    if (data->hFirstFrameShown) {
        SetEvent(data->hFirstFrameShown);
        CloseHandle(data->hFirstFrameShown);
    }

    // Auto-hide "unhide" completion (Potassiumuncher's engine) - same as the modern
    // thread, so the auto-hide feature behaves identically under the classic style.
    if (data->deferredMinimize) {
        int unhideMs = data->unhideDurationMs;
        int animMs = data->durationMs;
        if (data->requestedUnhide && unhideMs > animMs) {
            // Chunked sleep so the unload drain (which waits ~3 s for workers to
            // see g_unloading) never times out on the up-to-~5 s unhide wait and
            // leaves this thread in unmapped code.
            int remaining = unhideMs - animMs;
            while (remaining > 0 && !g_unloading.load(std::memory_order_relaxed)) {
                int chunk = remaining < 20 ? remaining : 20;
                Sleep(chunk);
                remaining -= chunk;
            }
        }
        if (!g_unloading.load(std::memory_order_relaxed) && IsWindow(data->hRealWnd)) {
            SetPropW(data->hRealWnd, L"GenieBypass", (HANDLE)1);
            SendMessageTimeoutW(data->hRealWnd, WM_SYSCOMMAND, SC_MINIMIZE, 0, SMTO_NORMAL, 1000, NULL);
            RemovePropW(data->hRealWnd, L"GenieBypass");
        }
        if (IsWindow(data->hRealWnd)) {
            MacGenieSetCloak(data->hRealWnd, FALSE);
            MacGenieSetDwmTransitions(data->hRealWnd, TRUE);
        }
        if (data->requestedUnhide && !g_unloading.load(std::memory_order_relaxed)) {
            if (data->hNextApp && IsWindow(data->hNextApp) && IsWindowVisible(data->hNextApp)) {
                SetForegroundWindow(data->hNextApp);
            } else {
                SetForegroundWindow(FindWindowW(L"Progman", NULL));
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(g_CacheMutex);
        g_AnimActive.erase(data->hRealWnd);
    }
    delete data;
    g_workerCount.fetch_sub(1, std::memory_order_release);
    return 0;
}

// -------------------------------------------------------------------------
// Core Setup Engine & Smart Tracking Logic
// -------------------------------------------------------------------------
// Returns TRUE if the worker thread was spawned (the animation is now in flight).
// `originalExStyle` is the window's TRUE extended style, captured by the caller
// BEFORE any WS_EX_LAYERED was added for the hide (rising callers hide first).
// `cloakHidden` says how a rising caller hid the window: TRUE = DWM cloak (restore),
// FALSE = WS_EX_LAYERED + alpha 0 (launch). The deferred* args drive the auto-hide
// "unhide" path (falling only): the caller cloaks the window after our first frame
// and we perform the real minimize at teardown.
bool StartMacGenieAnim(HWND hWnd, BOOL rising, LONG_PTR originalExStyle,
                       BOOL cloakHidden = FALSE, BOOL deferredMinimize = FALSE,
                       BOOL requestedUnhide = FALSE, HWND hNextApp = NULL) {
    // Bound the snapshot cache: windows closed while minimized (the common
    // WM_DESTROY + PostQuitMessage path never reaches DefWindowProcW, where the
    // usual cleanup lives) would otherwise leave their full DIB behind until
    // unload. Cheap sweep at the single animation entry point.
    {
        std::lock_guard<std::mutex> lock(g_CacheMutex);
        for (auto it = g_SnapshotCache.begin(); it != g_SnapshotCache.end();) {
            if (!IsWindow(it->first)) {
                DeleteObject(it->second.hBmp);
                it = g_SnapshotCache.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Animation geometry via DWM extended frame bounds (Goal D / bug #4670):
    // GetWindowRect returns the legacy frame (invisible resize borders / shadow),
    // which is offset a few px from where the pixels actually are on composited
    // windows. Snapshot AND warp both use the extended-frame rect, so keyboard /
    // AutoHotkey minimizes are pixel-aligned. Scheme from Potassiumuncher's engine.
    RECT winRect;
    if (!GetWindowRect(hWnd, &winRect)) {
        if (rising) MacGenieUndoRisingHide(hWnd, originalExStyle, cloakHidden);
        else MacGenieSetDwmTransitions(hWnd, TRUE);
        return false;
    }
    RECT rect = winRect, extRect;
    if (SUCCEEDED(DwmGetWindowAttribute(hWnd, DWMWA_EXTENDED_FRAME_BOUNDS, &extRect, sizeof(extRect)))) {
        rect = extRect;
    }
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;
    int offsetX = rect.left - winRect.left;
    int offsetY = rect.top - winRect.top;
    int rawW = winRect.right - winRect.left;
    int rawH = winRect.bottom - winRect.top;

    if (w <= 0 || h <= 0 || rawW <= 0 || rawH <= 0) {
        if (rising) MacGenieUndoRisingHide(hWnd, originalExStyle, cloakHidden);
        else MacGenieSetDwmTransitions(hWnd, TRUE);
        return false;
    }

    // One genie per window at a time. Refused while unloading.
    bool blocked = g_unloading.load(std::memory_order_relaxed);
    if (!blocked) {
        std::lock_guard<std::mutex> lock(g_CacheMutex);
        blocked = !g_AnimActive.insert(hWnd).second;
    }
    if (blocked) {
        if (rising) {
            MacGenieUndoRisingHide(hWnd, originalExStyle, cloakHidden);
        } else {
            bool owned;
            {
                std::lock_guard<std::mutex> lock(g_CacheMutex);
                owned = g_AnimActive.count(hWnd) != 0;
            }
            if (!owned) MacGenieSetDwmTransitions(hWnd, TRUE);
        }
        return false;
    }

    // The genie always funnels to the primary monitor's taskbar.
    HMONITOR hMon = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(hMon, &mi)) {
        mi.rcMonitor.left = 0; mi.rcMonitor.top = 0;
        mi.rcMonitor.right = GetSystemMetrics(SM_CXSCREEN);
        mi.rcMonitor.bottom = GetSystemMetrics(SM_CYSCREEN);
    }
    int monWidth = mi.rcMonitor.right - mi.rcMonitor.left;

    // Default dock target before UIA (Potassiumuncher's engine): left-aligned
    // taskbars start near the monitor's left, centered ones at its middle.
    DWORD alignVal = 1, dataSize = sizeof(alignVal);
    RegGetValueW(HKEY_CURRENT_USER,
                 L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
                 L"TaskbarAl", RRF_RT_REG_DWORD, NULL, &alignVal, &dataSize);
    int learnedTargetX = (alignVal == 0) ? (mi.rcMonitor.left + 160)
                                         : (mi.rcMonitor.left + monWidth / 2);

    // --- Per-window targeting ---
    // UI Automation (GetTaskbarButtonX) matches taskbar buttons by title / process
    // name, which can't reliably tell apart several windows of the SAME process
    // (e.g. multiple Brave profiles): they tie on the process-name score and it
    // resolves them all to the first-opened window's button. So MINE's cursor +
    // per-window signal takes priority; UIA is only a fallback for a window we've
    // never seen minimized from the taskbar (frame 0 of the warp is the identity,
    // so targetDockX is only needed for later frames).
    //
    // 1. Cursor over the taskbar = the user clicked THIS window's own taskbar
    //    button, so its X is exactly the icon - the only fully reliable per-window
    //    signal when windows share a process. Learn it, keyed by HWND (never shared
    //    across sibling windows).
    POINT pt;
    GetCursorPos(&pt);
    RECT workArea;
    MONITORINFO cursorMi;
    cursorMi.cbSize = sizeof(cursorMi);
    if (GetMonitorInfoW(MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST), &cursorMi)) {
        workArea = cursorMi.rcWork;
    } else {
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    }

    if (!PtInRect(&workArea, pt)) {
        learnedTargetX = pt.x;                        // steal the click X
        std::lock_guard<std::mutex> lock(g_CacheMutex);
        g_IconPositions[hWnd] = learnedTargetX;       // remember it for this window
    } else {
        // 2. Title-bar / keyboard minimize: locate the taskbar button via UI
        //    Automation EVERY time (Potassiumuncher's targeting). Reusing a
        //    per-window cache here made the button-minimize land past the icon when
        //    the cached spot was stale; running UIA fresh lands it on the icon.
        //    GetTaskbarButtonX still falls back to the per-window / per-process cache
        //    (including a position learned from an earlier taskbar click) internally
        //    if UIA can't match, so nothing is lost.
        learnedTargetX = GetTaskbarButtonX(hWnd, learnedTargetX, hMon);
    }

    MacGenieAnimData* data = new MacGenieAnimData();
    data->hRealWnd = hWnd;
    data->targetRect = rect;
    data->hMon = hMon;
    data->width = w;
    data->height = h;
    data->isRising = rising;
    data->targetDockX = learnedTargetX;
    data->originalExStyle = originalExStyle;
    data->hiddenByCloak = cloakHidden;
    data->hFirstFrameShown = NULL;
    data->durationMs = g_durationMs.load(std::memory_order_relaxed);
    data->requestedUnhide = requestedUnhide;
    data->hNextApp = hNextApp;
    data->unhideDurationMs = g_unhideDurationMs.load(std::memory_order_relaxed);
    data->deferredMinimize = deferredMinimize;
    data->pBits = nullptr;
    data->hBitmap = NULL;

    HDC hScreenDC = GetDC(NULL);

    // Premultiplied 32-bit top-down DIB - D2D's CreateBitmap reads pBits directly.
    BITMAPINFO bmi = {{0}};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    data->hBitmap = CreateDIBSection(hScreenDC, &bmi, DIB_RGB_COLORS, &data->pBits, NULL, 0);

    if (!data->hBitmap || !data->pBits) {
        ReleaseDC(NULL, hScreenDC);
        if (rising) MacGenieUndoRisingHide(hWnd, originalExStyle, cloakHidden);
        else MacGenieSetDwmTransitions(hWnd, TRUE);
        if (data->hBitmap) DeleteObject(data->hBitmap);
        delete data;
        std::lock_guard<std::mutex> lock(g_CacheMutex);
        g_AnimActive.erase(hWnd);
        return false;
    }

    // Copy a rawW x rawH PrintWindow capture into the w x h snapshot at the
    // extended-frame offset. Potassiumuncher's v1.5 grey-screen fix: PrintWindow
    // (PW_RENDERFULLCONTENT) returns pixels that are ALREADY premultiplied, so pass
    // them through untouched and keep a==0 pixels fully TRANSPARENT. The old code
    // forced every pixel opaque, which flattened acrylic / Mica backdrop regions
    // (alpha 0 or partial) into a solid grey - the "grey top" on translucent
    // windows. With alpha preserved, those regions stay see-through during the
    // genie and the desktop shows through them, matching the window's real look.
    auto CopySnapshot = [&](void* srcBits) {
        DWORD* src = (DWORD*)srcBits;
        DWORD* dst = (DWORD*)data->pBits;
        int inBounds = 0, zeroAlpha = 0;
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                int sx = x + offsetX;
                int sy = y + offsetY;
                if (sx >= 0 && sx < rawW && sy >= 0 && sy < rawH) {
                    DWORD p = src[sy * rawW + sx];
                    BYTE a = (BYTE)((p >> 24) & 0xFF);
                    inBounds++;
                    if (a == 0) zeroAlpha++;
                    dst[y * w + x] = (a == 0) ? 0 : p;
                } else {
                    dst[y * w + x] = 0;
                }
            }
        }
        // Fallback: on NON-PRIMARY monitors DWM often returns the entire Mica /
        // acrylic backdrop region as a==0 - with alpha preserved the whole window
        // background would simply VANISH during the genie (reported on Explorer on
        // a secondary display). If most of the window came back fully transparent,
        // the alpha channel clearly isn't describing real translucency, so force
        // those pixels opaque - restoring the pre-3.1 dark-backdrop look for this
        // case, which beats a see-through hole. Windows where transparency is
        // sparse (rounded corners, small blur regions) keep the faithful alpha.
        if (inBounds > 0 && zeroAlpha * 2 > inBounds) {
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    int sx = x + offsetX;
                    int sy = y + offsetY;
                    if (sx >= 0 && sx < rawW && sy >= 0 && sy < rawH) {
                        DWORD p = src[sy * rawW + sx];
                        if (((p >> 24) & 0xFF) == 0) {
                            dst[y * w + x] = 0xFF000000u | (p & 0x00FFFFFFu);
                        }
                    }
                }
            }
        }
    };

    // Window-only capture: grab the window into a rawW x rawH DIB via PrintWindow,
    // then CopySnapshot into the extended-frame snapshot. PrintWindow renders only
    // the window's own content, so the taskbar / other windows can never bleed into
    // the animation (Potassiumuncher's taskbar fix) - and it also works on a
    // cloaked window (the rising-no-cache path), where a screen grab would not.
    auto CaptureNow = [&]() -> bool {
        HDC hTempDC = CreateCompatibleDC(hScreenDC);
        BITMAPINFO bmiTemp = bmi;
        bmiTemp.bmiHeader.biWidth = rawW;
        bmiTemp.bmiHeader.biHeight = -rawH;
        void* pTempBits = nullptr;
        HBITMAP hTempBmp = CreateDIBSection(hScreenDC, &bmiTemp, DIB_RGB_COLORS, &pTempBits, NULL, 0);
        if (!hTempBmp || !pTempBits) {
            if (hTempBmp) DeleteObject(hTempBmp);
            DeleteDC(hTempDC);
            return false;
        }
        HBITMAP hOldTempBmp = (HBITMAP)SelectObject(hTempDC, hTempBmp);
        PrintWindow(hWnd, hTempDC, PW_RENDERFULLCONTENT);
        GdiFlush();
        CopySnapshot(pTempBits);
        SelectObject(hTempDC, hOldTempBmp);
        DeleteObject(hTempBmp);
        DeleteDC(hTempDC);
        return true;
    };

    if (rising) {
        BOOL fromCache = FALSE;
        {
            std::lock_guard<std::mutex> lock(g_CacheMutex);
            auto it = g_SnapshotCache.find(hWnd);
            if (it != g_SnapshotCache.end()) {
                SnapCache& c = it->second;
                if (c.w == w && c.h == h) {
                    memcpy(data->pBits, c.pBits, (size_t)w * h * 4);
                    fromCache = TRUE;
                }
                DeleteObject(c.hBmp);
                g_SnapshotCache.erase(it);
            }
        }
        // No cached minimize snapshot (e.g. window was already minimized before the
        // mod loaded): snapshot the freshly-restored (cloaked) window directly.
        if (!fromCache) CaptureNow();
    } else {
        // Falling: capture the WINDOW ONLY via PrintWindow (Potassiumuncher's
        // method), NOT a screen BitBlt. A screen grab of the window rect also pulls
        // in whatever is behind / around the window - most visibly the taskbar on
        // maximized / fullscreen apps, which then warps inside the genie. PrintWindow
        // renders just the window's own content, so the taskbar (a separate window)
        // can never bleed in. Tradeoff: DWM acrylic/translucency backdrops aren't
        // part of the window's own render, so translucent windows animate opaque - a
        // documented limitation, far less jarring than the taskbar appearing.
        CaptureNow();
        // Cache a copy keyed by HWND for the later restore animation.
        std::lock_guard<std::mutex> lock(g_CacheMutex);
        auto it = g_SnapshotCache.find(hWnd);
        if (it != g_SnapshotCache.end()) {
            DeleteObject(it->second.hBmp);
            g_SnapshotCache.erase(it);
        }
        void* pCacheBits = nullptr;
        HBITMAP hCacheBmp = CreateDIBSection(hScreenDC, &bmi, DIB_RGB_COLORS, &pCacheBits, NULL, 0);
        if (hCacheBmp && pCacheBits) {
            memcpy(pCacheBits, data->pBits, (size_t)w * h * 4);
            g_SnapshotCache[hWnd] = { hCacheBmp, pCacheBits, w, h };
        } else if (hCacheBmp) {
            DeleteObject(hCacheBmp);
        }
    }

    ReleaseDC(NULL, hScreenDC);

    // For a minimize (normal OR deferred) hold the caller back until the ghost's
    // first frame is composed - otherwise the window (or, on the deferred path, the
    // cloak-hide) would happen a few frames before the genie appears (a visible
    // gap, especially on power-throttled CPUs). Two handles to one event: the
    // thread owns its duplicate, we own and wait on the original.
    HANDLE hFirstShown = NULL;
    if (!rising) {
        hFirstShown = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (hFirstShown &&
            !DuplicateHandle(GetCurrentProcess(), hFirstShown, GetCurrentProcess(),
                             &data->hFirstFrameShown, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
            data->hFirstFrameShown = NULL;
        }
    }
    bool waitForFirstFrame = (data->hFirstFrameShown != NULL);

    g_workerCount.fetch_add(1, std::memory_order_relaxed);
    HANDLE hThread =
        CreateThread(NULL, 0, MacGenieAnimThreadClassic, data, 0, NULL);
    if (hThread) {
        CloseHandle(hThread);
        if (hFirstShown) {
            if (waitForFirstFrame) {
                WaitForSingleObject(hFirstShown, 200);   // capped: worst case = old behavior
            }
            CloseHandle(hFirstShown);
        }
        return true;
    }

    // Thread couldn't start - undo our state so the window isn't left invisible or
    // with transitions permanently disabled.
    g_workerCount.fetch_sub(1, std::memory_order_release);
    if (rising) {
        MacGenieUndoRisingHide(hWnd, data->originalExStyle, data->hiddenByCloak);
    } else {
        MacGenieSetDwmTransitions(hWnd, TRUE);
    }
    if (hFirstShown) CloseHandle(hFirstShown);
    if (data->hFirstFrameShown) CloseHandle(data->hFirstFrameShown);
    DeleteObject(data->hBitmap);
    delete data;
    {
        std::lock_guard<std::mutex> lock(g_CacheMutex);
        g_AnimActive.erase(hWnd);
    }
    return false;
}

// ShowWindow, dispatched to from the shared hook.
BOOL OnShowWindow(HWND hWnd, int nCmdShow) {
    if (nCmdShow == SW_MINIMIZE || nCmdShow == SW_SHOWMINIMIZED || nCmdShow == SW_SHOWMINNOACTIVE) {
        if (MacGenieShouldAnimate(hWnd)) {
            MacGenieSetDwmTransitions(hWnd, FALSE);
            StartMacGenieAnim(hWnd, FALSE, GetWindowLongPtrW(hWnd, GWL_EXSTYLE));
        }
        return ShowWindow_Original(hWnd, nCmdShow);
    }

    if ((nCmdShow == SW_RESTORE || nCmdShow == SW_SHOWNORMAL) && IsIconic(hWnd)) {
        if (g_openAnimation.load(std::memory_order_relaxed) && MacGenieShouldAnimate(hWnd)) {
            MacGenieSetDwmTransitions(hWnd, FALSE);
            // Cloak: a frame-level hide, with no layered-surface rebuild flash.
            MacGenieSetCloak(hWnd, TRUE);
            BOOL res = ShowWindow_Original(hWnd, nCmdShow);
            StartMacGenieAnim(hWnd, TRUE, GetWindowLongPtrW(hWnd, GWL_EXSTYLE), TRUE);
            return res;
        }
    }

    return ShowWindow_Original(hWnd, nCmdShow);
}

LRESULT WINAPI DefWindowProcW_Hook(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    if (Msg == WM_DESTROY) {
        std::lock_guard<std::mutex> lock(g_CacheMutex);
        auto it = g_SnapshotCache.find(hWnd);
        if (it != g_SnapshotCache.end()) {
            DeleteObject(it->second.hBmp);
            g_SnapshotCache.erase(it);
        }
        g_IconPositions.erase(hWnd);
    }

    if (Msg == WM_SYSCOMMAND) {
        UINT cmd = wParam & 0xFFF0;
        if (cmd == SC_MINIMIZE) {
            // Our own deferred minimize (auto-hide path) re-enters here via
            // SendMessageTimeoutW - let it pass straight through, no recursion.
            if (GetPropW(hWnd, L"GenieBypass")) {
                return DefWindowProcW_Original(hWnd, Msg, wParam, lParam);
            }
            if (MacGenieShouldAnimate(hWnd)) {
                // Auto-hide taskbar? Reveal it, defer the real minimize until the
                // animation finishes so it doesn't slide away mid-genie.
                // (Potassiumuncher's engine, reconciled with MINE's cloak hide.)
                BOOL requestedUnhide = FALSE;
                HWND hNext = NULL;
                if (g_unhideEnabled.load(std::memory_order_relaxed)) {
                    HWND hTray = FindTaskbarForMonitor(MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST));
                    if (hTray) {
                        APPBARDATA abd = { sizeof(APPBARDATA) };
                        abd.hWnd = hTray;
                        UINT uState = (UINT)AppBarGetState(&abd);
                        if (uState & ABS_AUTOHIDE) {
                            requestedUnhide = TRUE;
                            HWND hwndIter = GetWindow(hWnd, GW_HWNDNEXT);
                            while (hwndIter) {
                                if (IsWindowVisible(hwndIter) && !IsIconic(hwndIter) &&
                                    GetAncestor(hwndIter, GA_ROOT) == hwndIter &&
                                    GetWindowTextLengthW(hwndIter) > 0 &&
                                    hwndIter != hWnd && hwndIter != hTray) {
                                    DWORD exStyle = GetWindowLongPtrW(hwndIter, GWL_EXSTYLE);
                                    if (!(exStyle & WS_EX_TOOLWINDOW)) { hNext = hwndIter; break; }
                                }
                                hwndIter = GetWindow(hwndIter, GW_HWNDNEXT);
                            }
                            SetForegroundWindow(hTray);
                        }
                    }
                }

                MacGenieSetDwmTransitions(hWnd, FALSE);
                if (requestedUnhide) {
                    // Deferred path: animate, then (in the worker) cloak-hide is
                    // applied by us here after the first frame, and the worker does
                    // the real minimize + focus juggle at the end.
                    bool ok = StartMacGenieAnim(hWnd, FALSE, GetWindowLongPtrW(hWnd, GWL_EXSTYLE),
                                                FALSE, TRUE, TRUE, hNext);
                    if (ok) {
                        // First frame is up (StartMacGenieAnim waited): hide the real
                        // window with a flash-free cloak; worker minimizes it later.
                        MacGenieSetCloak(hWnd, TRUE);
                        return 0;
                    }
                    // Spawn failed: fall back to a normal immediate minimize.
                    return DefWindowProcW_Original(hWnd, Msg, wParam, lParam);
                }
                // Normal minimize (MINE): first-frame gate, then real minimize.
                StartMacGenieAnim(hWnd, FALSE, GetWindowLongPtrW(hWnd, GWL_EXSTYLE));
            }
            return DefWindowProcW_Original(hWnd, Msg, wParam, lParam);
        }
        else if (cmd == SC_RESTORE && IsIconic(hWnd) &&
                 g_openAnimation.load(std::memory_order_relaxed) &&
                 MacGenieShouldAnimate(hWnd)) {
            MacGenieSetDwmTransitions(hWnd, FALSE);
            MacGenieSetCloak(hWnd, TRUE);
            LRESULT res = DefWindowProcW_Original(hWnd, Msg, wParam, lParam);
            StartMacGenieAnim(hWnd, TRUE, GetWindowLongPtrW(hWnd, GWL_EXSTYLE), TRUE);
            return res;
        }
    }
    return DefWindowProcW_Original(hWnd, Msg, wParam, lParam);
}

// Custom-title-bar apps (Zed/GPUI, some Electron) minimize via ShowWindowAsync,
// SetWindowPlacement, or CloseWindow. Shared minimize kick-off for those paths (no
// unhide handling - that lives on the SC_MINIMIZE path only).
static void MacGenieTryMinimizeAnim(HWND hWnd) {
    if (!IsWindowVisible(hWnd) || IsIconic(hWnd)) return;
    if (!MacGenieShouldAnimate(hWnd)) return;
    MacGenieSetDwmTransitions(hWnd, FALSE);
    StartMacGenieAnim(hWnd, FALSE, GetWindowLongPtrW(hWnd, GWL_EXSTYLE));
}

BOOL WINAPI ShowWindowAsync_Hook(HWND hWnd, int nCmdShow) {
    if (nCmdShow == SW_MINIMIZE || nCmdShow == SW_SHOWMINIMIZED ||
        nCmdShow == SW_SHOWMINNOACTIVE) {
        MacGenieTryMinimizeAnim(hWnd);
    }
    return ShowWindowAsync_Original(hWnd, nCmdShow);
}

BOOL WINAPI SetWindowPlacement_Hook(HWND hWnd, const WINDOWPLACEMENT* lpwndpl) {
    if (lpwndpl && (lpwndpl->showCmd == SW_MINIMIZE ||
                    lpwndpl->showCmd == SW_SHOWMINIMIZED ||
                    lpwndpl->showCmd == SW_SHOWMINNOACTIVE)) {
        MacGenieTryMinimizeAnim(hWnd);
    }
    return SetWindowPlacement_Original(hWnd, lpwndpl);
}

BOOL WINAPI CloseWindow_Hook(HWND hWnd) {
    MacGenieTryMinimizeAnim(hWnd);
    return CloseWindow_Original(hWnd);
}

void Init() {
    LoadSettings();

    Wh_SetFunctionHook((void*)DefWindowProcW, (void*)DefWindowProcW_Hook, (void**)&DefWindowProcW_Original);
    Wh_SetFunctionHook((void*)ShowWindowAsync, (void*)ShowWindowAsync_Hook, (void**)&ShowWindowAsync_Original);
    Wh_SetFunctionHook((void*)SetWindowPlacement, (void*)SetWindowPlacement_Hook, (void**)&SetWindowPlacement_Original);
    Wh_SetFunctionHook((void*)CloseWindow, (void*)CloseWindow_Hook, (void**)&CloseWindow_Original);
}

// Windhawk unmaps the DLL right after uninit, so worker threads still running
// mod code would crash their host. Signal them to abort and wait for them to
// drain; the cap only guards a hung DWM.
void BeforeUninit() {
    g_unloading.store(true, std::memory_order_relaxed);
    for (int i = 0; i < 300 && g_workerCount.load(std::memory_order_acquire) > 0; ++i) {
        Sleep(10);
    }
}

void Uninit() {
    std::lock_guard<std::mutex> lock(g_CacheMutex);
    for (auto& pair : g_SnapshotCache) {
        DeleteObject(pair.second.hBmp);
    }
    g_SnapshotCache.clear();
    g_IconPositions.clear();
    g_ProcessIconPositions.clear();
}

}  // namespace genie

////////////////////////////////////////////////////////////////////////////////
// Auto-hide when maximized, from Taskbar auto-hide when maximized by m417z.

namespace autohide {

enum class Mode {
    intersected,
    maximized,
};

struct {
    Mode mode;
} g_settings;

std::atomic<bool> g_taskbarViewDllLoaded;
std::atomic<bool> g_initialized;

bool g_wasAutoHideProcessed;
bool g_wasAutoHideDisabled;
std::mutex g_winEventHookThreadMutex;
std::atomic<HANDLE> g_winEventHookThread;
std::unordered_map<void*, HWND> g_taskbarsKeptShown;
std::unordered_map<HWND, void*> g_taskbarToViewCoordinator;
UINT_PTR g_pendingEventsTimer;
std::atomic<HWND> g_multitaskingViewHwnd;
std::atomic<HWND> g_altTabViewHwnd;

// TrayUI::_HandleTrayPrivateSettingMessage
constexpr UINT kHandleTrayPrivateSettingMessage = WM_USER + 0x1CA;

enum {
    kTrayPrivateSettingAutoHideGet = 3,
    kTrayPrivateSettingAutoHideSet = 4,
};

constexpr WCHAR kUpdateTaskbarStatePendingTickCount[] =
    L"Windhawk_UpdateTaskbarStatePendingTickCount_" WH_MOD_ID;

static const UINT g_getTaskbarRectRegisteredMsg =
    RegisterWindowMessage(L"Windhawk_GetTaskbarRect_" WH_MOD_ID);

static const UINT g_updateTaskbarStateRegisteredMsg =
    RegisterWindowMessage(L"Windhawk_UpdateTaskbarState_" WH_MOD_ID);

enum {
    kTrayUITimerHide = 2,
    kTrayUITimerUnhide = 3,
};

#if __cplusplus < 202302L
// Missing in older MinGW headers.
DECLARE_HANDLE(CO_MTA_USAGE_COOKIE);
WINOLEAPI CoIncrementMTAUsage(CO_MTA_USAGE_COOKIE* pCookie);
WINOLEAPI CoDecrementMTAUsage(CO_MTA_USAGE_COOKIE Cookie);
#endif

// Missing in older MinGW headers.
#ifndef EVENT_OBJECT_CLOAKED
#define EVENT_OBJECT_CLOAKED 0x8017
#endif
#ifndef EVENT_OBJECT_UNCLOAKED
#define EVENT_OBJECT_UNCLOAKED 0x8018
#endif

using IsWindowArranged_t = BOOL(WINAPI*)(HWND hwnd);
IsWindowArranged_t pIsWindowArranged;

// Private API for window band (z-order band).
// https://blog.adeltax.com/window-z-order-in-windows-10/
using GetWindowBand_t = BOOL(WINAPI*)(HWND hWnd, PDWORD pdwBand);
GetWindowBand_t pGetWindowBand;


bool IsWindowCloaked(HWND hwnd) {
    BOOL isCloaked = FALSE;
    return SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &isCloaked,
                                           sizeof(isCloaked))) &&
           isCloaked;
}

enum class MultitaskingViewType {
    None,
    WinTab,
    AltTab,
};

// Detects Alt+Tab or Win+Tab and returns the type.
MultitaskingViewType GetMultitaskingViewType(HWND hWnd) {
    // Must be in the current process (explorer.exe).
    DWORD dwProcessId = 0;
    DWORD dwThreadId = GetWindowThreadProcessId(hWnd, &dwProcessId);
    if (!dwThreadId || dwProcessId != GetCurrentProcessId()) {
        return MultitaskingViewType::None;
    }

    WCHAR className[64];
    if (!GetClassName(hWnd, className, ARRAYSIZE(className)) ||
        _wcsicmp(className, L"XamlExplorerHostIslandWindow") != 0) {
        return MultitaskingViewType::None;
    }

    // The Win+Tab window uses band ZBID_IMMERSIVE_APPCHROME.
    constexpr DWORD ZBID_IMMERSIVE_APPCHROME = 5;

    // The Alt+Tab window uses band ZBID_SYSTEM_TOOLS. The virtual desktop
    // switcher (which we don't want) uses band ZBID_IMMERSIVE_EDGY.
    constexpr DWORD ZBID_SYSTEM_TOOLS = 16;

    DWORD band = 0;
    if (!pGetWindowBand || !pGetWindowBand(hWnd, &band)) {
        return MultitaskingViewType::None;
    }

    MultitaskingViewType type;
    if (band == ZBID_IMMERSIVE_APPCHROME) {
        type = MultitaskingViewType::WinTab;
    } else if (band == ZBID_SYSTEM_TOOLS) {
        type = MultitaskingViewType::AltTab;
    } else {
        return MultitaskingViewType::None;
    }

    // Check thread description for "MultitaskingView".
    HANDLE hThread =
        OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, dwThreadId);
    if (!hThread) {
        return MultitaskingViewType::None;
    }

    bool isMultitaskingView = false;
    PWSTR description = nullptr;
    if (SUCCEEDED(GetThreadDescription(hThread, &description)) && description) {
        isMultitaskingView = wcscmp(description, L"MultitaskingView") == 0;
        LocalFree(description);
    }

    CloseHandle(hThread);
    return isMultitaskingView ? type : MultitaskingViewType::None;
}

HWND FindCurrentProcessTaskbarWnd() {
    HWND hTaskbarWnd = nullptr;

    EnumWindows(
        [](HWND hWnd, LPARAM lParam) -> BOOL {
            DWORD dwProcessId;
            WCHAR className[32];
            if (GetWindowThreadProcessId(hWnd, &dwProcessId) &&
                dwProcessId == GetCurrentProcessId() &&
                GetClassName(hWnd, className, ARRAYSIZE(className)) &&
                _wcsicmp(className, L"Shell_TrayWnd") == 0) {
                *reinterpret_cast<HWND*>(lParam) = hWnd;
                return FALSE;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&hTaskbarWnd));

    return hTaskbarWnd;
}

bool IsTaskbarWindow(HWND hWnd) {
    WCHAR szClassName[32];
    if (!GetClassName(hWnd, szClassName, ARRAYSIZE(szClassName))) {
        return false;
    }

    return _wcsicmp(szClassName, L"Shell_TrayWnd") == 0 ||
           _wcsicmp(szClassName, L"Shell_SecondaryTrayWnd") == 0;
}

HWND FindTaskbarWindows(std::unordered_set<HWND>* secondaryTaskbarWindows) {
    secondaryTaskbarWindows->clear();

    HWND hTaskbarWnd = FindCurrentProcessTaskbarWnd();
    if (!hTaskbarWnd) {
        return nullptr;
    }

    DWORD taskbarThreadId = GetWindowThreadProcessId(hTaskbarWnd, nullptr);
    if (!taskbarThreadId) {
        return nullptr;
    }

    auto enumWindowsProc = [&secondaryTaskbarWindows](HWND hWnd) -> BOOL {
        WCHAR szClassName[32];
        if (GetClassName(hWnd, szClassName, ARRAYSIZE(szClassName)) == 0) {
            return TRUE;
        }

        if (_wcsicmp(szClassName, L"Shell_SecondaryTrayWnd") == 0) {
            secondaryTaskbarWindows->insert(hWnd);
        }

        return TRUE;
    };

    EnumThreadWindows(
        taskbarThreadId,
        [](HWND hWnd, LPARAM lParam) -> BOOL {
            auto& proc = *reinterpret_cast<decltype(enumWindowsProc)*>(lParam);
            return proc(hWnd);
        },
        reinterpret_cast<LPARAM>(&enumWindowsProc));

    return hTaskbarWnd;
}

bool GetTaskbarRectForMonitor(HMONITOR monitor, RECT* rect) {
    SetRectEmpty(rect);

    HWND hTaskbarWnd = FindCurrentProcessTaskbarWnd();
    if (!hTaskbarWnd) {
        return false;
    }

    SendMessage(hTaskbarWnd, g_getTaskbarRectRegisteredMsg, (WPARAM)monitor,
                (LPARAM)rect);
    return true;
}


bool CanHideTaskbarForWindow(HWND hWnd,
                             HMONITOR monitor,
                             const MONITORINFO* monitorInfo,
                             const RECT* taskbarRect) {
    if (!IsWindowVisible(hWnd) || IsWindowCloaked(hWnd) || IsIconic(hWnd) ||
        (GetWindowLong(hWnd, GWL_EXSTYLE) & WS_EX_NOACTIVATE)) {
        return false;
    }

    // Ignore the Alt+Tab overlay so it doesn't affect taskbar state.
    if (hWnd == g_altTabViewHwnd) {
        return false;
    }

    if (hWnd == GetShellWindow() || GetProp(hWnd, L"DesktopWindow")) {
        return false;
    }

    // Exclude menus (#32768).
    if (GetClassWord(hWnd, GCW_ATOM) == 32768) {
        return false;
    }

    WINDOWPLACEMENT wp{
        .length = sizeof(WINDOWPLACEMENT),
    };

    if (GetWindowPlacement(hWnd, &wp) && wp.showCmd == SW_SHOWMAXIMIZED) {
        if (MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST) == monitor) {
            return true;
        }

        return false;
    }

    bool isWindowArranged = pIsWindowArranged && pIsWindowArranged(hWnd);
    if (isWindowArranged &&
        MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST) != monitor) {
        return false;
    }

    RECT windowRect{};
    DwmGetWindowAttribute(hWnd, DWMWA_EXTENDED_FRAME_BOUNDS, &windowRect,
                          sizeof(windowRect));

    // Don't keep the taskbar shown for a fullscreen window.
    if (EqualRect(&windowRect, &monitorInfo->rcMonitor)) {
        return true;
    }

    // It makes sense to treat arranged windows (e.g. with Win+left) as
    // maximized, as they occupy the whole monitor height. Still check for
    // intersection, as a window can also just occupy the upper side of the
    // screen (e.g. Win+left, Win+up).
    if (g_settings.mode == Mode::intersected ||
        (g_settings.mode == Mode::maximized && isWindowArranged)) {
        RECT intersectRect;
        if (IntersectRect(&intersectRect, &windowRect, taskbarRect)) {
            return true;
        }
    }

    return false;
}

bool ShouldKeepTaskbarShown(HWND hTaskbarWnd, HMONITOR monitor) {
    // Always show taskbar when MultitaskingView (Win+Tab) is active.
    if (g_multitaskingViewHwnd) {
        return true;
    }

    MONITORINFO monitorInfo{
        .cbSize = sizeof(MONITORINFO),
    };
    GetMonitorInfo(monitor, &monitorInfo);

    RECT taskbarRect{};
    GetTaskbarRectForMonitor(monitor, &taskbarRect);

    bool canHideTaskbar = false;

    DWORD dwTaskbarThreadId = GetCurrentThreadId();

    auto enumWindowsProc = [&](HWND hWnd) -> BOOL {
        if (GetWindowThreadProcessId(hWnd, nullptr) == dwTaskbarThreadId) {
            return TRUE;
        }

        canHideTaskbar =
            CanHideTaskbarForWindow(hWnd, monitor, &monitorInfo, &taskbarRect);
        if (!canHideTaskbar) {
            return TRUE;
        }

        return FALSE;
    };

    EnumWindows(
        [](HWND hWnd, LPARAM lParam) -> BOOL {
            auto& proc = *reinterpret_cast<decltype(enumWindowsProc)*>(lParam);
            return proc(hWnd);
        },
        reinterpret_cast<LPARAM>(&enumWindowsProc));

    return !canHideTaskbar;
}

void* QueryViaVtable(void* object, void* vtable) {
    void* ptr = object;
    while (*(void**)ptr != vtable) {
        ptr = (void**)ptr + 1;
    }
    return ptr;
}

void* QueryViaVtableBackwards(void* object, void* vtable) {
    void* ptr = object;
    while (*(void**)ptr != vtable) {
        ptr = (void**)ptr - 1;
    }
    return ptr;
}

DWORD WINAPI WinEventHookThread(LPVOID lpThreadParameter);

void AdjustTaskbar(HWND hMMTaskbarWnd, bool clearPendingWhenDone = false) {
    if (!g_winEventHookThread) {
        std::lock_guard<std::mutex> guard(g_winEventHookThreadMutex);

        if (!g_winEventHookThread) {
            g_winEventHookThread = CreateThread(nullptr, 0, WinEventHookThread,
                                                nullptr, 0, nullptr);
        }
    }

    PostMessage(hMMTaskbarWnd, g_updateTaskbarStateRegisteredMsg,
                clearPendingWhenDone, 0);
}

void AdjustAllTaskbars() {
    std::unordered_set<HWND> secondaryTaskbarWindows;
    HWND hWnd = FindTaskbarWindows(&secondaryTaskbarWindows);
    if (hWnd) {
        AdjustTaskbar(hWnd);
    }

    for (HWND hSecondaryWnd : secondaryTaskbarWindows) {
        AdjustTaskbar(hSecondaryWnd);
    }
}

bool AdjustAllTaskbarsIfNotPending() {
    std::unordered_set<HWND> secondaryTaskbarWindows;
    HWND hWnd = FindTaskbarWindows(&secondaryTaskbarWindows);

    DWORD currentTickCount = GetTickCount();
    DWORD pendingTickCount = 0;

    if (hWnd) {
        DWORD tickCount = (DWORD)(DWORD_PTR)GetProp(
            hWnd, kUpdateTaskbarStatePendingTickCount);
        if (tickCount > pendingTickCount) {
            pendingTickCount = tickCount;
        }
    }

    for (HWND hSecondaryWnd : secondaryTaskbarWindows) {
        DWORD tickCount = (DWORD)(DWORD_PTR)GetProp(
            hSecondaryWnd, kUpdateTaskbarStatePendingTickCount);
        if (tickCount > pendingTickCount) {
            pendingTickCount = tickCount;
        }
    }

    // Consider times larger than 10 seconds as expired, to prevent having
    // it stuck in this state.
    if (pendingTickCount && currentTickCount - pendingTickCount < 1000 * 10) {
        return false;
    }

    if (hWnd) {
        SetProp(hWnd, kUpdateTaskbarStatePendingTickCount,
                (HANDLE)(DWORD_PTR)currentTickCount);
        AdjustTaskbar(hWnd, /*clearPendingWhenDone=*/true);
    }

    for (HWND hSecondaryWnd : secondaryTaskbarWindows) {
        SetProp(hSecondaryWnd, kUpdateTaskbarStatePendingTickCount,
                (HANDLE)(DWORD_PTR)currentTickCount);
        AdjustTaskbar(hSecondaryWnd, /*clearPendingWhenDone=*/true);
    }

    return true;
}

using ViewCoordinator_ShouldTaskbarBeExpanded_t =
    bool(WINAPI*)(void* pThis, HWND hMMTaskbarWnd, bool expanded);
ViewCoordinator_ShouldTaskbarBeExpanded_t
    ViewCoordinator_ShouldTaskbarBeExpanded_Original;
bool WINAPI ViewCoordinator_ShouldTaskbarBeExpanded_Hook(void* pThis,
                                                         HWND hMMTaskbarWnd,
                                                         bool expanded) {
    Wh_Log(L"> hMMTaskbarWnd=%08X, expanded=%d",
           (DWORD)(ULONG_PTR)hMMTaskbarWnd, expanded);

    g_taskbarToViewCoordinator[hMMTaskbarWnd] = pThis;

    // Return true if the taskbar should be kept shown.
    for (const auto& pair : g_taskbarsKeptShown) {
        if (pair.second == hMMTaskbarWnd) {
            Wh_Log(L"Returning true for taskbar kept shown");
            return true;
        }
    }

    return ViewCoordinator_ShouldTaskbarBeExpanded_Original(
        pThis, hMMTaskbarWnd, expanded);
}

using ViewCoordinator_UpdateIsExpanded_t = void(WINAPI*)(void* pThis,
                                                         HWND hMMTaskbarWnd,
                                                         int reason);
ViewCoordinator_UpdateIsExpanded_t ViewCoordinator_UpdateIsExpanded_Original;

void UpdateViewCoordinatorIsExpanded(HWND hWnd) {
    // From ViewCoordinator::HandleIsPointerOverTaskbarFrameChanged.
    constexpr int kReasonIsPointerOverTaskbarFrameChanged = 7;

    if (ViewCoordinator_UpdateIsExpanded_Original) {
        auto it = g_taskbarToViewCoordinator.find(hWnd);
        if (it != g_taskbarToViewCoordinator.end()) {
            ViewCoordinator_UpdateIsExpanded_Original(
                it->second, hWnd, kReasonIsPointerOverTaskbarFrameChanged);
        }
    }
}

void* TrayUI_vftable_IInspectable;
void* TrayUI_vftable_ITrayComponentHost;
void* CSecondaryTray_vftable_ISecondaryTray;

using TrayUI_GetStuckMonitor_t = HMONITOR(WINAPI*)(void* pThis);
TrayUI_GetStuckMonitor_t TrayUI_GetStuckMonitor_Original;

using CSecondaryTray_GetMonitor_t = HMONITOR(WINAPI*)(void* pThis);
CSecondaryTray_GetMonitor_t CSecondaryTray_GetMonitor_Original;

using TrayUI_GetStuckRectForMonitor_t = bool(WINAPI*)(void* pThis,
                                                      HMONITOR hMonitor,
                                                      RECT* rect);
TrayUI_GetStuckRectForMonitor_t TrayUI_GetStuckRectForMonitor_Original;

using TrayUI__Hide_t = void(WINAPI*)(void* pThis);
TrayUI__Hide_t TrayUI__Hide_Original;
void WINAPI TrayUI__Hide_Hook(void* pThis) {
    Wh_Log(L">");

    auto it = g_taskbarsKeptShown.find(pThis);
    if (it != g_taskbarsKeptShown.end()) {
        KillTimer(it->second, kTrayUITimerHide);
        return;
    }

    TrayUI__Hide_Original(pThis);
}

using CSecondaryTray__AutoHide_t = void(WINAPI*)(void* pThis, bool param1);
CSecondaryTray__AutoHide_t CSecondaryTray__AutoHide_Original;
void WINAPI CSecondaryTray__AutoHide_Hook(void* pThis, bool param1) {
    Wh_Log(L">");

    auto it = g_taskbarsKeptShown.find(pThis);
    if (it != g_taskbarsKeptShown.end()) {
        KillTimer(it->second, kTrayUITimerHide);
        return;
    }

    CSecondaryTray__AutoHide_Original(pThis, param1);
}

using TrayUI_Unhide_t = void(WINAPI*)(void* pThis,
                                      int trayUnhideFlags,
                                      int unhideRequest);
TrayUI_Unhide_t TrayUI_Unhide_Original;

using CSecondaryTray__Unhide_t = void(WINAPI*)(void* pThis,
                                               int trayUnhideFlags,
                                               int unhideRequest);
CSecondaryTray__Unhide_t CSecondaryTray__Unhide_Original;

using TrayUI_WndProc_t = LRESULT(WINAPI*)(void* pThis,
                                          HWND hWnd,
                                          UINT Msg,
                                          WPARAM wParam,
                                          LPARAM lParam,
                                          bool* flag);
TrayUI_WndProc_t TrayUI_WndProc_Original;
LRESULT WINAPI TrayUI_WndProc_Hook(void* pThis,
                                   HWND hWnd,
                                   UINT Msg,
                                   WPARAM wParam,
                                   LPARAM lParam,
                                   bool* flag) {
    if (Msg == WM_NCCREATE) {
        Wh_Log(L"WM_NCCREATE: %08X", (DWORD)(ULONG_PTR)hWnd);
        AdjustTaskbar(hWnd);
    } else if (Msg == WM_NCDESTROY) {
        Wh_Log(L"WM_NCDESTROY: %08X", (DWORD)(ULONG_PTR)hWnd);
        g_taskbarsKeptShown.erase(
            QueryViaVtableBackwards(pThis, TrayUI_vftable_IInspectable));
        g_taskbarToViewCoordinator.erase(hWnd);
    } else if (Msg == kHandleTrayPrivateSettingMessage) {
        // Prevent auto-hide from being disabled while the mod is loaded.
        if ((DWORD)wParam == 4) {
            BOOL bSetAutoHideEnabled = (BOOL)lParam;
            if (!bSetAutoHideEnabled) {
                return 0;
            }
        }
    } else if (Msg == g_getTaskbarRectRegisteredMsg) {
        HMONITOR monitor = (HMONITOR)wParam;
        RECT* rect = (RECT*)lParam;
        if (TrayUI_GetStuckRectForMonitor_Original) {
            if (!TrayUI_GetStuckRectForMonitor_Original(pThis, monitor, rect)) {
                SetRectEmpty(rect);
            }
        } else {
            SetRectEmpty(rect);
        }
    } else if (Msg == g_updateTaskbarStateRegisteredMsg) {
        if (!g_wasAutoHideProcessed) {
            g_wasAutoHideProcessed = true;
            g_wasAutoHideDisabled =
                !SendMessage(hWnd, kHandleTrayPrivateSettingMessage,
                             kTrayPrivateSettingAutoHideGet, 0);
            if (g_wasAutoHideDisabled) {
                SendMessage(hWnd, kHandleTrayPrivateSettingMessage,
                            kTrayPrivateSettingAutoHideSet, TRUE);
            }
        }

        HMONITOR monitor = TrayUI_GetStuckMonitor_Original(pThis);
        bool keepShown = ShouldKeepTaskbarShown(hWnd, monitor);

        void* pTrayUI_IInspectable =
            QueryViaVtableBackwards(pThis, TrayUI_vftable_IInspectable);

        bool keptShown = g_taskbarsKeptShown.contains(pTrayUI_IInspectable);

        if (keepShown != keptShown) {
            Wh_Log(L"> keepShown=%d", keepShown);

            if (keepShown) {
                g_taskbarsKeptShown[pTrayUI_IInspectable] = hWnd;

                void* pTrayUI_ITrayComponentHost =
                    QueryViaVtable(pThis, TrayUI_vftable_ITrayComponentHost);
                TrayUI_Unhide_Original(pTrayUI_ITrayComponentHost, 0, 0);

                UpdateViewCoordinatorIsExpanded(hWnd);
            } else {
                g_taskbarsKeptShown.erase(pTrayUI_IInspectable);

                SetTimer(hWnd, kTrayUITimerHide, 0, nullptr);

                UpdateViewCoordinatorIsExpanded(hWnd);
            }
        }

        if (wParam) {
            RemoveProp(hWnd, kUpdateTaskbarStatePendingTickCount);
        }
    }

    LRESULT ret =
        TrayUI_WndProc_Original(pThis, hWnd, Msg, wParam, lParam, flag);

    return ret;
}

using CSecondaryTray_v_WndProc_t = LRESULT(
    WINAPI*)(void* pThis, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
CSecondaryTray_v_WndProc_t CSecondaryTray_v_WndProc_Original;
LRESULT WINAPI CSecondaryTray_v_WndProc_Hook(void* pThis,
                                             HWND hWnd,
                                             UINT Msg,
                                             WPARAM wParam,
                                             LPARAM lParam) {
    if (Msg == WM_NCCREATE) {
        Wh_Log(L"WM_NCCREATE: %08X", (DWORD)(ULONG_PTR)hWnd);
        AdjustTaskbar(hWnd);
    } else if (Msg == WM_NCDESTROY) {
        Wh_Log(L"WM_NCDESTROY: %08X", (DWORD)(ULONG_PTR)hWnd);
        g_taskbarsKeptShown.erase(pThis);
        g_taskbarToViewCoordinator.erase(hWnd);
    } else if (Msg == g_updateTaskbarStateRegisteredMsg) {
        void* pCSecondaryTray_ISecondaryTray =
            QueryViaVtable(pThis, CSecondaryTray_vftable_ISecondaryTray);

        HMONITOR monitor =
            CSecondaryTray_GetMonitor_Original(pCSecondaryTray_ISecondaryTray);

        bool keepShown = ShouldKeepTaskbarShown(hWnd, monitor);

        bool keptShown = g_taskbarsKeptShown.contains(pThis);

        if (keepShown != keptShown) {
            Wh_Log(L"> keepShown=%d", keepShown);

            if (keepShown) {
                g_taskbarsKeptShown[pThis] = hWnd;

                CSecondaryTray__Unhide_Original(pThis, 0, 0);

                UpdateViewCoordinatorIsExpanded(hWnd);
            } else {
                g_taskbarsKeptShown.erase(pThis);

                SetTimer(hWnd, kTrayUITimerHide, 0, nullptr);

                UpdateViewCoordinatorIsExpanded(hWnd);
            }
        }

        if (wParam) {
            RemoveProp(hWnd, kUpdateTaskbarStatePendingTickCount);
        }
    }

    LRESULT ret =
        CSecondaryTray_v_WndProc_Original(pThis, hWnd, Msg, wParam, lParam);

    return ret;
}

void CALLBACK WinEventProc(HWINEVENTHOOK hWinEventHook,
                           DWORD event,
                           HWND hWnd,
                           LONG idObject,
                           LONG idChild,
                           DWORD dwEventThread,
                           DWORD dwmsEventTime) {
    if (idObject != OBJID_WINDOW ||
        (GetWindowLong(hWnd, GWL_STYLE) & WS_CHILD) || IsTaskbarWindow(hWnd)) {
        return;
    }

    HWND hParentWnd = GetAncestor(hWnd, GA_PARENT);
    if (hParentWnd && hParentWnd != GetDesktopWindow()) {
        return;
    }

    // Check for Multitasking View window state changes.
    {
        auto multitaskingViewType = GetMultitaskingViewType(hWnd);
        bool isTrackedWinTab = hWnd == g_multitaskingViewHwnd;
        bool isTrackedAltTab = hWnd == g_altTabViewHwnd;

        if (multitaskingViewType != MultitaskingViewType::None ||
            isTrackedWinTab || isTrackedAltTab) {
            auto& targetHwnd =
                (multitaskingViewType == MultitaskingViewType::AltTab ||
                 isTrackedAltTab)
                    ? g_altTabViewHwnd
                    : g_multitaskingViewHwnd;

            bool entering = event == EVENT_OBJECT_SHOW ||
                            event == EVENT_OBJECT_UNCLOAKED ||
                            event == EVENT_OBJECT_CREATE;
            bool leaving = event == EVENT_OBJECT_HIDE ||
                           event == EVENT_OBJECT_CLOAKED ||
                           event == EVENT_OBJECT_DESTROY;

            if (entering) {
                HWND expected = nullptr;
                if (!targetHwnd.compare_exchange_strong(expected, hWnd)) {
                    return;
                }
                Wh_Log(
                    L"MultitaskingView entering (%s)",
                    &targetHwnd == &g_altTabViewHwnd ? L"Alt+Tab" : L"Win+Tab");
            } else if (leaving) {
                HWND expected = hWnd;
                if (!targetHwnd.compare_exchange_strong(expected, nullptr)) {
                    return;
                }
                Wh_Log(
                    L"MultitaskingView leaving (%s)",
                    &targetHwnd == &g_altTabViewHwnd ? L"Alt+Tab" : L"Win+Tab");
            } else {
                return;
            }

            // Fall through to trigger timer for all taskbars.
        }
    }

    if (g_pendingEventsTimer) {
        return;
    }

    g_pendingEventsTimer = SetTimer(
        nullptr, 0, 200,
        [](HWND hwnd,         // handle of window for timer messages
           UINT uMsg,         // WM_TIMER message
           UINT_PTR idEvent,  // timer identifier
           DWORD dwTime       // current system time
        ) {
            Wh_Log(L">");

            if (!AdjustAllTaskbarsIfNotPending()) {
                Wh_Log(L"Adjustment already pending, will retry later...");
                return;
            }

            KillTimer(nullptr, g_pendingEventsTimer);
            g_pendingEventsTimer = 0;
        });
}

DWORD WINAPI WinEventHookThread(LPVOID lpThreadParameter) {
    HWINEVENTHOOK winObjectEventHook1 =
        SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_HIDE, nullptr,
                        WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
    if (!winObjectEventHook1) {
        Wh_Log(L"Error: SetWinEventHook");
    }

    HWINEVENTHOOK winObjectEventHook2 = SetWinEventHook(
        EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, nullptr,
        WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
    if (!winObjectEventHook2) {
        Wh_Log(L"Error: SetWinEventHook");
    }

    HWINEVENTHOOK winObjectEventHook3 =
        SetWinEventHook(EVENT_OBJECT_CLOAKED, EVENT_OBJECT_UNCLOAKED, nullptr,
                        WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
    if (!winObjectEventHook3) {
        Wh_Log(L"Error: SetWinEventHook");
    }

    BOOL bRet;
    MSG msg;
    while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0) {
        if (bRet == -1) {
            msg.wParam = 0;
            break;
        }

        if (msg.hwnd == NULL && msg.message == WM_APP) {
            PostQuitMessage(0);
            continue;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (winObjectEventHook1) {
        UnhookWinEvent(winObjectEventHook1);
    }

    if (winObjectEventHook2) {
        UnhookWinEvent(winObjectEventHook2);
    }

    if (winObjectEventHook3) {
        UnhookWinEvent(winObjectEventHook3);
    }

    g_multitaskingViewHwnd = nullptr;
    g_altTabViewHwnd = nullptr;

    return 0;
}

bool HookTaskbarViewDllSymbols(HMODULE module) {
    // Taskbar.View.dll
    WindhawkUtils::SYMBOL_HOOK symbolHooks[] = {
        {
            {LR"(public: bool __cdecl winrt::Taskbar::implementation::ViewCoordinator::ShouldTaskbarBeExpanded(unsigned __int64,bool))"},
            &ViewCoordinator_ShouldTaskbarBeExpanded_Original,
            ViewCoordinator_ShouldTaskbarBeExpanded_Hook,
            true,
        },
        {
            {LR"(public: void __cdecl winrt::Taskbar::implementation::ViewCoordinator::UpdateIsExpanded(unsigned __int64,enum TaskbarTipTest::TaskbarExpandCollapseReason))"},
            &ViewCoordinator_UpdateIsExpanded_Original,
            nullptr,
            true,
        },
    };

    if (!HookSymbols(module, symbolHooks, ARRAYSIZE(symbolHooks))) {
        Wh_Log(L"HookSymbols failed");
        return false;
    }

    return true;
}

HMODULE GetTaskbarViewModuleHandle() {
    HMODULE module = GetModuleHandle(L"Taskbar.View.dll");
    if (!module) {
        module = GetModuleHandle(L"ExplorerExtensions.dll");
    }

    return module;
}

void HandleLoadedModuleIfTaskbarView(HMODULE module, LPCWSTR lpLibFileName) {
    if (!g_taskbarViewDllLoaded &&
        GetTaskbarViewModuleHandle() == module &&
        !g_taskbarViewDllLoaded.exchange(true)) {
        Wh_Log(L"Loaded %s", lpLibFileName);

        if (HookTaskbarViewDllSymbols(module)) {
            Wh_ApplyHookOperations();
        }
    }
}

using LoadLibraryExW_t = decltype(&LoadLibraryExW);
LoadLibraryExW_t LoadLibraryExW_Original;
HMODULE WINAPI LoadLibraryExW_Hook(LPCWSTR lpLibFileName,
                                   HANDLE hFile,
                                   DWORD dwFlags) {
    HMODULE module = LoadLibraryExW_Original(lpLibFileName, hFile, dwFlags);
    if (module) {
        HandleLoadedModuleIfTaskbarView(module, lpLibFileName);
    }

    return module;
}

bool HookTaskbarSymbols() {
    HMODULE module =
        LoadLibraryEx(L"taskbar.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module) {
        Wh_Log(L"Couldn't load taskbar.dll");
        return false;
    }

    // Taskbar.dll, explorer.exe
    WindhawkUtils::SYMBOL_HOOK symbolHooks[] = {
        {
            {LR"(const TrayUI::`vftable'{for `IInspectable'})"},
            &TrayUI_vftable_IInspectable,
        },
        {
            {LR"(const TrayUI::`vftable'{for `ITrayComponentHost'})"},
            &TrayUI_vftable_ITrayComponentHost,
        },
        {
            {LR"(const CSecondaryTray::`vftable'{for `ISecondaryTray'})"},
            &CSecondaryTray_vftable_ISecondaryTray,
        },
        {
            {LR"(public: virtual struct HMONITOR__ * __cdecl TrayUI::GetStuckMonitor(void))"},
            &TrayUI_GetStuckMonitor_Original,
        },
        {
            {LR"(public: virtual struct HMONITOR__ * __cdecl CSecondaryTray::GetMonitor(void))"},
            &CSecondaryTray_GetMonitor_Original,
        },
        {
            {LR"(public: virtual bool __cdecl TrayUI::GetStuckRectForMonitor(struct HMONITOR__ *,struct tagRECT *))"},
            &TrayUI_GetStuckRectForMonitor_Original,
        },
        {
            {LR"(public: void __cdecl TrayUI::_Hide(void))"},
            &TrayUI__Hide_Original,
            TrayUI__Hide_Hook,
        },
        {
            {LR"(private: void __cdecl CSecondaryTray::_AutoHide(bool))"},
            &CSecondaryTray__AutoHide_Original,
            CSecondaryTray__AutoHide_Hook,
        },
        {
            {LR"(public: virtual void __cdecl TrayUI::Unhide(enum TrayCommon::TrayUnhideFlags,enum TrayCommon::UnhideRequest))"},
            &TrayUI_Unhide_Original,
        },
        {
            {LR"(private: void __cdecl CSecondaryTray::_Unhide(enum TrayCommon::TrayUnhideFlags,enum TrayCommon::UnhideRequest))"},
            &CSecondaryTray__Unhide_Original,
        },
        {
            {LR"(public: virtual __int64 __cdecl TrayUI::WndProc(struct HWND__ *,unsigned int,unsigned __int64,__int64,bool *))"},
            &TrayUI_WndProc_Original,
            TrayUI_WndProc_Hook,
        },
        {
            {LR"(private: virtual __int64 __cdecl CSecondaryTray::v_WndProc(struct HWND__ *,unsigned int,unsigned __int64,__int64))"},
            &CSecondaryTray_v_WndProc_Original,
            CSecondaryTray_v_WndProc_Hook,
        },
    };

    return HookSymbols(module, symbolHooks, ARRAYSIZE(symbolHooks));
}

void LoadSettings() {
    PCWSTR mode = Wh_GetStringSetting(L"autoHideMode");
    g_settings.mode =
        wcscmp(mode, L"maximized") == 0 ? Mode::maximized : Mode::intersected;
    Wh_FreeStringSetting(mode);
}

bool Init() {
    LoadSettings();

    HMODULE hUser32Module =
        LoadLibraryEx(L"user32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (hUser32Module) {
        pIsWindowArranged = (IsWindowArranged_t)GetProcAddress(
            hUser32Module, "IsWindowArranged");
        pGetWindowBand =
            (GetWindowBand_t)GetProcAddress(hUser32Module, "GetWindowBand");
    }

    if (HMODULE taskbarViewModule = GetTaskbarViewModuleHandle()) {
        g_taskbarViewDllLoaded = true;
        if (!HookTaskbarViewDllSymbols(taskbarViewModule)) {
            return false;
        }
    }

    if (!HookTaskbarSymbols()) {
        return false;
    }

    HMODULE kernelBaseModule = GetModuleHandle(L"kernelbase.dll");
    auto pKernelBaseLoadLibraryExW = (decltype(&LoadLibraryExW))GetProcAddress(
        kernelBaseModule, "LoadLibraryExW");
    WindhawkUtils::SetFunctionHook(pKernelBaseLoadLibraryExW,
                                       LoadLibraryExW_Hook,
                                       &LoadLibraryExW_Original);

    g_initialized = true;
    return true;
}

void AfterInit() {
    if (!g_taskbarViewDllLoaded) {
        if (HMODULE taskbarViewModule = GetTaskbarViewModuleHandle()) {
            if (!g_taskbarViewDllLoaded.exchange(true)) {
                if (HookTaskbarViewDllSymbols(taskbarViewModule)) {
                    Wh_ApplyHookOperations();
                }
            }
        }
    }

    WNDCLASS wndclass;
    if (GetClassInfo(GetModuleHandle(nullptr), L"Shell_TrayWnd", &wndclass)) {
        AdjustAllTaskbars();
    }
}

void Uninit() {
    if (g_winEventHookThread) {
        PostThreadMessage(GetThreadId(g_winEventHookThread), WM_APP, 0, 0);
        WaitForSingleObject(g_winEventHookThread, INFINITE);
        CloseHandle(g_winEventHookThread);
        g_winEventHookThread = nullptr;
    }

    // Auto-hide was switched on by this feature, so switch it back off.
    if (g_wasAutoHideDisabled) {
        HWND hTaskbarWnd = FindCurrentProcessTaskbarWnd();
        if (hTaskbarWnd) {
            SendMessage(hTaskbarWnd, kHandleTrayPrivateSettingMessage,
                        kTrayPrivateSettingAutoHideSet, FALSE);
        }
    }
}

}  // namespace autohide

////////////////////////////////////////////////////////////////////////////////
// Alt+Tab delay, from Alt+Tab window delayer by L3r0y.

namespace alttab {

std::atomic<DWORD> g_threadIdForAltTabShowWindow;
HWND g_taskSwitcherHwnd;
UINT_PTR g_timerId;
int g_nCmdShow;
int g_delayMilliseconds;

void ClearState() {
    if (g_timerId != 0) {
        KillTimer(nullptr, g_timerId);
        g_timerId = 0;
    }
    g_taskSwitcherHwnd = 0;
}

using XamlAltTabViewHost_ViewLoaded_t = void(WINAPI*)(void* pThis);
XamlAltTabViewHost_ViewLoaded_t XamlAltTabViewHost_ViewLoaded_Original;
void WINAPI XamlAltTabViewHost_ViewLoaded_Hook(void* pThis) {
    Wh_Log(L">");
    ClearState();
    g_threadIdForAltTabShowWindow = GetCurrentThreadId();
    XamlAltTabViewHost_ViewLoaded_Original(pThis);
    g_threadIdForAltTabShowWindow = 0;
}

using XamlAltTabViewHost_DisplayAltTab_t = void(WINAPI*)(void* pThis);
XamlAltTabViewHost_DisplayAltTab_t XamlAltTabViewHost_DisplayAltTab_Original;
void WINAPI XamlAltTabViewHost_DisplayAltTab_Hook(void* pThis) {
    Wh_Log(L">");

    if (g_threadIdForAltTabShowWindow) {
        // Likely called from ViewLoaded.
        return XamlAltTabViewHost_DisplayAltTab_Original(pThis);
    }

    ClearState();
    g_threadIdForAltTabShowWindow = GetCurrentThreadId();
    XamlAltTabViewHost_DisplayAltTab_Original(pThis);
    g_threadIdForAltTabShowWindow = 0;
}



void CALLBACK TimerProc(HWND hWnd, UINT uMsg, UINT_PTR idTimer, DWORD dwTime) {
    KillTimer(nullptr, g_timerId);
    g_timerId = 0;
    ShowWindow_Original(g_taskSwitcherHwnd, g_nCmdShow);
}

// ShowWindow, dispatched to from the shared hook. Returns true when it took the
// call over; *result is then what ShowWindow returns.
bool OnShowWindow(HWND hWnd, int nCmdShow, BOOL* result) {
    if (hWnd == g_taskSwitcherHwnd && nCmdShow == SW_HIDE) {
        ClearState();
        *result = ShowWindow_Original(hWnd, nCmdShow);
        return true;
    }

    if (g_threadIdForAltTabShowWindow != GetCurrentThreadId() ||
        nCmdShow == SW_HIDE) {
        return false;
    }

    // Hold the switcher back. A quick Alt+Tab is over before the timer fires,
    // and the SW_HIDE above cancels it, so the switcher never flashes.
    g_taskSwitcherHwnd = hWnd;
    g_nCmdShow = nCmdShow;
    g_timerId = SetTimer(nullptr, g_timerId, g_delayMilliseconds, TimerProc);
    *result = TRUE;
    return true;
}

bool Init() {
    g_delayMilliseconds =
        std::clamp(Wh_GetIntSetting(L"altTabDelayMs"), 0, 2000);

    HMODULE twinuiPcshellModule = LoadLibrary(L"twinui.pcshell.dll");
    if (!twinuiPcshellModule) {
        Wh_Log(L"Couldn't load twinui.pcshell.dll");
        return false;
    }

    WindhawkUtils::SYMBOL_HOOK twinuiPcshellSymbolHooks[] = {
        {
            // ViewLoaded either calls DisplayAltTab or queues it to be called
            // later, so hooking only ViewLoaded isn't enough.
            {LR"(public: virtual long __cdecl XamlAltTabViewHost::ViewLoaded(void))"},
            &XamlAltTabViewHost_ViewLoaded_Original,
            XamlAltTabViewHost_ViewLoaded_Hook,
        },
        {
            {LR"(private: void __cdecl XamlAltTabViewHost::DisplayAltTab(void))"},
            &XamlAltTabViewHost_DisplayAltTab_Original,
            XamlAltTabViewHost_DisplayAltTab_Hook,
        },
    };

    return HookSymbols(twinuiPcshellModule, twinuiPcshellSymbolHooks,
                       ARRAYSIZE(twinuiPcshellSymbolHooks));
}

}  // namespace alttab

////////////////////////////////////////////////////////////////////////////////
// MB/GB file sizes, from Better file sizes in Explorer details by m417z.
//
// Explorer asks the property system to format sizes with PDFF_ALWAYSKB; the
// flag is dropped for calls coming from explorerframe.dll / shell32.dll.

namespace filesizes {

// Hooks stay installed; this switches them, so no reload is needed.
std::atomic<bool> g_enabled{false};

using PSFormatForDisplayAlloc_t = decltype(&PSFormatForDisplayAlloc);
PSFormatForDisplayAlloc_t PSFormatForDisplayAlloc_Original;
HRESULT WINAPI PSFormatForDisplayAlloc_Hook(const PROPERTYKEY& key,
                                            const PROPVARIANT& propvar,
                                            PROPDESC_FORMAT_FLAGS pdff,
                                            PWSTR* ppszDisplay) {
    auto original = [=]() {
        return PSFormatForDisplayAlloc_Original(key, propvar, pdff,
                                                ppszDisplay);
    };

    if (!g_enabled.load(std::memory_order_relaxed)) {
        return original();
    }

    PROPDESC_FORMAT_FLAGS pdffNew = pdff & ~PDFF_ALWAYSKB;
    if (pdffNew == pdff) {
        return original();
    }

    void* retAddress = __builtin_return_address(0);

    HMODULE explorerFrame = GetModuleHandle(L"explorerframe.dll");
    if (!explorerFrame) {
        return original();
    }

    HMODULE module;
    if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (PCWSTR)retAddress, &module) ||
        module != explorerFrame) {
        return original();
    }

    Wh_Log(L">");

    return PSFormatForDisplayAlloc_Original(key, propvar, pdffNew, ppszDisplay);
}

using PSFormatForDisplay_t = decltype(&PSFormatForDisplay);
PSFormatForDisplay_t PSFormatForDisplay_Original;
HRESULT WINAPI PSFormatForDisplay_Hook(const PROPERTYKEY& propkey,
                                       const PROPVARIANT& propvar,
                                       PROPDESC_FORMAT_FLAGS pdfFlags,
                                       LPWSTR pwszText,
                                       DWORD cchText) {
    auto original = [=]() {
        return PSFormatForDisplay_Original(propkey, propvar, pdfFlags, pwszText,
                                           cchText);
    };

    if (!g_enabled.load(std::memory_order_relaxed)) {
        return original();
    }

    PROPDESC_FORMAT_FLAGS pdfFlagsNew = pdfFlags & ~PDFF_ALWAYSKB;
    if (pdfFlagsNew == pdfFlags) {
        return original();
    }

    void* retAddress = __builtin_return_address(0);

    HMODULE shell32 = GetModuleHandle(L"shell32.dll");
    if (!shell32) {
        return original();
    }

    HMODULE module;
    if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (PCWSTR)retAddress, &module) ||
        module != shell32) {
        return original();
    }

    Wh_Log(L">");

    return PSFormatForDisplay_Original(propkey, propvar, pdfFlagsNew, pwszText,
                                       cchText);
}

void LoadSettings() {
    g_enabled = Wh_GetIntSetting(L"fileSizesMb") != 0;
}

void Init() {
    LoadSettings();

    WindhawkUtils::SetFunctionHook(PSFormatForDisplayAlloc,
                                       PSFormatForDisplayAlloc_Hook,
                                       &PSFormatForDisplayAlloc_Original);

    // Used by older file dialogs, for example Regedit's export dialog.
    WindhawkUtils::SetFunctionHook(PSFormatForDisplay,
                                       PSFormatForDisplay_Hook,
                                       &PSFormatForDisplay_Original);
}

}  // namespace filesizes

////////////////////////////////////////////////////////////////////////////////
// Desktop drag-select with icons hidden (ours).

namespace dragselect {

#define DDSLOG(...) Wh_Log(__VA_ARGS__)

// =============================================================================
// Settings
// =============================================================================

static std::atomic<int> g_fillOpacity;
static std::atomic<int> g_borderOpacity;
static std::atomic<int> g_borderWidth;
static std::atomic<int> g_cornerRadius;
static std::atomic<int> g_fadeOutMs;

// The accent colour at the start of the drag, with each part's opacity baked
// into its alpha. Touched only on the desktop's UI thread.
static winrt::Windows::UI::Color g_fillColor;
static winrt::Windows::UI::Color g_borderColor;

static winrt::Windows::UI::Color WithOpacity(COLORREF color,
                                             int opacityPercent) {
    opacityPercent = std::clamp(opacityPercent, 0, 100);
    return winrt::Windows::UI::Color{
        (uint8_t)((opacityPercent * 255 + 50) / 100), GetRValue(color),
        GetGValue(color), GetBValue(color)};
}

// The accent colour proper, as shown in Settings. `AccentColorMenu` is
// 0xAABBGGRR and a COLORREF is 0x00BBGGRR, so the low three bytes drop straight
// in. DwmGetColorizationColor is only the fallback: it reports the colorization
// colour, which can be a blend rather than the accent.
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
// and no change notification to subscribe to.
static void RefreshAccentColor() {
    COLORREF accent = SystemAccentColor();
    g_fillColor = WithOpacity(accent, g_fillOpacity);
    g_borderColor = WithOpacity(accent, g_borderOpacity);
}

static int ClampSetting(PCWSTR name, int low, int high) {
    return std::clamp(Wh_GetIntSetting(name), low, high);
}

static void LoadSettings() {
    g_fillOpacity = ClampSetting(L"dragSelectFillOpacity", 0, 100);
    g_borderOpacity = ClampSetting(L"dragSelectBorderOpacity", 0, 100);
    g_borderWidth = ClampSetting(L"dragSelectBorderWidth", 0, 32);
    g_cornerRadius = ClampSetting(L"dragSelectCornerRadius", 0, 64);
    g_fadeOutMs = ClampSetting(L"dragSelectFadeOutMs", 0, 2000);
}

// =============================================================================
// The rectangle
// =============================================================================
//
// A rounded-rectangle shape in Windows' compositor, hosted by a click-through
// popup that covers the whole virtual screen for the length of a drag. The GPU
// draws it: a mouse move only sets the shape's position and size, and the fade
// is a compositor animation.
//
// This replaced a per-pixel layered window that the CPU repainted and
// re-uploaded to DWM on every mouse move. That cost grew with the rectangle's
// area and with the display's refresh rate, and lagged visibly on a 165Hz
// panel.
//
// Not drawn on the desktop's own DC: an animated wallpaper repaints over
// anything drawn there, while this is composited above it.
//
// The class is registered against explorer.exe's hInstance with DefWindowProcW
// as the window procedure, so nothing about it points into this DLL and it
// survives an unload harmlessly. Re-registering after a reload fails with
// ERROR_CLASS_ALREADY_EXISTS, which is fine. No composition event handlers are
// registered either, for the same reason.

namespace wuc = winrt::Windows::UI::Composition;

// Not declared by the toolchain's windows.ui.composition.interop.h.
// {29E691FA-4567-4DCA-B319-D0F207EB6807}
static constexpr GUID IID_ICompositorDesktopInterop = {
    0x29E691FA,
    0x4567,
    0x4DCA,
    {0xB3, 0x19, 0xD0, 0xF2, 0x07, 0xEB, 0x68, 0x07}};

struct ICompositorDesktopInterop : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE
    CreateDesktopWindowTarget(HWND hwndTarget,
                              BOOL isTopmost,
                              void** result) = 0;  // IDesktopWindowTarget**
    virtual HRESULT STDMETHODCALLTYPE EnsureOnThread(DWORD threadId) = 0;
};

static PCWSTR const kOverlayClass = L"WhDesktopDragSelectOverlay";

// Registered at init; see the top of the subclass procedure.
static UINT g_cleanupMessage;

// Everything below is touched only on the desktop's UI thread, from inside the
// subclass procedure.
static HWND g_overlay;
static POINT g_overlayOrigin;  // screen position of the overlay's top-left

static wuc::Compositor g_compositor{nullptr};
static wuc::Desktop::DesktopWindowTarget g_target{nullptr};
static wuc::ShapeVisual g_shape{nullptr};
static wuc::CompositionRoundedRectangleGeometry g_geometry{nullptr};
static wuc::CompositionSpriteShape g_sprite{nullptr};
static wuc::CompositionColorBrush g_fillBrush{nullptr};
static wuc::CompositionColorBrush g_borderBrush{nullptr};

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

// A compositor needs a DispatcherQueue on its thread. The desktop thread may
// already have one; if not, one is created and deliberately left running with
// the thread, so a mod reload finds and reuses it instead of stacking another.
static bool EnsureDispatcherQueue() {
    if (winrt::Windows::System::DispatcherQueue::GetForCurrentThread()) {
        return true;
    }

    DispatcherQueueOptions options{sizeof(DispatcherQueueOptions),
                                   DQTYPE_THREAD_CURRENT, DQTAT_COM_NONE};
    ABI::Windows::System::IDispatcherQueueController* controller = nullptr;
    HRESULT hr = CreateDispatcherQueueController(options, &controller);
    if (FAILED(hr)) {
        DDSLOG(L"CreateDispatcherQueueController failed: %08X", hr);
        return false;
    }

    // Intentionally not released: see above.
    (void)controller;
    return true;
}

static bool EnsureComposition() {
    if (g_compositor) {
        return true;
    }

    try {
        if (!EnsureDispatcherQueue()) {
            return false;
        }

        g_overlay = CreateWindowEx(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE |
                WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOREDIRECTIONBITMAP,
            kOverlayClass, L"", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr,
            GetModuleHandle(nullptr), nullptr);
        if (!g_overlay) {
            DDSLOG(L"CreateWindowEx failed: %u", GetLastError());
            return false;
        }

        // Layered and transparent is what makes the popup click-through;
        // full alpha leaves the composition content untouched.
        SetLayeredWindowAttributes(g_overlay, 0, 255, LWA_ALPHA);

        g_compositor = wuc::Compositor();

        winrt::com_ptr<ICompositorDesktopInterop> interop;
        winrt::check_hresult(
            reinterpret_cast<::IUnknown*>(winrt::get_abi(g_compositor))
                ->QueryInterface(IID_ICompositorDesktopInterop,
                                 interop.put_void()));
        winrt::check_hresult(interop->CreateDesktopWindowTarget(
            g_overlay, TRUE, winrt::put_abi(g_target)));

        g_geometry = g_compositor.CreateRoundedRectangleGeometry();
        g_fillBrush = g_compositor.CreateColorBrush();
        g_borderBrush = g_compositor.CreateColorBrush();

        g_sprite = g_compositor.CreateSpriteShape(g_geometry);
        g_sprite.FillBrush(g_fillBrush);
        g_sprite.StrokeBrush(g_borderBrush);

        g_shape = g_compositor.CreateShapeVisual();
        g_shape.Shapes().Append(g_sprite);
        g_target.Root(g_shape);
        return true;
    } catch (...) {
        DDSLOG(L"Composition setup failed: %08X", winrt::to_hresult());
    }

    return false;
}

static void ReleaseComposition() {
    g_sprite = nullptr;
    g_geometry = nullptr;
    g_fillBrush = nullptr;
    g_borderBrush = nullptr;
    g_shape = nullptr;
    g_target = nullptr;
    g_compositor = nullptr;
}

// Also called from the thread that unloads the mod as a last resort, which is
// safe: composition objects are agile, and by then the window is gone.
static void DestroyOverlay() {
    ReleaseComposition();
    if (g_overlay) {
        DestroyWindow(g_overlay);
        g_overlay = nullptr;
    }
}

// Covers the whole virtual screen, so a drag can cross monitors. Re-fitted at
// the start of every drag, which picks up monitors added or rearranged since.
static void PlaceOverlay() {
    int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    // One pixel short: a topmost window exactly covering a monitor is what the
    // shell takes for a fullscreen app, and it drops the taskbar for those.
    // With a single monitor the virtual screen *is* that monitor.
    g_overlayOrigin = {left, top};
    SetWindowPos(g_overlay, HWND_TOPMOST, left, top, width, height - 1,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

static void BeginMarquee() {
    if (!EnsureComposition()) {
        return;
    }

    try {
        const float border = (float)g_borderWidth;
        g_fillBrush.Color(g_fillColor);
        g_borderBrush.Color(g_borderColor);
        g_sprite.StrokeThickness(border);

        g_shape.StopAnimation(L"Opacity");
        g_shape.Opacity(1.0f);
        g_shape.Size({0, 0});
    } catch (...) {
        DDSLOG(L"BeginMarquee failed: %08X", winrt::to_hresult());
    }

    PlaceOverlay();
}

static void ShowMarquee(POINT cursor) {
    if (!g_shape) {
        return;
    }

    float left = (float)(std::min(g_anchor.x, cursor.x) - g_overlayOrigin.x);
    float top = (float)(std::min(g_anchor.y, cursor.y) - g_overlayOrigin.y);
    float width = (float)std::max(1L, std::abs(cursor.x - g_anchor.x));
    float height = (float)std::max(1L, std::abs(cursor.y - g_anchor.y));

    // The stroke straddles the geometry's edge, so the geometry is inset by
    // half the border to keep the whole border inside the rectangle, and the
    // corner radius by the same amount so the outer edge keeps its radius.
    const float half = std::min((float)g_borderWidth, std::min(width, height) / 2) / 2;
    const float radius =
        std::max(0.0f, std::min((float)g_cornerRadius, std::min(width, height) / 2) - half);

    try {
        g_shape.Offset({left, top, 0});
        g_shape.Size({width, height});
        g_geometry.Offset({half, half});
        g_geometry.Size({width - 2 * half, height - 2 * half});
        g_geometry.CornerRadius({radius, radius});
    } catch (...) {
        DDSLOG(L"ShowMarquee failed: %08X", winrt::to_hresult());
    }
}

// =============================================================================
// Fade-out
// =============================================================================
//
// The fade itself is a compositor animation. A timer on the subclassed desktop
// window only hides the overlay once it has finished: the overlay deliberately
// uses DefWindowProc, so it can't run a handler of its own, and a composition
// completion handler would point into this DLL. The id is far outside the
// small integers the shell uses for its own timers.

static const UINT_PTR kFadeTimerId = 0x57444453;  // 'WDDS'

static HWND g_fadeHost;
static bool g_fading;

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
}

static void BeginFadeOut(HWND host) {
    int duration = g_fadeOutMs;
    if (duration <= 0 || !host || !g_shape || !g_overlay ||
        !IsWindowVisible(g_overlay)) {
        HideMarquee();
        return;
    }

    StopFade();

    try {
        // Quadratic ease-out: holds near full opacity for a moment, then drops
        // away. A linear ramp reads as an abrupt cut at the end.
        auto easing = g_compositor.CreateCubicBezierEasingFunction(
            {0.55f, 0.0f}, {1.0f, 0.45f});
        auto fade = g_compositor.CreateScalarKeyFrameAnimation();
        fade.InsertKeyFrame(0.0f, 1.0f);
        fade.InsertKeyFrame(1.0f, 0.0f, easing);
        fade.Duration(std::chrono::milliseconds(duration));
        g_shape.StartAnimation(L"Opacity", fade);
    } catch (...) {
        DDSLOG(L"Fade failed: %08X", winrt::to_hresult());
        HideMarquee();
        return;
    }

    g_fadeHost = host;
    g_fading = true;

    if (!SetTimer(host, kFadeTimerId, duration + 16, nullptr)) {
        DDSLOG(L"SetTimer failed: %u", GetLastError());
        HideMarquee();
    }
}

static void OnFadeTick() {
    if (g_fading) {
        HideMarquee();
    }
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
        BeginMarquee();
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
        DestroyOverlay();
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
            DestroyOverlay();
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

bool Init() {
    LoadSettings();
    RegisterOverlayClass();

    g_cleanupMessage = RegisterWindowMessage(L"WhDesktopDragSelect_Cleanup");

    InitializeCriticalSection(&g_attachedMutex);

    g_wakeEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!g_wakeEvent) {
        DeleteCriticalSection(&g_attachedMutex);
        return false;
    }

    g_stopping = false;
    g_watcherThread =
        CreateThread(nullptr, 0, WatcherThreadProc, nullptr, 0, nullptr);
    if (!g_watcherThread) {
        CloseHandle(g_wakeEvent);
        g_wakeEvent = nullptr;
        DeleteCriticalSection(&g_attachedMutex);
        return false;
    }

    return true;
}

void BeforeUninit() {
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

void Uninit() {
    // Normally already done on the desktop thread by the cleanup message.
    ReleaseComposition();

    if (g_wakeEvent) {
        CloseHandle(g_wakeEvent);
        g_wakeEvent = nullptr;
    }

    DeleteCriticalSection(&g_attachedMutex);
}

}  // namespace dragselect

////////////////////////////////////////////////////////////////////////////////
// Entry points

bool g_isExplorer;

BOOL WINAPI ShowWindow_Hook(HWND hWnd, int nCmdShow) {
    if (g_active.altTabDelay) {
        BOOL result;
        if (alttab::OnShowWindow(hWnd, nCmdShow, &result)) {
            return result;
        }
    }

    // Returns straight to the original when the genie is switched off.
    return genie::OnShowWindow(hWnd, nCmdShow);
}

struct ExplorerToggles {
    bool autoHide;
    bool altTabDelay;
    bool dragSelect;

    bool operator==(const ExplorerToggles&) const = default;
};

ExplorerToggles ReadExplorerToggles() {
    return {
        Wh_GetIntSetting(L"autoHide") != 0,
        Wh_GetIntSetting(L"altTabDelay") != 0,
        Wh_GetIntSetting(L"dragSelect") != 0,
    };
}

// Never refuses to load: a process that refused isn't offered the mod again
// until it restarts, so a feature switched on later would stay off there.
//
// The genie and the file-size fix are hooked in every process whether or not
// they're switched on, and consult a flag instead. That is what lets their
// settings change without reloading the mod everywhere -- a reload of an
// everywhere-mod walks every running process one at a time, which takes long
// enough that the genie visibly goes missing meanwhile.
BOOL Wh_ModInit() {
    Wh_Log(L">");

    g_isExplorer = IsExplorerProcess();

    if (g_isExplorer) {
        ExplorerToggles toggles = ReadExplorerToggles();
        g_active.autoHide = toggles.autoHide;
        g_active.altTabDelay = toggles.altTabDelay;
        g_active.dragSelect = toggles.dragSelect;
    }

    // A feature that fails to hook turns itself off rather than taking the
    // others down with it.
    if (g_active.autoHide && !autohide::Init()) {
        Wh_Log(L"Auto-hide failed to initialize");
        g_active.autoHide = false;
    }

    if (g_active.altTabDelay && !alttab::Init()) {
        Wh_Log(L"Alt+Tab delay failed to initialize");
        g_active.altTabDelay = false;
    }

    if (g_active.dragSelect && !dragselect::Init()) {
        Wh_Log(L"Drag-select failed to initialize");
        g_active.dragSelect = false;
    }

    genie::Init();
    filesizes::Init();

    Wh_SetFunctionHook((void*)ShowWindow, (void*)ShowWindow_Hook,
                       (void**)&ShowWindow_Original);

    return TRUE;
}

void Wh_ModAfterInit() {
    if (g_active.autoHide) {
        autohide::AfterInit();
    }
}

void Wh_ModBeforeUninit() {
    genie::BeforeUninit();

    if (g_active.dragSelect) {
        dragselect::BeforeUninit();
    }
}

void Wh_ModUninit() {
    genie::Uninit();

    if (g_active.autoHide) {
        autohide::Uninit();
    }

    if (g_active.altTabDelay) {
        alttab::ClearState();
    }

    if (g_active.dragSelect) {
        dragselect::Uninit();
    }
}

BOOL Wh_ModSettingsChanged(BOOL* bReload) {
    genie::LoadSettings();
    filesizes::LoadSettings();

    if (!g_isExplorer) {
        return TRUE;
    }

    // Switching an explorer feature on or off changes which hooks explorer
    // needs; only then is explorer -- and only explorer -- reloaded.
    ExplorerToggles toggles = ReadExplorerToggles();
    if (!(toggles == ExplorerToggles{g_active.autoHide, g_active.altTabDelay,
                                     g_active.dragSelect})) {
        *bReload = TRUE;
        return TRUE;
    }

    if (g_active.autoHide) {
        autohide::LoadSettings();
        autohide::AdjustAllTaskbars();
    }

    if (g_active.altTabDelay) {
        alttab::g_delayMilliseconds =
            std::clamp(Wh_GetIntSetting(L"altTabDelayMs"), 0, 2000);
    }

    if (g_active.dragSelect) {
        dragselect::LoadSettings();
    }

    return TRUE;
}
