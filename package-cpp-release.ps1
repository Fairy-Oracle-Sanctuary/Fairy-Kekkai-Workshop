param(
    # Qt 安装根目录（需包含 bin\windeployqt.exe）
    [string]$QtRoot = 'D:\Qt\6.7.3\msvc2019_64',
    # OpenCV 预编译包目录（需包含 OpenCVConfig.cmake），本机为 world 版
    [string]$OpenCVDir = 'D:\CODE\opencv-4.12.0\build',
    # CMake 可执行文件；留空则先试默认路径，再退回 PATH
    [string]$CMakePath = '',
    # Visual Studio 生成器；CI 的 windows-latest 用 'Visual Studio 17 2022'
    [string]$Generator = 'Visual Studio 16 2019',
    [string]$Arch = 'x64',
    # 编译缓存目录（相对仓库根）
    [string]$BuildDir = 'build-cpp-release',
    # 待打包目录（相对仓库根）；安装包整体收录该目录
    [string]$StageDir = 'dist-cpp\Fairy-Kekkai-Workshop-Cpp.dist',
    # 额外用 Inno Setup 生成安装包
    [switch]$Setup,
    # ISCC.exe 路径；留空则自动查找
    [string]$ISCCPath = '',
    # 安装包文件名中的 OCR 变体，如 CPU-v3.7.0；留空则按 PADDLEOCR 标识推导
    [string]$Variant = '',
    # 写入待打包目录的 OCR 版本标识（即 PADDLEOCR 文件内容）；
    # 留空则沿用仓库根同名文件，-NoTools 时不写
    [string]$PaddleOcrTag = '',
    # 收录整个 tools 目录（安装包必须完整，-Setup 时自动开启）
    [switch]$FullTools,
    # Clear 增量升级包：不收录 tools 与 PADDLEOCR 标识
    [switch]$NoTools,
    [switch]$IncludeProjects
)

$ErrorActionPreference = 'Stop'

# C++ 主程序打包：编译 Release -> 铺 Qt/OpenCV 运行库与 tools -> 可选用 Inno Setup 出安装包。
# 安装包复用仓库根 Fairy-Kekkai-Workshop.iss，用 /D 覆盖主程序名与待打包目录，
# 因此 AppId / 安装目录 / 快捷方式与 Python 版保持一致，可直接覆盖升级旧版。
$repo = $PSScriptRoot
$source = Join-Path $repo 'cpp'
$target = 'Fairy-Kekkai-Workshop-Cpp'
$exeName = $target + '.exe'
$issName = 'Fairy-Kekkai-Workshop.iss'

if (-not (Test-Path (Join-Path $OpenCVDir 'OpenCVConfig.cmake'))) {
    throw "未找到 OpenCV 配置: $OpenCVDir\OpenCVConfig.cmake（可用 -OpenCVDir 指定）"
}
$opencvBin = Join-Path $OpenCVDir 'x64\vc16\bin'

if (-not $CMakePath) {
    $preferred = 'D:\Qt\Tools\CMake_64\bin\cmake.exe'
    if (Test-Path $preferred) { $CMakePath = $preferred }
}
if (-not $CMakePath) {
    $found = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($found) { $CMakePath = $found.Source }
}
if (-not $CMakePath) { throw '未找到 cmake.exe，请安装 CMake 或显式指定 -CMakePath。' }

$windeployqt = Join-Path $QtRoot 'bin\windeployqt.exe'
if (-not (Test-Path $windeployqt)) { throw "未找到 windeployqt: $windeployqt（可用 -QtRoot 指定）" }

