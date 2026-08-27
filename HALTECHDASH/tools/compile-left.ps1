. $PSScriptRoot\arduino-env.ps1

arduino-cli compile `
  --fqbn $Fqbn `
  --build-path (Join-Path $BuildRoot 'left_dash') `
  --libraries $Libraries `
  (Join-Path $ProjectRoot 'left_dash')
