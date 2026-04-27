#pragma once

#include "Commons.h"
#include "DelayLine.h"
#include "EnvFollower.h"
#include "SineOscillator.h"
#include "ParameterInterpolator.h"
#include "DjFilter.h"
#include "Compressor.h"
#include "AllPassFilter.h"
#include <stdint.h>

enum EchoTap
{
    TAP_LEFT_A,
    TAP_LEFT_B,
    TAP_RIGHT_A,
    TAP_RIGHT_B,
};

class Echo
{
private:
    static constexpr float kEchoFeedbackToneCoeff = 0.11f;
    static constexpr float kEchoFeedbackLowCoeff = 0.018f;
    static constexpr float kEchoFeedbackLowTrim = 0.18f;
    static constexpr float kEchoTapBlendLeft = 0.42f;
    static constexpr float kEchoTapBlendRight = 0.58f;
    static constexpr float kEchoFeedbackSkew = 0.015f;

    PatchCtrls* patchCtrls_;
    PatchCvs* patchCvs_;
    PatchState* patchState_;

    DelayLine* lines_[2]; // Optimized: Left and Right shared buffers
    DjFilter* filter_;
    EnvFollower* ef_[2];
    Compressor* comp_[2];

    HysteresisQuantizer densityQuantizer_;

    int clockRatiosIndex_;
    float echoDensity_, oldDensity_;

    float levels_[kEchoTaps], outs_[kEchoTaps];
    float tapsTimes_[kEchoTaps], newTapsTimes_[kEchoTaps], maxTapsTimes_[kEchoTaps];
    float repeats_, filterValue_;

    bool externalClock_;
    bool infinite_;
    float tapBlendLeft_;
    float tapBlendRight_;
    float feedbackTone_[2];
    float feedbackLow_[2];

    AllPassFilter* vibratoAp_[2];
    float vibratoPhase_;
    static constexpr float kEchoVibratoMinFreq = 600.f;
    static constexpr float kEchoVibratoMaxFreq = 3000.f;
    static constexpr float kEchoVibratoRate = 0.35f;

    inline float ShapeFeedback(float in, int channel)
    {
        feedbackTone_[channel] += kEchoFeedbackToneCoeff * (in - feedbackTone_[channel]);
        feedbackLow_[channel] += kEchoFeedbackLowCoeff * (feedbackTone_[channel] - feedbackLow_[channel]);
        return feedbackTone_[channel] - feedbackLow_[channel] * kEchoFeedbackLowTrim;
    }

    inline float ProcessVibrato(float in, int channel)
    {
        vibratoPhase_ += k2Pi * kEchoVibratoRate / patchState_->sampleRate;
        if (vibratoPhase_ > k2Pi) vibratoPhase_ -= k2Pi;

        float lfo = sinf(vibratoPhase_ + channel * kPi);
        float freq = kEchoVibratoMinFreq + (kEchoVibratoMaxFreq - kEchoVibratoMinFreq) * (0.5f + 0.5f * lfo);

        return vibratoAp_[channel]->Process(in, freq);
    }

    void SetTapTime(int idx, float time)
    {
        newTapsTimes_[idx] = Clamp(time, kEchoMinLengthSamples * kEchoTapsRatios[idx], (kEchoMaxLengthSamples - 1) * kEchoTapsRatios[idx]);
    }

    void SetMaxTapTime(int idx, float time)
    {
        maxTapsTimes_[idx] = time;
        SetTapTime(idx, Max(echoDensity_ * maxTapsTimes_[idx], kEchoMinLengthSamples));
    }

    void SetLevel(int idx, float value)
    {
        levels_[idx] = value;
    }

    void SetFilter(float value)
    {
        filterValue_ = value;
        filter_->SetFilter(value);
    }

    void SetRepeats(float value)
    {
        repeats_ = value;

        float r = repeats_;

        infinite_ = false;

        // Infinite feedback.
        if (r > kEchoInfiniteFeedbackThreshold)
        {
            r = kEchoInfiniteFeedbackLevel;
            infinite_ = true;
        }
        else if (r > 0.99f)
        {
            r = 1.f;
        }

        SetLevel(TAP_LEFT_A, r * kEchoTapsFeedbacks[TAP_LEFT_A]);
        SetLevel(TAP_LEFT_B, r * kEchoTapsFeedbacks[TAP_LEFT_B]);
        SetLevel(TAP_RIGHT_A, r * kEchoTapsFeedbacks[TAP_RIGHT_A]);
        SetLevel(TAP_RIGHT_B, r * kEchoTapsFeedbacks[TAP_RIGHT_B]);

        float thrs = Map(repeats_, 0.f, 1.f, kEchoCompThresMin, kEchoCompThresMax);
        comp_[LEFT_CHANNEL]->setThreshold(thrs);
        comp_[RIGHT_CHANNEL]->setThreshold(thrs);
    }

