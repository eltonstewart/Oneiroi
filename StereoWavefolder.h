#pragma once

#include "Commons.h"
#include "ParameterInterpolator.h"
#include "StereoEffect.h"

class StereoWavefolder : public StereoEffect
{
private:
    PatchCtrls* patchCtrls_;
    PatchCvs* patchCvs_;
    PatchState* patchState_;

    float oldAmount_;
    float oldDrive_;
    float oldOffset_;

    static inline float Wavefold(float in, float amount)
    {
        float x = in * (1.0f + amount * 3.0f);
        float fold = fabsf(fmodf(x + 1.0f, 4.0f) - 2.0f) - 1.0f;
        return fold * (1.0f / (1.0f + amount * 3.0f));
    }

    static inline float Saturate(float in, float drive)
    {
        float x = in * (1.0f + drive * 9.0f);
        float out = tanhf(x);
        return out * (1.0f + drive * 2.0f);
    }

public:
    StereoWavefolder(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState)
    {
        patchCtrls_ = patchCtrls;
        patchCvs_ = patchCvs;
        patchState_ = patchState;
        oldAmount_ = 0.0f;
        oldDrive_ = 0.0f;
        oldOffset_ = 0.0f;
    }

    ~StereoWavefolder() {}

    static StereoWavefolder* create(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState)
    {
        return new StereoWavefolder(patchCtrls, patchCvs, patchState);
    }

    static void destroy(StereoWavefolder* obj)
    {
        delete obj;
    }

    void process(AudioBuffer& input, AudioBuffer& output) override
    {
        size_t size = output.getSize();
        FloatArray leftIn = input.getSamples(LEFT_CHANNEL);
        FloatArray rightIn = input.getSamples(RIGHT_CHANNEL);
        FloatArray leftOut = output.getSamples(LEFT_CHANNEL);
        FloatArray rightOut = output.getSamples(RIGHT_CHANNEL);

            float amount = Modulate(patchCtrls_->resonatorFeedback, patchCtrls_->resonatorFeedbackModAmount, 
                               patchState_->modValue, patchCtrls_->resonatorFeedbackCvAmount, 
                               patchCvs_->resonatorFeedback, 0.0f, 1.0f, 
                               patchState_->modAttenuverters, patchState_->cvAttenuverters);
        
        float drive = Modulate(patchCtrls_->resonatorTune, patchCtrls_->resonatorTuneModAmount, 
                              patchState_->modValue, patchCtrls_->resonatorTuneCvAmount, 
                              patchCvs_->resonatorTune, 0.0f, 1.0f, 
                              patchState_->modAttenuverters, patchState_->cvAttenuverters);
        
        float offset = Modulate(patchCtrls_->resonatorDissonance, 0.0f, 
                               patchState_->modValue, 0.0f, 
                               0.0f, -1.0f, 1.0f, 
                               patchState_->modAttenuverters, patchState_->cvAttenuverters);

        ParameterInterpolator amountParam(&oldAmount_, amount, size, ParameterInterpolator::BY_SIZE);
        ParameterInterpolator driveParam(&oldDrive_, drive, size, ParameterInterpolator::BY_SIZE);
        ParameterInterpolator offsetParam(&oldOffset_, offset, size, ParameterInterpolator::BY_SIZE);

        for (size_t i = 0; i < size; i++)
        {
            float amt = amountParam.Next();
            float drv = driveParam.Next();
            float off = offsetParam.Next();

            float left = leftIn[i] + off;
            float right = rightIn[i] + off;

            float foldedLeft = Wavefold(left, amt);
            float foldedRight = Wavefold(right, amt);

            float saturatedLeft = Saturate(foldedLeft, drv);
            float saturatedRight = Saturate(foldedRight, drv);

            leftOut[i] = CheapEqualPowerCrossFade(leftIn[i], saturatedLeft * kWavefolderMakeupGain, 
                                                 patchCtrls_->resonatorVol, 1.4f);
            rightOut[i] = CheapEqualPowerCrossFade(rightIn[i], saturatedRight * kWavefolderMakeupGain, 
                                                  patchCtrls_->resonatorVol, 1.4f);
        }
    }
};