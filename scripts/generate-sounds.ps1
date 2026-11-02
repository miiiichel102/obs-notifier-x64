# Generates the plugin's notification sounds into assets/.
#
# on.wav    soft "whoosh" of air rising in pitch        (outputs starting / already running)
# off.wav   the same whoosh falling in pitch            (outputs stopping / already stopped)
# saved.wav short rounded "tock"                       (clip saved)
#
# All three are quiet, short and kept in a warm mid range (no hiss, no rumble)
# so they stay unobtrusive
# when heard many times per session. Needs only Windows PowerShell.
$ErrorActionPreference = 'Stop'

Add-Type -TypeDefinition @'
using System;
using System.IO;

public static class NotifierSounds
{
    const int Rate = 48000;

    // Cascade of one-pole low-passes with the given cutoff.
    static double[] LowPass(double[] x, double cutoff, int stages)
    {
        double a = 1 - Math.Exp(-2 * Math.PI * cutoff / Rate);
        var y = (double[])x.Clone();
        for (int s = 0; s < stages; s++)
        {
            double acc = 0;
            for (int i = 0; i < y.Length; i++) { acc += a * (y[i] - acc); y[i] = acc; }
        }
        return y;
    }

    public static double[] Swoosh(bool rising)
    {
        const double duration = 0.40;
        int n = (int)(Rate * duration);
        var rng = new Random(7);
        var out_ = new double[n];
        double low = 0, band = 0;
        for (int i = 0; i < n; i++)
        {
            double t = (double)i / Rate;
            // Gaussian-ish noise from the sum of uniforms.
            double noise = rng.NextDouble() + rng.NextDouble() + rng.NextDouble() - 1.5;
            // Band-pass (state-variable filter) whose centre glides between 220 and 780 Hz.
            double sweep = Math.Min(1, t / 0.30);
            sweep = 0.5 - 0.5 * Math.Cos(Math.PI * sweep);
            double centre = 220 * Math.Pow(780.0 / 220, rising ? sweep : 1 - sweep);
            double f = 2 * Math.Sin(Math.PI * centre / Rate);
            low += f * band;
            double high = noise - low - 1.1 * band;
            band += f * high;
            // Bell-shaped envelope: eases in, peaks a little before the middle, eases out.
            double env = Math.Pow(Math.Sin(Math.PI * t / duration), 2.5) * Math.Exp(-t / 0.6);
            out_[i] = band * env;
        }
        return LowPass(out_, 1500, 3);
    }

    public static double[] Tock()
    {
        int n = (int)(Rate * 0.22);
        var y = new double[n];
        double phase = 0;
        for (int i = 0; i < n; i++)
        {
            double t = (double)i / Rate;
            double frequency = 330 + 110 * Math.Exp(-t / 0.018);
            phase += 2 * Math.PI * frequency / Rate;
            double attack = 0.5 - 0.5 * Math.Cos(Math.PI * Math.Min(1, t / 0.012));
            y[i] = attack * Math.Exp(-t / 0.05) * (Math.Sin(phase) + 0.2 * Math.Sin(0.5 * phase));
        }
        return LowPass(y, 1600, 2);
    }

    public static string Write(string path, double[] signal, double peak)
    {
        int fade = (int)(0.05 * Rate);
        for (int i = 0; i < fade; i++)
        {
            double g = 1 - (double)i / fade;
            signal[signal.Length - fade + i] *= g * g;
        }
        double max = 0;
        foreach (var v in signal) max = Math.Max(max, Math.Abs(v));
        using (var w = new BinaryWriter(File.Create(path)))
        {
            int bytes = signal.Length * 2;
            w.Write("RIFF".ToCharArray()); w.Write(36 + bytes); w.Write("WAVE".ToCharArray());
            w.Write("fmt ".ToCharArray()); w.Write(16); w.Write((short)1); w.Write((short)1);
            w.Write(Rate); w.Write(Rate * 2); w.Write((short)2); w.Write((short)16);
            w.Write("data".ToCharArray()); w.Write(bytes);
            foreach (var v in signal) w.Write((short)Math.Round(v / max * peak * 32767));
        }
        return string.Format("{0} ms, {1:P0} peak", signal.Length * 1000 / Rate, peak);
    }
}
'@

$assets = Join-Path (Split-Path $PSScriptRoot -Parent) 'assets'
New-Item -ItemType Directory -Force $assets | Out-Null
foreach ($sound in @(
        @{ Name = 'on.wav';    Signal = [NotifierSounds]::Swoosh($true);  Peak = 0.12 },
        @{ Name = 'off.wav';   Signal = [NotifierSounds]::Swoosh($false); Peak = 0.12 },
        @{ Name = 'saved.wav'; Signal = [NotifierSounds]::Tock();         Peak = 0.10 })) {
    $info = [NotifierSounds]::Write((Join-Path $assets $sound.Name), $sound.Signal, $sound.Peak)
    Write-Host "Generated assets/$($sound.Name) ($info)"
}