    void SetDensity(float value)
    {
        if (ClockSource::CLOCK_SOURCE_EXTERNAL == patchState_->clockSource)
        {
            int newIndex = densityQuantizer_.Process(value);
            if (newIndex == clockRatiosIndex_ && externalClock_)
            {
                return;
            }
            clockRatiosIndex_ = newIndex;

            float d = kModClockRatios[clockRatiosIndex_] * patchState_->clockSamples * kEchoExternalClockMultiplier;
            for (size_t i = 0; i < kEchoTaps; i++)
            {
                SetTapTime(i, d * kEchoTapsRatios[i]);
            }

            // Reset max tap time the next time (...) the clock switches to internal.
            if (!externalClock_)
            {
                externalClock_ = true;
            }
        }
        else
        {
            if (externalClock_)
            {
                int32_t t = kEchoMaxLengthSamples - 1;
                for (size_t i = 0; i < kEchoTaps; i++)
                {
                    SetMaxTapTime(i, t * kEchoTapsRatios[i]);
                }
                externalClock_ = false;
            }

            echoDensity_ = value;

            float d = MapExpo(echoDensity_, 0.f, 1.f, kEchoMinLengthSamples, patchState_->clockSamples * kEchoInternalClockMultiplier);
            size_t s = kEchoFadeSamples;
            ParameterInterpolator densityParam(&oldDensity_, d, s, ParameterInterpolator::BY_SIZE);

            for (size_t i = 0; i < kEchoTaps; i++)
            {

                SetTapTime(i, densityParam.Next() * kEchoTapsRatios[i]);
            }
        }
    }

public:
    Echo(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState)
    {
        patchCtrls_ = patchCtrls;
        patchCvs_ = patchCvs;
        patchState_ = patchState;

        // Optimized: Create only 2 delay lines (Left/Right)
        for (size_t i = 0; i < 2; i++)
        {
            lines_[i] = DelayLine::create(kEchoMaxLengthSamples);
        }

        for (size_t i = 0; i < kEchoTaps; i++)
        {
            tapsTimes_[i] = kEchoMaxLengthSamples - 1;
            SetMaxTapTime(i, tapsTimes_[i] * kEchoTapsRatios[i]);
            levels_[i] = 0;
            outs_[i] = 0;
        }

        echoDensity_ = 1.f;
        clockRatiosIndex_ = 0;

        externalClock_ = false;
        infinite_ = false;
        tapBlendLeft_ = kEchoTapBlendLeft;
        tapBlendRight_ = kEchoTapBlendRight;
        feedbackTone_[LEFT_CHANNEL] = 0.f;
        feedbackTone_[RIGHT_CHANNEL] = 0.f;
        feedbackLow_[LEFT_CHANNEL] = 0.f;
        feedbackLow_[RIGHT_CHANNEL] = 0.f;
        vibratoPhase_ = 0.f;

        for (size_t i = 0; i < 2; i++)
        {
            vibratoAp_[i] = AllPassFilter::create(patchState_->sampleRate, true);
        }

        filter_ = DjFilter::create(patchState_->sampleRate);

        for (size_t i = 0; i < 2; i++)
        {
            comp_[i] = Compressor::create(patchState_->sampleRate);
            comp_[i]->setThreshold(-16);
            ef_[i] = EnvFollower::create();
        }

        densityQuantizer_.Init(kClockUnityRatioIndex, 0.15f, false);
    }
    ~Echo()
    {
        // Destroy only the 2 lines
        for (size_t i = 0; i < 2; i++)
        {
            DelayLine::destroy(lines_[i]);
        }
        DjFilter::destroy(filter_);
        for (size_t i = 0; i < 2; i++)
        {
            AllPassFilter::destroy(vibratoAp_[i]);
            Compressor::destroy(comp_[i]);
            EnvFollower::destroy(ef_[i]);
        }
    }

    static Echo* create(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState)
    {
        return new Echo(patchCtrls, patchCvs, patchState);
    }

