param([string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$projectText = Get-Content -LiteralPath "$root/CMakeLists.txt" -Raw
$version = [regex]::Match($projectText, 'project\(obs-notifier VERSION ([0-9.]+)').Groups[1].Value
if (!$version) { throw 'Could not determine the plugin version from CMakeLists.txt' }
$toolsets = Join-Path $root '.deps/msvc/Contents/VC/Tools/MSVC'
if (Test-Path -LiteralPath $toolsets) {
    $vc = (Get-ChildItem -LiteralPath $toolsets -Directory | Sort-Object Name -Descending | Select-Object -First 1).FullName
    $sdkRoot = "${env:ProgramFiles(x86)}/Windows Kits/10"
    $sdk = (Get-ChildItem "$sdkRoot/Include" -Directory | Sort-Object Name -Descending | Select-Object -First 1).Name
    if (!$sdk) { throw 'A Windows 10/11 SDK is required.' }
    $env:PATH = "$vc/bin/Hostx64/x64;$sdkRoot/bin/$sdk/x64;$env:PATH"
    $env:INCLUDE = "$vc/include;$sdkRoot/Include/$sdk/ucrt;$sdkRoot/Include/$sdk/shared;$sdkRoot/Include/$sdk/um;$sdkRoot/Include/$sdk/winrt"
    $env:LIB = "$vc/lib/x64;$sdkRoot/Lib/$sdk/ucrt/x64;$sdkRoot/Lib/$sdk/um/x64"
}
& cmake -S $root -B "$root/build" -G 'NMake Makefiles' "-DCMAKE_BUILD_TYPE=$Configuration"
if ($LASTEXITCODE) { throw 'CMake configure failed' }
& cmake --build "$root/build"
if ($LASTEXITCODE) { throw 'Build failed' }
& cmake --install "$root/build" --prefix "$root/dist/obs-notifier"
if ($LASTEXITCODE) { throw 'Packaging failed' }
Copy-Item "$root/README.md", "$root/LICENSE" -Destination "$root/dist/obs-notifier" -Force
Compress-Archive -Path "$root/dist/obs-notifier/*" -DestinationPath "$root/dist/obs-notifier-$version-windows-x64.zip" -Force
Write-Host "Ready: $root/dist/obs-notifier-$version-windows-x64.zip"
