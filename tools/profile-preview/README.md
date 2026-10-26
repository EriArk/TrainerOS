# Profile composition preview

Isolated QML design fixture for [the profile and Online contract](../../docs/PLAYER_PROFILE.md).
It uses the existing Theme, AvatarPreview3D and shared avatar pack. All profile
statistics and awards are sample data. No account, library or wallet is read or
modified. Navigation labels are illustrative, not implemented application routes.

On Windows with Qt Quick / Qt Quick 3D (the existing UCRT64 Qt environment):

```powershell
./tools/profile-preview/render.ps1
./tools/profile-preview/render.ps1 -Interactive
```

The script stages dependencies under ignored `build/profile-preview/` and resolves
the current shared avatar using an existing `build/avatar-resolve.exe`. Alternatively
pass `-AvatarPath` with an already resolved preview GLB. Override `-QtBin` if needed.
The visual preview opens a window, including during capture. Rendering requires
a working desktop graphics session (D3D11); offscreen QPA does
not prove that the 3D avatar was captured. Do not set `QT_QPA_PLATFORM=offscreen`.

Controls: `1` / `2` selects card / scene; `T` changes theme; arrow keys or dragging
rotate the avatar; small swatches change the backdrop; Escape closes the preview.
The capture run writes both variants at 1280×720 (light/dark), 800×450 and 1280×800.
Generated images stay outside source control. This is not the production shell,
device performance evidence, or a functional profile/navigation acceptance test.
