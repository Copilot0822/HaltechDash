. $PSScriptRoot\arduino-env.ps1

arduino-cli compile `
  --fqbn $Fqbn `
  --build-path (Join-Path $BuildRoot 'third_dash_fresh') `
  --libraries $Libraries `
  (Join-Path $ProjectRoot 'third_dash')
