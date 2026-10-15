param(
 [ValidateSet('window','monitor','device','synthetic','hwenc-swdec','swenc-hwdec','software')][string]$Source='window',
 [ValidateRange(2,600)][int]$Seconds=180,
 [ValidateSet('Debug','Release')][string]$Configuration='Release',
 [switch]$Require1080p60,
 [switch]$ForceSoftware,
 [string]$EvidenceDirectory
)
$ErrorActionPreference='Stop'
$repo=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if(-not $EvidenceDirectory){$EvidenceDirectory=Join-Path $repo ('.superpowers/qualification-'+$Source+'-'+[guid]::NewGuid().ToString('N'))}
if(Test-Path -LiteralPath $EvidenceDirectory){throw 'Evidence directory already exists; choose a new path'}
New-Item -ItemType Directory -Path $EvidenceDirectory | Out-Null
$capture=$Source -in @('window','monitor','device')
if($capture){
 $exe=Join-Path $repo ('build/'+$Configuration+'/FriendPushScreenCaptureLoopProbe.exe')
 $probeArgs=@('--source',$Source,'--seconds',[string]$Seconds)
 if($Require1080p60){$probeArgs+='--require-1080p60'}
 if($ForceSoftware){$probeArgs+='--software'}
}else{
 $exe=Join-Path $repo ('build/'+$Configuration+'/FriendPushScreenCodecLoopProbe.exe')
 $probeArgs=@('--frames',[string]($Seconds*60))
 if($Source -eq 'software'){$probeArgs+='--software'}
 if($Source -eq 'hwenc-swdec'){$probeArgs+='--software-decode'}
 if($Source -eq 'swenc-hwdec'){$probeArgs+='--software-encode'}
}
if(-not (Test-Path -LiteralPath $exe)){throw ('Executable missing: '+$exe)}
$stdout=Join-Path $EvidenceDirectory 'stdout.log'
$stderr=Join-Path $EvidenceDirectory 'stderr.log'
$started=Get-Date
$process=Start-Process -FilePath $exe -ArgumentList $probeArgs -WorkingDirectory $repo -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
$probeProcessId=$process.Id
# Force opening/retaining the process handle before an early exit (Windows PowerShell).
$processHandle=$process.Handle
Write-Output ('EVIDENCE_DIR='+$EvidenceDirectory+' PROBE_PID='+$probeProcessId)
@{source=$Source;seconds=$Seconds;configuration=$Configuration;exe=$exe;args=$probeArgs;process_id=$probeProcessId;started_utc=$started.ToUniversalTime().ToString('o');git_head=(& git -C $repo rev-parse HEAD);git_status=@(& git -C $repo status --porcelain);binary_sha256=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash} | ConvertTo-Json -Depth 4 | Set-Content -Encoding utf8 (Join-Path $EvidenceDirectory 'identity.json')
$gpuSamples=@();$processSamples=@();$counterError=$null;$timedOut=$false
while(-not $process.HasExited){
 try{
  $live=Get-Process -Id $probeProcessId -ErrorAction Stop
  $processSamples+=[PSCustomObject]@{utc=(Get-Date).ToUniversalTime().ToString('o');cpu_seconds=$live.CPU;private_bytes=$live.PrivateMemorySize64;working_set_bytes=$live.WorkingSet64}
 }catch{}
 try{
  $rows=Get-CimInstance Win32_PerfFormattedData_GPUPerformanceCounters_GPUEngine -Filter ("Name LIKE 'pid_"+$probeProcessId+"_%'") -ErrorAction Stop
  foreach($row in $rows){$gpuSamples+=[PSCustomObject]@{utc=(Get-Date).ToUniversalTime().ToString('o');name=$row.Name;utilization_percent=$row.UtilizationPercentage}}
 }catch{$counterError=$_.Exception.Message}
 if(((Get-Date)-$started).TotalSeconds -gt $Seconds+45){$timedOut=$true;Stop-Process -Id $probeProcessId -ErrorAction SilentlyContinue;break}
 Start-Sleep -Seconds 1
 $process.Refresh()
}
$process.WaitForExit();$process.Refresh();$code=$process.ExitCode
if($null -eq $code){$code=99}
$lastLine=Get-Content -LiteralPath $stdout -Tail 1
$expectedMarker=if($capture){'CAPTURE_LOOP_PASS'}else{'SYNTHETIC_LOOP_PASS'}
if($code -eq 0 -and $lastLine -ne $expectedMarker){$code=98}
$processSamples | Export-Csv -NoTypeInformation -Encoding UTF8 (Join-Path $EvidenceDirectory 'process.csv')
$gpuSamples | Export-Csv -NoTypeInformation -Encoding UTF8 (Join-Path $EvidenceDirectory 'gpu-engines.csv')
$summary=@{
 exit_code=$code;timed_out=$timedOut;elapsed_seconds=((Get-Date)-$started).TotalSeconds;
 process_samples=$processSamples.Count;gpu_samples=$gpuSamples.Count;counter_error=$counterError;
 private_min=($processSamples | Measure-Object private_bytes -Minimum).Minimum;
 private_max=($processSamples | Measure-Object private_bytes -Maximum).Maximum;
 gpu_engines=@($gpuSamples | Group-Object name | ForEach-Object{
  [PSCustomObject]@{name=$_.Name;samples=$_.Count;average=($_.Group | Measure-Object utilization_percent -Average).Average;max=($_.Group | Measure-Object utilization_percent -Maximum).Maximum}
 })
}
$summary | ConvertTo-Json -Depth 6 | Set-Content -Encoding utf8 (Join-Path $EvidenceDirectory 'resources.json')
Get-Content -LiteralPath $stdout -Tail 3
if((Get-Item -LiteralPath $stderr).Length -gt 0){Get-Content -LiteralPath $stderr -Tail 8}
Write-Output ('QUALIFICATION_EXIT='+$code+' EVIDENCE_DIR='+$EvidenceDirectory)
exit $code
