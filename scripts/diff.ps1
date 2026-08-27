<#
.SYNOPSIS
  Show your customizations as a diff against the vendored upstream baseline.
.DESCRIPTION
  Normalizes line endings on both sides first -- Windhawk's ModsSource files are
  CRLF while GitHub raw is LF, so without normalization every line shows as changed.
.EXAMPLE
  .\scripts\diff.ps1                  # diff all forks
  .\scripts\diff.ps1 start-menu       # diff one
  .\scripts\diff.ps1 -Save            # also write patches\<fork>.patch
  .\scripts\diff.ps1 -Stat            # summary only
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)][string]$Mod,
    [switch]$Save,
    [switch]$Stat
)

. "$PSScriptRoot\env.ps1"

$forksFile = Join-Path $RepoRoot 'forks.json'
if (-not (Test-Path $forksFile)) { throw "forks.json not found at $forksFile" }
$forks = Get-Content -LiteralPath $forksFile -Raw | ConvertFrom-Json

$names = $forks.PSObject.Properties.Name
if ($Mod) { $names = @($names | Where-Object { $_ -like "*$Mod*" }) }
if ($names.Count -eq 0) { throw "No fork matching '$Mod' in forks.json" }

function Write-NormalizedCopy {
    param([string]$Source, [string]$Dest)
    $text = [System.IO.File]::ReadAllText($Source) -replace "`r`n", "`n"
    [System.IO.File]::WriteAllText($Dest, $text, (New-Object System.Text.UTF8Encoding($false)))
}

foreach ($forkId in $names) {
    $info     = $forks.$forkId
    $modPath  = Join-Path $ModsDir "$forkId.wh.cpp"
    $basePath = Join-Path $UpstreamDir "$($info.upstreamId)@$($info.baseVersion).wh.cpp"

    if (-not (Test-Path $modPath))  { Write-Warning "missing fork source: $modPath";  continue }
    if (-not (Test-Path $basePath)) { Write-Warning "missing baseline: $basePath";    continue }

    Write-Host "`n=== $forkId  vs  $($info.upstreamId)@$($info.baseVersion) ===" -ForegroundColor Cyan

    $tmpBase = [System.IO.Path]::GetTempFileName()
    $tmpMod  = [System.IO.Path]::GetTempFileName()
    try {
        Write-NormalizedCopy $basePath $tmpBase
        Write-NormalizedCopy $modPath  $tmpMod

        # Always take the full diff; -Stat summarizes it below. Using git's own
        # --stat would print the raw temp paths we can't cleanly rewrite.
        $gitArgs = @('diff', '--no-index', '--no-color', $tmpBase, $tmpMod)

        $r = Invoke-Native -FilePath 'git.exe' -Arguments $gitArgs

        if ([string]::IsNullOrWhiteSpace($r.StdOut)) {
            Write-Host "  (identical to upstream baseline -- no customizations yet)" -ForegroundColor DarkGray
        } else {
            # Rewrite the temp paths in the diff header to something readable.
            # git escapes backslashes and quotes Windows paths, so match the
            # header lines structurally instead of substituting the paths.
            $aName = "upstream/$($info.upstreamId)@$($info.baseVersion).wh.cpp"
            $bName = "mods/$forkId.wh.cpp"
            $lines = $r.StdOut.TrimEnd() -split "`r?`n"
            $inHeader = $true
            $out = foreach ($ln in $lines) {
                if ($inHeader) {
                    if ($ln -match '^@@')            { $inHeader = $false; $ln }
                    elseif ($ln -match '^diff --git') { "diff --git a/$aName b/$bName" }
                    elseif ($ln -match '^--- ')       { "--- a/$aName" }
                    elseif ($ln -match '^\+\+\+ ')    { "+++ b/$bName" }
                    else                              { $ln }
                } else { $ln }
            }
            $pretty = $out -join "`r`n"
            if ($Stat) {
                $adds  = @($lines | Where-Object { $_ -like '+*' -and $_ -notlike '+++*' }).Count
                $dels  = @($lines | Where-Object { $_ -like '-*' -and $_ -notlike '---*' }).Count
                $hunks = @($lines | Where-Object { $_ -like '@@*' }).Count
                Write-Host ("  {0} hunk(s), +{1} / -{2} lines vs upstream baseline" -f $hunks, $adds, $dels)
            } else {
                Write-Host $pretty
            }

            if ($Save) {
                if (-not (Test-Path $PatchesDir)) { New-Item -ItemType Directory -Path $PatchesDir | Out-Null }
                $patchPath = Join-Path $PatchesDir "$forkId.patch"
                [System.IO.File]::WriteAllText($patchPath, $pretty, (New-Object System.Text.UTF8Encoding($false)))
                Write-Host "  saved -> patches\$forkId.patch" -ForegroundColor Green
            }
        }
        if ($r.StdErr) { Write-Host $r.StdErr -ForegroundColor DarkYellow }
    } finally {
        Remove-Item -LiteralPath $tmpBase, $tmpMod -Force -ErrorAction SilentlyContinue
    }
}
