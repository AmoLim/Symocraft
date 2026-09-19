param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,
    [Parameter(Mandatory = $true)]
    [string]$WorkingDirectory,
    [Parameter(Mandatory = $true)]
    [string]$EvidenceName,
    [ValidateRange(1, 60)]
    [int]$ObserveSeconds = 20
)

$ErrorActionPreference = 'Stop'
if ($EvidenceName -notmatch '^[a-zA-Z0-9-]+$') {
    throw 'EvidenceName must contain only ASCII letters, digits, and hyphens.'
}

$exe = (Resolve-Path -LiteralPath $Executable).Path
$cwd = (Resolve-Path -LiteralPath $WorkingDirectory).Path
$evidenceDirectory = Join-Path $PSScriptRoot 'evidence'
$recordPath = Join-Path $evidenceDirectory "$EvidenceName.json"
$stdoutPath = Join-Path $evidenceDirectory "$EvidenceName.stdout.log"
$stderrPath = Join-Path $evidenceDirectory "$EvidenceName.stderr.log"
foreach ($path in @($recordPath, $stdoutPath, $stderrPath)) {
    if (Test-Path -LiteralPath $path) {
        throw "Evidence already exists: $path. Choose a new EvidenceName."
    }
}

$startedAt = [DateTimeOffset]::Now
$process = $null
$forcedStop = $false
$naturalExitCode = $null
$failure = $null
$peakWorkingSet = 0L
$lastWindowTitle = ''

try {
    $process = Start-Process -FilePath $exe -WorkingDirectory $cwd `
        -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath
    Write-Output "Started test process $($process.Id); observation limit: $ObserveSeconds seconds."
    $deadline = [DateTimeOffset]::Now.AddSeconds($ObserveSeconds)
    while (-not $process.HasExited -and [DateTimeOffset]::Now -lt $deadline) {
        $process.Refresh()
        if (-not $process.HasExited) {
            $peakWorkingSet = [Math]::Max($peakWorkingSet, $process.PeakWorkingSet64)
            $lastWindowTitle = $process.MainWindowTitle
        }
        Start-Sleep -Milliseconds 200
    }
    if ($process.HasExited) {
        $process.WaitForExit()
        $naturalExitCode = $process.ExitCode
    }
} catch {
    $failure = $_.Exception.Message
} finally {
    # Stop only the process created by this probe, never a pre-existing game.
    if ($null -ne $process -and -not $process.HasExited) {
        $forcedStop = $true
        Stop-Process -Id $process.Id -ErrorAction Stop
        $process.WaitForExit()
    }
    $result = [ordered]@{
        executable = $exe
        workingDirectory = $cwd
        startedAt = $startedAt.ToString('o')
        elapsedSeconds = [Math]::Round(([DateTimeOffset]::Now - $startedAt).TotalSeconds, 3)
        observationLimitSeconds = $ObserveSeconds
        processId = $(if ($null -ne $process) { $process.Id } else { $null })
        naturalExitCode = $naturalExitCode
        forcedStopAtObservationLimit = $forcedStop
        sampledPeakWorkingSetBytes = $peakWorkingSet
        lastWindowTitle = $lastWindowTitle
        probeError = $failure
        interpretation = 'Startup observation only; survival is not gameplay, rendering, or performance acceptance.'
    }
    $json = $result | ConvertTo-Json -Depth 3
    $json | Set-Content -LiteralPath $recordPath -Encoding utf8
    Write-Output $json
    if ($null -ne $process) {
        $process.Dispose()
    }
}

if ($null -ne $failure) {
    throw $failure
}
