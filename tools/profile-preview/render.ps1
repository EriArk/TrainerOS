param(
    [string]$QtBin = (Join-Path $env:USERPROFILE 'msys64/ucrt64/bin'),
    [string]$AvatarPath,
    [switch]$Interactive
)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$stage = Join-Path $repo 'build/profile-preview'
New-Item -ItemType Directory -Force -Path $stage | Out-Null
if (-not $AvatarPath) {
    $resolver = Join-Path $repo 'build/avatar-resolve.exe'
    if (-not (Test-Path -LiteralPath $resolver)) { throw 'Pass -AvatarPath or build tools/avatar/Resolve.cpp against the current AvatarAssets implementation.' }
    $env:PATH = "$QtBin;$env:PATH"
    & $resolver (Join-Path $repo 'assets/avatar/packs/trainer-avatar-1') (Join-Path $stage 'avatar') (Join-Path $stage 'avatar.json') | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Could not resolve the shared avatar.' }
    $AvatarPath = (Get-Content (Join-Path $stage 'avatar.json') -Raw | ConvertFrom-Json).preview
}
if (-not $AvatarPath -or -not (Test-Path -LiteralPath $AvatarPath)) { throw 'Pass -AvatarPath for an already resolved shared Trainer avatar preview GLB.' }
foreach ($file in @('AvatarPreview3D.qml','MenuGlyph.qml')) {
    Copy-Item -LiteralPath (Join-Path $repo "src/qml/$file") -Destination (Join-Path $stage $file)
}
$assetUri = ([Uri](Join-Path $repo 'assets')).AbsoluteUri
$theme = [IO.File]::ReadAllText((Join-Path $repo 'src/qml/Theme.qml')).Replace('qrc:/fonts/',"$assetUri/fonts/")
$utf8 = New-Object Text.UTF8Encoding($false)
[IO.File]::WriteAllText((Join-Path $stage 'Theme.qml'),$theme,$utf8)
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Concepts.qml') -Destination (Join-Path $stage 'Concepts.qml')
[IO.File]::WriteAllText((Join-Path $stage 'qmldir'),"singleton Theme 1.0 Theme.qml`nsingleton PreviewConfig 1.0 PreviewConfig.qml`n",$utf8)
$avatarUri = ([Uri]([IO.Path]::GetFullPath($AvatarPath))).AbsoluteUri
$outputJson = ConvertTo-Json $stage.Replace('\','/') -Compress
$config = "pragma Singleton`nimport QtQuick`nQtObject { readonly property url avatar: `"$avatarUri`"; readonly property string assets: `"$assetUri`"; readonly property string output: $outputJson; readonly property bool capture: $((-not $Interactive).ToString().ToLowerInvariant()) }`n"
[IO.File]::WriteAllText((Join-Path $stage 'PreviewConfig.qml'),$config,$utf8)
$env:PATH = "$QtBin;$env:PATH"
$env:QT_FORCE_STDERR_LOGGING = '1'
$env:QSG_RHI_BACKEND = 'd3d11'
$env:QT_SCALE_FACTOR_ROUNDING_POLICY = 'Floor'
# This is the interactive visual preview itself. It must be exposed for Qt's
# render loop/3D captures; a hidden startup window never produces a frame.
$preview = Start-Process -FilePath (Join-Path $QtBin 'qml.exe') -ArgumentList ('"'+(Join-Path $stage 'Concepts.qml')+'"') -WindowStyle Normal -PassThru -RedirectStandardOutput (Join-Path $stage 'stdout.log') -RedirectStandardError (Join-Path $stage 'render.log')
if ($Interactive) { return }
$null = $preview.Handle # Retain the exit handle before Windows releases the process.
if (-not $preview.WaitForExit(180000)) { Stop-Process -Id $preview.Id; throw 'Preview exceeded its bounded capture time.' }
if ($preview.ExitCode -ne 0) { Get-Content (Join-Path $stage 'render.log'); throw "QML preview failed: $($preview.ExitCode)" }
Get-Content (Join-Path $stage 'render.log')
