// =============================================================================
//  Magneto - Meteora Plugins
//  Motore tape echo: carattere a meta' tra Echoplex EP-3 e Roland RE-201.
//  Nastro ad anello con testina di lettura modulata (wow / flutter / drift),
//  saturazione asimmetrica in registrazione (SUSTAIN), filtri nel loop di
//  feedback (ogni ripetizione si scurisce), head bump, fruscio e dropout.
// =============================================================================
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

class TapeEcho
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        const int size = (int) std::ceil (sr * 2.2) + 2048;

        for (int c = 0; c < 2; ++c)
        {
            ch[c].buf.assign ((size_t) size, 0.0f);
            ch[c].size = size;
            ch[c].rng = 0x9E3779B9u ^ (uint32_t) (c * 7919 + 17);
        }

        glideCoef  = 1.0 - std::exp (-1.0 / (0.12 * sr));
        smoothCoef = (float) (1.0 - std::exp (-1.0 / (0.03 * sr)));
        driftCoef  = (float) (1.0 - std::exp (-1.0 / (0.25 * sr)));
        dropCoef   = (float) (1.0 - std::exp (-1.0 / (0.006 * sr)));
        dropProb   = (float) (0.35 / sr);

        currentMode = -1;
        wowPh = flutPh = 0.0;
        reset();
    }

    void reset()
    {
        for (auto& c : ch)
        {
            std::fill (c.buf.begin(), c.buf.end(), 0.0f);
            c.w = 0;
            c.first = true;
            c.lp.reset(); c.hp.reset(); c.bump.reset();
            c.dcX = c.dcY = 0.0f;
            c.dropGain = c.dropTarget = 1.0f;
            c.dropTimer = 0;
            c.drift = c.driftTarget = 0.0f;
            c.driftTimer = 0;
        }
    }

    void setParams (float delayMs, float record01, float repeat01, int mode, bool stereoIn, bool echoBypass, float hiss01)
    {
        targetDelay = std::max (4.0, (double) delayMs * 0.001 * sr);
        driveTarget = 0.6f * std::pow (10.0f, std::clamp (record01, 0.0f, 1.0f));
        fbTarget    = 1.12f * std::pow (std::clamp (repeat01, 0.0f, 1.0f), 1.2f);
        inTarget    = echoBypass ? 0.0f : 1.0f;
        stereo      = stereoIn;

        // HISS: 0 = silenzio, 3 = livello di default del nastro, 10 = circa +18 dB
        const float h = std::clamp (hiss01, 0.0f, 1.0f) * 10.0f;
        hissMul = h <= 0.0f ? 0.0f : std::pow (h / 3.0f, 1.72f);

        mode = std::clamp (mode, 0, 3);
        if (mode != currentMode)
        {
            currentMode = mode;
            updateMode();
        }
    }

    void process (float inL, float inR, float& outL, float& outR)
    {
        drive  += (driveTarget - drive) * smoothCoef;
        fb     += (fbTarget - fb) * smoothCoef;
        inGain += (inTarget - inGain) * smoothCoef;

        const float logD = std::log (drive);
        driveComp = std::exp (-0.75f * logD);
        fbNorm    = std::exp (-0.25f * logD);

        wowPh  += wowInc;  if (wowPh  > twoPi) wowPh  -= twoPi;
        flutPh += flutInc; if (flutPh > twoPi) flutPh -= twoPi;

        if (stereo)
        {
            outL = processChannel (ch[0], inL, targetDelay, 0.0);
            outR = processChannel (ch[1], inR, targetDelay * 1.018, 1.0);
        }
        else
        {
            const float m = 0.5f * (inL + inR);
            outL = outR = processChannel (ch[0], m, targetDelay, 0.0);
        }
    }

