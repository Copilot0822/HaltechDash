. $PSScriptRoot\arduino-env.ps1

arduino-cli compile `
  --fqbn $Fqbn `
  --build-path (Join-Path $BuildRoot 'right_dash') `
  --libraries $Libraries `
  (Join-Path $ProjectRoot 'right_dash')
