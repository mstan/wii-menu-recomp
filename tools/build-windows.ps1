$ErrorActionPreference = 'Stop'
$buildScript = Join-Path $PSScriptRoot 'build.py'
$pythonLauncher = Get-Command py.exe -ErrorAction SilentlyContinue
if ($pythonLauncher) {
    & $pythonLauncher.Source -3 $buildScript @args
} else {
    $pythonExecutable = Get-Command python.exe -ErrorAction Stop
    & $pythonExecutable.Source $buildScript @args
}
exit $LASTEXITCODE
