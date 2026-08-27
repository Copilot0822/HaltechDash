param(
  [string]$Port = 'COM3'
)

. $PSScriptRoot\arduino-env.ps1

arduino-cli compile --upload `
  -p $Port `
  --fqbn $Fqbn `
  --build-path (Join-Path $BuildRoot 'right_dash') `
  --libraries $Libraries `
  (Join-Path $ProjectRoot 'right_dash')
