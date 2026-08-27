param(
  [string]$Port = 'COM3'
)

. $PSScriptRoot\arduino-env.ps1

arduino-cli compile --upload `
  -p $Port `
  --fqbn $Fqbn `
  --build-path (Join-Path $BuildRoot 'left_dash') `
  --libraries $Libraries `
  (Join-Path $ProjectRoot 'left_dash')
