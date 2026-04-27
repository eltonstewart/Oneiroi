#pragma once

#include "Commons.h"

class AllPassFilter
{
private:
    float a_;
    float x1_;
    float y1_;
    float x2_;
    float y2_;
    bool is2Pole_;

    inline void UpdateCoeff(float freq)
    {
        float omega = k2Pi * freq / sampleRate_;
        float tanHalfOmega = tanf(omega * 0.5f);
        a_ = (1.0f - tanHalfOmega) / (1.0f + tanHalfOmega);
    }

public:
    float sampleRate_;

    AllPassFilter(float sampleRate, bool is2Pole = false)
        : sampleRate_(sampleRate), a_(0.f), x1_(0.f), y1_(0.f), x2_(0.f), y2_(0.f), is2Pole_(is2Pole)
    {
    }

    ~AllPassFilter() {}

    static AllPassFilter* create(float sampleRate, bool is2Pole = false)
    {
        return new AllPassFilter(sampleRate, is2Pole);
    }

    static void destroy(AllPassFilter* obj)
    {
        delete obj;
    }

    void SetSampleRate(float sampleRate)
    {
        sampleRate_ = sampleRate;
    }

    void Reset()
    {
        x1_ = 0.f;
        y1_ = 0.f;
        x2_ = 0.f;
        y2_ = 0.f;
    }

    float Process(float in, float freq)
    {
        UpdateCoeff(freq);

        float y = -a_ * in + x1_ + a_ * y1_;
        x1_ = in;
        y1_ = y;

        if (is2Pole_)
        {
            float y2 = -a_ * y + x2_ + a_ * y2_;
            x2_ = y;
            y2_ = y2;
            return y2;
        }

        return y;
    }

    float ProcessStatic(float in)
    {
        float y = -a_ * in + x1_ + a_ * y1_;
        x1_ = in;
        y1_ = y;

        if (is2Pole_)
        {
            float y2 = -a_ * y + x2_ + a_ * y2_;
            x2_ = y;
            y2_ = y2;
            return y2;
        }

        return y;
    }

    void SetFreq(float freq)
    {
        UpdateCoeff(freq);
    }
};
