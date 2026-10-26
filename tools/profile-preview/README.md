# Profile composition preview

Isolated QML design fixture of the selected **A** composition for
[the profile and Online contract](../../docs/PLAYER_PROFILE.md).
It uses the existing Theme, AvatarPreview3D and shared avatar pack. All profile
statistics, levels, sparks and awards are sample data, labelled on screen. No account, library or wallet is read or
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

Controls: `T` changes theme; `D` cycles illustrative account states (fresh example,
unavailable, cached/offline, signed out); arrow keys or dragging rotate the avatar;
small swatches change the backdrop; Escape closes the preview.
The capture run writes seven `profile-a-*.png` frames: 1280×720 (light/dark),
800×450, 1280×800, unavailable, offline and signed-out states.
The sample wallet is 1500 eligible base sparks minus 250 spent = 1250 available;
an illustrative ×1→×3 top-up adds 3000 = 4250 prospective. This is not a reward
schedule or an actual supporter tier. Level 12 and the progress bar are also
illustrative, not a selected XP formula. Runtime must use verified server data.
Recent RA awards and Player Journey are content blocks linking to full views,
not profile tabs. Original A/B alternatives remain in commit `40e423fc`.
Generated images stay outside source control. This is not the production shell,
device performance evidence, or a functional profile/navigation acceptance test.
