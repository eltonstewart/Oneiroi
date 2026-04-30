#pragma once

#include "Commons.h"
#include "StereoEffect.h"
#include "AllPassFilter.h"

class PhasePhaser : public StereoEffect
{
private:
    PatchCtrls* patchCtrls_;
    PatchCvs* patchCvs_;
    PatchState* patchState_;

    static constexpr int kPhaserStages = 4;
    AllPassFilter* apLeft_[kPhaserStages];
    AllPassFilter* apRight_[kPhaserStages];

    float oldDepth_;
    float oldRate_;
    float oldFreq_;
    float phase_;

    static constexpr float kPhaserMinFreq = 200.f;
    static constexpr float kPhaserMaxFreq = 3000.f;
    static constexpr float kPhaserMakeupGain = 1.05f;
    static constexpr float kPhaserFeedbackAmount = 0.35f;

public:
    PhasePhaser(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState)
        : patchCtrls_(patchCtrls), patchCvs_(patchCvs), patchState_(patchState),
          oldDepth_(0.f), oldRate_(0.f), oldFreq_(0.f), phase_(0.f)
    {
        for (int i = 0; i < kPhaserStages; i++)
        {
            apLeft_[i] = AllPassFilter::create(patchState_->sampleRate, true);
            apRight_[i] = AllPassFilter::create(patchState_->sampleRate, true);
        }
    }

    ~PhasePhaser()
    {
        for (int i = 0; i < kPhaserStages; i++)
        {
            AllPassFilter::destroy(apLeft_[i]);
            AllPassFilter::destroy(apRight_[i]);
        }
    }

    static PhasePhaser* create(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState)
    {
        return new PhasePhaser(patchCtrls, patchCvs, patchState);
    }

    void process(AudioBuffer& input, AudioBuffer& output) override
    {
        FloatArray left = input.getSamples(LEFT_CHANNEL);
        FloatArray right = input.getSamples(RIGHT_CHANNEL);
        size_t size = input.getSize();

        float depth = Clamp(patchCtrls_->resonatorVol, 0.f, 1.f);
        float rate = Clamp(patchCtrls_->resonatorFeedback, 0.01f, 10.f);
        float baseFreq = Map(Clamp(patchCtrls_->resonatorTune, 0.f, 1.f), 0.f, 1.f, kPhaserMinFreq, kPhaserMaxFreq);

        ParameterInterpolator depthParam(&oldDepth_, depth, size, ParameterInterpolator::BY_SIZE);
        ParameterInterpolator rateParam(&oldRate_, rate, size, ParameterInterpolator::BY_SIZE);
        ParameterInterpolator freqParam(&oldFreq_, baseFreq, size, ParameterInterpolator::BY_SIZE);

        float phaseInc = k2Pi * rate / patchState_->sampleRate;
        int coeffUpdateCounter = 0;

        for (size_t i = 0; i < size; i++)
        {
            float d = depthParam.Next();
            float r = rateParam.Next();
            float f = freqParam.Next();

            phase_ += phaseInc;
            if (phase_ > k2Pi) phase_ -= k2Pi;

            float lfo = sinf(phase_);
            float modFreq = f + f * lfo * 0.5f;

            // Update allpass coefficients every 8 samples to save CPU
            if (++coeffUpdateCounter >= 8)
            {
                coeffUpdateCounter = 0;
                for (int s = 0; s < kPhaserStages; s++)
                {
                    float stageFreq = modFreq * (1.0f + s * 0.08f);
                    apLeft_[s]->SetFreq(stageFreq);
                    apRight_[s]->SetFreq(stageFreq);
                }
            }

            float le = left[i];
            float ri = right[i];

            float apL = le;
            float apR = ri;

            for (int s = 0; s < kPhaserStages; s++)
            {
                apL = apLeft_[s]->ProcessStatic(apL);
                apR = apRight_[s]->ProcessStatic(apR);
            }

            float fbL = (le + apL * kPhaserFeedbackAmount) * d;
            float fbR = (ri + apR * kPhaserFeedbackAmount) * d;

            float wetL = le + fbL;
            float wetR = ri + fbR;

            left[i] = SoftLimit(wetL * kPhaserMakeupGain);
            right[i] = SoftLimit(wetR * kPhaserMakeupGain);
        }
    }
};
