<#
.SYNOPSIS
    Read-only diagnostics for the X6100 JS8 firmware (github.com/randal007/x6100-js8).

.DESCRIPTION
    Talks to the X6100's Linux console over the USB cable (the radio's DEV
    USB-C port), logs in, and collects what we need to find a problem:
    firmware and BASE versions, the transmit settings (power, TX codec gain,
    band gain corrections, the FT8/JS8 learned drive level), the app log and
    kernel messages. Optionally it records the app log while YOU transmit
    (TUNE, SSB, JS8), and asks what your power meter showed.

    It only READS. It changes no setting, writes nothing on the radio and
    never transmits by itself. Everything goes into one text file on your
    Desktop: send that file back.

    If the radio is running the stock Xiegu firmware (no SD card), the
    script stops without typing anything.

.EXAMPLE
    Right-click the file > Run with PowerShell
.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\x6100-diag.ps1
.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\x6100-diag.ps1 -Port COM7 -SkipTx
#>
param(
    [string]$Port = "",    # e.g. COM7; found automatically when left out
    [switch]$SkipTx        # only collect settings and logs, no transmit tests
)

$ErrorActionPreference = 'Stop'
$Baud = 115200

$Stamp   = Get-Date -Format 'yyyy-MM-dd_HHmmss'
$Desktop = [Environment]::GetFolderPath('Desktop')
if (-not $Desktop -or -not (Test-Path $Desktop)) { $Desktop = (Get-Location).Path }
$OutFile = Join-Path $Desktop "x6100-diag-$Stamp.txt"

function Log([string]$text) {
    Add-Content -LiteralPath $OutFile -Value $text -Encoding UTF8
}
function Say([string]$text, [string]$color = 'White') {
    Write-Host $text -ForegroundColor $color
    Log "## $text"
}
function Stop-Script([string]$why) {
    Say $why 'Red'
    Write-Host ""
    Write-Host "Report so far: $OutFile"
    Read-Host "Press Enter to close" | Out-Null
    exit 1
}

Log "X6100 diagnostics, $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss zzz')"
Log "PC: $([Environment]::OSVersion.VersionString), PowerShell $($PSVersionTable.PSVersion)"
Log ""

# ---------------------------------------------------------------------------
# Finding the radio's console port
# ---------------------------------------------------------------------------
# The X6100's USB-C DEV port is a WCH CH342 (USB VID 1A86, PID 55D2) with two
# serial ports: interface 00 is the Linux console (the one we want), the
# other one is CAT. Windows 10/11 usually installs its own driver; WCH's
# CH343SER driver works too.

function Get-RadioPorts {
    $all = @(Get-CimInstance Win32_PnPEntity -ErrorAction SilentlyContinue |
             Where-Object { $_.Name -match '\((COM\d+)\)' })
    foreach ($d in $all) {
        $id = [string]$d.PNPDeviceID
        if ($id -notmatch 'VID_1A86') { continue }
        [pscustomobject]@{
            Port   = [regex]::Match($d.Name, '\((COM\d+)\)').Groups[1].Value
            Iface  = [regex]::Match($id, 'MI_(\d\d)').Groups[1].Value
            IsX6100 = ($id -match 'PID_55D2')
            Name   = $d.Name
            Id     = $id
        }
    }
}

function Open-Port([string]$name) {
    $sp = New-Object System.IO.Ports.SerialPort $name, $Baud, 'None', 8, 'One'
    $sp.Handshake    = 'None'
    $sp.ReadTimeout  = 500
    $sp.WriteTimeout = 2000
    $sp.NewLine      = "`r"
    $sp.Encoding     = [System.Text.Encoding]::UTF8
    $sp.Open()
    # DTR/RTS on, as a terminal program does (not needed by every driver)
    try { $sp.DtrEnable = $true; $sp.RtsEnable = $true } catch { }
    return $sp
}

# Read whatever arrives for up to $ms milliseconds; stop early once $until
# (a regex) matches everything read so far.
function Read-For($sp, [int]$ms, [string]$until = '') {
    $sb = New-Object System.Text.StringBuilder
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.ElapsedMilliseconds -lt $ms) {
        if ($sp.BytesToRead -gt 0) {
            [void]$sb.Append($sp.ReadExisting())
            if ($until -and ($sb.ToString() -match $until)) { break }
        } else {
            Start-Sleep -Milliseconds 40
        }
    }
    return $sb.ToString()
}

# What the console says to a single Enter.
function Get-ConsoleState([string]$text) {
    $tail = $text
    if ($tail.Length -gt 300) { $tail = $tail.Substring($tail.Length - 300) }
    if ($text -match 'XIEGU')                { return 'stock' }
    if ($tail -match '\]#\s*$|#\s*$')        { return 'shell' }
    if ($tail -match 'login:\s*$')           { return 'login' }
    if ($tail -match 'assword:\s*$')         { return 'password' }
    if ($text.Trim().Length -gt 0)           { return 'other' }
    return 'silent'
}

