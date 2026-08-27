<#
.SYNOPSIS
  Push a mod from this repo into Windhawk's editor workspace so you can Compile it.
.DESCRIPTION
  Windhawk has no command-line compile (see command-line.txt in the install dir),
  so the final build always goes through the UI. This copies your repo version
  over EditorWorkspace\mod.wh.cpp, backing up whatever was there first.

  Use ONLY while you have that mod open in the Windhawk editor -- otherwise
  Windhawk will overwrite it the next time you open a different mod.
.EXAMPLE
  .\scripts\stage.ps1 start-menu
  .\scripts\stage.ps1 start-menu -Clipboard   # copy to clipboard instead
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0, Mandatory)][string]$Mod,
    [switch]$Clipboard
)

. "$PSScriptRoot\env.ps1"

$modPath = Resolve-ModPath $Mod
$meta    = Get-ModMeta $modPath
$name    = Split-Path $modPath -Leaf

if ($Clipboard) {
    Get-Content -LiteralPath $modPath -Raw | Set-Clipboard
    Write-Host "Copied $name to the clipboard." -ForegroundColor Green
    Write-Host "Paste it over the whole buffer in the Windhawk editor, then Compile." -ForegroundColor DarkGray
    return
}

$target = Join-Path $WhWorkspace 'mod.wh.cpp'
if (-not (Test-Path $WhWorkspace)) { throw "Windhawk editor workspace not found: $WhWorkspace" }

if (Test-Path $target) {
    if (-not (Test-Path $BuildDir)) { New-Item -ItemType Directory -Path $BuildDir | Out-Null }
    $stamp  = Get-Date -Format 'yyyyMMdd-HHmmss'
    $backup = Join-Path $BuildDir "editor-backup-$stamp.wh.cpp"
    Copy-Item -LiteralPath $target -Destination $backup -Force

    $existing = Get-ModMeta $target
    Write-Host "Backed up existing editor buffer (@id = $($existing.id)) -> build\editor-backup-$stamp.wh.cpp" -ForegroundColor DarkGray

    if ($existing.id -ne $meta.id -and $existing.id -ne 'mod-id-placeholder') {
        Write-Warning "Editor currently holds '$($existing.id)' but you are staging '$($meta.id)'."
        Write-Warning "Make sure the Windhawk editor has THIS mod open, or Compile will target the wrong mod."
    }
}

Copy-Item -LiteralPath $modPath -Destination $target -Force
Write-Host "Staged $name -> $target" -ForegroundColor Green
Write-Host ""
Write-Host "Next:" -ForegroundColor Cyan
Write-Host "  1. In VS Code (the Windhawk editor window), the file will reload from disk."
Write-Host "  2. Hit Compile in the Windhawk UI."
Write-Host "  3. Windhawk builds the DLL and injects it into: $(@($meta.include) -join ', ')"
