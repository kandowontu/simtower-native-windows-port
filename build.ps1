$ErrorActionPreference = 'Stop'

$workspace = $PSScriptRoot
$python = (Get-Command python -ErrorAction Stop).Source

Push-Location (Join-Path $workspace 'web')
try {
    pnpm install
    if ($LASTEXITCODE -ne 0) { throw 'pnpm install failed' }
    pnpm run build
    if ($LASTEXITCODE -ne 0) { throw 'web build failed' }
}
finally {
    Pop-Location
}

& $python (Join-Path $workspace 'tools\embed_web_assets.py') `
    (Join-Path $workspace 'web\dist') `
    (Join-Path $workspace 'native\generated\web_assets.rc') `
    (Join-Path $workspace 'native\generated\web_assets.h')
if ($LASTEXITCODE -ne 0) { throw 'asset embedding failed' }

& 'C:\Strawberry\c\bin\windres.exe' -O coff `
    (Join-Path $workspace 'native\generated\web_assets.rc') `
    (Join-Path $workspace 'native\generated\web_assets.res.o')
if ($LASTEXITCODE -ne 0) { throw 'Windows resource compilation failed' }

$buildDirectory = Join-Path $workspace 'build\native'
$gcc = 'C:\Strawberry\c\bin\gcc.exe'
$gxx = 'C:\Strawberry\c\bin\g++.exe'
cmake -S (Join-Path $workspace 'native') -B $buildDirectory -G Ninja `
    -D CMAKE_BUILD_TYPE=Release `
    -D CMAKE_C_COMPILER=$gcc `
    -D CMAKE_CXX_COMPILER=$gxx
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
cmake --build $buildDirectory --config Release
if ($LASTEXITCODE -ne 0) { throw 'native build failed' }

$releaseDirectory = Join-Path $workspace 'dist'
New-Item -ItemType Directory -Force -Path $releaseDirectory | Out-Null
Copy-Item -Force -LiteralPath (Join-Path $buildDirectory 'bin\SimTowerNative.exe') `
    -Destination (Join-Path $releaseDirectory 'SimTowerNative.exe')

$artifact = Get-Item -LiteralPath (Join-Path $releaseDirectory 'SimTowerNative.exe')
$hash = Get-FileHash -Algorithm SHA256 -LiteralPath $artifact.FullName
Write-Output "Built $($artifact.FullName)"
Write-Output "Size: $($artifact.Length) bytes"
Write-Output "SHA256: $($hash.Hash)"
