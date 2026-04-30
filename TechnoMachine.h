#pragma once

#include "Commons.h"
#include "WaveTableBuffer.h"
#include "StereoSineOscillator.h"
#include "StereoSuperSaw.h"
#include "StereoWaveTableOscillator.h"
#include "Ambience.h"
#include "Filter.h"
#include "MoogLadderFilter.h"
#include "Resonator.h"
#include "StereoWavefolder.h"
#include "StereoWidener.h"
#include "PhasePhaser.h"
#include "TonalShadow.h"
#include "Echo.h"
#include "Looper.h"
#include "Schmitt.h"
#include "TGate.h"
#include "EnvFollower.h"
#include "DcBlockingFilter.h"
#include "SmoothValue.h"
#include "Modulation.h"
#include "Limiter.h"

class Oneiroi
{
private:
    PatchCtrls* patchCtrls_;
    PatchCvs* patchCvs_;
    PatchState* patchState_;

    StereoSineOscillator* sine_;
    StereoSuperSaw* saw_;
    StereoWaveTableOscillator* wt_;
    WaveTableBuffer* wtBuffer_;
    Filter* filter_;
    MoogLadderFilter* moogFilter_;
    bool useMoogFilter_;
    StereoEffect* effects_[kNumEffects];
    Echo* echo_;
    Ambience* ambience_;
    Looper* looper_;
    TonalShadow* tonalShadow_;
    Limiter* limiter_;

    Modulation* modulation_;

    AudioBuffer* resample_;
    AudioBuffer* osc1Out_;
    AudioBuffer* osc2Out_;

    StereoDcBlockingFilter* inputDcFilter_;
    StereoDcBlockingFilter* outputDcFilter_;

    EnvFollower* inEnvFollower_[2];

    FilterPosition filterPosition_, lastFilterPosition_;

    float oldInputVol_;

public:
    Oneiroi(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState)
    {
        patchCtrls_ = patchCtrls;
        patchCvs_ = patchCvs;
        patchState_ = patchState;

        looper_ = Looper::create(patchCtrls_, patchCvs_, patchState_);
        wtBuffer_ = WaveTableBuffer::create(looper_->GetBuffer());

        sine_ = StereoSineOscillator::create(patchCtrls_, patchCvs_, patchState_, true);
        saw_ = StereoSuperSaw::create(patchCtrls_, patchCvs_, patchState_, true);
        wt_ = StereoWaveTableOscillator::create(patchCtrls_, patchCvs_, patchState_, wtBuffer_);

        filter_ = Filter::create(patchCtrls_, patchCvs_, patchState_);
        moogFilter_ = MoogLadderFilter::create(patchState_->sampleRate);
        moogFilter_->setCutoff(800.0f);
        moogFilter_->setResonance(0.4f);
        moogFilter_->setDrive(1.5f);
        useMoogFilter_ = true;  // Moog mode enabled
        effects_[kEffectResonator] = Resonator::create(patchCtrls_, patchCvs_, patchState_);
        effects_[kEffectWavefolder] = StereoWavefolder::create(patchCtrls_, patchCvs_, patchState_);
        effects_[kEffectStereoWidener] = StereoWidener::create(patchCtrls_, patchCvs_, patchState_);
        effects_[kEffectPhaser] = PhasePhaser::create(patchCtrls_, patchCvs_, patchState_);
        tonalShadow_ = TonalShadow::create(patchState_->sampleRate);
        echo_ = Echo::create(patchCtrls_, patchCvs_, patchState_);
        ambience_ = Ambience::create(patchCtrls_, patchCvs_, patchState_);

        modulation_ = Modulation::create(patchCtrls_, patchCvs_, patchState_);

        limiter_ = Limiter::create();

        resample_ = AudioBuffer::create(2, patchState_->blockSize);
        osc1Out_ = AudioBuffer::create(2, patchState_->blockSize);
        osc2Out_ = AudioBuffer::create(2, patchState_->blockSize);

        for (size_t i = 0; i < 2; i++)
        {
            inEnvFollower_[i] = EnvFollower::create();
            inEnvFollower_[i]->setLambda(0.9f);
        }

        inputDcFilter_ = StereoDcBlockingFilter::create();
        outputDcFilter_ = StereoDcBlockingFilter::create();
        oldInputVol_ = 0.0f;
    }
    ~Oneiroi()
    {
        AudioBuffer::destroy(resample_);
        AudioBuffer::destroy(osc1Out_);
        AudioBuffer::destroy(osc2Out_);
        WaveTableBuffer::destroy(wtBuffer_);
        Looper::destroy(looper_);
        StereoSineOscillator::destroy(sine_);
        StereoSuperSaw::destroy(saw_);
        StereoWaveTableOscillator::destroy(wt_);
        Filter::destroy(filter_);
        MoogLadderFilter::destroy(moogFilter_);
        StereoEffect::destroy(effects_[kEffectResonator]);
        StereoEffect::destroy(effects_[kEffectWavefolder]);
        StereoEffect::destroy(effects_[kEffectStereoWidener]);
        StereoEffect::destroy(effects_[kEffectPhaser]);
        TonalShadow::destroy(tonalShadow_);
        Echo::destroy(echo_);
        Ambience::destroy(ambience_);
        Modulation::destroy(modulation_);
        Limiter::destroy(limiter_);

        for (size_t i = 0; i < 2; i++)
        {
            EnvFollower::destroy(inEnvFollower_[i]);
        }
    }

