# 构建并运行渲染/解析基准测试
#
# 用法：
#   pwsh -File scripts/bench.ps1
#   pwsh -File scripts/bench.ps1 -Points 500000 -ParseFile data\1_1_2_2025-07-02_rtk_raw.pos
param(
    [int]$Points = 200000,
    [string]$ParseFile = '',
    [string]$QtBin = '',
    [string]$Make = 'mingw32-make'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

if ($QtBin -and (Test-Path (Join-Path $QtBin 'qmake.exe'))) {
    $qtBin = $QtBin
} elseif (Test-Path 'C:\Qt\Qt5.14.1\5.14.1\mingw73_64\bin\qmake.exe') {
    $qtBin = 'C:\Qt\Qt5.14.1\5.14.1\mingw73_64\bin'
} else {
    $qmake = Get-Command qmake -ErrorAction SilentlyContinue
    if (-not $qmake) { throw '找不到 qmake，请用 -QtBin 指定' }
    $qtBin = Split-Path -Parent $qmake.Source
}

$mingwBin = Join-Path (Split-Path -Parent (Split-Path -Parent $qtBin)) '..\Tools\mingw730_64\bin'
$env:Path = "$qtBin;$mingwBin;$env:Path"

$buildDir = Join-Path $root 'build-bench'
New-Item -ItemType Directory -Force $buildDir | Out-Null

Push-Location $buildDir
try {
    & (Join-Path $qtBin 'qmake.exe') ..\bench\bench_plot.pro -spec win32-g++ 'CONFIG+=release'
    if ($LASTEXITCODE -ne 0) { throw "qmake 失败 ($LASTEXITCODE)" }

    & $Make "-j$([Environment]::ProcessorCount)"
    if ($LASTEXITCODE -ne 0) { throw "编译失败 ($LASTEXITCODE)" }
} finally {
    Pop-Location
}

$exe = Join-Path $buildDir 'bench_plot.exe'
$arguments = @("$Points")
if ($ParseFile) { $arguments += (Join-Path $root $ParseFile) }

$env:QT_QPA_PLATFORM = 'offscreen'
& $exe @arguments

# 控件级交互帧耗时（拖动/缩放的真实节奏）
$widgetBuild = Join-Path $root 'build-bench-widget'
New-Item -ItemType Directory -Force $widgetBuild | Out-Null

Push-Location $widgetBuild
try {
    & (Join-Path $qtBin 'qmake.exe') ..\bench\widget\widget.pro -spec win32-g++ 'CONFIG+=release'
    if ($LASTEXITCODE -ne 0) { throw "qmake 失败 ($LASTEXITCODE)" }

    & $Make "-j$([Environment]::ProcessorCount)"
    if ($LASTEXITCODE -ne 0) { throw "编译失败 ($LASTEXITCODE)" }
} finally {
    Pop-Location
}

$widgetExe = Join-Path $widgetBuild 'bench_widget.exe'
& $widgetExe @arguments