$buildPath = Join-Path $repo $BuildDir
$stagePath = Join-Path $repo $StageDir
$exe = Join-Path $buildPath ('Release\' + $exeName)

Write-Host '[1/4] 配置 Release...'
# 旧构建目录里可能残留 vcpkg 工具链和 FFMPEG 缓存（会去找 vcpkg 版 OpenCV），这里清掉。
$configureArgs = @(
    "-DCMAKE_PREFIX_PATH=$QtRoot"
    "-DOpenCV_DIR=$OpenCVDir"
    '-UCMAKE_TOOLCHAIN_FILE'
    '-UFFMPEG_DIR'
    '-UCMAKE_MODULE_PATH'
)
& $CMakePath -S $source -B $buildPath -G $Generator -A $Arch @configureArgs
if ($LASTEXITCODE -ne 0) { throw 'CMake 配置失败。' }

Write-Host '[2/4] 编译 Release...'
& $CMakePath --build $buildPath --config Release --target $target --parallel
if ($LASTEXITCODE -ne 0) { throw 'Release 编译失败。' }
if (-not (Test-Path $exe)) { throw "找不到编译产物: $exe" }

Write-Host '[3/4] 收集运行库与配套文件...'
if (Test-Path $stagePath) { Remove-Item -LiteralPath $stagePath -Recurse -Force }
New-Item -ItemType Directory -Path $stagePath -Force | Out-Null
Copy-Item -LiteralPath $exe -Destination $stagePath
$stageExe = Join-Path $stagePath $exeName

& $windeployqt --release --compiler-runtime --dir $stagePath $stageExe
if ($LASTEXITCODE -ne 0) { throw 'windeployqt 失败。' }

# OpenCV Release 运行库（world 版 + videoio 的 ffmpeg 后端）
foreach ($dll in @('opencv_world4120.dll', 'opencv_videoio_ffmpeg4120_64.dll')) {
    $path = Join-Path $opencvBin $dll
    if (Test-Path $path) { Copy-Item -LiteralPath $path -Destination $stagePath }
}

if (-not $NoTools) {
    $toolSource = Join-Path $repo 'tools'
    if (-not (Test-Path $toolSource)) { throw "未找到 tools 目录: $toolSource" }
    $toolDest = Join-Path $stagePath 'tools'
    New-Item -ItemType Directory -Path $toolDest -Force | Out-Null
    # -Setup 时安装包必须自带完整工具链，等价于 -FullTools
    if ($FullTools -or $Setup) {
        # Whisper.model 由用户自行下载，*.zip 是下载缓存，都不进安装包
        Get-ChildItem -LiteralPath $toolSource -Force | Where-Object {
            $_.Name -ne 'Whisper.model' -and $_.Extension -ne '.zip'
        } | ForEach-Object {
            Copy-Item -LiteralPath $_.FullName -Destination $toolDest -Recurse
        }
    } else {
        foreach ($file in @('ffmpeg.exe', 'yt-dlp.exe', 'videocr-cli.exe')) {
            $path = Join-Path $toolSource $file
            if (Test-Path $path) { Copy-Item -LiteralPath $path -Destination $toolDest }
        }
    }
    # OCR 版本标识：程序运行期从 exe 同级目录读取第一行，决定 tools 下的引擎子目录
    $markerDest = Join-Path $stagePath 'PADDLEOCR'
    if ($PaddleOcrTag) {
        Set-Content -LiteralPath $markerDest -Value $PaddleOcrTag -Encoding ascii -NoNewline
    } elseif (Test-Path (Join-Path $repo 'PADDLEOCR')) {
        Copy-Item -LiteralPath (Join-Path $repo 'PADDLEOCR') -Destination $markerDest
    }
}

$background = Join-Path $repo 'background.jpg'
if (Test-Path $background) { Copy-Item -LiteralPath $background -Destination $stagePath }

if ($IncludeProjects) {
    Get-ChildItem -LiteralPath $repo -Directory | ForEach-Object {
        if (Test-Path (Join-Path $_.FullName '标题.txt')) {
            Copy-Item -LiteralPath $_.FullName -Destination $stagePath -Recurse
        }
    }
}

Write-Host '[4/4] 汇总...'
$stageFiles = Get-ChildItem -LiteralPath $stagePath -Recurse -File
$stageTotal = ($stageFiles | Measure-Object -Property Length -Sum).Sum
Write-Host ''
Write-Host "待打包目录: $stagePath"
Write-Host "文件数: $($stageFiles.Count)，体积: $([math]::Round($stageTotal / 1MB, 1)) MB"
Write-Host "可执行文件: $stageExe"
Write-Host '用户配置保存在 Local AppData；待打包目录不包含本机配置。'

if (-not $NoTools -and -not ($FullTools -or $Setup)) {
    Write-Host 'OCR/Whisper 模型未复制；需要完整包时加 -FullTools（-Setup 会自动包含）。'
}
if (-not $IncludeProjects) { Write-Host '本地项目目录未复制；需要时加 -IncludeProjects。' }

if (-not $Setup) {
    Write-Host '未生成安装包；加 -Setup 会用 Inno Setup 打包成 Setup。'
    return
}

# ---- Inno Setup：生成 Setup ----
# 变体名决定安装包文件名后缀，默认由 PADDLEOCR 标识推导（PaddleOCR-GPU-v3.7.0-CUDA-12.9 -> GPU-v3.7.0-CUDA-12.9）
if (-not $Variant) {
    $markerPath = Join-Path $stagePath 'PADDLEOCR'
    if (Test-Path $markerPath) {
        $marker = (Get-Content -LiteralPath $markerPath -TotalCount 1).Trim()
        if ($marker.StartsWith('PaddleOCR-')) { $Variant = $marker.Substring('PaddleOCR-'.Length) }
    }
    if (-not $Variant) {
        throw '无法推导安装包变体名，请用 -Variant 指定（如 CPU-v3.7.0，Clear 包请直接写 Clear）。'
    }
}

# 版本号取自 C++ 运行期读取的 setting_data.json，保证安装包文件名与程序内显示一致
$version = (Get-Content -LiteralPath (Join-Path $source 'resources\setting_data.json') -Raw |
    ConvertFrom-Json).VERSION
if (-not $version) { throw '无法从 cpp\resources\setting_data.json 读取 VERSION。' }

if (-not $ISCCPath) {
    # 1) 常见的 Program Files 位置（本机 Inno Setup 装在 D 盘，故一并探测）
    $candidates = @()
    foreach ($root in @(${env:ProgramFiles(x86)}, $env:ProgramFiles,
                        'D:\Program Files (x86)', 'D:\Program Files')) {
        if ($root) { $candidates += (Join-Path $root 'Inno Setup 6\ISCC.exe') }
    }
    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) { $ISCCPath = $candidate; break }
    }
}
if (-not $ISCCPath) {
    # 2) 注册表登记的安装位置
    foreach ($key in @(
        'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Inno Setup 6_is1',
        'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\Inno Setup 6_is1',
        'HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Inno Setup 6_is1'
    )) {
        if (-not (Test-Path $key)) { continue }
        $location = (Get-ItemProperty -Path $key -ErrorAction SilentlyContinue).InstallLocation
        if ($location) {
            $candidate = Join-Path $location 'ISCC.exe'
            if (Test-Path $candidate) { $ISCCPath = $candidate; break }
        }
    }
}
if (-not $ISCCPath) {
    $found = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($found) { $ISCCPath = $found.Source }
}
if (-not $ISCCPath) { throw '未找到 ISCC.exe，请安装 Inno Setup 6 或显式指定 -ISCCPath。' }

