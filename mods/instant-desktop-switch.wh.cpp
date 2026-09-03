// ==WindhawkMod==
// @id              instant-desktop-switch
// @name            Instant Virtual Desktop Switch
// @description     Drops the slide transition when switching virtual desktops, so the taskbar stops blinking
// @version         2.0
// @author          ashtonlariccia
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -luser32
// ==/WindhawkMod==

// Source code is published under The GNU General Public License v3.0.

// ==WindhawkModReadme==
/*
# Instant Virtual Desktop Switch

Switching virtual desktops with `Ctrl+Win+Left/Right` plays a slide transition,
and during it the taskbar appears to blink out and come back. This mod removes
that transition, so the taskbar, the tray and the desktop pager stay put while
the icons update in place.

## What is actually going on

The taskbar is never hidden. Tracking `Shell_TrayWnd`, its XAML island and every
child at 8ms across a switch shows *no* state change at all -- `IsWindowVisible`
stays 1, `DWMWA_CLOAKED` stays 0, the rect never moves. Rendering the taskbar's
own content with `PrintWindow(PW_RENDERFULLCONTENT)` every 20ms shows it fully
painted in all 91 frames.

So nothing in explorer takes the taskbar away, and there is nothing there to
hook. What blinks is DWM compositing the slide over the whole screen. The only
lever that reaches it is `SPI_SETCLIENTAREAANIMATION`, the system's "animation
effects" flag.

## Modes

**Always** (default). The flag is held off for as long as the mod is enabled.
This is unconditional -- it does not depend on catching a keystroke, so it works
no matter what has focus, and it covers every way of switching: the keyboard,
Task View, a pager chip, a touchpad swipe.

The cost is that the same flag animates elements *inside* windows, most visibly
the Start menu opening. Turning it off loses that. `MinAnimate` is untouched, so
minimize and maximize animations are unaffected.

**Only around the switch shortcut.** Clears the flag for `RestoreDelayMs` around
`Ctrl+Win+Left/Right` (and optionally `Ctrl+Win+D` / `Ctrl+Win+F4`) via a
low-level keyboard hook, keeping the Start menu animation the rest of the time.

Be aware of a real limitation before choosing it: a `WH_KEYBOARD_LL` hook inside
medium-integrity `explorer.exe` is silently skipped by UIPI whenever an
**elevated window has focus**. Measured on this machine, 3 of 6 switches did
nothing for exactly that reason -- every miss had an elevated app focused, every
hit a normal one. If you run elevated apps, this mode will quietly do nothing a
lot of the time. It fails safe: the flag simply stays on and you get the stock
slide.

## Restoring

In Always mode the flag is put back when the mod is disabled or unloaded, but
only if the mod is the one that turned it off. If the flag was already off when
the mod loaded, the mod leaves it alone and restores nothing -- it never
switches an animation on that you had deliberately off.

The flag is also re-asserted every few seconds, so anything else that turns
animations back on (the Settings app, a profile sync) does not quietly bring the
blink back.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- Mode: always
  $name: When to remove the transition
  $options:
  - always: Always -- reliable, but loses the Start menu animation
  - perSwitch: Only around the switch shortcut -- keeps the Start menu animation, but does nothing while an elevated window has focus
- RestoreDelayMs: 500
  $name: Restore delay (ms)
  $description: >-
    Only used by "Only around the switch shortcut". How long after the last
    switch keypress the flag is put back. The transition is around 300ms; leave
    some headroom. Every further keypress pushes the restore out again.
- HandleNewDesktop: true
  $name: Also cover Ctrl+Win+D
  $description: Only used by "Only around the switch shortcut".
- HandleCloseDesktop: true
  $name: Also cover Ctrl+Win+F4
  $description: Only used by "Only around the switch shortcut".
- DebugLogToFile: false
  $name: Write the log to a file
  $description: >-
    Also append this mod's log to ids-debug.log in your temp folder. Windhawk's
    log window and any other debug-output viewer compete for the same messages,
    so a file is the reliable way to capture a full trace.
*/
// ==/WindhawkModSettings==

#include <windows.h>

#include <windhawk_utils.h>

#include <atomic>

// =============================================================================
// Logging
// =============================================================================

static std::atomic<bool> g_debugLogToFile;

