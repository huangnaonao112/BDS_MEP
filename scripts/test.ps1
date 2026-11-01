# 构建并运行单元测试 + UI 冒烟测试
#
# 用法：
#   powershell -ExecutionPolicy Bypass -File scripts/test.ps1
#   powershell -ExecutionPolicy Bypass -File scripts/test.ps1 -Filter "TestGnssCore::distribution*"
#   powershell -ExecutionPolicy Bypass -File scripts/test.ps1 -SkipUi
param(
    [string]$Filter = '',
    [switch]$SkipUi,
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

$jobs = @(
    @{ Name = 'core'; Project = 'tests\tests.pro'; Build = 'build-tests'; Exe = 'tst_bdsmep.exe' }
)
if (-not $SkipUi) {
    $jobs += @{ Name = 'ui'; Project = 'tests\ui\ui.pro'; Build = 'build-uitest'; Exe = 'tst_ui.exe' }
}

$failed = 0

foreach ($job in $jobs) {
    $buildDir = Join-Path $root $job.Build
    New-Item -ItemType Directory -Force $buildDir | Out-Null

    Write-Host "==> 构建 $($job.Name) 测试 ($buildDir)"
    Push-Location $buildDir
    try {
        & (Join-Path $qtBin 'qmake.exe') (Join-Path '..' $job.Project) -spec win32-g++ 'CONFIG+=release'
        if ($LASTEXITCODE -ne 0) { throw "qmake 失败 ($LASTEXITCODE)" }

        & $Make "-j$([Environment]::ProcessorCount)"
        if ($LASTEXITCODE -ne 0) { throw "编译失败 ($LASTEXITCODE)" }
    } finally {
        Pop-Location
    }

    $exe = Join-Path $buildDir $job.Exe
    $arguments = @()
    if ($Filter -and $job.Name -eq 'core') { $arguments += $Filter }

    Write-Host "==> 运行 $($job.Name) 测试"
    & $exe @arguments
    if ($LASTEXITCODE -ne 0) {
        $failed++
        Write-Host "!! $($job.Name) 测试失败" -ForegroundColor Red
    }
}

if ($failed -gt 0) {
    throw "$failed 组测试失败"
}

Write-Host "全部测试通过" -ForegroundColor Green
exit 0
