param(
 [string]$SourceDirectory,
 [string]$BuildDirectory,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [string]$RuntimeDirectory,
 [ValidatePattern('^[0-9a-fA-F]{40}$')][string]$SourceCommit
)
$ErrorActionPreference='Stop'
if(-not $SourceDirectory){$SourceDirectory=Join-Path $PSScriptRoot '../..'}
$SourceDirectory=(Resolve-Path $SourceDirectory).Path
if(-not $BuildDirectory){$BuildDirectory=Join-Path $SourceDirectory 'build/Release'}
$BuildDirectory=(Resolve-Path $BuildDirectory).Path
$version=([IO.File]::ReadAllText((Join-Path $SourceDirectory 'VERSION'))).Trim()
if($version -notmatch '^\d+\.\d+\.\d+([-.][0-9A-Za-z.-]+)?$'){throw 'Invalid VERSION'}
if(-not $RuntimeDirectory){
 $vswhere=Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio/Installer/vswhere.exe'
 $vs=(& $vswhere -latest -products '*' -property installationPath).Trim()
 if($LASTEXITCODE -ne 0 -or -not $vs){throw 'Set RuntimeDirectory to the matching non-preview x64 VC Redist CRT directory'}
 $tools=([IO.File]::ReadAllText((Join-Path $vs 'VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt'))).Trim()
 $RuntimeDirectory=Join-Path $vs ('VC/Redist/MSVC/'+$tools+'/x64/Microsoft.VC145.CRT')
}
$RuntimeDirectory=(Resolve-Path $RuntimeDirectory).Path
[void][IO.Directory]::CreateDirectory($OutputDirectory)
$OutputDirectory=(Resolve-Path $OutputDirectory).Path
$base='friendpushscreen-v'+$version
$stage=Join-Path $OutputDirectory $base
$zip=Join-Path $OutputDirectory ($base+'-portable.zip')
if((Test-Path $stage) -or (Test-Path $zip)){throw 'Output already exists; use a fresh output directory'}
[void][IO.Directory]::CreateDirectory($stage)
[void][IO.Directory]::CreateDirectory((Join-Path $stage 'licenses'))
foreach($name in @('FriendPushScreen.exe','msquic.dll')){Copy-Item (Join-Path $BuildDirectory $name) (Join-Path $stage $name)}
$expectedQuic='290B6C8716AC09EEA043913F86164E0B2F5096C666C9F1CA81C0C5C1864E1CB0'
if((Get-FileHash (Join-Path $stage 'msquic.dll')).Hash -ne $expectedQuic){throw 'Pinned MsQuic DLL mismatch'}
$runtimes=@('msvcp140.dll','msvcp140_atomic_wait.dll','vcruntime140.dll','vcruntime140_1.dll')
foreach($name in $runtimes){
 $path=Join-Path $RuntimeDirectory $name
 $info=(Get-Item $path).VersionInfo
 if($info.IsDebug -or $info.IsPreRelease -or $info.FileVersion -match '(?i)preview|pre-release' -or (Get-AuthenticodeSignature $path).Status -ne 'Valid'){throw ('Unusable redistributable runtime: '+$name)}
 # The qualified release uses this stable CRT set, independent of a newer installed preview toolchain.
 if($info.FileVersion -ne '14.51.36247.0'){throw ('Unqualified CRT version: '+$info.FileVersion)}
 Copy-Item $path (Join-Path $stage $name)
}
Copy-Item (Join-Path $SourceDirectory 'LICENSE') (Join-Path $stage 'LICENSE')
Copy-Item (Join-Path $SourceDirectory 'VERSION') (Join-Path $stage 'VERSION')
Copy-Item (Join-Path $SourceDirectory 'docs/USER_GUIDE_TH.md') (Join-Path $stage 'README-TH.md')
foreach($relative in @('third-party/opus-notice.txt','third_party/notices/msquic-2.6.3-LICENSE.txt','third_party/notices/msquic-2.6.3-THIRD-PARTY-NOTICES.txt','third_party/notices/microsoft-vc-runtime.md')){
 Copy-Item (Join-Path $SourceDirectory $relative) (Join-Path $stage ('licenses/'+[IO.Path]::GetFileName($relative)))
}
[IO.File]::WriteAllBytes((Join-Path $stage 'portable.flag'),[byte[]]@())
$files=@(Get-ChildItem $stage -Recurse -File|Sort-Object FullName|ForEach-Object{
 [pscustomobject]@{path=$_.FullName.Substring($stage.Length+1).Replace('\','/');bytes=$_.Length;sha256=(Get-FileHash $_.FullName).Hash.ToLowerInvariant()}
})
$state=if($SourceCommit){'exact-commit'}else{'qualification-only-uncommitted-metadata'}
$manifest=[ordered]@{schema=1;project='FriendPushScreen';version=$version;architecture='x64';source_commit=$SourceCommit;source_state=$state;msquic_version='2.6.3 OpenSSL';opus_version='1.6.1 static';vc_runtime_version='14.51.36247.0';files=$files}
[IO.File]::WriteAllText((Join-Path $stage 'package-manifest.json'),($manifest|ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive=[IO.Compression.ZipFile]::OpenRead($zip)
try{
 $entries=@($archive.Entries)
 if(-not ($entries|Where-Object{$_.FullName -eq 'portable.flag' -and $_.Length -eq 0})){throw 'portable.flag missing from ZIP'}
 if($entries|Where-Object{$_.FullName -match '(^|[\\/])(data|\.git|build)([\\/]|$)|\.(dpapi|friendpair|pfx|pdb)$'}){throw 'Unexpected private or development files in ZIP'}
 foreach($file in $files){
  $entry=$entries|Where-Object{$_.FullName.Replace('\','/') -eq $file.path}
  if(@($entry).Count -ne 1 -or $entry.Length -ne $file.bytes){throw ('ZIP entry mismatch: '+$file.path)}
 }
}finally{$archive.Dispose()}
[pscustomobject]@{version=$version;source_commit=$SourceCommit;stage=$stage;archive=$zip;bytes=(Get-Item $zip).Length;sha256=(Get-FileHash $zip).Hash.ToLowerInvariant();file_count=$files.Count+1}|ConvertTo-Json -Compress
