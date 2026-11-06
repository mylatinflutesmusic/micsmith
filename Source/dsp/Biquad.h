#pragma once

#include <cmath>

namespace micsmith
{
constexpr double kPi = 3.14159265358979323846;

enum class BandType
{
    Peak,
    LowShelf,
    HighShelf,
    HighPass,
    LowPass
};

/** One parametric band. */
struct Band
{
    BandType type   = BandType::Peak;
    double   freq   = 1000.0;
    double   q      = 0.707;
    double   gainDb = 0.0;
};

/** Normalised biquad coefficients (a0 == 1). */
struct Coeffs
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a1 = 0.0, a2 = 0.0;
};

/**
    RBJ audio-EQ-cookbook coefficients.

    gainScale multiplies the band gain. Because every band type used for mic
    modelling (peak / shelves) has coefficients that are exactly symmetric under
    A -> 1/A, passing gainScale = -1 produces the exact mathematical inverse of
    the same band. That property is what makes "source -> target" modelling a
    simple cascade: target bands, then the source bands inverted.
*/
inline Coeffs makeCoeffs (const Band& band, double sampleRate, double gainScale = 1.0)
{
    const double sr = sampleRate > 0.0 ? sampleRate : 48000.0;

    double f = band.freq;
    if (f < 10.0)            f = 10.0;
    if (f > sr * 0.49)       f = sr * 0.49;

    const double q = band.q > 1.0e-4 ? band.q : 0.707;
    const double w0 = 2.0 * kPi * f / sr;
    const double cw = std::cos (w0);
    const double sw = std::sin (w0);
    const double alpha = sw / (2.0 * q);
    const double A = std::pow (10.0, (band.gainDb * gainScale) / 40.0);

    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a0 = 1.0, a1 = 0.0, a2 = 0.0;

    switch (band.type)
    {
        case BandType::Peak:
            b0 = 1.0 + alpha * A;
            b1 = -2.0 * cw;
            b2 = 1.0 - alpha * A;
            a0 = 1.0 + alpha / A;
            a1 = -2.0 * cw;
            a2 = 1.0 - alpha / A;
            break;

        case BandType::LowShelf:
        {
            const double s = 2.0 * std::sqrt (A) * alpha;
            b0 = A * ((A + 1.0) - (A - 1.0) * cw + s);
            b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cw);
            b2 = A * ((A + 1.0) - (A - 1.0) * cw - s);
            a0 = (A + 1.0) + (A - 1.0) * cw + s;
            a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cw);
            a2 = (A + 1.0) + (A - 1.0) * cw - s;
            break;
        }

        case BandType::HighShelf:
        {
            const double s = 2.0 * std::sqrt (A) * alpha;
            b0 = A * ((A + 1.0) + (A - 1.0) * cw + s);
            b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cw);
            b2 = A * ((A + 1.0) + (A - 1.0) * cw - s);
            a0 = (A + 1.0) - (A - 1.0) * cw + s;
            a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cw);
            a2 = (A + 1.0) - (A - 1.0) * cw - s;
            break;
        }

        case BandType::HighPass:
            b0 = (1.0 + cw) * 0.5;
            b1 = -(1.0 + cw);
            b2 = (1.0 + cw) * 0.5;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cw;
            a2 = 1.0 - alpha;
            break;

        case BandType::LowPass:
            b0 = (1.0 - cw) * 0.5;
            b1 = 1.0 - cw;
            b2 = (1.0 - cw) * 0.5;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cw;
            a2 = 1.0 - alpha;
            break;
    }

    Coeffs c;
    const double inv = 1.0 / a0;
    c.b0 = b0 * inv;
    c.b1 = b1 * inv;
    c.b2 = b2 * inv;
    c.a1 = a1 * inv;
    c.a2 = a2 * inv;
    return c;
}

/** |H(e^jw)| for a normalised biquad. */
inline double magnitudeAt (const Coeffs& c, double freq, double sampleRate)
{
    const double sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    const double w = 2.0 * kPi * freq / sr;
    const double cw = std::cos (w), sw = std::sin (w);
    const double c2w = std::cos (2.0 * w), s2w = std::sin (2.0 * w);

    const double nr = c.b0 + c.b1 * cw + c.b2 * c2w;
    const double ni = -(c.b1 * sw + c.b2 * s2w);
    const double dr = 1.0 + c.a1 * cw + c.a2 * c2w;
    const double di = -(c.a1 * sw + c.a2 * s2w);

    const double num = nr * nr + ni * ni;
    const double den = dr * dr + di * di;

    if (den < 1.0e-30)
        return 1.0;

    return std::sqrt (num / den);
}

/** True when both poles of 1 + a1 z^-1 + a2 z^-2 lie strictly inside the unit circle. */
inline bool isStable (const Coeffs& c)
{
    return std::abs (c.a2) < 1.0 && std::abs (c.a1) < 1.0 + c.a2;
}

/** Transposed direct form II. */
struct Biquad
{
    Coeffs c;
    double z1 = 0.0, z2 = 0.0;

    void reset() noexcept { z1 = z2 = 0.0; }

    inline double process (double x) noexcept
    {
        const double y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return y;
    }
};
} // namespace micsmith
