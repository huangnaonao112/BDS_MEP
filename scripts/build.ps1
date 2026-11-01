# 构建 GNSS 数据评估系统（MinGW + qmake）
#
# 用法：
#   pwsh -File scripts/build.ps1              # Release
#   pwsh -File scripts/build.ps1 -Config Debug
#   pwsh -File scripts/build.ps1 -QtBin "C:\Qt\Qt5.14.1\5.14.1\mingw73_64\bin"
param(
    [ValidateSet('Release', 'Debug')]
    [string]$Config = 'Release',
    [string]$QtBin = '',
    [string]$Make = 'mingw32-make'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

function Resolve-QtBin {
    param([string]$Hint)
    if ($Hint -and (Test-Path (Join-Path $Hint 'qmake.exe'))) { return $Hint }

    $candidates = @(
        'C:\Qt\Qt5.14.1\5.14.1\mingw73_64\bin',
        'C:\Qt\5.15.2\mingw81_64\bin',
        'C:\Qt\6.5.3\mingw_64\bin'
    )
    foreach ($candidate in $candidates) {
        if (Test-Path (Join-Path $candidate 'qmake.exe')) { return $candidate }
    }

    $qmake = Get-Command qmake -ErrorAction SilentlyContinue
    if ($qmake) { return (Split-Path -Parent $qmake.Source) }

    throw '找不到 qmake，请用 -QtBin 指定 Qt 的 bin 目录'
}

$qtBin = Resolve-QtBin -Hint $QtBin
$mingwBin = Join-Path (Split-Path -Parent (Split-Path -Parent $qtBin)) '..\Tools\mingw730_64\bin'
$env:Path = "$qtBin;$mingwBin;$env:Path"

$buildDir = Join-Path $root "build-$($Config.ToLower())"
New-Item -ItemType Directory -Force $buildDir | Out-Null

Write-Host "[1/2] qmake ($Config) -> $buildDir"
Push-Location $buildDir
try {
    & (Join-Path $qtBin 'qmake.exe') ..\BDSMEP.pro -spec win32-g++ "CONFIG+=$($Config.ToLower())"
    if ($LASTEXITCODE -ne 0) { throw "qmake 失败 ($LASTEXITCODE)" }

    Write-Host "[2/2] $Make -j$([Environment]::ProcessorCount)"
    & $Make "-j$([Environment]::ProcessorCount)"
    if ($LASTEXITCODE -ne 0) { throw "编译失败 ($LASTEXITCODE)" }
} finally {
    Pop-Location
}

$exe = Join-Path $buildDir 'BDSMEP.exe'
if (Test-Path $exe) {
    Write-Host "构建完成: $exe"
} else {
    throw "未找到产物 $exe"
}

exit 0
