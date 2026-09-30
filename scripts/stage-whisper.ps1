param(
    [Parameter(Mandatory = $true)][string]$SourceTools,
    [Parameter(Mandatory = $true)][string]$DestinationTools
)
$ErrorActionPreference = 'Stop'
# 精确收录运行文件：不复制备份、旧 main.exe、转录模型或下载缓存。
$payload = @(
    'whisper\whisper-cli.exe', 'whisper\whisper.dll',
    'whisper\ggml.dll', 'whisper\ggml-base.dll',
    'whisper\ggml-cpu.dll', 'whisper\ggml-vulkan.dll',
    'Whisper.model\ggml-silero-v6.2.0.bin'
)
foreach ($relative in $payload) {
    $source = Join-Path $SourceTools $relative
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "缺少 Whisper/VAD 发布文件: $source"
    }
}
foreach ($relative in $payload) {
    $destination = Join-Path $DestinationTools $relative
    New-Item -ItemType Directory -Path (Split-Path $destination -Parent) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $SourceTools $relative) -Destination $destination -Force
}
Write-Host 'Whisper 引擎与 Silero VAD 模型已收录（不含转录模型和备份）。'