Write-Host ""
Write-Host "X6100 JS8 diagnostics (read-only)" -ForegroundColor Cyan
Write-Host "Report file: $OutFile"
Write-Host ""

$candidates = @()
if ($Port) {
    $candidates = @($Port)
    Say "Using the port you gave: $Port"
} else {
    $found = @(Get-RadioPorts)
    foreach ($p in $found) { Log "found: $($p.Port)  $($p.Name)  $($p.Id)" }
    $x6100 = @($found | Where-Object { $_.IsX6100 } | Sort-Object Iface)
    if ($x6100.Count -gt 0) {
        $candidates = @($x6100 | ForEach-Object { $_.Port })
    } elseif ($found.Count -gt 0) {
        # A WCH chip, but not the one we expect: try them, console first.
        $candidates = @($found | Sort-Object Iface | ForEach-Object { $_.Port })
    }
    if ($candidates.Count -eq 0) {
        $names = [System.IO.Ports.SerialPort]::GetPortNames() -join ', '
        Log "all COM ports: $names"
        Stop-Script ("Can't find the X6100 on USB. Check: the cable is in the radio's DEV USB-C port " +
                     "(not HOST), the radio is on and booted from the SD card, and Device Manager shows " +
                     "two 'USB Serial Device' COM ports. If it shows an unknown device instead, install " +
                     "WCH's CH343SER driver (wch-ic.com), then run this again. COM ports seen: $names")
    }
    Say "X6100 port(s) found: $($candidates -join ', ')"
}

# Probe each candidate with one Enter; the console answers, CAT stays silent.
$sp = $null
$state = ''
$greeting = ''
foreach ($c in $candidates) {
    Write-Host "Trying $c ..."
    try { $try = Open-Port $c } catch { Log "$c : can't open ($($_.Exception.Message))"; continue }
    [void](Read-For $try 300)                   # drop anything already waiting
    $try.Write("`r")
    $text = Read-For $try 2500 '(login:|assword:|#)\s*$'
    $st = Get-ConsoleState $text
    Log "$c answered ($st): $($text -replace "`r", '')"
    if ($st -eq 'stock') {
        $try.Close()
        Stop-Script ("The radio is running the stock Xiegu firmware (no SD card?). Nothing was sent. " +
                     "Put the JS8 SD card in, power the radio on, wait for the main screen, run this again.")
    }
    if ($st -in @('shell', 'login', 'password')) { $sp = $try; $state = $st; $greeting = $text; break }
    $try.Close()
}
if (-not $sp) {
    Stop-Script ("No console answered on $($candidates -join ', '). Wait until the radio shows its main " +
                 "screen (about a minute after power on) and run this again, or pass -Port COMn.")
}
Say "Console on $($sp.PortName)" 'Green'

# ---------------------------------------------------------------------------
# Logging in (root / 123, as on every R1CBU card)
# ---------------------------------------------------------------------------
if ($state -eq 'password') {
    # A login half done: an empty password fails it and brings back login:
    $sp.Write("`r")
    $text = Read-For $sp 8000 'login:\s*$'
    $state = Get-ConsoleState $text
}
if ($state -eq 'login') {
    $sp.Write("root`r")
    $text = Read-For $sp 5000 'assword:\s*$'
    if ((Get-ConsoleState $text) -eq 'password') {
        $sp.Write("123`r")
    }
    $text = Read-For $sp 6000 '#\s*$'
    if ((Get-ConsoleState $text) -ne 'shell') {
        Log $text
        $sp.Close()
        Stop-Script "Couldn't log in to the radio (root / 123). The console said: $($text.Trim())"
    }
}
Say "Logged in" 'Green'

# ---------------------------------------------------------------------------
# Running commands
# ---------------------------------------------------------------------------
# Each command ends with an end mark printed by the radio. The mark is
# written with quotes on our side so the echoed command line doesn't match.
$script:seq = 0
function Invoke-Radio([string]$cmd, [int]$timeoutMs = 20000) {
    $script:seq++
    $mark = "END$($script:seq)X"
    [void](Read-For $sp 100)
    $sp.Write("$cmd; echo '__EN''D_$mark'`r")
    $text = Read-For $sp $timeoutMs "__END_$mark"
    $text = ($text -replace "`r", '') -replace "\x1b\[[0-9;?]*[A-Za-z]", ''
    $lines = @($text -split "`n")
    # drop the echoed command (up to the line with our quoted mark) and the end mark
    $echo = [regex]::Escape("__EN''D_$mark")
    $start = 0
    for ($i = 0; $i -lt $lines.Count; $i++) { if ($lines[$i] -match $echo) { $start = $i + 1; break } }
    $body = @($lines | Select-Object -Skip $start | Where-Object { $_ -notmatch "__END_$mark" -and $_ -notmatch '^\[root@.*\]#\s*$' })
    if ($text -notmatch "__END_$mark") { $body += "(timed out after $($timeoutMs / 1000) s)" }
    return ($body -join "`n").TrimEnd()
}

