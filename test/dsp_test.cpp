#include "TapeSaturator.h"
#include <cstdio>
#include <vector>
#include <complex>

using namespace oxide;

// Goertzel magnitude at a given bin frequency
static double mag (const std::vector<float>& x, double fs, double f)
{
    const double w = 2 * M_PI * f / fs;
    std::complex<double> acc = 0;
    for (size_t n = 0; n < x.size(); ++n) acc += (double) x[n] * std::polar (1.0, -w * (double) n);
    return 2.0 * std::abs (acc) / (double) x.size();
}

int main()
{
    const double fs = 44100.0 * 4;
    const int N = 1 << 16;
    const double f0 = fs / N * 60; // bin-aligned ~161 Hz
    const double levelDb = -18.0;

    for (int t = 0; t < 3; ++t)
    for (float drive : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
    {
        TapeChannel ch; ch.prepare (fs); ch.setTone ((Tone) t, true);
        const float g = std::pow (10.0f, TapeChannel::driveToDb (drive) / 20.0f);
        std::vector<float> y (N);
        const float a = std::pow (10.0f, (float) levelDb / 20.0f);
        // warm up filters
        for (int n = 0; n < 20000; ++n) ch.process (a * std::sin (2 * M_PI * f0 * n / fs), g, 1.0f);
        double dc = 0;
        for (int n = 0; n < N; ++n) { y[n] = ch.process (a * std::sin (2 * M_PI * f0 * (n + 20000) / fs), g, 1.0f); dc += y[n]; }
        dc /= N;
        const double h1 = mag (y, fs, f0), h2 = mag (y, fs, 2 * f0), h3 = mag (y, fs, 3 * f0);
        std::printf ("tone %d drive %.2f (+%4.1f dB): out %6.1f dBFS  H2 %6.1f dBc  H3 %6.1f dBc  DC %8.2e\n",
                     t, drive, TapeChannel::driveToDb (drive),
                     20 * std::log10 (h1), 20 * std::log10 (h2 / h1), 20 * std::log10 (h3 / h1), dc);
    }

    // frequency response at drive 0 per tone (small signal)
    for (int t = 0; t < 3; ++t)
    {
        std::printf ("tone %d small-signal response: ", t);
        for (double f : { 100.0, 1000.0, 5000.0, 10000.0, 16000.0 })
        {
            TapeChannel ch; ch.prepare (fs); ch.setTone ((Tone) t, true);
            std::vector<float> y (N);
            for (int n = 0; n < 20000; ++n) ch.process (0.01f * std::sin (2 * M_PI * f * n / fs), 1.0f, 1.0f);
            for (int n = 0; n < N; ++n) y[n] = ch.process (0.01f * std::sin (2 * M_PI * f * (n + 20000) / fs), 1.0f, 1.0f);
            std::printf ("%5.0fHz %+5.1f  ", f, 20 * std::log10 (mag (y, fs, f) / 0.01));
        }
        std::printf ("\n");
    }
    return 0;
}
