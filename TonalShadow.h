#pragma once

#include "Commons.h"
#include "DelayLine.h"

class TonalShadow
{
private:
    static const int32_t kShadowBufferSize = 8192;
    static constexpr float kShadowLeftDelaySeconds = 0.061f;
    static constexpr float kShadowRightDelaySeconds = 0.089f;
    static constexpr float kShadowMaxAmount = 0.18f;
    static constexpr float kShadowMidGain = 0.3f;
    static constexpr float kShadowSideWidth = 1.35f;
    static constexpr float kShadowDuckAmount = 0.42f;
    static constexpr float kShadowMaxDuck = 0.58f;
    static constexpr float kShadowLowTrim = 0.92f;
    static constexpr float kShadowLowpassCoeff = 0.08f;
    static constexpr float kShadowLowCoeff = 0.012f;

    DelayLine* lines_[2];

    float lpState_[2];
    float lowState_[2];
    float env_;
    float delayLeft_;
    float delayRight_;

    inline float ShapeShadow(float in, int channel)
    {
        lpState_[channel] += kShadowLowpassCoeff * (in - lpState_[channel]) + 1e-18f;
        lowState_[channel] += kShadowLowCoeff * (lpState_[channel] - lowState_[channel]) + 1e-18f;
        return lpState_[channel] - lowState_[channel] * kShadowLowTrim;
    }

public:
    TonalShadow(float sampleRate)
    {
        lines_[LEFT_CHANNEL] = DelayLine::create(kShadowBufferSize);
        lines_[RIGHT_CHANNEL] = DelayLine::create(kShadowBufferSize);

        lpState_[LEFT_CHANNEL] = 0.f;
        lpState_[RIGHT_CHANNEL] = 0.f;
        lowState_[LEFT_CHANNEL] = 0.f;
        lowState_[RIGHT_CHANNEL] = 0.f;
        env_ = 0.f;

        delayLeft_ = Clamp(sampleRate * kShadowLeftDelaySeconds, 1.f, static_cast<float>(kShadowBufferSize - 2));
        delayRight_ = Clamp(sampleRate * kShadowRightDelaySeconds, 1.f, static_cast<float>(kShadowBufferSize - 2));
    }

    ~TonalShadow()
    {
        DelayLine::destroy(lines_[LEFT_CHANNEL]);
        DelayLine::destroy(lines_[RIGHT_CHANNEL]);
    }

    static TonalShadow* create(float sampleRate)
    {
        return new TonalShadow(sampleRate);
    }

    static void destroy(TonalShadow* obj)
    {
        delete obj;
    }

    void Process(AudioBuffer& buffer, float amount)
    {
        amount = Clamp(amount, 0.f, kShadowMaxAmount);
        if (amount <= 0.0001f)
        {
            return;
        }

        FloatArray left = buffer.getSamples(LEFT_CHANNEL);
        FloatArray right = buffer.getSamples(RIGHT_CHANNEL);
        size_t size = buffer.getSize();

        for (size_t i = 0; i < size; i++)
        {
            float l = Clamp(left[i], -3.f, 3.f);
            float r = Clamp(right[i], -3.f, 3.f);

            float mid;
            float side;
            LR2MS(l, r, mid, side);

            SLOPE(env_, fabsf(mid), 0.02f, 0.0018f);
            float duck = 1.f - Clamp(env_ * kShadowDuckAmount, 0.f, kShadowMaxDuck);

            float shadowLeft = ShapeShadow(lines_[LEFT_CHANNEL]->read(delayLeft_), LEFT_CHANNEL);
            float shadowRight = ShapeShadow(lines_[RIGHT_CHANNEL]->read(delayRight_), RIGHT_CHANNEL);
            float shadowMid;
            float shadowSide;
            LR2MS(shadowLeft, shadowRight, shadowMid, shadowSide, kShadowSideWidth);

            shadowMid *= amount * kShadowMidGain;
            shadowSide *= amount;

            float wetLeft;
            float wetRight;
            MS2LR(shadowMid, shadowSide, wetLeft, wetRight);

            left[i] = SoftLimit(l + wetLeft * duck);
            right[i] = SoftLimit(r + wetRight * duck);

            lines_[LEFT_CHANNEL]->write(l);
            lines_[RIGHT_CHANNEL]->write(r);
        }
    }
};