function Section([string]$title, [string]$cmd, [int]$timeoutMs = 20000) {
    Write-Host "  $title"
    Log ""
    Log "=== $title"
    Log ('$ ' + $cmd)
    $r = Invoke-Radio $cmd $timeoutMs
    Log $r
    return $r
}

Say "Collecting (about a minute) ..."

Section 'Radio clock, uptime, kernel'    'date -u; uptime; uname -a' | Out-Null
Section 'Firmware'                       'cat /etc/os-release; ls -l /usr/sbin/x6100_gui; md5sum /usr/sbin/x6100_gui' | Out-Null
$js8 = Section 'JS8 app in this firmware'  "grep -c 'JS8 window' /usr/sbin/x6100_gui"
Section 'GUI running'                    'ps | grep -v grep | grep x6100' | Out-Null
$base = Section 'BASE version (from the app log)' "grep -i -m 3 'version' /tmp/x6100_log.txt"
Section 'SD card and DATA'               'mount | grep -E "mmc|/mnt"; df -h /mnt; ls -la /mnt; ls -la /mnt/app_logs 2>/dev/null | tail -n 5' | Out-Null
Section 'Audio mixer'                    'amixer 2>&1 | head -n 80' 30000 | Out-Null

# Settings: read-only queries of params.db. Values are stored scaled:
# pwr = W x 10, output_gain (TX codec gain) = dB x 5, dac_offset = dB x 10.
$ro = "sqlite3 -readonly /mnt/params.db 'select 1' >/dev/null 2>&1 && RO=-readonly || RO=; sqlite3 `$RO -list -noheader /mnt/params.db"
$keyNames = "'pwr','output_gain','ft8_output_gain_offset','tx_filter_low','tx_filter_high','cessb_on','cessb_power_up','mic','line_in','line_out','key_mode','vox_en','band'"
$keys = Section 'Transmit settings' "$ro `"select name||'='||val from params where name in ($keyNames) order by name`""
$bands = Section 'Band output gain corrections (dac_offset)' "$ro `"select b.name||'|'||b.start_freq||'|'||b.stop_freq||'|'||p.val from band_params p join bands b on b.id=p.bands_id where p.name='dac_offset' order by b.start_freq`""
Section 'All settings'                   "$ro `"select name||'='||val from params order by name`"" 30000 | Out-Null
Section 'Mode settings'                  "$ro `"select mode||' '||name||'='||val from mode_params order by mode,name`"" 30000 | Out-Null
Section 'Database version'               "$ro `"select id from version`"" | Out-Null
Section 'App log (start)'                'head -n 80 /tmp/x6100_log.txt' 30000 | Out-Null
Section 'App log (latest)'               'tail -n 120 /tmp/x6100_log.txt' 30000 | Out-Null
Section 'Kernel messages (latest)'       'dmesg | tail -n 60' 30000 | Out-Null

# ---------------------------------------------------------------------------
# A readable summary
# ---------------------------------------------------------------------------
$vals = @{}
foreach ($l in ($keys -split "`n")) {
    if ($l -match '^([a-z0-9_]+)=(.*)$') { $vals[$Matches[1]] = $Matches[2].Trim() }
}
function Num([string]$k) {
    if ($vals.ContainsKey($k)) { return [double]::Parse($vals[$k], [Globalization.CultureInfo]::InvariantCulture) }
    return $null
}

