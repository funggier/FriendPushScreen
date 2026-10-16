param([Parameter(Mandatory=$true)][string]$ProbePath,[Parameter(Mandatory=$true)][string]$WorkDirectory)
$ErrorActionPreference='Stop'
New-Item -ItemType Directory $WorkDirectory -Force|Out-Null
& $ProbePath --prepare $WorkDirectory
if($LASTEXITCODE -ne 0){throw 'Process-pair preparation failed'}
function Quoted([string]$value){return '"'+$value.Replace('"','\"')+'"'}
foreach($role in @('server','client')){$old=Join-Path $WorkDirectory ($role+'-result.txt');if(Test-Path $old){Remove-Item $old}}
$server=$null;$client=$null
try {
 $serverOut=Join-Path $WorkDirectory 'server.stdout';$serverErr=Join-Path $WorkDirectory 'server.stderr'
 $serverArgs=@('--process',(Quoted (Join-Path $WorkDirectory 'b')),(Quoted (Join-Path $WorkDirectory 'a.friendpair')),'0','0','4500',(Quoted (Join-Path $WorkDirectory 'server-result.txt')))
 $server=Start-Process $ProbePath -ArgumentList $serverArgs -RedirectStandardOutput $serverOut -RedirectStandardError $serverErr -PassThru
 [void]$server.Handle
 $deadline=[DateTime]::UtcNow.AddSeconds(5);$port=$null
 while([DateTime]::UtcNow -lt $deadline -and -not $port){
  if(Test-Path $serverOut){$stream=[IO.File]::Open($serverOut,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite);try{$reader=New-Object IO.StreamReader($stream);$ready=$reader.ReadToEnd()}finally{$stream.Dispose()};if($ready -match 'READY pid=\d+ port=(\d+)'){$port=$Matches[1]}}
  if(-not $port){Start-Sleep -Milliseconds 50}
 }
 if(-not $port){throw 'Separate server did not become ready'}
 $clientOut=Join-Path $WorkDirectory 'client.stdout';$clientErr=Join-Path $WorkDirectory 'client.stderr'
 $clientArgs=@('--process',(Quoted (Join-Path $WorkDirectory 'a')),(Quoted (Join-Path $WorkDirectory 'b.friendpair')),'0',$port,'3000',(Quoted (Join-Path $WorkDirectory 'client-result.txt')))
 $client=Start-Process $ProbePath -ArgumentList $clientArgs -RedirectStandardOutput $clientOut -RedirectStandardError $clientErr -PassThru
 [void]$client.Handle
 if(-not $client.WaitForExit(7000)){throw 'Separate client timed out'}
 if(-not $server.WaitForExit(7000)){throw 'Separate server shutdown timed out (possible callback/transport lock inversion)'}
 $client.Refresh();$server.Refresh();
 if($client.ExitCode -ne 0 -or $server.ExitCode -ne 0){throw 'Separate process returned a failing exit code'}
 foreach($role in @('client','server')){
  $path=Join-Path $WorkDirectory ($role+'-result.txt')
  if(-not(Test-Path $path)){throw ('Missing '+$role+' result')}
  $text=[IO.File]::ReadAllText($path)
  Write-Output ($role+': '+($text -replace '[\r\n]+',' '))
  if($text -notmatch '(?m)^result=PASS\r?$'){throw ('Separate '+$role+' exchange failed')}
 }
 Write-Output 'PASS SeparateProcessMediaAndShutdown'
}finally{
 foreach($p in @($client,$server)){if($null -ne $p -and -not $p.HasExited){Stop-Process -Id $p.Id -Force}}
}
exit 0
