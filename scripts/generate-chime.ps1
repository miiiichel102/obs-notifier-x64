$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$rate = 48000
$duration = 0.72
$count = [int]($rate * $duration)
$samples = [double[]]::new($count)
$peak = 0.0
for ($i = 0; $i -lt $count; $i++) {
    $t = $i / [double]$rate
    $sample = 0.0
    for ($note = 0; $note -lt 2; $note++) {
        $frequency = if ($note -eq 0) { 391.9954 } else { 493.8833 }
        $age = $t - $note * 0.07
        if ($age -lt 0) { continue }
        $attackPhase = [Math]::Min(1.0, $age / 0.035)
        $attack = 0.5 - 0.5 * [Math]::Cos([Math]::PI * $attackPhase)
        $envelope = $attack * [Math]::Exp(-$age / 0.16)
        $phase = 2.0 * [Math]::PI * $frequency * $age
        $body = [Math]::Sin($phase) + 0.08 * [Math]::Sin($phase * 0.5)
        $sample += $body * $envelope * @(0.8, 1.0)[$note]
    }
    $tailPhase = [Math]::Min(1.0, [Math]::Max(0.0, ($duration - $t) / 0.12))
    $sample *= 0.5 - 0.5 * [Math]::Cos([Math]::PI * $tailPhase)
    $samples[$i] = $sample
    $peak = [Math]::Max($peak, [Math]::Abs($sample))
}
$assetDir = Join-Path $root 'assets'
New-Item -ItemType Directory -Path $assetDir -Force | Out-Null
$stream = [IO.File]::Create((Join-Path $assetDir 'notification-chime.wav'))
$writer = [IO.BinaryWriter]::new($stream)
try {
    $writer.Write([Text.Encoding]::ASCII.GetBytes('RIFF'))
    $writer.Write([int](36 + $count * 2))
    $writer.Write([Text.Encoding]::ASCII.GetBytes('WAVEfmt '))
    $writer.Write([int]16)
    $writer.Write([int16]1)
    $writer.Write([int16]1)
    $writer.Write([int]$rate)
    $writer.Write([int]($rate * 2))
    $writer.Write([int16]2)
    $writer.Write([int16]16)
    $writer.Write([Text.Encoding]::ASCII.GetBytes('data'))
    $writer.Write([int]($count * 2))
    foreach ($sample in $samples) {
        $writer.Write([int16][Math]::Round($sample / $peak * 0.09 * 32767))
    }
} finally {
    $writer.Dispose()
}
Write-Host 'Generated assets/notification-chime.wav (quiet 720 ms cue, rounded attack, 9% peak).'