Write-Host ''
Write-Host "生成安装包：版本 $version，变体 $Variant"
# /DMyRemoveLegacyExe=1 让安装包在覆盖升级时清掉 Python 版残留的旧主程序
Push-Location $repo
try {
    & $ISCCPath "/DMyAppVersion=$version" "/DMyVariant=$Variant" "/DMyAppExeName=$exeName" `
        "/DMySourceDir=$stagePath" '/DMyRemoveLegacyExe=1' $issName
    if ($LASTEXITCODE -ne 0) { throw "Inno Setup 编译失败（退出码 $LASTEXITCODE）。" }
} finally {
    Pop-Location
}

# 注意：局部变量不要与上面的参数同名——PowerShell 变量名大小写不敏感，
# 例如 $setup 会命中 [switch]$Setup，把字符串赋给 [switch] 会抛类型转换错误，故用 $setupPath。
$setupPath = Join-Path (Join-Path $repo 'Output') "Fairy-Kekkai-Workshop-v$version-$Variant-Windows-x86_64-Setup.exe"
if (-not (Test-Path $setupPath)) { throw "未找到安装包产物: $setupPath" }
Write-Host ''
Write-Host "安装包: $setupPath"
Write-Host "体积: $([math]::Round((Get-Item -LiteralPath $setupPath).Length / 1MB, 1)) MB"