    static void destroy(Echo* obj)
    {
        delete obj;
    }

    // Process entry point - dispatches to specialized version based on clock mode.
    // Optimization: Branch hoisted outside loop to eliminate misprediction stalls.
    void process(AudioBuffer &input, AudioBuffer &output)
    {
        size_t size = output.getSize();

        SetFilter(patchCtrls_->echoFilter);

        float d = Modulate(patchCtrls_->echoDensity, patchCtrls_->echoDensityModAmount, patchState_->modValue, patchCtrls_->echoDensityCvAmount, patchCvs_->echoDensity, -1.f, 1.f, patchState_->modAttenuverters, patchState_->cvAttenuverters);
        float r = Modulate(patchCtrls_->echoRepeats, patchCtrls_->echoRepeatsModAmount, patchState_->modValue, patchCtrls_->echoRepeatsCvAmount, patchCvs_->echoRepeats, -1.f, 1.f, patchState_->modAttenuverters, patchState_->cvAttenuverters);
        SetRepeats(r);

        if (StartupPhase::STARTUP_DONE != patchState_->startupPhase)
        {
            return;
        }

        // Update density ONCE per block for internal clock (was being called every sample!)
        // For external clock, SetDensity is called to handle clock ratio quantization.
        if (externalClock_)
        {
            SetDensity(d);
            processExternalClock(input, output, size);
        }
        else
        {
            SetDensity(d);
            processInternalClock(input, output, size);
        }
    }

private:
    // External clock: tap times crossfade smoothly between old and new values.
    void processExternalClock(AudioBuffer &input, AudioBuffer &output, size_t size)
    {
        FloatArray leftIn = input.getSamples(LEFT_CHANNEL);
        FloatArray rightIn = input.getSamples(RIGHT_CHANNEL);
        FloatArray leftOut = output.getSamples(LEFT_CHANNEL);
        FloatArray rightOut = output.getSamples(RIGHT_CHANNEL);

        float x = 0;
        float xi = 1.f / static_cast<float>(size);

        for (size_t i = 0; i < size; i++)
        {
            outs_[TAP_LEFT_A] = lines_[LEFT_CHANNEL]->read(tapsTimes_[TAP_LEFT_A], newTapsTimes_[TAP_LEFT_A], x);
            outs_[TAP_LEFT_B] = lines_[LEFT_CHANNEL]->read(tapsTimes_[TAP_LEFT_B], newTapsTimes_[TAP_LEFT_B], x);
            outs_[TAP_RIGHT_A] = lines_[RIGHT_CHANNEL]->read(tapsTimes_[TAP_RIGHT_A], newTapsTimes_[TAP_RIGHT_A], x);
            outs_[TAP_RIGHT_B] = lines_[RIGHT_CHANNEL]->read(tapsTimes_[TAP_RIGHT_B], newTapsTimes_[TAP_RIGHT_B], x);
            x += xi;

            float leftFb = HardClip(outs_[TAP_LEFT_A] * levels_[TAP_LEFT_A] * (1.f - kEchoFeedbackSkew) + outs_[TAP_RIGHT_A] * levels_[TAP_RIGHT_A] * (1.f + kEchoFeedbackSkew));
            float rightFb = HardClip(outs_[TAP_LEFT_B] * levels_[TAP_LEFT_B] * (1.f + kEchoFeedbackSkew) + outs_[TAP_RIGHT_B] * levels_[TAP_RIGHT_B] * (1.f - kEchoFeedbackSkew));

            if (infinite_)
            {
                leftFb *= 1.08f - ef_[LEFT_CHANNEL]->process(leftFb);
                rightFb *= 1.08f - ef_[RIGHT_CHANNEL]->process(rightFb);
            }
            leftFb = ShapeFeedback(leftFb, LEFT_CHANNEL);
            rightFb = ShapeFeedback(rightFb, RIGHT_CHANNEL);
            leftFb = ProcessVibrato(leftFb, LEFT_CHANNEL);
            rightFb = ProcessVibrato(rightFb, RIGHT_CHANNEL);

            float lIn = Clamp(leftIn[i], -3.f, 3.f);
            float rIn = Clamp(rightIn[i], -3.f, 3.f);

            float leftFilter, rightFilter;
            filter_->Process(lIn, rIn, leftFilter, rightFilter);

            leftFb += leftFilter;
            rightFb += rightFilter;

            lines_[LEFT_CHANNEL]->write(leftFb);
            lines_[RIGHT_CHANNEL]->write(rightFb);

            float left = LinearCrossFade(outs_[TAP_LEFT_A], outs_[TAP_LEFT_B], tapBlendLeft_);
            float right = LinearCrossFade(outs_[TAP_RIGHT_A], outs_[TAP_RIGHT_B], tapBlendRight_);

            left = comp_[LEFT_CHANNEL]->process(left) * kEchoMakeupGain;
            right = comp_[RIGHT_CHANNEL]->process(right) * kEchoMakeupGain;

            leftOut[i] = CheapEqualPowerCrossFade(lIn, left, patchCtrls_->echoVol, 1.8f);
            rightOut[i] = CheapEqualPowerCrossFade(rIn, right, patchCtrls_->echoVol, 1.8f);
        }

        for (size_t j = 0; j < kEchoTaps; j++)
        {
            tapsTimes_[j] = newTapsTimes_[j];
        }
    }

