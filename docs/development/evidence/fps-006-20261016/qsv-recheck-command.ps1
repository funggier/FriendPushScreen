$ErrorActionPreference='Stop'
$ffmpeg=(Get-Command ffmpeg).Source
$dir=Join-Path $PSScriptRoot ('hardware-recheck-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $dir | Out-Null
Write-Output ('EVIDENCE_DIR='+$dir)
& $ffmpeg -version 2>&1 | Out-File -Encoding utf8 (Join-Path $dir 'ffmpeg-version.txt')
$results=@()
foreach($codec in @('h264','hevc','av1')) {
    $file=Join-Path $dir ($codec+'.mkv')
    $encLog=Join-Path $dir ($codec+'-encode.log')
    $decLog=Join-Path $dir ($codec+'-decode.log')
    $ErrorActionPreference='Continue'
    & $ffmpeg -hide_banner -nostdin -f lavfi -i 'testsrc2=size=1920x1080:rate=60' -frames:v 120 -vf format=nv12 -c:v ($codec+'_qsv') -b:v 20M -an -y $file 2>&1 | Out-File -Encoding utf8 $encLog
    $enc=$LASTEXITCODE
    $dec=$null
    if($enc -eq 0) {
        & $ffmpeg -hide_banner -nostdin -hwaccel qsv -c:v ($codec+'_qsv') -i $file -frames:v 120 -f null NUL 2>&1 | Out-File -Encoding utf8 $decLog
        $dec=$LASTEXITCODE
    }
    $results+=[PSCustomObject]@{codec=$codec;encoder=($codec+'_qsv');decoder=($codec+'_qsv');frames=120;width=1920;height=1080;encode_exit=$enc;decode_exit=$dec;encode_log=$encLog;decode_log=$decLog}
    Write-Output ($results[-1] | ConvertTo-Json -Compress)
}
$results | ConvertTo-Json -Depth 4 | Set-Content -Encoding utf8 (Join-Path $dir 'results.json')
Write-Output ('COMPLETE='+$dir)
if(@($results | Where-Object {$_.encode_exit -ne 0 -or $_.decode_exit -ne 0}).Count -gt 0){exit 1}
exit 0
