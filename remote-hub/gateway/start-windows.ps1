param([switch]$EnableAudio)
# Run from PowerShell on the Windows computer with iPhone USB Internet and CareRover Wi-Fi.
# This opens only a loopback SSH forward; it never changes either network adapter.
$ErrorActionPreference = 'Stop'
$sshKey = Join-Path $env:USERPROFILE '.ssh\carerover_remote_v2'
$server = 'root@221.194.149.100'
$relayPort = 18088
if (-not (Test-Path -LiteralPath $sshKey)) { throw 'CareRover SSH key not found.' }
if (Get-NetTCPConnection -LocalAddress '127.0.0.1' -LocalPort $relayPort -State Listen -ErrorAction SilentlyContinue) {
  throw 'Local relay port 18088 is already in use; refusing to attach to an unknown service.'
}
$sshOptions = @('-o','BatchMode=yes','-o','StrictHostKeyChecking=yes','-o','ConnectTimeout=8','-i',$sshKey)
$tokenLine = & ssh.exe @sshOptions $server 'sed -n "s/^AUDIO_DEVICE_TOKEN=//p" /etc/carerover/remote-hub.env'
if ($LASTEXITCODE -ne 0 -or $tokenLine -notmatch '^[A-Za-z0-9._~-]{24,256}$') {
  throw 'Unable to obtain the device token over the pinned SSH connection.'
}
$env:CAREROVER_DEVICE_TOKEN = $tokenLine
if ($EnableAudio) { $env:CAREROVER_AUDIO_GATEWAY = '1' }
$tokenLine = $null
$tunnel = $null
try {
  $args = @('-N','-o','BatchMode=yes','-o','StrictHostKeyChecking=yes',
    '-o','ExitOnForwardFailure=yes','-o','ServerAliveInterval=15',
    '-L','127.0.0.1:18088:127.0.0.1:8088','-i',$sshKey,$server)
  $tunnel = Start-Process -FilePath 'ssh.exe' -ArgumentList $args -PassThru -WindowStyle Hidden
  Start-Sleep -Seconds 2
  if ($tunnel.HasExited) { throw 'SSH loopback tunnel exited immediately.' }
  $health = Invoke-RestMethod -Uri 'http://127.0.0.1:18088/health' -TimeoutSec 5
  if ($null -eq $health.audioPaired -or $null -eq $health.controlDevice) {
    throw 'The loopback endpoint did not return the expected CareRover health shape.'
  }
  Write-Host 'CareRover relay tunnel verified. Starting Windows video/control gateway.'
  & node.exe (Join-Path $PSScriptRoot 'gateway.mjs')
} finally {
  Remove-Item Env:CAREROVER_DEVICE_TOKEN -ErrorAction SilentlyContinue
  Remove-Item Env:CAREROVER_AUDIO_GATEWAY -ErrorAction SilentlyContinue
  if ($tunnel -and -not $tunnel.HasExited) { Stop-Process -Id $tunnel.Id -ErrorAction SilentlyContinue }
}
