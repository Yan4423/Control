# Read-only check. Does not connect to or flash hardware.
[CmdletBinding()]
param([switch]$BuildOnly)

$ErrorActionPreference = 'Stop'
$required = @('git', 'cmake', 'ninja', 'arm-none-eabi-gcc', 'arm-none-eabi-objcopy')
if (-not $BuildOnly) { $required += 'STM32_Programmer_CLI' }
$missing = @()
foreach ($name in $required) {
    $command = Get-Command $name -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($null -eq $command) {
        Write-Host ("MISSING: {0}" -f $name)
        $missing += $name
    }
    else {
        Write-Host ("OK: {0} -> {1}" -f $name, $command.Source)
    }
}
if ($missing.Count -gt 0) {
    Write-Host 'Add the required tool bin directories to PATH, then restart your terminal/editor.'
    exit 1
}
Write-Host 'Build tools are available. Configure: cmake --preset Debug'
if (-not $BuildOnly) {
    Write-Host 'Programmer found. No hardware connection or flash was attempted.'
}
exit 0
