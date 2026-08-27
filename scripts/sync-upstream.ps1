<#
.SYNOPSIS
  Fetch the latest upstream source for each fork and vendor it as a new baseline.
.DESCRIPTION
  Downloads ramensoftware/windhawk-mods raw sources, reads the @version from the
  header, and stores it as upstream\<id>@<version>.wh.cpp. Never touches your
  files in mods\ -- it only tells you what changed upstream so you can merge.

  Files are stored with LF endings so diffs against them stay meaningful.
.EXAMPLE
  .\scripts\sync-upstream.ps1                  # check + vendor new versions
  .\scripts\sync-upstream.ps1 -ShowDiff        # also diff old baseline -> new
  .\scripts\sync-upstream.ps1 -UpdateBaseline  # after merging, move the baseline pointer
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)][string]$Mod,
    [switch]$ShowDiff,
    [switch]$UpdateBaseline
)

. "$PSScriptRoot\env.ps1"

[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$forksFile = Join-Path $RepoRoot 'forks.json'
$forks = Get-Content -LiteralPath $forksFile -Raw | ConvertFrom-Json

$names = $forks.PSObject.Properties.Name
if ($Mod) { $names = @($names | Where-Object { $_ -like "*$Mod*" }) }
if ($names.Count -eq 0) { throw "No fork matching '$Mod' in forks.json" }

$changed = $false

foreach ($forkId in $names) {
    $info       = $forks.$forkId
    $upstreamId = $info.upstreamId
    $url        = "$UpstreamRawBase/$upstreamId.wh.cpp"

    Write-Host "`n=== $upstreamId ===" -ForegroundColor Cyan
    Write-Host "  fetching $url" -ForegroundColor DarkGray

    try {
        $resp = Invoke-WebRequest -Uri $url -UseBasicParsing -TimeoutSec 60
    } catch {
        Write-Warning "  fetch failed: $_"
        continue
    }

    $text = $resp.Content -replace "`r`n", "`n"

    # Read @version straight out of the header we just downloaded.
    $newVersion = $null
    foreach ($ln in ($text -split "`n")) {
        if ($ln -match '^//\s*==/WindhawkMod==') { break }
        if ($ln -match '^//\s*@version\s+(.+?)\s*$') { $newVersion = $Matches[1]; break }
    }
    if (-not $newVersion) { Write-Warning "  could not read @version from upstream"; continue }

    $destPath = Join-Path $UpstreamDir "$upstreamId@$newVersion.wh.cpp"
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)

    if (Test-Path $destPath) {
        $existing = [System.IO.File]::ReadAllText($destPath) -replace "`r`n", "`n"
        if ($existing -eq $text) {
            Write-Host "  upstream is v$newVersion -- already vendored, unchanged" -ForegroundColor DarkGray
        } else {
            [System.IO.File]::WriteAllText($destPath, $text, $utf8NoBom)
            Write-Host "  v$newVersion re-published upstream; refreshed baseline file" -ForegroundColor Yellow
            $changed = $true
        }
    } else {
        [System.IO.File]::WriteAllText($destPath, $text, $utf8NoBom)
        Write-Host "  NEW: vendored v$newVersion -> upstream\$upstreamId@$newVersion.wh.cpp" -ForegroundColor Green
        $changed = $true
    }

    if ($newVersion -eq $info.baseVersion) {
        Write-Host "  your baseline (v$($info.baseVersion)) is current." -ForegroundColor Green
        continue
    }

    Write-Host "  UPDATE AVAILABLE: baseline v$($info.baseVersion) -> upstream v$newVersion" -ForegroundColor Yellow

    $oldBase = Join-Path $UpstreamDir "$upstreamId@$($info.baseVersion).wh.cpp"
    if ($ShowDiff -and (Test-Path $oldBase)) {
        Write-Host "  --- what changed upstream between v$($info.baseVersion) and v$newVersion ---" -ForegroundColor DarkGray
        $r = Invoke-Native -FilePath 'git.exe' -Arguments @('diff','--no-index','--no-color','--stat',$oldBase,$destPath)
        if ($r.StdOut) { Write-Host $r.StdOut }
    }

    if ($UpdateBaseline) {
        $info.baseVersion = $newVersion
        Write-Host "  baseline pointer updated to v$newVersion" -ForegroundColor Green
    } else {
        Write-Host "  next: merge upstream changes into mods\$forkId.wh.cpp," -ForegroundColor DarkGray
        Write-Host "        then re-run with -UpdateBaseline to move the pointer." -ForegroundColor DarkGray
    }
}

if ($UpdateBaseline) {
    $json = $forks | ConvertTo-Json -Depth 10
    [System.IO.File]::WriteAllText($forksFile, $json, (New-Object System.Text.UTF8Encoding($false)))
    Write-Host "`nforks.json written." -ForegroundColor Green
}
