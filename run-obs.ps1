param(
  [ValidateSet("Debug","RelWithDebInfo","Release")]
  [string]$Config = "RelWithDebInfo",

  [string]$Preset = "windows-x64",

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

Write-Host "== Configure ==" -ForegroundColor Cyan
cmake --preset $Preset -DENABLE_QT=ON -DENABLE_FRONTEND_API=ON
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

Write-Host "== Launch OBS ==" -ForegroundColor Green
Start-Process -FilePath $ObsExe -WorkingDirectory (Split-Path -Parent $ObsExe)