[CmdletBinding()]
param(
    [ValidateSet('x64')]
    [string] $Target = 'x64',
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo',
    [string] $Codesign = 'false',
    [string] $CreatePackage = 'false'
)

$ErrorActionPreference = 'Stop'

if ( $DebugPreference -eq 'Continue' ) {
    $VerbosePreference = 'Continue'
    $InformationPreference = 'Continue'
}

if ( $env:CI -eq $null ) {
    throw "Package-Windows.ps1 requires CI environment"
}

if ( ! ( [System.Environment]::Is64BitOperatingSystem ) ) {
    throw "Packaging script requires a 64-bit system to build and run."
}

if ( $PSVersionTable.PSVersion -lt '7.2.0' ) {
    Write-Warning 'The packaging script requires PowerShell Core 7. Install or upgrade your PowerShell version: https://aka.ms/pscore6'
    exit 2
}

function Package {
    trap {
        Write-Error $_
        exit 2
    }

    $ScriptHome = $PSScriptRoot
    $ProjectRoot = Resolve-Path -Path "$PSScriptRoot/../.."
    $BuildSpecFile = "${ProjectRoot}/buildspec.json"

    $UtilityFunctions = Get-ChildItem -Path $PSScriptRoot/utils.pwsh/*.ps1 -Recurse

    foreach( $Utility in $UtilityFunctions ) {
        Write-Debug "Loading $($Utility.FullName)"
        . $Utility.FullName
    }

    $BuildSpec = Get-Content -Path ${BuildSpecFile} -Raw | ConvertFrom-Json
    $ProductName = $BuildSpec.name
    $ProductVersion = $BuildSpec.version

    $OutputName = "${ProductName}-${ProductVersion}-windows-${Target}"

    $RemoveArgs = @{
        ErrorAction = 'SilentlyContinue'
        Path = @(
            "${ProjectRoot}/release/${ProductName}-*-windows-*.zip",
            "${ProjectRoot}/release/*_Setup_*.exe"
        )
    }

    Remove-Item @RemoveArgs

    # 1. Sign DLLs
    $CertPath = Join-Path $env:RUNNER_TEMP "certificate.pfx"
    $Signtool = $null
    
    if ($Codesign -eq 'true' -and $env:WINDOWS_SIGNING_CERT) {
        Log-Group "Codesigning Plugin DLLs..."
        [IO.File]::WriteAllBytes($CertPath, [Convert]::FromBase64String($env:WINDOWS_SIGNING_CERT))
        
        $Signtool = (Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin\*\x64\signtool.exe" | Select-Object -First 1).FullName
        if ($null -eq $Signtool) {
            Write-Warning "Could not find signtool.exe"
        } else {
            $FilesToSign = Get-ChildItem -Path "${ProjectRoot}/release/${Configuration}" -Include "*.dll", "*.exe" -Recurse
            foreach ($File in $FilesToSign) {
                Write-Host "Signing $($File.FullName)..."
                & $Signtool sign /f $CertPath /p $env:WINDOWS_SIGNING_CERT_PASSWORD /tr http://timestamp.digicert.com /td sha256 /fd sha256 $File.FullName
            }
        }
        Log-Group
    }

    # 2. Installer
    if ($CreatePackage -eq 'true') {
        Log-Group "Building Installer..."
        # InnoSetup expects _obs_stage layout at the repo root
        $StageRoot = Join-Path $ProjectRoot "_obs_stage"
        if (Test-Path $StageRoot) { Remove-Item -Recurse -Force $StageRoot }
        New-Item -ItemType Directory -Force $StageRoot | Out-Null
        
        # Copy release files to _obs_stage
        Copy-Item -Recurse -Force "${ProjectRoot}/release/${Configuration}/*" $StageRoot
        
        $IssPath = Join-Path $ProjectRoot "InnoSetup\TaffyLabs_Lore.iss"
        if (Test-Path $IssPath) {
            # Build installer using ISCC
            Write-Host "Running ISCC on $IssPath"
            # Append dynamic version via /D
            & iscc /DMyAppVersion="$ProductVersion" $IssPath
            
            # 3. Sign Installer
            if ($Codesign -eq 'true' -and $env:WINDOWS_SIGNING_CERT -and $null -ne $Signtool) {
                Write-Host "Signing generated installer..."
                $InstallerFiles = Get-ChildItem -Path "${ProjectRoot}/InnoSetup/Output" -Filter "*_Setup_*.exe"
                foreach ($InstallerFile in $InstallerFiles) {
                    & $Signtool sign /f $CertPath /p $env:WINDOWS_SIGNING_CERT_PASSWORD /tr http://timestamp.digicert.com /td sha256 /fd sha256 $InstallerFile.FullName
                }
            }
            
            # Copy installer to release directory so uploading artifacts will catch it
            $SetupFiles = Get-ChildItem -Path "${ProjectRoot}/InnoSetup/Output" -Filter "*_Setup_*.exe"
            foreach ($SetupFile in $SetupFiles) {
                Copy-Item -Force $SetupFile.FullName "${ProjectRoot}/release/"
            }
        }
        Log-Group
    }

    # Cleanup cert securely
    if (Test-Path $CertPath) { Remove-Item -Force $CertPath }

    Log-Group "Archiving ${ProductName}..."
    $CompressArgs = @{
        Path = (Get-ChildItem -Path "${ProjectRoot}/release/${Configuration}" -Exclude "${OutputName}*.*")
        CompressionLevel = 'Optimal'
        DestinationPath = "${ProjectRoot}/release/${OutputName}.zip"
        Verbose = ($Env:CI -ne $null)
    }
    Compress-Archive -Force @CompressArgs
    Log-Group
}

Package
