# OBS Notifier

Native Windows notifications for OBS Studio, styled using the active OBS theme.

## Features

- Streaming and recording start/stop notifications.
- Replay buffer on/off and saved-clip notifications, with optional filenames.
- Virtual camera start/stop notifications.
- One checkbox to enable every notification.
- New notifications immediately replace the current message.
- Repeated start/stop shortcuts report an already-running or already-stopped output.
- Display-paced slide/fade animation with fractional-pixel movement.
- Optional quiet notification sound, embedded in the plugin.
- Configurable display, corner, duration, size, and edge spacing.
- Preview choices follow OBS's order: Streaming, Recording, Replay Buffer, Virtual Camera.

The plugin follows your existing OBS hotkey bindings. Shared start/stop bindings
retain their toggle behavior. Status feedback stays quiet while OBS dialogs are
open and respects the configured hotkey focus behavior.

## Requirements

- Windows x64.
- OBS Studio 33.x; built against OBS 33.0.1 and Qt 6.11.
- For compilation: CMake 3.28+, a Windows 10/11 SDK, and MSVC v143.

## Build

Run in PowerShell from the repository root:

```powershell
./scripts/prepare-deps.ps1
./scripts/build.ps1
```

The preparation script downloads official OBS headers, the matching Qt SDK, and
a local Microsoft compiler toolset into `.deps`. It verifies the Qt and Microsoft
package hashes against their official manifests. Microsoft toolchain license terms
apply. The Windows SDK must already be installed.

The DLL and release ZIP are written to `dist`. The sound asset is included; its
original synthesis source is `scripts/generate-chime.ps1`.

## Install

Close OBS and copy `obs-notifier.dll` into
`C:\Program Files\obs-studio\obs-plugins\64bit`. Restart OBS and choose
**Tools → OBS Notifier…** to enable notifications, adjust appearance, and preview.
Use the equivalent plugin directory for a portable OBS installation. OBS supplies
the Qt runtime; no additional Qt DLLs are needed. To uninstall, close OBS and remove
the plugin DLL.


Settings are saved as UTF-8 JSON under the active OBS configuration's
`plugin_config/obs-notifier/settings.json`.

Desktop notifications work over desktop and borderless/windowed applications.
Exclusive fullscreen applications may cover them. Display Capture can include
the popup; Game Capture captures only the game.

## License

GPL-2.0-or-later. See [LICENSE](LICENSE).
