param(
  [ValidateSet("Debug","RelWithDebInfo","Release")]
  [string]$Config = "RelWithDebInfo",

  [string]$Preset = "windows-x64",

  [switch]$Asan,

  # Change this if OBS is installed elsewhere
  [string]$ObsExe = "C:\Program Files\obs-studio\bin\64bit\obs64.exe"
)

$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $repo

# Where we will stage the plugin for OBS to load
$stageRoot    = Join-Path $repo "_obs_stage"
$stagePlugins = Join-Path $stageRoot "obs-plugins\64bit"
$stageData    = Join-Path $stageRoot "data\obs-plugins\taffylabs_mvp_obs"

# Configure args
$configureArgs = @(
  "--preset", $Preset,
  "-DENABLE_QT=ON",
  "-DENABLE_FRONTEND_API=ON"
)

if ($Asan) {
  $configureArgs += "-DENABLE_ASAN=ON"
} else {
  $configureArgs += "-DENABLE_ASAN=OFF"
}

Write-Host "== Configure ==" -ForegroundColor Cyan
if ($Asan) {
  Write-Host "AddressSanitizer: ON" -ForegroundColor Yellow
} else {
  Write-Host "AddressSanitizer: OFF" -ForegroundColor Yellow
}

& cmake @configureArgs
if ($LASTEXITCODE -ne 0) {
  throw "CMake configure failed."
}

Write-Host "== Build ($Config) ==" -ForegroundColor Cyan
cmake --build --preset $Preset --config $Config
if ($LASTEXITCODE -ne 0) {
  throw "CMake build failed."
}

# Your build output DLL
$dll = Join-Path $repo "build_x64\$Config\taffylabs_mvp_obs.dll"
if (!(Test-Path $dll)) {
  throw "DLL not found: $dll"
}

Write-Host "== Stage plugin DLL ==" -ForegroundColor Cyan
New-Item -ItemType Directory -Force -Path $stagePlugins | Out-Null
Copy-Item -Force $dll $stagePlugins

# Copy import lib if useful for debugging / inspection
$lib = Join-Path $repo "build_x64\$Config\taffylabs_mvp_obs.lib"
if (Test-Path $lib) {
  Copy-Item -Force $lib $stagePlugins
}

# Copy PDB if present
$pdb = Join-Path $repo "build_x64\$Config\taffylabs_mvp_obs.pdb"
if (Test-Path $pdb) {
  Copy-Item -Force $pdb $stagePlugins
}

# Stage locale/resources so OBS_MODULE_USE_DEFAULT_LOCALE works
$srcData = Join-Path $repo "data"
$srcLocale = Join-Path $srcData "locale"
if (Test-Path $srcLocale) {
  Write-Host "== Stage locale/resources ==" -ForegroundColor Cyan
  New-Item -ItemType Directory -Force -Path $stageData | Out-Null
  Copy-Item -Recurse -Force (Join-Path $srcData "*") $stageData
} else {
  Write-Warning "No $srcLocale found; locale may fail to load."
}

# Close OBS if running (prevents DLL locking)
Get-Process obs64 -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

# Launch OBS with env vars pointing at our staged folders
if (!(Test-Path $ObsExe)) {
  throw "OBS exe not found: $ObsExe"
}

$env:OBS_PLUGINS_PATH = $stagePlugins
$env:OBS_PLUGINS_DATA_PATH = (Join-Path $stageRoot "data")

if ($Asan) {
  # Make ASan stop immediately on the first error for easier debugging.
  # MSVC ASan supports runtime options through ASAN_OPTIONS.
  $env:ASAN_OPTIONS = "halt_on_error=1"
  Write-Host "ASAN_OPTIONS=$env:ASAN_OPTIONS" -ForegroundColor Yellow
  Write-Host "ASAN_SAVE_DUMPS=$env:ASAN_SAVE_DUMPS" -ForegroundColor Yellow
} else {
  Remove-Item Env:ASAN_OPTIONS -ErrorAction SilentlyContinue
}

#Write-Host "== Launch OBS ==" -ForegroundColor Green
#Start-Process -FilePath $ObsExe -WorkingDirectory (Split-Path -Parent $ObsExe)

Write-Host "== Launch OBS CMD Out==" -ForegroundColor Green

$obsDir = Split-Path -Parent $ObsExe
Push-Location $obsDir
try {
    & $ObsExe
    $obsExitCode = $LASTEXITCODE
}
finally {
    Pop-Location
}

if ($null -ne $obsExitCode) {
    exit $obsExitCode
}