$summary = @()
$summary += "Firmware with JS8: " + $(if ($js8.Trim() -match '^[1-9]') { 'yes' } else { "no ($($js8.Trim()))" })
$summary += "BASE: " + (($base -split "`n" | Select-Object -First 1).Trim())
$p = Num 'pwr';         if ($null -ne $p) { $summary += ('TX power setting: {0:0.0} W' -f ($p / 10)) } else { $summary += 'TX power setting: not saved (default 5.0 W)' }
$g = Num 'output_gain'; if ($null -ne $g) { $summary += ('TX codec gain: {0:+0.0;-0.0;0} dB' -f ($g / 5)) } else { $summary += 'TX codec gain: not saved (default 0 dB)' }
$f = Num 'ft8_output_gain_offset'; if ($null -ne $f) { $summary += ('FT8/JS8 learned drive: {0:+0.0;-0.0;0} dB' -f $f) } else { $summary += 'FT8/JS8 learned drive: none yet' }
if ($vals.ContainsKey('tx_filter_low')) { $summary += "TX filter: $($vals['tx_filter_low'])-$($vals['tx_filter_high']) Hz" }
if ($vals.ContainsKey('cessb_on'))      { $summary += "CESSB: $($vals['cessb_on'])" }
$bandLines = @()
foreach ($l in ($bands -split "`n")) {
    $x = $l.Split('|')
    if ($x.Count -eq 4 -and $x[3] -match '^-?\d+$' -and [int]$x[3] -ne 0) {
        $bandLines += ('  {0}: {1:+0.0;-0.0} dB' -f $x[0], ([int]$x[3] / 10))
    }
}
if ($bandLines.Count) { $summary += 'Band output gain corrections:'; $summary += $bandLines }
else { $summary += 'Band output gain corrections: all 0' }

Log ""
Log "=== SUMMARY"
foreach ($s in $summary) { Log $s }
Write-Host ""
Write-Host "Summary:" -ForegroundColor Cyan
foreach ($s in $summary) { Write-Host "  $s" }

# ---------------------------------------------------------------------------
# Transmit tests: YOU transmit, the script only records the app log
# ---------------------------------------------------------------------------
function Watch-Log([string]$title, [int]$seconds) {
    Log ""
    Log "=== $($title): app log while transmitting ($seconds s)"
    [void](Read-For $sp 100)
    $sp.Write("tail -n 0 -f /tmp/x6100_log.txt`r")
    $sb = New-Object System.Text.StringBuilder
    for ($i = $seconds; $i -gt 0; $i--) {
        Write-Host -NoNewline ("`r  recording... {0,3} s left " -f $i)
        [void]$sb.Append((Read-For $sp 1000))
    }
    Write-Host ""
    $sp.Write([string][char]3)           # Ctrl-C ends tail
    [void]$sb.Append((Read-For $sp 1500 '#\s*$'))
    Log (($sb.ToString() -replace "`r", '') -replace "\x1b\[[0-9;?]*[A-Za-z]", '')
}

function Ask-Meter([string]$what) {
    $w = Read-Host "  What did the radio's power meter show during $($what)? (watts, or 0, or 'none')"
    $a = Read-Host "  Did the ALC bar move? (y/n)"
    Log "$what : meter '$w', ALC moved '$a'"
}

if (-not $SkipTx) {
    Write-Host ""
    Write-Host "Transmit tests" -ForegroundColor Cyan
    Write-Host "  Use a dummy load (or a clear frequency on your antenna), and put any"
    Write-Host "  amplifier on STANDBY. You do the transmitting; this only records."
    $go = Read-Host "  Do the transmit tests now? (y/n)"
    Log "transmit tests: '$go'"
    if ($go -match '^[yY]') {
        Write-Host ""
        Write-Host "1) TUNE: after you press Enter, press TUNE on the radio for about 5 seconds." -ForegroundColor Yellow
        Read-Host "   Press Enter when ready" | Out-Null
        Watch-Log 'TUNE' 20
        Ask-Meter 'TUNE'

        Write-Host ""
        Write-Host "2) SSB: after Enter, key the mic (PTT) in USB or LSB and talk or whistle for 5 s." -ForegroundColor Yellow
        Read-Host "   Press Enter when ready" | Out-Null
        Watch-Log 'SSB' 20
        Ask-Meter 'SSB'

        Write-Host ""
        Write-Host "3) JS8: open JS8 on the radio first. After Enter, press CQ (page 1)." -ForegroundColor Yellow
        Write-Host "   It waits for the next 15 s slot and sends for about 13 s."
        Read-Host "   Press Enter when JS8 is open and you're ready" | Out-Null
        Watch-Log 'JS8 CQ' 45
        Ask-Meter 'JS8 CQ'

        # the learned drive level after the JS8 transmission
        Section 'FT8/JS8 learned drive after the test' "$ro `"select name||'='||val from params where name in ('ft8_output_gain_offset','pwr')`"" | Out-Null
    }
}

$sp.Close()

$note = Read-Host "Anything else to add (what you see, which modes work)? Press Enter to skip"
if ($note) { Log ""; Log "=== Notes from the operator"; Log $note }

Write-Host ""
Write-Host "Done. Please send this file back:" -ForegroundColor Green
Write-Host "  $OutFile" -ForegroundColor Green
Write-Host "Nothing was changed on the radio."
try { Start-Process explorer.exe "/select,`"$OutFile`"" } catch { }
Read-Host "Press Enter to close" | Out-Null
