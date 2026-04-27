#pragma once

#include "Commons.h"
#include "StereoEffect.h"
#include "AllPassFilter.h"

class StereoWidener : public StereoEffect
{
private:
    PatchCtrls* patchCtrls_;
    PatchCvs* patchCvs_;
    PatchState* patchState_;

    AllPassFilter* apLeft_;
    AllPassFilter* apRight_;

    float oldWidth_;
    float oldFreq_;

    static constexpr float kWidenerMinFreq = 200.f;
    static constexpr float kWidenerMaxFreq = 4000.f;
    static constexpr float kWidenerMakeupGain = 1.15f;
    static constexpr float kWidenerDefaultSpread = 1.18f;

public:
    StereoWidener(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState)
        : patchCtrls_(patchCtrls), patchCvs_(patchCvs), patchState_(patchState),
          oldWidth_(0.f), oldFreq_(0.f)
    {
        apLeft_ = AllPassFilter::create(patchState_->sampleRate, false);
        apRight_ = AllPassFilter::create(patchState_->sampleRate, false);
    }

    ~StereoWidener()
    {
        AllPassFilter::destroy(apLeft_);
        AllPassFilter::destroy(apRight_);
    }

    static StereoWidener* create(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState)
    {
        return new StereoWidener(patchCtrls, patchCvs, patchState);
    }

    void process(AudioBuffer& input, AudioBuffer& output) override
    {
        FloatArray left = input.getSamples(LEFT_CHANNEL);
        FloatArray right = input.getSamples(RIGHT_CHANNEL);
        size_t size = input.getSize();

        float width = Clamp(patchCtrls_->resonatorVol, 0.f, 1.f);
        float freq = Map(Clamp(patchCtrls_->resonatorTune, 0.f, 1.f), 0.f, 1.f, kWidenerMinFreq, kWidenerMaxFreq);

        ParameterInterpolator widthParam(&oldWidth_, width, size, ParameterInterpolator::BY_SIZE);
        ParameterInterpolator freqParam(&oldFreq_, freq, size, ParameterInterpolator::BY_SIZE);

        for (size_t i = 0; i < size; i++)
        {
            float w = widthParam.Next();
            float f = freqParam.Next();

            float le = left[i];
            float ri = right[i];

            float apL = apLeft_->Process(le, f);
            float apR = apRight_->Process(ri, f * kWidenerDefaultSpread);

            float wetL = LinearCrossFade(le, apL, w);
            float wetR = LinearCrossFade(ri, apR, w);

            left[i] = wetL * kWidenerMakeupGain;
            right[i] = wetR * kWidenerMakeupGain;
        }
    }
};
