<#
.SYNOPSIS
    Traces taskbar tray flyout open/close against mouse clicks, to tell a real
    toggle from the "dismiss then immediately reopen" bug.

.DESCRIPTION
    Open/close comes from SetWinEventHook -- SHOW/HIDE, CLOAKED/UNCLOAKED,
    DESTROY and foreground changes -- not from polling. A first version of this
    script polled a discovered window list once a second and caught 36 clicks
    with zero opens: the flyouts were opening and closing inside the polling
    gap, and its size filter had thrown away the hosts anyway (the tray overflow
    window is 234x74). Events have no such gap and no size guesswork.

    Mouse transitions are still sampled on a tight loop, interleaved with the
    message pump the hooks need.

    Each close is then classified: a close followed by an open of the same
    window with no mouse-down in between is a REOPEN -- the bug, caught
    red-handed.

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\trace-flyout.ps1 -Seconds 45
#>
[CmdletBinding()]
param(
    [int]$Seconds = 45,
    [string]$LogPath,
    # A close only counts as a REOPEN if the window comes back within this many
    # ms with no click in between.
    [int]$ReopenWindowMs = 1500
)

if (-not $LogPath) {
    $LogPath = Join-Path $env:TEMP ("flyout-trace-{0:yyyyMMdd-HHmmss}.log" -f (Get-Date))
}

Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

public static class FlyoutTrace
{
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT
    {
        public int L, T, R, B;
        public bool Has(POINT p) { return p.X >= L && p.X < R && p.Y >= T && p.Y < B; }
        public bool Same(RECT o) { return L == o.L && T == o.T && R == o.R && B == o.B; }
        public override string ToString() { return L + "," + T + " " + (R - L) + "x" + (B - T); }
    }

    [StructLayout(LayoutKind.Sequential)]
    struct MSG { public IntPtr hwnd; public uint msg; public IntPtr w, l; public uint time; public POINT pt; }

    delegate void WinEventProc(IntPtr hook, uint ev, IntPtr hwnd, int idObj, int idChild, uint thread, uint time);

    [DllImport("user32.dll")] static extern short GetAsyncKeyState(int k);
    [DllImport("user32.dll")] static extern bool GetCursorPos(out POINT p);
    [DllImport("user32.dll")] static extern IntPtr WindowFromPoint(POINT p);
    [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] static extern IntPtr GetAncestor(IntPtr h, uint flags);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern IntPtr FindWindowW(string c, string w);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern IntPtr FindWindowExW(IntPtr p, IntPtr c, string cls, string w);
    [DllImport("user32.dll")] static extern IntPtr SetWinEventHook(uint min, uint max, IntPtr mod, WinEventProc cb, uint pid, uint tid, uint flags);
    [DllImport("user32.dll")] static extern bool UnhookWinEvent(IntPtr h);
    [DllImport("user32.dll")] static extern bool PeekMessageW(out MSG m, IntPtr h, uint min, uint max, uint remove);
    [DllImport("user32.dll")] static extern bool TranslateMessage(ref MSG m);
    [DllImport("user32.dll")] static extern IntPtr DispatchMessageW(ref MSG m);

    const uint EVENT_SYSTEM_FOREGROUND = 0x0003;
    const uint EVENT_OBJECT_DESTROY    = 0x8001;
    const uint EVENT_OBJECT_SHOW       = 0x8002;
    const uint EVENT_OBJECT_HIDE       = 0x8003;
    const uint EVENT_OBJECT_CLOAKED    = 0x8017;
    const uint EVENT_OBJECT_UNCLOAKED  = 0x8018;
    const uint WINEVENT_OUTOFCONTEXT   = 0;
    const uint PM_REMOVE               = 1;
    const uint GA_ROOT                 = 2;

    // Every button that can reach a tray flyout, not just the left one. Plus
    // the Windows key, because Win+A and Win+N open these same two flyouts --
    // tracking the modifier alone rules that out without capturing any
    // text-bearing keystroke.
    static readonly int[] WatchKeys    = { 0x01, 0x02, 0x04, 0x05, 0x06, 0x5B, 0x5C };
    static readonly string[] KeyNames  = { "LMB", "RMB", "MMB", "XB1", "XB2", "LWIN", "RWIN" };

    static RECT _tray;
    static bool _haveTray;

    // A flyout host is big enough to read but not necessarily square-ish: the
    // tray overflow window is 234x74. Anything smaller than this is a
    // 1x1 DesktopWindowXamlSource stub and pure noise.
    const int MinW = 150, MinH = 60;

    public class Ev { public double T; public string Kind; public string Key; public string Text; }
    public static List<Ev> Events = new List<Ev>();

