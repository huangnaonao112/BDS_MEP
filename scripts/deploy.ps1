# 打包：把 Release 产物和 Qt 运行时 DLL 放到 dist/
#
# 用法：
#   pwsh -File scripts/deploy.ps1
param(
    [string]$QtBin = ''
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

if (-not $QtBin) {
    if (Test-Path 'C:\Qt\Qt5.14.1\5.14.1\mingw73_64\bin\qmake.exe') {
        $QtBin = 'C:\Qt\Qt5.14.1\5.14.1\mingw73_64\bin'
    } else {
        throw '找不到 Qt，请用 -QtBin 指定 Qt 的 bin 目录'
    }
}

$exe = Join-Path $root 'build-release\BDSMEP.exe'
if (-not (Test-Path $exe)) {
    throw "未找到 $exe，请先运行 scripts/build.ps1"
}

$dist = Join-Path $root 'dist'
New-Item -ItemType Directory -Force $dist | Out-Null

Copy-Item $exe $dist -Force

$env:Path = "$QtBin;$env:Path"
& (Join-Path $QtBin 'windeployqt.exe') --release --no-compiler-runtime (Join-Path $dist 'BDSMEP.exe')
if ($LASTEXITCODE -ne 0) { throw "windeployqt 失败 ($LASTEXITCODE)" }

# MinGW 运行时
$mingwBin = Join-Path (Split-Path -Parent (Split-Path -Parent $QtBin)) '..\Tools\mingw730_64\bin'
foreach ($dll in @('libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll')) {
    $source = Join-Path $mingwBin $dll
    if (Test-Path $source) { Copy-Item $source $dist -Force }
}

Write-Host "已部署到 $dist"
