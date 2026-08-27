# Shared paths, config, and helpers for the Windhawk mods repo.
# Dot-source this from the other scripts:  . "$PSScriptRoot\env.ps1"

$ErrorActionPreference = 'Stop'

$RepoRoot     = Split-Path -Parent $PSScriptRoot
$UpstreamDir  = Join-Path $RepoRoot 'upstream'
$ModsDir      = Join-Path $RepoRoot 'mods'
$PatchesDir   = Join-Path $RepoRoot 'patches'
$BuildDir     = Join-Path $RepoRoot 'build'

$WhInstall    = 'C:\Program Files\Windhawk'
$WhData       = 'C:\ProgramData\Windhawk'
$WhCompiler   = Join-Path $WhInstall 'Compiler'
$WhClang      = Join-Path $WhCompiler 'bin\clang++.exe'
$WhInclude    = Join-Path $WhCompiler 'include'
$WhModsSource = Join-Path $WhData 'ModsSource'
$WhWorkspace  = Join-Path $WhData 'EditorWorkspace'
$WhExe        = Join-Path $WhInstall 'windhawk.exe'

$UpstreamRawBase = 'https://raw.githubusercontent.com/ramensoftware/windhawk-mods/main/mods'

function Get-WhEngineVersion {
    $engineRoot = Join-Path $WhInstall 'Engine'
    $dirs = Get-ChildItem -Path $engineRoot -Directory -ErrorAction SilentlyContinue
    if (-not $dirs) { throw "No Windhawk engine found under $engineRoot" }
    ($dirs | Sort-Object Name -Descending | Select-Object -First 1).Name
}

# Parse the // ==WindhawkMod== ... // ==/WindhawkMod== header into a hashtable.
# Repeated keys (e.g. @include) are collected into an array under the same key.
function Get-ModMeta {
    param([Parameter(Mandatory)][string]$Path)

    if (-not (Test-Path $Path)) { throw "Mod source not found: $Path" }

    $meta = @{}
    $inHeader = $false
    foreach ($line in [System.IO.File]::ReadLines($Path)) {
        if ($line -match '^//\s*==WindhawkMod==')  { $inHeader = $true;  continue }
        if ($line -match '^//\s*==/WindhawkMod==') { break }
        if (-not $inHeader) { continue }

        if ($line -match '^//\s*@(\w+)\s+(.*?)\s*$') {
            $key = $Matches[1]; $val = $Matches[2]
            if ($meta.ContainsKey($key)) {
                $meta[$key] = @($meta[$key]) + $val
            } else {
                $meta[$key] = $val
            }
        }
    }
    if (-not $meta.ContainsKey('id')) { throw "No @id found in mod header: $Path" }
    $meta
}

# Windhawk @architecture value -> clang target triple.
function Get-ClangTarget {
    param([string]$Architecture)
    switch -Regex ($Architecture) {
        'x86-64|x86_64|amd64' { return 'x86_64-w64-mingw32' }
        'arm64|aarch64'       { return 'aarch64-w64-mingw32' }
        'x86|i686'            { return 'i686-w64-mingw32' }
        default               { return 'x86_64-w64-mingw32' }
    }
}

# Resolve a mod name/path to a full path under mods/.
function Resolve-ModPath {
    param([Parameter(Mandatory)][string]$Mod)

    if (Test-Path $Mod) { return (Resolve-Path $Mod).Path }

    $name = $Mod
    if ($name -notlike '*.wh.cpp') { $name = "$name.wh.cpp" }
    $candidate = Join-Path $ModsDir $name
    if (Test-Path $candidate) { return $candidate }

    $found = Get-ChildItem -Path $ModsDir -Filter '*.wh.cpp' -ErrorAction SilentlyContinue |
             Where-Object { $_.Name -like "*$($Mod)*" }
    if ($found.Count -eq 1) { return $found[0].FullName }
    if ($found.Count -gt 1) { throw "Ambiguous mod '$Mod': $(($found.Name) -join ', ')" }

    throw "Mod not found: $Mod (looked in $ModsDir)"
}

# Run a native exe capturing stdout/stderr cleanly.
# PowerShell 5.1 wraps native stderr in NativeCommandError when you use 2>&1,
# which corrupts compiler diagnostics -- so redirect to files instead.
function Invoke-Native {
    param(
        [Parameter(Mandatory)][string]$FilePath,
        [string[]]$Arguments = @()
    )
    $outFile = [System.IO.Path]::GetTempFileName()
    $errFile = [System.IO.Path]::GetTempFileName()
    try {
        $quoted = @($Arguments | ForEach-Object {
            if ($_ -match '[\s"]') { '"' + ($_ -replace '"', '\"') + '"' } else { $_ }
        })
        $p = Start-Process -FilePath $FilePath -ArgumentList $quoted -NoNewWindow -Wait -PassThru `
                           -RedirectStandardOutput $outFile -RedirectStandardError $errFile
        [pscustomobject]@{
            ExitCode = $p.ExitCode
            StdOut   = (Get-Content -LiteralPath $outFile -Raw -ErrorAction SilentlyContinue)
            StdErr   = (Get-Content -LiteralPath $errFile -Raw -ErrorAction SilentlyContinue)
        }
    } finally {
        Remove-Item -LiteralPath $outFile, $errFile -Force -ErrorAction SilentlyContinue
    }
}