    static Stopwatch _sw;
    static StreamWriter _log;
    static Action<string> _echo;
    static WinEventProc _cb;          // must outlive the hooks -- do not inline
    static readonly List<IntPtr> _hooks = new List<IntPtr>();
    static readonly Dictionary<IntPtr, bool> _open = new Dictionary<IntPtr, bool>();
    static readonly Dictionary<IntPtr, string> _keyCache = new Dictionary<IntPtr, string>();

    static string ClassOf(IntPtr h)
    { var sb = new StringBuilder(256); GetClassNameW(h, sb, sb.Capacity); return sb.ToString(); }

    static string TitleOf(IntPtr h)
    { var sb = new StringBuilder(256); GetWindowTextW(h, sb, sb.Capacity); return sb.ToString(); }

    static string ProcOf(IntPtr h)
    {
        uint pid; GetWindowThreadProcessId(h, out pid);
        try { return Process.GetProcessById((int)pid).ProcessName.ToLowerInvariant(); }
        catch { return "?"; }
    }

    static string KeyOf(IntPtr h)
    {
        string k;
        if (_keyCache.TryGetValue(h, out k)) return k;
        k = ProcOf(h) + "|" + ClassOf(h) + "|" + TitleOf(h);
        _keyCache[h] = k;
        return k;
    }

    static void Emit(string kind, string text, string key)
    {
        double t = _sw.Elapsed.TotalMilliseconds;
        string line = string.Format("{0,9:F1}ms  {1,-6} {2}", t, kind, text);
        Events.Add(new Ev { T = t, Kind = kind, Key = key, Text = text });
        _log.WriteLine(line);
        _echo(line);
    }

    static void OnWinEvent(IntPtr hook, uint ev, IntPtr hwnd, int idObj, int idChild, uint thread, uint time)
    {
        try
        {
            if (hwnd == IntPtr.Zero) return;
            if (idObj != 0 || idChild != 0) return;              // OBJID_WINDOW only
            if (GetAncestor(hwnd, GA_ROOT) != hwnd) return;      // top-level only

            if (ev == EVENT_SYSTEM_FOREGROUND)
            {
                Emit("FG", string.Format("{0} [{1}] \"{2}\"", ProcOf(hwnd), ClassOf(hwnd), TitleOf(hwnd)), "");
                return;
            }

            bool wasOpen = _open.ContainsKey(hwnd) && _open[hwnd];

            if (ev == EVENT_OBJECT_DESTROY)
            {
                if (wasOpen)
                    Emit("CLOSE", string.Format("{0} (destroyed) hwnd={1:X}", KeyOf(hwnd), (long)hwnd), KeyOf(hwnd));
                _open.Remove(hwnd);
                _keyCache.Remove(hwnd);
                return;
            }

            RECT r;
            if (!GetWindowRect(hwnd, out r)) return;
            int w = r.R - r.L, h = r.B - r.T;

            bool opening = (ev == EVENT_OBJECT_SHOW || ev == EVENT_OBJECT_UNCLOAKED);
            if (opening && (w < MinW || h < MinH)) return;       // 1x1 XAML source stubs
            if (!opening && !wasOpen) return;                    // never saw it open

            if (opening == wasOpen) return;                      // no state change
            _open[hwnd] = opening;

            // Where the pointer was sitting matters: a flyout that toggles
            // while the cursor rests on the taskbar implicates hover-driven
            // work, one that toggles with the cursor parked elsewhere does not.
            POINT cp; GetCursorPos(out cp);
            string where = _haveTray && _tray.Has(cp) ? " cursor-OVER-TASKBAR" : "";

            Emit(opening ? "OPEN" : "CLOSE",
                 string.Format("{0} {1} hwnd={2:X} cursor=({3},{4}){5}",
                     KeyOf(hwnd), r, (long)hwnd, cp.X, cp.Y, where),
                 KeyOf(hwnd));
        }
        catch { }
    }

    public static string Run(int seconds, string logPath, Action<string> echo)
    {
        _sw = Stopwatch.StartNew();
        _echo = echo;
        _log = new StreamWriter(logPath, false, Encoding.UTF8);
        _log.AutoFlush = true;

        IntPtr tray = FindWindowW("Shell_TrayWnd", null);
        IntPtr notify = tray == IntPtr.Zero ? IntPtr.Zero : FindWindowExW(tray, IntPtr.Zero, "TrayNotifyWnd", null);
        RECT lastTray = new RECT(), lastNotify = new RECT();
        GetWindowRect(tray, out lastTray);
        if (notify != IntPtr.Zero) GetWindowRect(notify, out lastNotify);

        Emit("INIT", "Shell_TrayWnd " + lastTray + "   TrayNotifyWnd " +
             (notify == IntPtr.Zero ? "<not found>" : lastNotify.ToString()), "");

        _cb = new WinEventProc(OnWinEvent);
        _hooks.Add(SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, IntPtr.Zero, _cb, 0, 0, WINEVENT_OUTOFCONTEXT));
        _hooks.Add(SetWinEventHook(EVENT_OBJECT_DESTROY, EVENT_OBJECT_HIDE, IntPtr.Zero, _cb, 0, 0, WINEVENT_OUTOFCONTEXT));
        _hooks.Add(SetWinEventHook(EVENT_OBJECT_CLOAKED, EVENT_OBJECT_UNCLOAKED, IntPtr.Zero, _cb, 0, 0, WINEVENT_OUTOFCONTEXT));

