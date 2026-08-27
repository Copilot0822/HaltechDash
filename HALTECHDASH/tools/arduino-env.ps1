$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot '..')

$Fqbn = 'esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,CDCOnBoot=cdc,USBMode=hwcdc'
$BuildRoot = Join-Path $ProjectRoot '.arduino-build'

$CandidateLibraries = @(
  (Join-Path $ProjectRoot 'arduino_libraries'),
  'C:\Users\copil\AppData\Local\Temp\waveshare_28c_demo_36778f5d3f0f471dace92f259a92abde\Arduino\libraries'
)

$Libraries = $CandidateLibraries | Where-Object { Test-Path (Join-Path $_ 'lvgl') } | Select-Object -First 1
if (-not $Libraries) {
  throw 'LVGL 8.3.10 library not found. Download the Waveshare 2.8C demo package and set $Libraries in tools/arduino-env.ps1.'
}

New-Item -ItemType Directory -Force -Path $BuildRoot | Out-Null
