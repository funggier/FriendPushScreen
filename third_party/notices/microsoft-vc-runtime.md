# Microsoft Visual C++ runtime in the portable package

The portable package contains these unmodified signed x64 release runtime DLLs from the installed Visual Studio Community 2026 Redistributable directory, version 14.51.36247.0:

- msvcp140.dll
- msvcp140_atomic_wait.dll
- vcruntime140.dll
- vcruntime140_1.dll

These Microsoft files are not covered by the FriendPushScreen MIT License. Redistribution is subject to the applicable Microsoft Visual Studio license terms. No debug_nonredist or 14.52 preview toolchain files are packaged. Windows UCRT/system DLLs are provided by Windows.

Primary distribution references checked 2026-10-17:
- https://learn.microsoft.com/en-us/visualstudio/releases/2026/redistribution
- https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist
- https://visualstudio.microsoft.com/license-terms/

The package manifest records the exact DLL versions and SHA-256 digests. App-local deployment makes future updates to these bundled DLLs part of the application's update process.