        int live = 0;
        foreach (var hk in _hooks) if (hk != IntPtr.Zero) live++;
        Emit("HOOK", live + " of " + _hooks.Count + " window-event hooks installed", "");

        _tray = lastTray; _haveTray = tray != IntPtr.Zero;

        var keyPrev = new bool[WatchKeys.Length];
        for (int i = 0; i < WatchKeys.Length; i++)
            keyPrev[i] = (GetAsyncKeyState(WatchKeys[i]) & 0x8000) != 0;

        POINT cprev; GetCursorPos(out cprev);
        bool overPrev = _haveTray && _tray.Has(cprev);
        Emit("HOVER", overPrev ? "cursor starts over the taskbar" : "cursor starts off the taskbar", "");

        while (_sw.Elapsed.TotalSeconds < seconds)
        {
            // The hooks are out-of-context, so they only fire while this thread
            // pumps messages.
            MSG m;
            while (PeekMessageW(out m, IntPtr.Zero, 0, 0, PM_REMOVE))
            { TranslateMessage(ref m); DispatchMessageW(ref m); }

            for (int i = 0; i < WatchKeys.Length; i++)
            {
                bool down = (GetAsyncKeyState(WatchKeys[i]) & 0x8000) != 0;
                if (down == keyPrev[i]) continue;
                keyPrev[i] = down;

                POINT p; GetCursorPos(out p);
                IntPtr under = WindowFromPoint(p);
                RECT nr = new RECT();
                if (notify != IntPtr.Zero) GetWindowRect(notify, out nr);
                Emit(down ? "DOWN" : "UP",
                     string.Format("{0} ({1},{2}) under=[{3}] fg=[{4}] inTrayNotify={5}",
                         KeyNames[i], p.X, p.Y, ClassOf(under), ClassOf(GetForegroundWindow()),
                         notify == IntPtr.Zero ? "?" : nr.Has(p).ToString()), "");
            }

            // Pointer entering or leaving the taskbar is itself a stimulus --
            // it is what starts and stops taskbar-dock-animation's per-frame
            // render loop.
            POINT cnow; GetCursorPos(out cnow);
            bool overNow = _haveTray && _tray.Has(cnow);
            if (overNow != overPrev)
            {
                overPrev = overNow;
                Emit("HOVER", string.Format("cursor {0} taskbar at ({1},{2})",
                     overNow ? "ENTERED" : "left", cnow.X, cnow.Y), "");
            }

            // The legacy tray rect is the thing most often accused of drifting
            // from where the XAML pill actually draws. Log every move rather
            // than sampling it once after the fact.
            RECT r2;
            if (tray != IntPtr.Zero && GetWindowRect(tray, out r2) && !r2.Same(lastTray))
            { lastTray = r2; _tray = r2; Emit("MOVE", "Shell_TrayWnd -> " + r2, ""); }
            if (notify != IntPtr.Zero && GetWindowRect(notify, out r2) && !r2.Same(lastNotify))
            { lastNotify = r2; Emit("MOVE", "TrayNotifyWnd -> " + r2, ""); }

            System.Threading.Thread.Sleep(5);
        }

        foreach (var hk in _hooks) if (hk != IntPtr.Zero) UnhookWinEvent(hk);
        _log.Close();
        return logPath;
    }
}
'@

Write-Host ""
Write-Host "Tracing for $Seconds seconds. Log: $LogPath" -ForegroundColor Cyan
Write-Host ""
Write-Host "  Click a tray pill button (Quick Settings / clock / notifications) to open it," -ForegroundColor Gray
Write-Host "  then click the SAME button again to close it. Repeat ~10 times, mixing fast" -ForegroundColor Gray
Write-Host "  repeats with pauses of a few seconds, so both the good and the bad case land" -ForegroundColor Gray
Write-Host "  in one trace." -ForegroundColor Gray
Write-Host ""

$echo = [Action[string]] { param($s) Write-Host $s }
[void][FlyoutTrace]::Run($Seconds, $LogPath, $echo)

$events = [FlyoutTrace]::Events

Write-Host ""
Write-Host "==== Summary ====" -ForegroundColor Cyan

