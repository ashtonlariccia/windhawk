<#
.SYNOPSIS
    Print where each taskbar pill starts and ends, so pill geometry constants
    can be measured instead of guessed.

.DESCRIPTION
    Walks every taskbar window (Shell_TrayWnd and each Shell_SecondaryTrayWnd)
    with UI Automation and reports the bounding rectangle of the app frame, the
    tray buttons and the virtual-desktop pager chips.

    Coordinates come out in DIPs, not physical pixels, because this script is
    deliberately left DPI-unaware: UI Automation then hands back rects already
    divided by the scale factor. DIPs are the unit XAML's MaxWidth, Margin and
    Padding use, so the numbers can be pasted straight into a theme constant.
    At 150% scaling a 2880-px-wide screen therefore reads as 1920.

    Its reason for existing is the WindowGlass Pill theme's $AppPillGutter --
    the strip reserved at each end of the bar for the tray and pager pills. See
    README "How wide the app pill gets".

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\measure-pills.ps1
#>
[CmdletBinding()]
param(
    # Also print elements the summary ignores, e.g. individual task buttons.
    [switch]$All
)

Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes

Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class Bars {
    public delegate bool EnumProc(IntPtr h, IntPtr p);
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc cb, IntPtr p);
    [DllImport("user32.dll")] static extern int GetClassName(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
    public static string Cls(IntPtr h) {
        var sb = new StringBuilder(256); GetClassName(h, sb, 256); return sb.ToString();
    }
    public static List<IntPtr> All() {
        var found = new List<IntPtr>();
        EnumWindows((h, p) => {
            var c = Cls(h);
            if (c == "Shell_TrayWnd" || c == "Shell_SecondaryTrayWnd") found.Add(h);
            return true;
        }, IntPtr.Zero);
        return found;
    }
}
'@

# try/catch is a statement in PowerShell 5.1, not an expression, so property
# reads that can throw (a control disposed mid-walk) go through here.
function Get-Prop {
    param([scriptblock]$Read)
    try { & $Read } catch { '' }
}

# Every element on the bar, flattened, with the depth it was found at. The
# control view is used rather than the raw view: the raw view is full of
# presenters and panels that have no rect of their own.
function Get-BarElements {
    param($Element, [int]$Depth = 0)
    if ($Depth -gt 6) { return }
    $walker = [System.Windows.Automation.TreeWalker]::ControlViewWalker
    $child  = $walker.GetFirstChild($Element)
    while ($child) {
        $rect = $null
        try { $rect = $child.Current.BoundingRectangle } catch {}
        if ($rect -and $rect.Width -gt 0) {
            [pscustomobject]@{
                Depth = $Depth
                Class = Get-Prop { $child.Current.ClassName }
                # Tray tooltips are multi-line; keep the summary one line each.
                Name  = (Get-Prop { $child.Current.Name }) -replace '\s+', ' '
                Left  = [int]$rect.X
                Width = [int]$rect.Width
                Right = [int]($rect.X + $rect.Width)
            }
        }
        Get-BarElements -Element $child -Depth ($Depth + 1)
        $child = $walker.GetNextSibling($child)
    }
}

# Reports one group's extent, and -- for the two groups that sit against a bar
# edge -- how much of that edge a centred app pill has to stay clear of. The
# reach is measured from the bar edge the group is up against, not from the
# group, since that is what $AppPillGutter reserves.
function Write-Span {
    param([string]$Label, $Items, [int]$BarWidth, [ValidateSet('Left', 'Right', 'None')][string]$Edge)
    if (-not $Items) { Write-Host ("  {0,-12} (absent)" -f $Label) -ForegroundColor DarkGray; return $null }
    $l = ($Items | Measure-Object Left  -Minimum).Minimum
    $r = ($Items | Measure-Object Right -Maximum).Maximum
    $reach = switch ($Edge) { 'Right' { $BarWidth - $l } 'Left' { $r } default { $null } }
    $line = "  {0,-12} {1,5} -> {2,5}   width {3,5}" -f $Label, $l, $r, ($r - $l)
    if ($null -ne $reach) { $line += "   reaches {0,5} from its edge" -f $reach }
    Write-Host $line
    return $reach
}

foreach ($hwnd in [Bars]::All()) {
    $r = New-Object Bars+RECT
    [void][Bars]::GetWindowRect($hwnd, [ref]$r)
    $barWidth = $r.R - $r.L
    Write-Host ("`n=== {0}  {1},{2}  {3}x{4} DIP ===" -f [Bars]::Cls($hwnd), $r.L, $r.T, $barWidth, ($r.B - $r.T)) -ForegroundColor Cyan

    $els = @(Get-BarElements -Element ([System.Windows.Automation.AutomationElement]::FromHandle($hwnd)))
    if (-not $els) { Write-Host "  (no elements -- is the bar hidden?)" -ForegroundColor DarkGray; continue }

    [void](Write-Span 'app buttons' @($els | Where-Object { $_.Class -like 'Taskbar.TaskbarFrame*' }) $barWidth 'None')
    $tray  = Write-Span 'tray icons'  @($els | Where-Object { $_.Class -like 'SystemTray.*' })          $barWidth 'Right'
    $pager = Write-Span 'pager chips' @($els | Where-Object { $_.Name -match '^Desktop \d+$' })         $barWidth 'Left'

    # UI Automation reports the buttons, not the pill drawn around them: the
    # pill's border sits one $TrayPillPadding (10) further out on the side
    # facing the apps. Add that, then 20 for a tray icon's worth of headroom,
    # and round up to a tidy number.
    $reach = @($tray, $pager | Where-Object { $null -ne $_ })
    if ($reach) {
        $suggest = [Math]::Ceiling(((($reach | Measure-Object -Maximum).Maximum + 10 + 20) / 10)) * 10
        Write-Host ("  => `$AppPillGutter {0}, leaving the app pill {1} DIP to grow into" -f $suggest, ($barWidth - 2 * $suggest)) -ForegroundColor Green
    }

    if ($All) {
        Write-Host "  --- all elements ---" -ForegroundColor DarkGray
        $els | ForEach-Object {
            '{0}{1,-36} {2,5} -> {3,5}  {4}' -f (' ' * ($_.Depth * 2)), $_.Class, $_.Left, $_.Right, $_.Name
        }
    }
}
