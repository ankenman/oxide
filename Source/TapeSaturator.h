#pragma once

// Pure C++ tape-saturation core. No JUCE dependency so it can be unit-tested
// on its own. One instance per channel; call prepare() before process().
//
// Signal path (runs at the oversampled rate):
//   in -> drive gain -> pre-emphasis shelf -> asymmetric soft saturator
//      -> DC blocker -> de-emphasis shelf -> HF roll-off -> makeup -> out
//
// The tone switch (Normal / Medium / Bright) changes how hard the highs are
// pushed into the saturator and how much top end is rolled off afterwards.

#include <cmath>
#include <algorithm>

namespace oxide
{

// ---------------------------------------------------------------------------
// Minimal transposed-direct-form-II biquad with RBJ cookbook designers.
// ---------------------------------------------------------------------------
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    void reset() noexcept { z1 = z2 = 0; }

    inline float process (float x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return static_cast<float> (y);
    }

    void setIdentity() noexcept { b0 = 1; b1 = b2 = a1 = a2 = 0; }

    // High shelf, gainDb applied above freq.
    void setHighShelf (double sampleRate, double freq, double gainDb, double q = 0.7071)
    {
        const double A     = std::pow (10.0, gainDb / 40.0);
        const double w0    = 2.0 * M_PI * std::min (freq, sampleRate * 0.45) / sampleRate;
        const double cosw  = std::cos (w0), sinw = std::sin (w0);
        const double alpha = sinw / (2.0 * q);
        const double sqA2a = 2.0 * std::sqrt (A) * alpha;

        const double a0 =  (A + 1) - (A - 1) * cosw + sqA2a;
        b0 =  A * ((A + 1) + (A - 1) * cosw + sqA2a) / a0;
        b1 = -2 * A * ((A - 1) + (A + 1) * cosw)      / a0;
        b2 =  A * ((A + 1) + (A - 1) * cosw - sqA2a) / a0;
        a1 =  2 * ((A - 1) - (A + 1) * cosw)          / a0;
        a2 = ((A + 1) - (A - 1) * cosw - sqA2a)       / a0;
    }

    void setLowPass (double sampleRate, double freq, double q = 0.7071)
    {
        if (freq >= sampleRate * 0.45) { setIdentity(); return; }
        const double w0    = 2.0 * M_PI * freq / sampleRate;
        const double cosw  = std::cos (w0), sinw = std::sin (w0);
        const double alpha = sinw / (2.0 * q);
        const double a0    = 1 + alpha;
        b0 = (1 - cosw) / 2 / a0;
        b1 = (1 - cosw)     / a0;
        b2 = b0;
        a1 = -2 * cosw      / a0;
        a2 = (1 - alpha)    / a0;
    }

    void setHighPass (double sampleRate, double freq, double q = 0.7071)
    {
        const double w0    = 2.0 * M_PI * freq / sampleRate;
        const double cosw  = std::cos (w0), sinw = std::sin (w0);
        const double alpha = sinw / (2.0 * q);
        const double a0    = 1 + alpha;
        b0 =  (1 + cosw) / 2 / a0;
        b1 = -(1 + cosw)     / a0;
        b2 = b0;
        a1 = -2 * cosw       / a0;
        a2 = (1 - alpha)     / a0;
    }
};

// ---------------------------------------------------------------------------
enum class Tone { Normal = 0, Medium = 1, Bright = 2 };

struct ToneProfile
{
    double preShelfDb;      // HF boost into the saturator
    double preShelfHz;
    double deEmphasisRatio; // fraction of the pre-emphasis undone afterwards (1 = flat)
    double rollOffHz;       // post low-pass ("tape head" HF loss)
};

inline ToneProfile toneProfile (Tone t) noexcept
{
    switch (t)
    {
        case Tone::Normal: return { 2.0, 2500.0, 1.00, 16500.0 };
        case Tone::Medium: return { 5.0, 3200.0, 0.85, 20000.0 };
        case Tone::Bright: return { 8.0, 4000.0, 0.65, 60000.0 };
    }
    return { 2.0, 2500.0, 1.0, 14000.0 };
}

// ---------------------------------------------------------------------------
class TapeChannel
{
public:
    static constexpr float kMaxDriveDb = 36.0f;

    void prepare (double oversampledRate)
    {
        fs = oversampledRate;
        dcBlock.setHighPass (fs, 8.0);
        setTone (tone, true);
        reset();
    }

    void reset() noexcept
    {
        preEmph.reset(); deEmph.reset(); rollOff.reset(); dcBlock.reset();
    }

    void setTone (Tone t, bool force = false)
    {
        if (t == tone && ! force) return;
        tone = t;
        const auto p = toneProfile (t);
        preEmph.setHighShelf (fs, p.preShelfHz,  p.preShelfDb);
        deEmph .setHighShelf (fs, p.preShelfHz, -p.preShelfDb * p.deEmphasisRatio);
        rollOff.setLowPass   (fs, p.rollOffHz);
    }

    // drive: 0..1 (knob position). Returns the drive gain in dB (for display).
    static float driveToDb (float drive) noexcept
    {
        // Gentle curve: more resolution in the subtle range, big gains at the top.
        const float d = std::clamp (drive, 0.0f, 1.0f);
        return kMaxDriveDb * (0.35f * d + 0.65f * d * d);
    }

    // driveGain: linear pre-gain. trimGain: linear output gain.
    inline float process (float x, float driveGain, float trimGain) noexcept
    {
        float s = x * driveGain;
        s = preEmph.process (s);
        s = saturate (s, driveGain);
        s = dcBlock.process (s);
        s = deEmph.process (s);
        s = rollOff.process (s);

        // Partial makeup (-1/4 of the drive gain in dB): the output still gets
        // louder as you push Drive, like the real thing, but stays in a range
        // where Trim can comfortably bring it back.
        const float makeup = std::pow (std::max (1.0f, driveGain), -0.25f);
        return s * makeup * trimGain;
    }

private:
    // Asymmetric tanh: the bias term adds even harmonics (tape-like "fullness"),
    // scaled so it grows with drive and vanishes at unity gain. The subtraction
    // keeps the static operating point at zero; the DC blocker catches the rest.
    // kKnee sets how hot the signal has to be before it bends: 0.5 means a
    // 0 dBFS sine at Drive = 0 only just starts to round over.
    static constexpr float kKnee = 0.5f;

    static inline float saturate (float s, float driveGain) noexcept
    {
        const float bias = 0.12f * std::min (1.0f, (driveGain - 1.0f) * 0.25f);
        const float k    = kKnee * (1.0f + 0.15f * std::tanh (bias * 4.0f));
        const float y    = std::tanh (k * s + bias) - std::tanh (bias);
        return y / kKnee;
    }

    double fs = 44100.0 * 4.0;
    Tone   tone = Tone::Normal;
    Biquad preEmph, deEmph, rollOff, dcBlock;
};

} // namespace oxide
