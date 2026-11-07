#pragma once

#include <cmath>
#include <vector>

namespace quell
{
/**
    In-place iterative radix-2 complex FFT.

    This is deliberately small and dependency-free: the DSP tests build without
    JUCE, and the transform is only ever run a few hundred times a second.
*/
class Fft
{
public:
    explicit Fft (int size)
        : n (size),
          bits (0),
          bitReverse ((size_t) size),
          cosTable ((size_t) size / 2 + 1),
          sinTable ((size_t) size / 2 + 1)
    {
        while ((1 << bits) < n)
            ++bits;

        for (int i = 0; i < n; ++i)
        {
            int x = i;
            int r = 0;

            for (int b = 0; b < bits; ++b)
            {
                r = (r << 1) | (x & 1);
                x >>= 1;
            }

            bitReverse[(size_t) i] = r;
        }

        const double twoPi = 6.283185307179586476925286766559;

        for (int k = 0; k <= n / 2; ++k)
        {
            cosTable[(size_t) k] = (float) std::cos (twoPi * (double) k / (double) n);
            sinTable[(size_t) k] = (float) std::sin (twoPi * (double) k / (double) n);
        }
    }

    int getSize() const noexcept { return n; }

    void forward (float* re, float* im) const noexcept { transform (re, im, false); }

    /** Inverse transform, including the 1/N scaling. */
    void inverse (float* re, float* im) const noexcept { transform (re, im, true); }

private:
    void transform (float* re, float* im, bool inverseTransform) const noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const int j = bitReverse[(size_t) i];

            if (j > i)
            {
                std::swap (re[i], re[j]);
                std::swap (im[i], im[j]);
            }
        }

        const float sign = inverseTransform ? 1.0f : -1.0f;

        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len >> 1;
            const int stride = n / len;

            for (int start = 0; start < n; start += len)
            {
                for (int j = 0; j < half; ++j)
                {
                    const int k = j * stride;
                    const float wr = cosTable[(size_t) k];
                    const float wi = sign * sinTable[(size_t) k];

                    const int a = start + j;
                    const int b = a + half;

                    const float tr = re[b] * wr - im[b] * wi;
                    const float ti = re[b] * wi + im[b] * wr;

                    re[b] = re[a] - tr;
                    im[b] = im[a] - ti;
                    re[a] += tr;
                    im[a] += ti;
                }
            }
        }

        if (inverseTransform)
        {
            const float scale = 1.0f / (float) n;

            for (int i = 0; i < n; ++i)
            {
                re[i] *= scale;
                im[i] *= scale;
            }
        }
    }

    int n;
    int bits;
    std::vector<int> bitReverse;
    std::vector<float> cosTable, sinTable;
};
} // namespace quell
