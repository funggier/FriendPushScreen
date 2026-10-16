# Native secure transport foundation — 2026-10-16

Scope: isolated native transport qualification on CDQ-P, Windows 10 19045 x64. This is not application video or WAN acceptance.

- Official Microsoft.Native.Quic.MsQuic.OpenSSL 2.6.3 archive: SHA256 41505C3861EF386C8788DD6D9E0FFEA75B275B7D0651C2B940628AB729CF27BD.
- Loaded x64 msquic.dll: SHA256 290B6C8716AC09EEA043913F86164E0B2F5096C666C9F1CA81C0C5C1864E1CB0; Microsoft Authenticode Valid.
- Native runtime reports MsQuic 2.6.3.160436875 / OpenSSL / TLS 1.3. Dependency notices retained in third_party/notices.
- Normal certificate validation plus synchronous full-DER SHA256 comparison, mutually supplied credentials, required client authentication. No validation bypass, 0-RTT, OS certificate installation, firewall or registry change.
- Windows CNG RSA-2048/SHA256 identity generated ephemerally; persisted private PKCS#12 is DPAPI current-user protected with explicit user/SYSTEM ACL. Public pairing format excludes private material.
- Identity RED confirmed in stub, then restart regression confirmed. Root cause: certificate store copy omits ephemeral key context. Linked store retains key for PFX export. Release identity regression passed in proc-1792119433742-200.
- Transport RED confirmed missing listener; actual mutual exchange GREEN in proc-1792119853353-203.
- Negative tests proc-1792119970145-204: unknown client rejected before any payload, duplicate port rejected, self-endpoint rejected, loopback restriction, oversized messages, bounded partial/in-flight queues, stalled receiver and three reconnects passed.
- Full Release and Debug builds and CTest: both 2/2 passed, proc-1792120093627-207, completed 2026-10-16T03:09:33Z.

## Independent OS processes

Two copied executables and runtime DLLs, separate folders/identity roots:
A PID 16580, listener 42811; B PID 11184, listener 42812. OS UDP ownership captured while both ran. Both exited 0.

A sent/received 31,065,960 bytes; 473 complete verified media replies.
B received 474 media and control messages; last echo coincided with A shutdown.
Payload 65,536 bytes, SHA256 D0FB80B239A23260482AFA5BC8360BCABE5604B5F50A93078C6E5C99A0B0E53A on both.
Peak pending media count 1 and bytes 65,536 in each direction; zero drops.
Public fingerprints differ. Raw reports and UDP ownership are adjacent evidence files.

## Limits and remaining qualification

MsQuic remains a provisional development candidate. This proves loopback authenticated bytes, not H.264 decode/render, audio, non-loopback routes or final DG-001 selection. Missing-client credential, expired/wrong-SAN fixtures, malformed protocol, native callback fault coverage and comparison with encrypted TCP remain for the final security/transport qualification. No permanent backend or full-V1 claim is made.

The first probe exposed a blocking cleanup-order defect: RegistrationClose waits for child configurations. The probe was stopped by owned PID, and cleanup now closes listener/configurations before RegistrationClose; repeat self-tests and process exits pass.
