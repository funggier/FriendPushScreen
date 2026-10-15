param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [string]$OutputRoot = "",

    [ValidateRange(1, 65534)]
    [int]$BaseSharePort = 42800
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Exe = Join-Path $RepoRoot "build\$Configuration\FriendPushScreen.exe"

if (-not (Test-Path $Exe)) {
    throw "FriendPushScreen.exe not found: $Exe"
}

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $RepoRoot ".superpowers\dual-portable"
}

$A = Join-Path $OutputRoot "FriendPushScreen-A"
$B = Join-Path $OutputRoot "FriendPushScreen-B"

foreach ($Dir in @($A, $B)) {
    New-Item -ItemType Directory -Force -Path $Dir | Out-Null
    Copy-Item -Force $Exe (Join-Path $Dir "FriendPushScreen.exe")
    Set-Content -Encoding ASCII -NoNewline -Path (Join-Path $Dir "portable.flag") -Value "portable"
    New-Item -ItemType Directory -Force -Path (Join-Path $Dir "data") | Out-Null
}

$PortA = $BaseSharePort
$PortB = $BaseSharePort + 1

$SettingsA = @"
version=1
settings_visible=true
borderless=false
always_on_top=false
connection_mode=direct_ip_ddns
host=127.0.0.1
share_port=$PortA
remote_port=$PortB
show_cursor=true
audio_source=none
receiver_muted=false
receiver_volume=100
"@

$SettingsB = @"
version=1
settings_visible=true
borderless=false
always_on_top=false
connection_mode=direct_ip_ddns
host=127.0.0.1
share_port=$PortB
remote_port=$PortA
show_cursor=true
audio_source=none
receiver_muted=false
receiver_volume=100
"@

[IO.File]::WriteAllText((Join-Path $A "data\settings.ini"), $SettingsA, [Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText((Join-Path $B "data\settings.ini"), $SettingsB, [Text.UTF8Encoding]::new($false))

Write-Host "Prepared dual portable instances:"
Write-Host "A: $A  Share=$PortA  Remote=$PortB"
Write-Host "B: $B  Share=$PortB  Remote=$PortA"
Write-Host ""
Write-Host "Both use 127.0.0.1 and independent data/settings/log folders."
