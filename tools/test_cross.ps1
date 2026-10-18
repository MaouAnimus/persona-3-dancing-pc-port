Remove-Item -Path "game_crash.log" -ErrorAction SilentlyContinue
Set-Content -Path "test_pad.txt" -Value ""
$env:PATH = "C:\msys64\clang64\bin;" + $env:PATH
$env:GOW3_UPSCALER = "none"
$env:GOW3_VK_RECORD_THREAD = "0"
$env:GOW3_PAD_FILE = (Resolve-Path "test_pad.txt").Path

$p3d = (Resolve-Path "p3d.exe").Path
$boot = (Resolve-Path "out\p3d_test\boot-linked.bin").Path
$app0 = (Resolve-Path "CUSA12636").Path

$proc = Start-Process -FilePath $p3d -ArgumentList "`"$boot`"", "--app0", "`"$app0`"" -PassThru -RedirectStandardOutput "test_stdout.log" -RedirectStandardError "test_stderr.log"

Write-Host "Started process $($proc.Id). Waiting for engine to initialize..."
Start-Sleep -Seconds 5

for ($i = 0; $i -lt 8; $i++) {
    if ($proc.HasExited) {
        Write-Host "Process exited early with code $($proc.ExitCode)"
        break
    }
    Write-Host "Pulsing CROSS iteration $i..."
    Set-Content -Path "test_pad.txt" -Value "cross"
    Start-Sleep -Milliseconds 800
    Set-Content -Path "test_pad.txt" -Value ""
    Start-Sleep -Milliseconds 600
}

Start-Sleep -Seconds 3

if ($proc.HasExited) {
    Write-Host "Process exited with code $($proc.ExitCode)"
} else {
    $ws = [math]::Round($proc.WorkingSet64 / 1MB, 2)
    Write-Host "PROCESS IS STILL RUNNING AND STABLE! Working set: $ws MB"
    Stop-Process -Id $proc.Id -Force
    Write-Host "Test completed cleanly."
}
