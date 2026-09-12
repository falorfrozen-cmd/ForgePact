<#
.SYNOPSIS
  Send a command to the running BloodPactPlugin and print just its reply.

.DESCRIPTION
  The plugin's IPC is file-based: it reads and deletes <game>\bin\bp_ipc\cmd.txt
  every 6 frames and appends output to out.txt in the same folder. Every
  research session before 2026-09-11 drove that by hand - write cmd.txt in an
  editor, then scroll a multi-megabyte out.txt looking for the new lines. This
  does both halves properly: it records out.txt's length first, waits for the
  game to actually consume cmd.txt, then prints only what was appended.

  agents.md's "Limit Rebuilds & Reruns" section asks for exactly this kind of
  tool so a live session spends its time measuring rather than shuffling files.

.PARAMETER Command
  The command line to send, e.g. "citrace methods". Multiple positional words
  are joined with spaces, so quoting is optional.

.PARAMETER Lines
  Several commands to send in one cmd.txt write (the plugin runs each line in
  order). Prefer one at a time for anything that mutates game state - plan C
  §4 rule 4 is "one item, one call, one observation".

.PARAMETER Tail
  Print the last N lines of out.txt and exit without sending anything.

.PARAMETER TimeoutSec
  How long to wait for the game to consume cmd.txt (default 10). A timeout
  almost always means the game is not running or the plugin did not load.

.EXAMPLE
  .\ForgePact\tools\ipc.ps1 citrace methods

.EXAMPLE
  .\ForgePact\tools\ipc.ps1 -Lines "petquest 1","petquest stat"

.EXAMPLE
  .\ForgePact\tools\ipc.ps1 -Tail 40
#>
[CmdletBinding(DefaultParameterSetName = "Send")]
param(
    [Parameter(ParameterSetName = "Send", Position = 0, ValueFromRemainingArguments = $true)]
    [string[]] $Command,

    [Parameter(ParameterSetName = "Many")]
    [string[]] $Lines,

    [Parameter(ParameterSetName = "Tail")]
    [int] $Tail = 40,

    [int] $TimeoutSec = 10
)

$ErrorActionPreference = "Stop"

function Get-IpcDir {
    # Resolve from the panel's own config rather than hardcoding a Steam path,
    # so this keeps working if the game is moved or a second copy is used.
    $cfgPath = Join-Path $env:LOCALAPPDATA "Hero_Siege\forgepact.json"
    if (-not (Test-Path $cfgPath)) {
        throw "forgepact.json not found at $cfgPath - set the game path in the panel once, then retry."
    }
    $cfg = Get-Content $cfgPath -Raw | ConvertFrom-Json
    if (-not $cfg.game_exe) { throw "forgepact.json has no game_exe - set the game path in the panel." }
    $ipc = Join-Path (Split-Path $cfg.game_exe -Parent) "bp_ipc"
    if (-not (Test-Path $ipc)) {
        throw "bp_ipc not found at $ipc - launch the modded game at least once so the plugin creates it."
    }
    return $ipc
}

$ipcDir = Get-IpcDir
$outPath = Join-Path $ipcDir "out.txt"
$cmdPath = Join-Path $ipcDir "cmd.txt"

if ($PSCmdlet.ParameterSetName -eq "Tail") {
    if (-not (Test-Path $outPath)) { Write-Host "out.txt does not exist yet"; exit 0 }
    Get-Content $outPath -Tail $Tail
    exit 0
}

$toSend = if ($PSCmdlet.ParameterSetName -eq "Many") { $Lines } else { , ($Command -join " ") }
$toSend = $toSend | Where-Object { $_ -and $_.Trim() -ne "" }
if (-not $toSend) { throw "nothing to send - pass a command, -Lines, or -Tail" }

# Byte offset, not line count: out.txt is appended to by the game continuously
# (other mods' status lines), so a line-count delta would drift.
$before = 0
if (Test-Path $outPath) { $before = (Get-Item $outPath).Length }

if (Test-Path $cmdPath) {
    Write-Host "note: a cmd.txt was already pending - the game has not consumed it. Overwriting." -ForegroundColor Yellow
}

# Plain ASCII, no BOM: the plugin reads the file as raw bytes and splits on
# newlines, so a UTF-8 BOM would corrupt the first command.
[System.IO.File]::WriteAllText($cmdPath, (($toSend -join "`r`n") + "`r`n"), (New-Object System.Text.ASCIIEncoding))
foreach ($l in $toSend) { Write-Host ">> $l" -ForegroundColor Cyan }

$deadline = (Get-Date).AddSeconds($TimeoutSec)
$consumed = $false
while ((Get-Date) -lt $deadline) {
    if (-not (Test-Path $cmdPath)) { $consumed = $true; break }
    Start-Sleep -Milliseconds 100
}

if (-not $consumed) {
    Write-Host "TIMEOUT after ${TimeoutSec}s - cmd.txt was never consumed." -ForegroundColor Red
    Write-Host "  Is Hero_Siege.exe running with the plugin loaded? (mods\aurie\BloodPactPlugin.dll)" -ForegroundColor Red
    exit 1
}

# The plugin appends output while running the command, so give the slower ones
# (full instance dumps run to a few hundred Out() calls) a moment to finish
# before deciding the reply is complete. Settles as soon as the file stops
# growing rather than always sleeping the full budget.
$stableFor = 0
$last = -1
while ($stableFor -lt 4 -and (Get-Date) -lt $deadline.AddSeconds(10)) {
    Start-Sleep -Milliseconds 150
    $now = 0
    if (Test-Path $outPath) { $now = (Get-Item $outPath).Length }
    if ($now -eq $last) { $stableFor++ } else { $stableFor = 0 }
    $last = $now
}

if (-not (Test-Path $outPath)) { Write-Host "(no out.txt)"; exit 0 }
$fs = [System.IO.File]::Open($outPath, "Open", "Read", "ReadWrite")
try {
    $fs.Seek($before, "Begin") | Out-Null
    $sr = New-Object System.IO.StreamReader($fs)
    $new = $sr.ReadToEnd()
} finally { $fs.Dispose() }

if ($new.Trim() -eq "") { Write-Host "(command consumed, but nothing was appended to out.txt)" -ForegroundColor Yellow }
else { Write-Output $new.TrimEnd() }
