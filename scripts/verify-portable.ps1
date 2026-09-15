param(
    [Parameter(Mandatory=$true)][string]$Executable,
    [Parameter(Mandatory=$true)][string]$OutputRoot,
    [Parameter(Mandatory=$true)][string]$Dumpbin
)
$ErrorActionPreference = 'Stop'
$testRoot = [IO.Path]::GetFullPath($OutputRoot)
if (-not $testRoot.StartsWith('C:\buildfiles\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Portable verification output must remain under C:\buildfiles.'
}
New-Item -ItemType Directory -Force $testRoot | Out-Null
$imports = & $Dumpbin /dependents $Executable
if ($LASTEXITCODE) { throw 'DLL import inspection failed' }
$imports | Set-Content -LiteralPath (Join-Path $testRoot 'imports.txt')
$dependencyNames = @($imports | Select-String '^\s+([A-Za-z0-9_.-]+\.dll)\s*$' | ForEach-Object { $_.Matches[0].Groups[1].Value })
if (-not $dependencyNames.Count) { throw 'Could not parse executable imports' }
if ($dependencyNames -match '(?i)^(SDL|imgui|msvcp|msvcr|vcruntime|ucrtbase|api-ms-win-crt)') {
    throw "Unexpected runtime DLL dependency: $($dependencyNames -join ', ')"
}
$sourceFolder = Split-Path -Parent $Executable
$folderName = 'USB Tree ' + [char]0x03A9
$relocated = Join-Path $testRoot $folderName
New-Item -ItemType Directory -Force $relocated | Out-Null
Copy-Item -LiteralPath $Executable -Destination (Join-Path $relocated 'UsbTree.exe') -Force
foreach ($item in @('assets','licenses','README.md')) {
    Copy-Item -LiteralPath (Join-Path $sourceFolder $item) -Destination $relocated -Recurse -Force
}
$usedDrives = @([IO.DriveInfo]::GetDrives() | ForEach-Object { $_.Name.Substring(0,1) })
$letter = @('Z','Y','X','W','V','U','T','S','R') | Where-Object { $_ -notin $usedDrives } | Select-Object -First 1
if (-not $letter) { throw 'No free drive letter for relocation verification' }
$drive = $letter + ':'
$mapped = $false
$originalPath = $env:PATH
try {
    & "$env:SystemRoot\System32\subst.exe" $drive $testRoot
    if ($LASTEXITCODE) { throw 'Could not map the relocation test drive' }
    $mapped = $true
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
    $runPath = $drive + '\' + $folderName + '\UsbTree.exe'
    $process = Start-Process -FilePath $runPath -ArgumentList '--demo','--smoke','--capture','portable.bmp' -WorkingDirectory $env:SystemRoot -PassThru -WindowStyle Hidden
    if (-not $process.WaitForExit(30000)) { $process.Kill(); throw 'Relocated application timed out' }
    if ($process.ExitCode -ne 0) { throw "Relocated application failed: $($process.ExitCode)" }
    if (-not (Test-Path -LiteralPath (Join-Path $relocated 'captures\portable.bmp'))) { throw 'Relocated application did not render its screenshot' }
} finally {
    $env:PATH = $originalPath
    if ($mapped) { & "$env:SystemRoot\System32\subst.exe" $drive /D }
}
"Passed: static runtime imports, Unicode folder, different executable drive, foreign working directory, minimal PATH." | Tee-Object -FilePath (Join-Path $testRoot 'result.txt')

