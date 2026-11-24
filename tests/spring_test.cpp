#include "../Source/SpringDsp.h"
#include <chrono>
#include <cstdio>
#include <xmmintrin.h>
using namespace spr;

static std::vector<double> impulseL (int model, double decay, double tension, double bright, double boingy, double drive, double fs, double secs)
{
    SpringReverb r; r.prepare (fs); r.setModel (model); r.setParams (decay, tension, bright, boingy, drive);
    std::vector<double> ir ((size_t) (fs * secs));
    for (size_t i = 0; i < ir.size(); ++i) { float l, rr; r.process (i == 0 ? 0.5f : 0.f, i == 0 ? 0.5f : 0.f, l, rr); ir[i] = l; }
    return ir;
}
static double t60 (const std::vector<double>& x, double fs)
{
    std::vector<double> edc (x.size()); double acc = 0;
    for (size_t i = x.size(); i-- > 0;) { acc += x[i] * x[i]; edc[i] = acc; }
    const double e0 = edc[0]; double t5 = -1, t25 = -1;
    for (size_t i = 0; i < x.size(); ++i)
    {
        const double db = 10 * std::log10 (edc[i] / e0 + 1e-30);
        if (t5 < 0 && db < -5) t5 = (double) i / fs;
        if (t25 < 0 && db < -25) { t25 = (double) i / fs; break; }
    }
    return t25 > t5 && t5 >= 0 ? 60.0 * (t25 - t5) / 20.0 : -1;
}
static double rms (const std::vector<double>& x, size_t a, size_t b)
{ double s = 0; for (size_t i = a; i < std::min(b, x.size()); ++i) s += x[i]*x[i]; return std::sqrt (s / std::max<size_t>(1, std::min(b,x.size()) - a)); }

