param([string]$ObsVersion = '33.0.1')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$deps = Join-Path $root '.deps'
New-Item -ItemType Directory -Force $deps | Out-Null
function Download([string]$Url, [string]$Path, [string]$Hash = '') {
    if (!(Test-Path -LiteralPath $Path)) {
        Write-Host "Downloading $(Split-Path $Path -Leaf)"
        Invoke-WebRequest -Uri $Url -OutFile $Path
    }
    if ($Hash -and (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ne $Hash) {
        throw "SHA256 mismatch: $Path"
    }
}
$source = Join-Path $deps "obs-studio-$ObsVersion-sources"
if (!(Test-Path "$source/libobs/obs-module.h")) {
    Download "https://github.com/obsproject/obs-studio/releases/download/$ObsVersion/OBS-Studio-$ObsVersion-Sources.tar.gz" "$deps/obs-sources.tar.gz"
    & tar -xzf "$deps/obs-sources.tar.gz" -C $deps --exclude='*/build-aux/*'
    if ($LASTEXITCODE) { throw 'OBS header extraction failed' }
}
$presets = Get-Content "$source/CMakePresets.json" -Raw | ConvertFrom-Json
$spec = ($presets.configurePresets | Where-Object name -eq 'dependencies').vendor.'obsproject.com/obs-studio'.dependencies.qt6
$qtZip = Join-Path $deps "qt-$($spec.version).zip"
Download "$($spec.baseUrl)/$($spec.version)/windows-deps-qt6-$($spec.version)-x64.zip" $qtZip $spec.hashes.'windows-x64'
if (!(Test-Path "$deps/qt/include/QtCore/qglobal.h")) {
    Expand-Archive -LiteralPath $qtZip -DestinationPath "$deps/qt" -Force
}
if (!(Test-Path "$deps/msvc/.ready")) {
    Download 'https://aka.ms/vs/17/release/channel' "$deps/vs-channel.json"
    $channel = Get-Content "$deps/vs-channel.json" -Raw | ConvertFrom-Json
    $catalog = $channel.channelItems | Where-Object id -eq 'Microsoft.VisualStudio.Manifests.VisualStudio'
    Download $catalog.payloads[0].url "$deps/vs-catalog.json"
    $packages = (Get-Content "$deps/vs-catalog.json" -Raw | ConvertFrom-Json).packages
    $ids = @('Microsoft.VC.14.44.17.14.Tools.HostX64.TargetX64.base', 'Microsoft.VC.14.44.17.14.CRT.Headers.base', 'Microsoft.VC.14.44.17.14.CRT.x64.Desktop.base', 'Microsoft.VC.14.44.17.14.CRT.x64.Store.base', 'Microsoft.VC.14.44.17.14.Tools.HostX64.TargetX64.Res.base')
    foreach ($id in $ids) {
        $package = $packages | Where-Object { $_.id -eq $id -and (!$_.language -or $_.language -eq 'en-US') } | Select-Object -First 1
        if (!$package) { throw "Missing compiler package: $id" }
        foreach ($payload in $package.payloads) {
            $archive = Join-Path $deps "$id.zip"
            Download $payload.url $archive $payload.sha256
            Expand-Archive -LiteralPath $archive -DestinationPath "$deps/msvc" -Force
        }
    }
    New-Item -ItemType File -Path "$deps/msvc/.ready" -Force | Out-Null
}
Write-Host 'Build dependencies ready.'
