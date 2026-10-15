# FriendPushScreen Branding Assets

FriendPushScreen uses one canonical application icon: a monitor rendered with layered oak / bronze / amber gradients on a transparent background.

## Canonical files

- `assets/branding/FriendPushScreen-icon.png` — 1024x1024 branding master generated for documentation / previews.
- `resources/FriendPushScreen.ico` — Windows multi-size application icon.
- `resources/FriendPushScreen.rc` — Win32 resource binding.
- `resources/resource.h` — stable resource ID declaration.
- `tools/branding/generate_icon.py` — deterministic source generator for the PNG and ICO.

The ICO contains Windows icon sizes:

- 16x16
- 24x24
- 32x32
- 48x48
- 64x64
- 128x128
- 256x256

## Regeneration

Developer prerequisite:

```powershell
python -m pip install pillow
```

Regenerate both branding outputs:

```powershell
python tools\branding\generate_icon.py
```

Do not manually maintain a second application icon. The executable, title bar, shortcuts and the future FPS-015 installer should reuse `resources/FriendPushScreen.ico`.

The generator is a development tool only. Pillow is not a FriendPushScreen runtime dependency.
