param(
    [Parameter(Mandatory = $true)]
    [string]$DkpRoot
)

Set-Location -LiteralPath $PSScriptRoot
$gameName = 'Pstro Laucher'
$targetName = 'pstro_launcher'

$make = Join-Path $DkpRoot 'msys2\usr\bin\make.exe'
$cygpath = Join-Path $DkpRoot 'msys2\usr\bin\cygpath.exe'
if (-not (Test-Path -LiteralPath $make)) {
    Write-Host "[ERROR] Khong tim thay make.exe: $make"
    exit 1
}

$dkpPosix = $null
if (Test-Path -LiteralPath $cygpath) {
    $dkpPosix = (& $cygpath -u $DkpRoot 2>$null | Select-Object -First 1)
    if ($null -ne $dkpPosix) { $dkpPosix = $dkpPosix.ToString().Trim() }
}
if ([string]::IsNullOrWhiteSpace($dkpPosix)) {
    $drive = [System.IO.Path]::GetPathRoot($DkpRoot).Substring(0, 1).ToLowerInvariant()
    $tail = $DkpRoot.Substring(3).Replace('\', '/')
    $dkpPosix = "/$drive/$tail"
}

$env:PATH = "$(Join-Path $DkpRoot 'msys2\usr\bin');$(Join-Path $DkpRoot 'devkitARM\bin');$(Join-Path $DkpRoot 'tools\bin');$env:PATH"
$env:DEVKITPRO = $dkpPosix
$env:DEVKITARM = "$dkpPosix/devkitARM"

$logDir = Join-Path $PSScriptRoot 'build_logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$lastLog = Join-Path $PSScriptRoot 'last_build.log'
$archiveLog = Join-Path $logDir ("build_{0}.log" -f $stamp)
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$newLine = [Environment]::NewLine

$header = @(
    "$gameName NDS KVM build log"
    "Time: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
    "Project: $PSScriptRoot"
    "devkitPro Windows: $DkpRoot"
    "DEVKITPRO MSYS2: $dkpPosix"
    'ROM code: J2DS'
    'Runtime: FAT JAR launcher, no audio'
    '============================================================'
)
[System.IO.File]::WriteAllLines($lastLog, $header, $utf8NoBom)

function Write-LogLine {
    param([AllowEmptyString()][string]$Text)
    Write-Host $Text
    [System.IO.File]::AppendAllText($lastLog, $Text + $newLine, $utf8NoBom)
}

function Invoke-NativeLogged {
    param([string]$Executable, [string[]]$Arguments = @())
    & $Executable @Arguments 2>&1 | ForEach-Object { Write-LogLine ([string]$_) }
    return [int]$LASTEXITCODE
}

Write-LogLine '[1/2] Cleaning...'
$cleanCode = Invoke-NativeLogged -Executable $make -Arguments @('clean')
if ($cleanCode -ne 0) {
    Write-LogLine "[ERROR] Clean failed with code $cleanCode"
    Copy-Item -LiteralPath $lastLog -Destination $archiveLog -Force
    exit $cleanCode
}

Write-LogLine '[2/2] Building Pstro Laucher...'
$buildCode = Invoke-NativeLogged -Executable $make
if ($buildCode -eq 0) {
    Write-LogLine "[OK] Build completed: $targetName.nds"
} else {
    Write-LogLine "[ERROR] Build failed with code $buildCode"
}

Copy-Item -LiteralPath $lastLog -Destination $archiveLog -Force
Write-Host "[LOG] $lastLog"
Write-Host "[ARCHIVE LOG] $archiveLog"
exit $buildCode
