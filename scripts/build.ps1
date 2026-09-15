param([ValidateSet('Release','Debug')][string]$Configuration = 'Release', [switch]$Test, [switch]$Package)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw 'Install Visual Studio C++ desktop build tools and the Windows SDK.' }
$env:PATH = "$(Split-Path $vswhere);$env:PATH"
& "$vsPath\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
$preset = "windows-$($Configuration.ToLowerInvariant())"
Push-Location $taskRoot
try {
  cmake --preset $preset
  if ($LASTEXITCODE) { throw 'CMake configure failed' }
  cmake --build --preset $preset --parallel 8
  if ($LASTEXITCODE) { throw 'Build failed' }
  if ($Test) {
    ctest --test-dir "C:/buildfiles/usbtree/$($Configuration.ToLowerInvariant())" --output-on-failure
    if ($LASTEXITCODE) { throw 'Tests failed' }
  }
  if ($Package) {
    $buildFolder = "C:/buildfiles/usbtree/$($Configuration.ToLowerInvariant())"
    cmake --install $buildFolder --prefix "$buildFolder/dist/UsbTree"
    if ($LASTEXITCODE) { throw 'Portable folder staging failed' }
    cpack --config "$buildFolder/CPackConfig.cmake" -C $Configuration
    if ($LASTEXITCODE) { throw 'ZIP packaging failed' }
  }
} finally { Pop-Location }