// Windhawk's log window and any other debug-output viewer compete for the same
// DBWIN messages, so a file is the only reliable full trace. Same reasoning as
// the Virtual Desktop Pager mod.
static void DebugFileLog(PCWSTR format, ...) {
    if (!g_debugLogToFile) {
        return;
    }

    wchar_t path[MAX_PATH];
    DWORD pathLength = GetTempPath(ARRAYSIZE(path), path);
    if (!pathLength || pathLength >= ARRAYSIZE(path) - 20) {
        return;
    }
    wcscat_s(path, L"ids-debug.log");

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

#define IDSLOG(...)                \
    do {                           \
        Wh_Log(__VA_ARGS__);       \
        DebugFileLog(__VA_ARGS__); \
    } while (0)

// =============================================================================
// Settings
// =============================================================================

// How often Always mode re-asserts the flag, so anything else that switches
// animations back on cannot quietly bring the blink back.
static const DWORD kReassertIntervalMs = 4000;

struct {
    bool alwaysMode;
    bool handleNewDesktop;
    bool handleCloseDesktop;
    DWORD restoreDelayMs;
} g_settings;

static void LoadSettings() {
    PCWSTR mode = Wh_GetStringSetting(L"Mode");
    g_settings.alwaysMode = wcscmp(mode, L"perSwitch") != 0;
    Wh_FreeStringSetting(mode);

    g_settings.handleNewDesktop = Wh_GetIntSetting(L"HandleNewDesktop") != 0;
    g_settings.handleCloseDesktop =
        Wh_GetIntSetting(L"HandleCloseDesktop") != 0;

    int delay = Wh_GetIntSetting(L"RestoreDelayMs");
    if (delay < 50) {
        delay = 50;
    } else if (delay > 5000) {
        delay = 5000;
    }
    g_settings.restoreDelayMs = static_cast<DWORD>(delay);

    g_debugLogToFile = Wh_GetIntSetting(L"DebugLogToFile") != 0;
}

// =============================================================================
// Animation flag
// =============================================================================

// Guarded by g_animationMutex. `g_suppressed` means we are the ones who turned
// the flag off and still owe a restore. It is never set when the flag was
// already off, so a user who deliberately runs without animations is left alone
// and never has them switched back on.
static CRITICAL_SECTION g_animationMutex;
static bool g_suppressed;
static ULONGLONG g_restoreDueTick;

static HANDLE g_wakeEvent;
static HANDLE g_workerThread;
static DWORD g_workerThreadId;
static HHOOK g_keyboardHook;
static std::atomic<bool> g_stopping;
static std::atomic<bool> g_reconfigure;

static bool GetClientAreaAnimation() {
    BOOL enabled = TRUE;
    if (!SystemParametersInfo(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0)) {
        // Assume enabled, which is the stock state.
        return true;
    }
    return enabled != FALSE;
}

// No SPIF_SENDCHANGE: the transition reads the live flag, and broadcasting
// WM_SETTINGCHANGE to every top-level window would cost far more than the
// animation being removed. Measures ~17us, so it is safe to call from a hook.
static void SetClientAreaAnimation(bool enabled) {
    // The value travels in pvParam itself, not as a pointer to a value.
    SystemParametersInfo(
        SPI_SETCLIENTAREAANIMATION, 0,
        reinterpret_cast<void*>(static_cast<UINT_PTR>(enabled ? TRUE : FALSE)),
        0);
}

// Clear the flag if it is on, and remember that we owe a restore. Doubles as
// the re-assert in Always mode.
static void SuppressAnimation(bool extendDeadline) {
    EnterCriticalSection(&g_animationMutex);
    if (GetClientAreaAnimation()) {
        SetClientAreaAnimation(false);
        if (!g_suppressed) {
            g_suppressed = true;
            IDSLOG(L"Animation suppressed");
        }
    }
    if (extendDeadline && g_suppressed) {
        // Every keypress pushes the deadline out, so holding the shortcut down
        // never restores the flag mid-slide.
        g_restoreDueTick = GetTickCount64() + g_settings.restoreDelayMs;
    }
    LeaveCriticalSection(&g_animationMutex);
}

static void RestoreAnimation(PCWSTR reason) {
    bool restored = false;

    EnterCriticalSection(&g_animationMutex);
    if (g_suppressed) {
        SetClientAreaAnimation(true);
        g_suppressed = false;
        restored = true;
    }
    LeaveCriticalSection(&g_animationMutex);

    if (restored) {
        IDSLOG(L"Animation restored (%s)", reason);
    }
}

// =============================================================================
// Keyboard hook -- only used by "Only around the switch shortcut"
// =============================================================================

static bool IsDown(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

static bool ShouldSuppressFor(DWORD vk) {
    // Ctrl and Win are required by every shortcut we care about, so check them
    // once and bail early on the overwhelmingly common case.
    if (!IsDown(VK_CONTROL) || (!IsDown(VK_LWIN) && !IsDown(VK_RWIN))) {
        return false;
    }

    switch (vk) {
        case VK_LEFT:
        case VK_RIGHT:
            return true;
        case 'D':
            return g_settings.handleNewDesktop;
        case VK_F4:
            return g_settings.handleCloseDesktop;
        default:
            return false;
    }
}

static LRESULT CALLBACK LowLevelKeyboardProc(int code,
                                             WPARAM wParam,
                                             LPARAM lParam) {
    if (code == HC_ACTION &&
        (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        const KBDLLHOOKSTRUCT* info =
            reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
        if (info && ShouldSuppressFor(info->vkCode)) {
            SuppressAnimation(/*extendDeadline=*/true);
            SetEvent(g_wakeEvent);
        }
    }

    // Never swallow the key. The shell still performs the switch itself, which
    // keeps this mod out of the undocumented virtual desktop COM API entirely
    // and removes any chance of switching twice.
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

// Hooks must be installed and removed on the thread that pumps for them.
static void ApplyModeOnWorkerThread() {
    if (g_settings.alwaysMode) {
        if (g_keyboardHook) {
            UnhookWindowsHookEx(g_keyboardHook);
            g_keyboardHook = nullptr;
            IDSLOG(L"Hook removed (always mode)");
        }
        return;
    }

    if (!g_keyboardHook) {
        // Our own module, not explorer.exe: the hook procedure lives in this
        // DLL, and naming the DLL keeps its refcount honest.
        HMODULE self = nullptr;
        GetModuleHandleEx(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&LowLevelKeyboardProc), &self);

        g_keyboardHook =
            SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, self, 0);
        if (g_keyboardHook) {
            IDSLOG(L"Hook installed");
        } else {
            IDSLOG(L"SetWindowsHookEx failed: %u", GetLastError());
        }
    }
}

// =============================================================================
// Worker thread -- owns the hook, the re-assert tick and the restore timer
// =============================================================================

static DWORD WINAPI WorkerThreadProc(LPVOID) {
    ApplyModeOnWorkerThread();

    if (g_settings.alwaysMode) {
        SuppressAnimation(/*extendDeadline=*/false);
    }

    while (!g_stopping) {
        DWORD timeout = INFINITE;

        if (g_settings.alwaysMode) {
            timeout = kReassertIntervalMs;
        } else {
            EnterCriticalSection(&g_animationMutex);
            if (g_suppressed) {
                ULONGLONG now = GetTickCount64();
                timeout = (now >= g_restoreDueTick)
                              ? 0
                              : static_cast<DWORD>(g_restoreDueTick - now);
            }
            LeaveCriticalSection(&g_animationMutex);
        }

        // MWMO_INPUTAVAILABLE so a hook callback already queued cannot be
        // missed between the check above and the wait.
        MsgWaitForMultipleObjectsEx(1, &g_wakeEvent, timeout, QS_ALLINPUT,
                                    MWMO_INPUTAVAILABLE);

        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                g_stopping = true;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (g_stopping) {
            break;
        }

        if (g_reconfigure.exchange(false)) {
            // Start the new mode from a clean slate rather than inheriting a
            // half-applied state from the old one.
            RestoreAnimation(L"mode change");
            ApplyModeOnWorkerThread();
        }

        if (g_settings.alwaysMode) {
            SuppressAnimation(/*extendDeadline=*/false);
            continue;
        }

        bool due = false;
        EnterCriticalSection(&g_animationMutex);
        due = g_suppressed && GetTickCount64() >= g_restoreDueTick;
        LeaveCriticalSection(&g_animationMutex);

        if (due) {
            RestoreAnimation(L"timer");
        }
    }

    if (g_keyboardHook) {
        UnhookWindowsHookEx(g_keyboardHook);
        g_keyboardHook = nullptr;
    }

    return 0;
}

// =============================================================================
// Mod lifecycle
// =============================================================================

BOOL Wh_ModInit() {
    IDSLOG(L"Init");

    LoadSettings();

    InitializeCriticalSection(&g_animationMutex);

    g_wakeEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!g_wakeEvent) {
        IDSLOG(L"CreateEvent failed: %u", GetLastError());
        DeleteCriticalSection(&g_animationMutex);
        return FALSE;
    }

    g_stopping = false;
    g_workerThread =
        CreateThread(nullptr, 0, WorkerThreadProc, nullptr, 0,
                     &g_workerThreadId);
    if (!g_workerThread) {
        IDSLOG(L"CreateThread failed: %u", GetLastError());
        CloseHandle(g_wakeEvent);
        g_wakeEvent = nullptr;
        DeleteCriticalSection(&g_animationMutex);
        return FALSE;
    }

    return TRUE;
}

void Wh_ModSettingsChanged() {
    IDSLOG(L"Settings changed");

    LoadSettings();

    g_reconfigure = true;
    if (g_wakeEvent) {
        SetEvent(g_wakeEvent);
    }
}

void Wh_ModBeforeUninit() {
    IDSLOG(L"Before uninit");

    g_stopping = true;

    // Wake the pump both ways: the event covers the wait, PostThreadMessage
    // covers a thread sitting in a hook dispatch.
    if (g_wakeEvent) {
        SetEvent(g_wakeEvent);
    }
    if (g_workerThreadId) {
        PostThreadMessage(g_workerThreadId, WM_QUIT, 0, 0);
    }

    if (g_workerThread) {
        WaitForSingleObject(g_workerThread, 2000);
        CloseHandle(g_workerThread);
        g_workerThread = nullptr;
    }

    // Unconditionally, and last: never leave the user with animations switched
    // off because the mod went away. No-op if we did not turn them off.
    RestoreAnimation(L"uninit");
}

void Wh_ModUninit() {
    IDSLOG(L"Uninit");

    if (g_wakeEvent) {
        CloseHandle(g_wakeEvent);
        g_wakeEvent = nullptr;
    }

    DeleteCriticalSection(&g_animationMutex);
}
