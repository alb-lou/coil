// Coil DSP core - plain C++ (no JUCE), so it can be unit-tested.
// Structure: dispersive-allpass-chain waveguide loops (the standard physical-modelling approach
// to spring reverb, after Parker / Abel et al.), driven by a saturating "amp" stage.
// All models are original voicings built from published spring-reverb behaviour.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace spr
{
constexpr double kPi = 3.14159265358979323846;
inline double dbToLin (double db) { return std::pow (10.0, db / 20.0); }
inline double linToDb (double l)  { return 20.0 * std::log10 (std::max (l, 1e-12)); }

struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    inline double process (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void set (double B0, double B1, double B2, double A0, double A1, double A2)
    { b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0; }
    void lowpass (double fs, double f, double q = 0.70710678)
    {
        f = std::min (f, 0.49 * fs);
        const double w = 2 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void highpass (double fs, double f, double q = 0.70710678)
    {
        f = std::min (f, 0.49 * fs);
        const double w = 2 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void bandpass (double fs, double f, double q)
    {
        f = std::min (f, 0.49 * fs);
        const double w = 2 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        set (al, 0, -al, 1 + al, -2 * c, 1 - al);
    }
    void onePoleLP (double fs, double f)
    { f = std::min (f, 0.49 * fs); const double K = std::tan (kPi * f / fs); set (K, K, 0, 1 + K, K - 1, 0); }
    void onePoleHP (double fs, double f)
    { f = std::min (f, 0.49 * fs); const double K = std::tan (kPi * f / fs); set (1, -1, 0, 1 + K, K - 1, 0); }
    void lowShelf (double fs, double f, double gDb, double q = 0.70710678)
    {
        f = std::min (f, 0.49 * fs);
        const double A = std::pow (10.0, gDb / 40), w = 2 * kPi * f / fs, c = std::cos (w);
        const double al = std::sin (w) / (2 * q), sq = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
             (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
    }
    void highShelf (double fs, double f, double gDb, double q = 0.70710678)
    {
        f = std::min (f, 0.49 * fs);
        const double A = std::pow (10.0, gDb / 40), w = 2 * kPi * f / fs, c = std::cos (w);
        const double al = std::sin (w) / (2 * q), sq = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
             (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
    }
    void peaking (double fs, double f, double gDb, double q)
    {
        f = std::min (f, 0.49 * fs);
        const double A = std::pow (10.0, gDb / 40), w = 2 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
};

// Cascade of identical first-order allpass sections H(z) = (a + z^-1) / (1 + a z^-1).
// With a < 0 low frequencies are delayed more than highs -> the falling "chirp" of a spring.
// Per-section DC group delay is tau = (1 - a) / (1 + a) samples.
struct AllpassChain
{
    std::vector<double> st;
    int K = 0;
    double a = -0.7;
    void init (int maxSections) { st.assign ((size_t) maxSections, 0.0); K = maxSections; }
    void setSections (int k) { if (k != K) { K = std::min (k, (int) st.size()); std::fill (st.begin(), st.end(), 0.0); } }
    void setTau (double tau) { a = (1.0 - tau) / (1.0 + tau); }
    void reset() { std::fill (st.begin(), st.end(), 0.0); }
    inline double process (double y)
    {
        const double aa = a;
        for (int k = 0; k < K; ++k)
        {
            const double o = aa * y + st[(size_t) k];
            st[(size_t) k] = y - aa * o;
            y = o;
        }
        return y;
    }
};

// Voicing of each spring tank. Names are descriptive, not product names.
struct Model
{
    const char* name;
    double loopMs;       // round-trip time of the first spring
    int    sections;     // dispersive sections per spring
    double tau;          // per-section DC delay (samples @ 48 kHz) = amount of chirp
    double decayScale;   // multiplies the Decay control
    double dampHz;       // high-frequency damping inside the loop
    double hpHz;         // low cut inside the loop
    double inLoHz, inHiHz; // transducer band-limit on the way in
    double boing;        // strength of the transient "boing" branch
    double driveBase;    // built-in amp drive (0..1)
    double spread;       // loop-time ratio between the parallel springs
    double trim;         // output level trim
};

inline const Model kModels[4] = {
    //  name       loop  K    tau  decay  damp  hp   inLo inHi boing drive spread trim
    { "Vintage",   38,  90,  8.0, 1.00, 4200, 110, 160, 4800, 1.00, 0.10, 1.13, 1.0 },
    { "Keys",      24,  60,  4.3, 0.80, 5600, 140, 220, 6500, 0.70, 0.00, 1.09, 1.0 },
    { "Hi-Gain",   46, 110,  9.0, 1.25, 3300, 100, 120, 3800, 1.30, 0.35, 1.16, 1.0 },
    { "Studio",    31,  75,  5.8, 0.95, 5200, 120, 150, 7000, 0.55, 0.00, 1.11, 1.0 },
};
constexpr int kNumModels = 4;
constexpr int kMaxSections = 128;

// One spring: a delay line feeding a dispersive chain, damping, and back into the delay.
class SpringLine
{
public:
    void prepare (double fsIn, int seed)
    {
        fs = fsIn;
        int n = 1; while (n < (int) (fs * 0.35)) n <<= 1;
        buf.assign ((size_t) n, 0.0f); mask = n - 1; wp = 0;
        chain.init (kMaxSections);
        slew = 1.0 - std::exp (-1.0 / (0.08 * fs));
        slewTau = 1.0 - std::exp (-16.0 / (0.08 * fs));
        lfoPhase = 0.9 * seed; lfoInc = 2 * kPi * (0.41 + 0.17 * seed) / fs;
        lp.reset(); hp.reset(); first = true;
    }
    void setSections (int k) { chain.setSections (k); }
    // totalLoopSamples = desired round trip; tau at current fs; t60 in seconds
    void setTarget (double totalLoopSamples, double tau, double t60, double dampHz, double hpHz, double lfoDepthSamples)
    {
        tauTarget = tau;
        const double apDelay = chain.K * tau;
        delayTarget = std::max (totalLoopSamples - apDelay, 24.0);
        const double actual = delayTarget + apDelay;
        g = std::pow (10.0, -3.0 * (actual / fs) / std::max (t60, 0.05));
        g = std::min (g, 0.9995);
        norm = std::sqrt (std::max (1.0 - g * g, 1e-4));
        lp.onePoleLP (fs, dampHz); hp.onePoleHP (fs, hpHz);
        lfoDepth = lfoDepthSamples;
        if (first) { delay = delayTarget; tauCur = tau; chain.setTau (tauCur); first = false; }
    }
    inline double process (double in)
    {
        delay += slew * (delayTarget - delay);
        if ((++sub & 15) == 0) { tauCur += slewTau * (tauTarget - tauCur); chain.setTau (tauCur); }
        lfoPhase += lfoInc; if (lfoPhase > 2 * kPi) lfoPhase -= 2 * kPi;
        const double d = delay + lfoDepth * std::sin (lfoPhase);
        const double rp = (double) wp - d;
        const long long ir = (long long) std::floor (rp);
        const double fr = rp - (double) ir;
        const double s0 = buf[(size_t) (ir & mask)], s1 = buf[(size_t) ((ir + 1) & mask)];
        double y = s0 + fr * (s1 - s0);
        y = chain.process (y);
        y = lp.process (y);
        y = hp.process (y);
        buf[(size_t) (wp & mask)] = (float) (in + g * y);
        wp = (wp + 1) & mask;
        return y * norm;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); chain.reset(); lp.reset(); hp.reset(); }

private:
    double fs = 48000, g = 0.9, norm = 0.4, delay = 1000, delayTarget = 1000, slew = 0.001, slewTau = 0.01;
    double tauCur = 8, tauTarget = 8, lfoPhase = 0, lfoInc = 0, lfoDepth = 0;
    std::vector<float> buf; int mask = 0; long long wp = 0; unsigned sub = 0; bool first = true;
    AllpassChain chain; Biquad lp, hp;
};

// Tube-ish saturation of the stage that drives the spring, with static level compensation.
struct DriveStage
{
    double g = 1, comp = 1, bias = 0.15, tb = 0, sl = 1;
    DriveStage() { tb = std::tanh (bias); sl = 1.0 - tb * tb; }
    void set (double drive01, double c) { g = dbToLin (drive01 * 30.0); comp = c; }
    inline float process (float x) const { return (float) (comp * (std::tanh (g * x + bias) - tb) / (g * sl)); }
};

// Fast-minus-slow envelope: lights up on attacks
struct TransientDetector
{
    double fast = 0, slow = 0, af = 0, rf = 0, as = 0, rs = 0;
    void prepare (double fs)
    {
        af = std::exp (-1.0 / (0.001 * fs)); rf = std::exp (-1.0 / (0.02 * fs));
        as = std::exp (-1.0 / (0.02 * fs));  rs = std::exp (-1.0 / (0.25 * fs));
        fast = slow = 0;
    }
    inline double process (double x)
    {
        const double a = std::fabs (x);
        fast = a + (a > fast ? af : rf) * (fast - a);
        slow = a + (a > slow ? as : rs) * (slow - a);
        return std::min (std::max ((fast - slow) / (slow + 0.02), 0.0), 1.0);
    }
};

class SpringReverb
{
public:
    static constexpr int kDriveSteps = 33;

    void prepare (double fsIn)
    {
        fs = fsIn;
        for (int i = 0; i < 4; ++i) lines[(size_t) i].prepare (fs, i);
        boingChain.init (64); boingChain.setSections (56); boingChain.setTau (14.0 * fs / 48000.0);
        earlyL.init (32); earlyR.init (32);
        earlyL.setSections (28); earlyR.setSections (28);
        earlyL.setTau (3.5 * fs / 48000.0); earlyR.setTau (3.9 * fs / 48000.0);
        det.prepare (fs);
        computeDriveComp();
        for (auto* b : { &inHpL, &inHpR, &inLpL, &inLpR, &shelfL, &shelfR }) b->reset();
        setModel (0);
        setParams (2.0, 0.5, 0.5, 0.5, 0.2);
    }

    void setModel (int m)
    {
        model = std::min (std::max (m, 0), kNumModels - 1);
        const auto& M = kModels[model];
        for (auto& l : lines) l.setSections (M.sections);
        for (auto* b : { &inHpL, &inHpR }) b->onePoleHP (fs, M.inLoHz);
        for (auto* b : { &inLpL, &inLpR }) b->lowpass (fs, M.inHiHz);
    }

    void setParams (double decaySec, double tension, double bright, double boingy, double drive01)
    {
        const auto& M = kModels[model];
        const double t60u = decaySec * M.decayScale;
        const double t60 = t60u * (1.2 + 0.14 * t60u);   // compensates for damping so the knob reads close to the true T60
        const double loopScale = std::exp (-(tension - 0.5) * 1.0);        // tighter spring -> shorter round trip
        const double damp = M.dampHz * std::pow (2.0, (bright - 0.5) * 2.5) * std::pow (1.4, (tension - 0.5) * 2.0);
        const double tau = M.tau * (fs / 48000.0) * (0.75 + 0.5 * boingy);  // boingy also deepens the chirp
        const double s = M.spread;
        const double mult[4] = { 1.0, s, 1.0 + (s - 1.0) * 0.45, 1.0 + (s - 1.0) * 1.55 };
        for (int i = 0; i < 4; ++i)
            lines[(size_t) i].setTarget (M.loopMs * 0.001 * fs * loopScale * mult[i], tau, t60, damp, M.hpHz, 0.4 * fs / 48000.0);
        boingAmt = boingy * M.boing * 2.4;
        const double d = std::min (std::max (drive01 + M.driveBase, 0.0), 1.0);
        const double pos = d * (kDriveSteps - 1); const int i0 = std::min ((int) pos, kDriveSteps - 2);
        const double c = driveComp[(size_t) i0] + (pos - i0) * (driveComp[(size_t) i0 + 1] - driveComp[(size_t) i0]);
        drvL.set (d, c); drvR.set (d, c);
        const double shelf = (bright - 0.5) * 12.0;
        shelfL.highShelf (fs, 3000.0, shelf); shelfR.highShelf (fs, 3000.0, shelf);
        outGain = M.trim;
    }

    inline void process (float inL, float inR, float& wetL, float& wetR)
    {
        const double bl = inHpL.process (inLpL.process (drvL.process (inL)));
        const double br = inHpR.process (inLpR.process (drvR.process (inR)));
        const double tr = det.process (0.5 * ((double) inL + (double) inR));
        const double boing = boingAmt * boingChain.process (tr * 0.5 * (bl + br));
        const double sigL = 0.75 * bl + 0.25 * br + boing, sigR = 0.25 * bl + 0.75 * br + boing;
        const double l = lines[0].process (sigL) + lines[1].process (sigL);
        const double r = lines[2].process (sigR) + lines[3].process (sigR);
        const double eL = earlyL.process (bl) + 0.5 * boing, eR = earlyR.process (br) + 0.5 * boing;
        wetL = (float) (shelfL.process ((0.7 * l + 0.5 * eL) * outGain));
        wetR = (float) (shelfR.process ((0.7 * r + 0.5 * eR) * outGain));
    }

    void reset()
    {
        for (auto& l : lines) l.reset();
        boingChain.reset(); earlyL.reset(); earlyR.reset(); det.prepare (fs);
        for (auto* b : { &inHpL, &inHpR, &inLpL, &inLpR, &shelfL, &shelfR }) b->reset();
    }
    int getModel() const { return model; }

private:
    void computeDriveComp()
    {
        const int len = (int) (0.25 * 48000);
        std::vector<double> ref ((size_t) len);
        uint32_t s = 12345; double lpS = 0, ss = 0;
        for (auto& v : ref) { s = s * 1664525u + 1013904223u; const double w = (double) (s >> 8) / 8388608.0 - 1.0; lpS += 0.3 * (w - lpS); v = lpS; ss += v * v; }
        const double sc = dbToLin (-18.0) / std::sqrt (ss / len);
        for (int i = 0; i < kDriveSteps; ++i)
        {
            DriveStage d; d.set ((double) i / (kDriveSteps - 1), 1.0);
            double sIn = 0, sOut = 0;
            for (auto v : ref) { const double x = v * sc; const double y = d.process ((float) x); sIn += x * x; sOut += y * y; }
            driveComp[(size_t) i] = std::sqrt (sIn / std::max (sOut, 1e-18));
        }
    }

    double fs = 48000, boingAmt = 0.5, outGain = 1.0;
    int model = 0;
    std::array<SpringLine, 4> lines;
    AllpassChain boingChain, earlyL, earlyR;
    TransientDetector det;
    DriveStage drvL, drvR;
    Biquad inHpL, inHpR, inLpL, inLpR, shelfL, shelfR;
    std::array<double, kDriveSteps> driveComp {};
};
} // namespace spr
