# Building FriendPushScreen

## Supported development baseline

FriendPushScreen V1 is a Windows x64 native C++20 application.

Validated local baseline:

- Windows 10 Pro 22H2 / build 19045
- Visual Studio Community 2026 18.10.x
- MSVC x64
- CMake 4.4.x
- Windows SDK 10.0.19041.0 or a newer compatible Windows SDK

The Windows SDK installation must include Desktop C++ headers and x64 libraries. At minimum, verify:

```text
Windows.h
d3d11.h
mfapi.h
User32.lib
d3d11.lib
mfplat.lib
```

## Configure

From the repository root:

```powershell
cmake -S . -B build -A x64
```

## Debug build

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Test executable:

```text
build\Debug\FriendPushScreenTests.exe
```

## Release build

```powershell
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The Release executable is build\Release\FriendPushScreen.exe. The pinned MsQuic/OpenSSL DLL is copied beside it; Opus is statically linked. VERSION and the CMake project both identify1.0.0.

## Clean build verification

Generated output lives under `build\`, which is intentionally git-ignored. To validate a clean local build, remove only that generated directory and re-run configure/build/test. Do not reset or clean the Git worktree.

## Portable release packaging

Run tools/release/package-portable.ps1 with an explicit fresh OutputDirectory after the Release build. It includes portable.flag, a Thai guide, notices, the pinned MsQuic DLL and the qualified signed x64 VC runtime14.51.36247.0. For publication pass SourceCommit and stage SourceDirectory from the immutable tagged tree; build source/document ZIPs with git archive from that same tag. See docs/releases/v1.0.0.md for required CI and post-publication checks. The packaging script rejects an existing output, mismatched MsQuic and unqualified/debug CRT files.