private:
    static constexpr double twoPi = 6.283185307179586;

    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

        void reset() { z1 = z2 = 0.0; }

        void set (double nb0, double nb1, double nb2, double a0, double na1, double na2)
        {
            b0 = nb0 / a0; b1 = nb1 / a0; b2 = nb2 / a0; a1 = na1 / a0; a2 = na2 / a0;
        }

        void setLowpass (double fs, double f, double q)
        {
            const double w = twoPi * f / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
            set ((1 - c) * 0.5, 1 - c, (1 - c) * 0.5, 1 + al, -2 * c, 1 - al);
        }

        void setHighpass (double fs, double f, double q)
        {
            const double w = twoPi * f / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
            set ((1 + c) * 0.5, -(1 + c), (1 + c) * 0.5, 1 + al, -2 * c, 1 - al);
        }

        void setPeak (double fs, double f, double q, double gainDb)
        {
            const double A = std::pow (10.0, gainDb / 40.0);
            const double w = twoPi * f / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
            set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
        }

        float process (float x)
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return (float) y;
        }
    };

    struct Chan
    {
        std::vector<float> buf;
        int size = 0, w = 0;
        bool first = true;
        double dSm = 0.0;
        Biquad lp, hp, bump;
        float dcX = 0, dcY = 0;
        float dropGain = 1, dropTarget = 1;
        int dropTimer = 0;
        float drift = 0, driftTarget = 0;
        int driftTimer = 0;
        uint32_t rng = 1;

        uint32_t next() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
        float rnd()   { return (float) (next() * (2.0 / 4294967295.0) - 1.0); }
        float rnd01() { return (float) (next() * (1.0 / 4294967295.0)); }

        float read (double d) const
        {
            double rp = (double) w - d;
            if (rp < 0.0) rp += size;
            int i1 = (int) rp;
            const float f = (float) (rp - i1);
            if (i1 >= size) i1 -= size;
            int i0 = i1 - 1; if (i0 < 0) i0 += size;
            int i2 = i1 + 1; if (i2 >= size) i2 -= size;
            int i3 = i2 + 1; if (i3 >= size) i3 -= size;

            const float y0 = buf[(size_t) i0], y1 = buf[(size_t) i1], y2 = buf[(size_t) i2], y3 = buf[(size_t) i3];
            const float c1 = 0.5f * (y2 - y0);
            const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
            const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            return ((c3 * f + c2) * f + c1) * f + y1;
        }
    };

    struct TapeType
    {
        float wow, flutter, lpHz, hpHz, hissDb, bias, bumpDb;
        bool dropouts;
    };

    void updateMode()
    {
        // 1 Nuovo, 2 Studio (EP-3), 3 Usurato (RE-201), 4 Morente
        static const TapeType types[4] = {
            { 0.0008f, 0.0004f, 7500.0f,  60.0f, -80.0f, 0.04f, 1.5f, false },
            { 0.0016f, 0.0008f, 5200.0f, 110.0f, -72.0f, 0.10f, 2.5f, false },
            { 0.0032f, 0.0015f, 3600.0f,  90.0f, -66.0f, 0.15f, 3.0f, false },
            { 0.0060f, 0.0025f, 2400.0f, 140.0f, -60.0f, 0.20f, 2.0f, true  }
        };
        const auto& t = types[currentMode];

        const double wowHz = 0.65, flutHz = 7.3;
        wowInc  = twoPi * wowHz / sr;
        flutInc = twoPi * flutHz / sr;
        wowAmp   = t.wow * sr / (twoPi * wowHz);
        flutAmp  = t.flutter * sr / (twoPi * flutHz);
        driftAmp = wowAmp * 0.7;

        hissAmp  = std::pow (10.0f, t.hissDb / 20.0f) * 1.73f;
        bias     = t.bias;
        tanhBias = std::tanh (bias);
        dropouts = t.dropouts;

        for (auto& c : ch)
        {
            c.lp.setLowpass (sr, std::min ((double) t.lpHz, sr * 0.45), 0.707);
            c.hp.setHighpass (sr, t.hpHz, 0.707);
            c.bump.setPeak (sr, 100.0, 0.8, t.bumpDb);
        }
    }

    float processChannel (Chan& c, float in, double target, double phaseOff)
    {
        if (c.first) { c.dSm = target; c.first = false; }

        // cambio di tempo "a nastro": glide con velocita' massima limitata
        const double step = std::clamp ((target - c.dSm) * glideCoef, -0.5, 0.5);
        c.dSm += step;

        if (--c.driftTimer <= 0)
        {
            c.driftTarget = c.rnd();
            c.driftTimer = (int) (sr * (0.25 + 0.35 * c.rnd01()));
        }
        c.drift += (c.driftTarget - c.drift) * driftCoef;

        const double mod = wowAmp  * std::sin (wowPh  + phaseOff * 1.57)
                         + flutAmp * std::sin (flutPh + phaseOff * 2.10)
                         + driftAmp * c.drift;

        const double d = std::clamp (c.dSm + mod, 4.0, (double) c.size - 8.0);

        // testina di lettura
        float wet = c.read (d);
        wet = c.bump.process (wet);
        wet = c.lp.process (wet);
        wet = c.hp.process (wet);

        if (dropouts)
        {
            if (c.dropTimer > 0)
            {
                if (--c.dropTimer == 0) c.dropTarget = 1.0f;
            }
            else if (c.rnd01() < dropProb)
            {
                c.dropTarget = 0.35f + 0.4f * c.rnd01();
                c.dropTimer = (int) (sr * (0.04 + 0.1 * c.rnd01()));
            }
        }
        else
        {
            c.dropTarget = 1.0f;
        }
        c.dropGain += (c.dropTarget - c.dropGain) * dropCoef;
        wet *= c.dropGain;

        // testina di registrazione: ingresso + feedback, saturazione nastro
        const float rec = in * inGain + wet * fb * fbNorm;
        float y = (std::tanh (drive * rec + bias) - tanhBias) * driveComp;
        y += hissAmp * hissMul * c.rnd();

        c.buf[(size_t) c.w] = y;
        if (++c.w >= c.size) c.w = 0;

        // DC blocker in uscita
        const float o = wet - c.dcX + 0.995f * c.dcY;
        c.dcX = wet;
        c.dcY = o;
        return o;
    }

    double sr = 44100.0;
    Chan ch[2];

    double targetDelay = 4410.0;
    double glideCoef = 0.0;
    float smoothCoef = 0.001f, driftCoef = 0.0001f, dropCoef = 0.01f, dropProb = 0.0f;

    float drive = 1.0f, driveTarget = 1.0f, driveComp = 1.0f, fbNorm = 1.0f;
    float fb = 0.0f, fbTarget = 0.0f;
    float inGain = 1.0f, inTarget = 1.0f;
    bool stereo = false;

    int currentMode = -1;
    double wowPh = 0.0, flutPh = 0.0, wowInc = 0.0, flutInc = 0.0;
    double wowAmp = 0.0, flutAmp = 0.0, driftAmp = 0.0;
    float hissAmp = 0.0f, hissMul = 1.0f, bias = 0.0f, tanhBias = 0.0f;
    bool dropouts = false;
};