int main()
{
    _mm_setcsr (_mm_getcsr() | 0x8040);   // flush denormals
    const double fs = 48000;
    const char* nm[] = { "Vintage", "Keys", "Hi-Gain", "Studio" };

    std::printf ("== Decay: set (x model scale) vs measured T60, default controls ==\n");
    for (int m = 0; m < 4; ++m)
        for (double d : { 1.0, 2.0, 4.0 })
        {
            auto ir = impulseL (m, d, 0.5, 0.5, 0.5, 0.2, fs, 14);
            std::printf ("%-8s decay %.1f -> target %.2f s, measured T60 %.2f s\n", nm[m], d, d * kModels[m].decayScale, t60 (ir, fs));
        }

    std::printf ("\n== Chirp direction: arrival time of low vs high band through the dispersive chain (Vintage) ==\n");
    {
        AllpassChain c; c.init (90); c.setSections (90); c.setTau (8.0);
        std::vector<double> ir (8000); for (size_t i = 0; i < ir.size(); ++i) ir[i] = c.process (i == 0 ? 1.0 : 0.0);
        for (double f : { 200.0, 800.0, 3000.0, 8000.0 })
        {
            Biquad bp; bp.bandpass (fs, f, 4.0); size_t pk = 0; double best = 0;
            for (size_t i = 0; i < ir.size(); ++i) { const double y = std::fabs (bp.process (ir[i])); if (y > best) { best = y; pk = i; } }
            std::printf ("  band %5.0f Hz peaks at %.1f ms\n", f, 1000.0 * (double) pk / fs);
        }
    }

    std::printf ("\n== Tension: echo spacing (autocorrelation peak 15..120 ms of IR tail) ==\n");
    for (int m : { 0, 1 })
        for (double t : { 0.2, 0.5, 0.8 })
        {
            auto ir = impulseL (m, 3.0, t, 0.5, 0.0, 0.0, fs, 3);
            double best = 0; int lag = 0; const int lo = (int) (0.015 * fs), hi = (int) (0.12 * fs);
            for (int L = lo; L < hi; ++L) { double s = 0; for (size_t i = (size_t) (0.2 * fs); i + L < ir.size(); ++i) s += ir[i] * ir[i + L]; if (s > best) { best = s; lag = L; } }
            std::printf ("%-8s tension %.1f -> dominant spacing %.1f ms\n", nm[m], t, 1000.0 * lag / fs);
        }

    std::printf ("\n== Boingy: energy in first 150 ms for a click, boingy 0 / 0.5 / 1 (dB re boingy 0) ==\n");
    for (int m = 0; m < 4; ++m)
    {
        double base = 0; std::printf ("%-8s", nm[m]);
        for (double b : { 0.0, 0.5, 1.0 })
        {
            // a short burst of noise = transient
            SpringReverb r; r.prepare (fs); r.setModel (m); r.setParams (2.0, 0.5, 0.5, b, 0.2);
            uint32_t s = 1; double e = 0;
            for (int i = 0; i < (int) (0.5 * fs); ++i)
            {
                s = s * 1664525u + 1013904223u; const float w = ((float) (s >> 8) / 8388608.f - 1.f) * 0.5f;
                float l, rr; r.process (i < 200 ? w : 0.f, i < 200 ? w : 0.f, l, rr);
                if (i < 0.15 * fs) e += (double) l * l;
            }
            if (b == 0.0) base = e;
            std::printf ("  b=%.1f: %+5.1f dB", b, 10 * std::log10 (e / base));
        }
        std::printf ("\n");
    }

    std::printf ("\n== Level: wet RMS / dry RMS for -18 dBFS noise, default controls ==\n");
    for (int m = 0; m < 4; ++m)
    {
        SpringReverb r; r.prepare (fs); r.setModel (m); r.setParams (2.0, 0.5, 0.5, 0.5, 0.2);
        uint32_t s = 7; double lp = 0, sw = 0, sd = 0; long n = 0;
        for (int i = 0; i < (int) (6 * fs); ++i)
        {
            s = s * 1664525u + 1013904223u; const double w = (double) (s >> 8) / 8388608.0 - 1.0; lp += 0.35 * (w - lp);
            const float x = (float) (lp * 0.35); float l, rr; r.process (x, x, l, rr);
            if (i > 2 * fs) { sw += (double) l * l; sd += (double) x * x; n++; }
        }
        std::printf ("%-8s wet/dry = %+.1f dB\n", nm[m], 10 * std::log10 (sw / sd));
    }

    std::printf ("\n== Stability: max drive, full-scale noise, long decay, extreme settings ==\n");
    for (int m = 0; m < 4; ++m)
    {
        SpringReverb r; r.prepare (fs); r.setModel (m); r.setParams (8.0, 1.0, 1.0, 1.0, 1.0);
        uint32_t s = 3; double pk = 0; bool bad = false;
        for (int i = 0; i < (int) (10 * fs); ++i)
        {
            s = s * 1664525u + 1013904223u; const float w = ((float) (s >> 8) / 8388608.f - 1.f);
            float l, rr; r.process (i < 5 * fs ? w : 0.f, i < 5 * fs ? -w : 0.f, l, rr);
            if (! std::isfinite (l) || ! std::isfinite (rr)) bad = true; pk = std::max (pk, (double) std::fabs (l));
        }
        std::printf ("%-8s peak %.2f %s\n", nm[m], pk, bad ? "NON-FINITE" : "ok");
    }

    std::printf ("\n== CPU: 10 s stereo at 48k, all four models ==\n");
    for (int m = 0; m < 4; ++m)
    {
        SpringReverb r; r.prepare (fs); r.setModel (m); r.setParams (2.0, 0.5, 0.5, 0.5, 0.3);
        auto t0 = std::chrono::steady_clock::now(); float l, rr, acc = 0;
        for (int i = 0; i < (int) (10 * fs); ++i) { r.process (0.01f * (float) (i & 7), 0.01f, l, rr); acc += l; }
        double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("%-8s %.2f s for 10 s audio (%.1f%% of one core) [%g]\n", nm[m], sec, sec * 10, (double) acc);
    }
    return 0;
}