$downs  = @($events | Where-Object { $_.Kind -eq 'DOWN' })
$closes = @($events | Where-Object { $_.Kind -eq 'CLOSE' })
$opens  = @($events | Where-Object { $_.Kind -eq 'OPEN' })

Write-Host ("clicks: {0}   opens: {1}   closes: {2}" -f $downs.Count, $opens.Count, $closes.Count)

if ($opens.Count -eq 0) {
    Write-Host "No flyout opens were seen at all -- the trace captured nothing to judge." -ForegroundColor Yellow
    Write-Host "Check the HOOK line above: if fewer than 3 hooks installed, rerun." -ForegroundColor Yellow
}

$reopens = @()
$cleanToggles = 0
foreach ($c in $closes) {
    $back = $opens |
        Where-Object { $_.Key -eq $c.Key -and $_.T -gt $c.T -and ($_.T - $c.T) -le $ReopenWindowMs } |
        Select-Object -First 1

    # A click in the gap means the user reopened it themselves -- not the bug.
    $clickBetween = $null
    if ($back) { $clickBetween = @($downs | Where-Object { $_.T -gt $c.T -and $_.T -lt $back.T }) }

    if ((-not $back) -or $clickBetween.Count -gt 0) { $cleanToggles++; continue }

    $prevClick = $downs | Where-Object { $_.T -le $c.T } | Select-Object -Last 1
    $prevOpen  = $opens | Where-Object { $_.Key -eq $c.Key -and $_.T -lt $c.T } | Select-Object -Last 1

    $clickToClose = $null
    if ($prevClick) { $clickToClose = [math]::Round($c.T - $prevClick.T, 1) }
    $openFor = $null
    if ($prevOpen) { $openFor = [math]::Round($c.T - $prevOpen.T, 1) }

    $reopens += [pscustomobject]@{
        ClosedAtMs      = [math]::Round($c.T, 1)
        ReopenedAfterMs = [math]::Round($back.T - $c.T, 1)
        ClickToCloseMs  = $clickToClose
        OpenForMs       = $openFor
        Window          = $c.Text
    }
}

Write-Host ("closes that stayed closed: {0}" -f $cleanToggles)

if ($reopens.Count -eq 0) {
    if ($opens.Count -gt 0) { Write-Host "No auto-reopen observed in this trace." -ForegroundColor Green }
} else {
    Write-Host ("AUTO-REOPEN x{0} (closed, then reopened with no click in between):" -f $reopens.Count) -ForegroundColor Yellow
    ($reopens | Format-Table ClosedAtMs, ReopenedAfterMs, ClickToCloseMs, OpenForMs -AutoSize | Out-String) | Write-Host
    Write-Host "OpenForMs is how long the flyout had been open before the click that" -ForegroundColor Gray
    Write-Host "dismissed it. If the bad cases cluster at low values and the good ones do" -ForegroundColor Gray
    Write-Host "not, the trigger is time-since-open, not geometry." -ForegroundColor Gray
}

# The sharpest signal, and the one the reopen table buries: an open that no
# click can account for at all. A burst of these is the bug free-running.
$unprovoked = @()
foreach ($o in $opens) {
    $near = @($downs | Where-Object { $_.T -le $o.T -and ($o.T - $_.T) -le 750 })
    if ($near.Count -gt 0) { continue }
    $lastClick = $downs | Where-Object { $_.T -le $o.T } | Select-Object -Last 1
    $since = $null
    if ($lastClick) { $since = [math]::Round($o.T - $lastClick.T, 1) }
    $unprovoked += [pscustomobject]@{
        OpenedAtMs        = [math]::Round($o.T, 1)
        MsSinceLastInput  = $since
        Window            = ($o.Key -split '\|')[-1]
    }
}

if ($unprovoked.Count -gt 0) {
    Write-Host ("UNPROVOKED OPEN x{0} (no button or Win key in the preceding 750ms):" -f $unprovoked.Count) -ForegroundColor Red
    ($unprovoked | Format-Table OpenedAtMs, MsSinceLastInput, Window -AutoSize | Out-String) | Write-Host
    Write-Host "No mouse button (any of five) and no Windows key can account for these." -ForegroundColor Gray
    Write-Host "Cross-check the cursor= field on those OPEN lines: if it reads" -ForegroundColor Gray
    Write-Host "cursor-OVER-TASKBAR, hover-driven work is still in play; if the cursor" -ForegroundColor Gray
    Write-Host "was parked elsewhere, nothing the user did explains them." -ForegroundColor Gray
} else {
    Write-Host "No unprovoked opens: every open followed an input event." -ForegroundColor Green
}

$moves = @($events | Where-Object { $_.Kind -eq 'MOVE' })
Write-Host ("legacy tray rect changes during trace: {0}" -f $moves.Count)

Write-Host ""
Write-Host "Full log: $LogPath" -ForegroundColor Cyan