    // Internal clock: direct tap reads, no crossfade needed.
    void processInternalClock(AudioBuffer &input, AudioBuffer &output, size_t size)
    {
        FloatArray leftIn = input.getSamples(LEFT_CHANNEL);
        FloatArray rightIn = input.getSamples(RIGHT_CHANNEL);
        FloatArray leftOut = output.getSamples(LEFT_CHANNEL);
        FloatArray rightOut = output.getSamples(RIGHT_CHANNEL);

        for (size_t i = 0; i < size; i++)
        {
            outs_[TAP_LEFT_A] = lines_[LEFT_CHANNEL]->read(newTapsTimes_[TAP_LEFT_A]);
            outs_[TAP_LEFT_B] = lines_[LEFT_CHANNEL]->read(newTapsTimes_[TAP_LEFT_B]);
            outs_[TAP_RIGHT_A] = lines_[RIGHT_CHANNEL]->read(newTapsTimes_[TAP_RIGHT_A]);
            outs_[TAP_RIGHT_B] = lines_[RIGHT_CHANNEL]->read(newTapsTimes_[TAP_RIGHT_B]);

            float leftFb = HardClip(outs_[TAP_LEFT_A] * levels_[TAP_LEFT_A] * (1.f - kEchoFeedbackSkew) + outs_[TAP_RIGHT_A] * levels_[TAP_RIGHT_A] * (1.f + kEchoFeedbackSkew));
            float rightFb = HardClip(outs_[TAP_LEFT_B] * levels_[TAP_LEFT_B] * (1.f + kEchoFeedbackSkew) + outs_[TAP_RIGHT_B] * levels_[TAP_RIGHT_B] * (1.f - kEchoFeedbackSkew));

            if (infinite_)
            {
                leftFb *= 1.08f - ef_[LEFT_CHANNEL]->process(leftFb);
                rightFb *= 1.08f - ef_[RIGHT_CHANNEL]->process(rightFb);
            }
            leftFb = ShapeFeedback(leftFb, LEFT_CHANNEL);
            rightFb = ShapeFeedback(rightFb, RIGHT_CHANNEL);
            leftFb = ProcessVibrato(leftFb, LEFT_CHANNEL);
            rightFb = ProcessVibrato(rightFb, RIGHT_CHANNEL);

            float lIn = Clamp(leftIn[i], -3.f, 3.f);
            float rIn = Clamp(rightIn[i], -3.f, 3.f);

            float leftFilter, rightFilter;
            filter_->Process(lIn, rIn, leftFilter, rightFilter);

            leftFb += leftFilter;
            rightFb += rightFilter;

            lines_[LEFT_CHANNEL]->write(leftFb);
            lines_[RIGHT_CHANNEL]->write(rightFb);

            float left = LinearCrossFade(outs_[TAP_LEFT_A], outs_[TAP_LEFT_B], tapBlendLeft_);
            float right = LinearCrossFade(outs_[TAP_RIGHT_A], outs_[TAP_RIGHT_B], tapBlendRight_);

            left = comp_[LEFT_CHANNEL]->process(left) * kEchoMakeupGain;
            right = comp_[RIGHT_CHANNEL]->process(right) * kEchoMakeupGain;

            leftOut[i] = CheapEqualPowerCrossFade(lIn, left, patchCtrls_->echoVol, 1.8f);
            rightOut[i] = CheapEqualPowerCrossFade(rIn, right, patchCtrls_->echoVol, 1.8f);
        }
    }
};