    static Oneiroi* create(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState)
    {
        return new Oneiroi(patchCtrls, patchCvs, patchState);
    }

    static void destroy(Oneiroi* obj)
    {
        delete obj;
    }

    inline void Process(AudioBuffer &buffer)
    {
        FloatArray left = buffer.getSamples(LEFT_CHANNEL);
        FloatArray right = buffer.getSamples(RIGHT_CHANNEL);

        inputDcFilter_->process(buffer, buffer);

        const int size = buffer.getSize();

        // Input leds - branch hoisted outside loop for better pipelining.
        if (patchCtrls_->looperResampling)
        {
            FloatArray resampleLeft = resample_->getSamples(LEFT_CHANNEL);
            FloatArray resampleRight = resample_->getSamples(RIGHT_CHANNEL);
            for (int i = 0; i < size; i++)
            {
                float l = Mix2(inEnvFollower_[0]->process(resampleLeft[i]), inEnvFollower_[1]->process(resampleRight[i])) * kLooperResampleLedAtt;
                patchState_->inputLevel[i] = l;
            }
        }
        else
        {
            for (int i = 0; i < size; i++)
            {
                float l = Mix2(inEnvFollower_[0]->process(left[i]), inEnvFollower_[1]->process(right[i]));
                patchState_->inputLevel[i] = l;
            }
        }

        modulation_->Process();

        // Input volume processing - store original input on stack, scale in-place, add back after looper.
        // Optimization: eliminates input_ AudioBuffer allocation (~512 bytes RAM).
        float vol = Modulate(patchCtrls_->inputVol, patchCtrls_->inputVolModAmount, patchState_->modValue, patchCtrls_->inputVolCvAmount, patchCvs_->inputVol, 0.f, 1.f, patchState_->modAttenuverters, patchState_->cvAttenuverters);
        ParameterInterpolator inputVolParam(&oldInputVol_, vol, size, ParameterInterpolator::BY_SIZE);
        
        // Save original input and apply volume in single loop.
        float inputL[AUDIO_MAX_BLOCK_SIZE];
        float inputR[AUDIO_MAX_BLOCK_SIZE];
        for (int i = 0; i < size; i++)
        {
            float v = inputVolParam.Next();
            inputL[i] = left[i] * v;
            inputR[i] = right[i] * v;
        }

        if (patchCtrls_->looperResampling)
        {
            looper_->Process(*resample_, buffer);
        }
        else
        {
            looper_->Process(buffer, buffer);
        }
        
        // Mix scaled input back into buffer.
        for (int i = 0; i < size; i++)
        {
            left[i] += inputL[i];
            right[i] += inputR[i];
        }

        sine_->Process(*osc1Out_);
        buffer.add(*osc1Out_);
        patchCtrls_->oscUseWavetable > 0.5f ? wt_->Process(*osc2Out_) : saw_->Process(*osc2Out_);
        buffer.add(*osc2Out_);

        buffer.multiply(kSourcesMakeupGain);

        if (patchCtrls_->filterPosition < 0.25f)
        {
            filterPosition_ = FilterPosition::POSITION_1;
        }
        else if (patchCtrls_->filterPosition >= 0.25f && patchCtrls_->filterPosition < 0.5f)
        {
            filterPosition_ = FilterPosition::POSITION_2;
        }
        else if (patchCtrls_->filterPosition >= 0.5f && patchCtrls_->filterPosition < 0.75f)
        {
            filterPosition_ = FilterPosition::POSITION_3;
        }
        else if (patchCtrls_->filterPosition >= 0.75f)
        {
            filterPosition_ = FilterPosition::POSITION_4;
        }

        if (filterPosition_ != lastFilterPosition_)
        {
            lastFilterPosition_ = filterPosition_;
            patchState_->filterPositionFlag = true;
        }
        else
        {
            patchState_->filterPositionFlag = false;
        }

        // Apply filter (Moog or standard)
        if (useMoogFilter_)
        {
            // Moog mode: filter right after oscillators (subtractive synthesis)
            // Update Moog filter parameters from controls
            float cutoff = Modulate(patchCtrls_->filterCutoff, patchCtrls_->filterCutoffModAmount, 
                                   patchState_->modValue, patchCtrls_->filterCutoffCvAmount, 
                                   patchCvs_->filterCutoff, -1.f, 1.f, 
                                   patchState_->modAttenuverters, patchState_->cvAttenuverters);
            // Map 0-1 to 20Hz-20kHz exponentially
            float cutoffHz = fast_expf(Map(cutoff, -1.f, 1.f, fast_logf(20.f), fast_logf(20000.f)));
            moogFilter_->setCutoff(cutoffHz);
            
            float resonance = Modulate(patchCtrls_->filterResonance, patchCtrls_->filterResonanceModAmount,
                                      patchState_->modValue, patchCtrls_->filterResonanceCvAmount,
                                      patchCvs_->filterResonance, -1.f, 1.f,
                                      patchState_->modAttenuverters, patchState_->cvAttenuverters);
            moogFilter_->setResonance(Map(resonance, -1.f, 1.f, 0.0f, 4.0f));
            
            moogFilter_->process(buffer, buffer);
        }
        else
        {
            // Standard mode: use original filter positions
            if (FilterPosition::POSITION_1 == filterPosition_)
            {
                filter_->process(buffer, buffer);
            }
        }
        
        int effectIndex = (int)(patchCtrls_->effectType * (kNumEffects - 0.001f));
        if (effectIndex < 0) effectIndex = 0;
        if (effectIndex >= kNumEffects) effectIndex = kNumEffects - 1;
        effects_[effectIndex]->process(buffer, buffer);
        float shadowAmount = 0.f;
        if (effectIndex == kEffectWavefolder)
        {
            shadowAmount = patchCtrls_->resonatorVol * 0.18f;
        }
        else if (effectIndex == kEffectStereoWidener)
        {
            shadowAmount = patchCtrls_->resonatorVol * 0.12f;
        }
        else if (effectIndex == kEffectPhaser)
        {
            shadowAmount = patchCtrls_->resonatorFeedback * 0.08f;
        }
        tonalShadow_->Process(buffer, shadowAmount);

        if (!useMoogFilter_ && FilterPosition::POSITION_2 == filterPosition_)
        {
            filter_->process(buffer, buffer);
        }
        echo_->process(buffer, buffer);
        if (!useMoogFilter_ && FilterPosition::POSITION_3 == filterPosition_)
        {
            filter_->process(buffer, buffer);
        }
        ambience_->process(buffer, buffer);
        if (!useMoogFilter_ && FilterPosition::POSITION_4 == filterPosition_)
        {
            filter_->process(buffer, buffer);
        }

        outputDcFilter_->process(buffer, buffer);

        buffer.multiply(kOutputMakeupGain);
        limiter_->ProcessSoft(buffer, buffer);

        if (StartupPhase::STARTUP_DONE == patchState_->startupPhase)
        {
            buffer.multiply(patchState_->outLevel);
        }
        else
        {
            // TODO: Fade in
            buffer.clear();
        }

        resample_->copyFrom(buffer);
    }
};
