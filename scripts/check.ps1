<#
.SYNOPSIS
  Fast syntax/semantic check of a Windhawk mod using Windhawk's own bundled clang.
.DESCRIPTION
  Runs clang -fsyntax-only with the same flags Windhawk's editor uses. Catches
  every compile error without going through the Windhawk UI. Takes ~10s on a
  16k-line mod vs. a full UI compile+inject cycle.

  This does NOT produce a DLL -- see README "Why builds go through the UI".
.EXAMPLE
  .\scripts\check.ps1                     # check every mod in mods\
  .\scripts\check.ps1 start-menu          # check one mod (substring match)
  .\scripts\check.ps1 -Warnings           # add -Wall -Wextra
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)][string]$Mod,
    [switch]$Warnings
)

. "$PSScriptRoot\env.ps1"

if (-not (Test-Path $WhClang)) { throw "Windhawk clang not found at $WhClang" }

if ($Mod) {
    $targets = @(Resolve-ModPath $Mod)
} else {
    $targets = @(Get-ChildItem -Path $ModsDir -Filter '*.wh.cpp' -ErrorAction SilentlyContinue |
                 ForEach-Object { $_.FullName })
}

if ($targets.Count -eq 0) { Write-Warning "No mods found in $ModsDir"; exit 0 }

$failed = 0
foreach ($path in $targets) {
    $meta   = Get-ModMeta $path
    $triple = Get-ClangTarget $meta.architecture
    $name   = Split-Path $path -Leaf

    $clangArgs = @(
        '-x', 'c++', '-std=c++23', '-target', $triple,
        '-DUNICODE', '-D_UNICODE',
        '-DWINVER=0x0A00', '-D_WIN32_WINNT=0x0A00', '-D_WIN32_IE=0x0A00',
        '-DNTDDI_VERSION=0x0A000008', '-D__USE_MINGW_ANSI_STDIO=0',
        '-DWH_MOD', '-DWH_EDITING',
        '-include', 'windhawk_api.h',
        '-Wno-pragma-pack', '-Wno-pragma-system-header-outside-header'
    )
    if ($Warnings) {
        $clangArgs += @('-Wall', '-Wextra', '-Wno-unused-parameter',
                        '-Wno-missing-field-initializers',
                        '-Wno-cast-function-type-mismatch')
    }
    $clangArgs += @('-fsyntax-only', $path)

    Write-Host "check  $name  [$triple]" -ForegroundColor Cyan
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $r  = Invoke-Native -FilePath $WhClang -Arguments $clangArgs
    $sw.Stop()
    $code = $r.ExitCode

    foreach ($stream in @($r.StdOut, $r.StdErr)) {
        if ($stream) {
            $stream.TrimEnd() -split "`r?`n" | ForEach-Object { Write-Host "  $_" }
        }
    }

    if ($code -eq 0) {
        Write-Host ("  OK  ({0:N1}s)" -f $sw.Elapsed.TotalSeconds) -ForegroundColor Green
    } else {
        Write-Host ("  FAILED (exit $code, {0:N1}s)" -f $sw.Elapsed.TotalSeconds) -ForegroundColor Red
        $failed++
    }
}

if ($failed -gt 0) { Write-Host "`n$failed of $($targets.Count) mod(s) failed." -ForegroundColor Red; exit 1 }
Write-Host "`nAll $($targets.Count) mod(s) OK." -ForegroundColor Green
