#pragma once

#include "Commons.h"
#include "RampOscillator.h"
#include "SquareWaveOscillator.h"

/**
 * @brief 7 sawtooth oscillators with detuning and mixing.
 *        Adapted from
 *        https://web.archive.org/web/20110627045129/https://www.nada.kth.se/utbildning/grukth/exjobb/rapportlistor/2010/rapporter10/szabo_adam_10131.pdf
 *
 */
class SuperSaw
{
private:
  AntialiasedRampOscillator* oscs_[7];
  AntialiasedSquareWaveOscillator* subOsc_;
  float detunes_[7];
  float volumes_[7];
  float oldFreq_;
  float oldVol_;
  float detune_;

  // Analog drift state
  float driftState_;
  float driftTarget_;
  int driftCounter_;

public:
    SuperSaw(float sampleRate)
    {
        for (size_t i = 0; i < 7; i++)
        {
            oscs_[i] = AntialiasedRampOscillator::create(sampleRate);
            detunes_[i] = 0;
            volumes_[i] = 0;
        }
        subOsc_ = AntialiasedSquareWaveOscillator::create(sampleRate);
        detune_ = 0;
        oldVol_ = 0.0f;
        driftState_ = 0.f;
        driftTarget_ = 0.f;
        driftCounter_ = 0;
    }
    ~SuperSaw()
    {
        for (size_t i = 0; i < 7; i++)
        {
            AntialiasedRampOscillator::destroy(oscs_[i]);
        }
        AntialiasedSquareWaveOscillator::destroy(subOsc_);
    }

    void SetFreq(float value)
    {
        for (size_t i = 0; i < 7; i++)
        {
            oscs_[i]->setFrequency(value * detunes_[i]);
        }
    }

    void SetDetune(float value, bool minor = false)
    {
        detune_ = value * 0.6f; // Increased from 0.4f for wider detuning

        if (minor)
        {
            detunes_[0] = 1 - detune_ * 0.877538f;
            detunes_[1] = 1 - detune_ * 0.66516f;
            detunes_[2] = 1 - detune_ * 0.318207f;
            detunes_[3] = 1;
            detunes_[4] = 1 + detune_ * 0.189207f;
            detunes_[5] = 1 + detune_ * 0.498307f;
            detunes_[6] = 1 + detune_ * 0.781797f;
        }
        else
        {
            detunes_[0] = 1 - detune_ * 0.11002313f;
            detunes_[1] = 1 - detune_ * 0.06288439f;
            detunes_[2] = 1 - detune_ * 0.01952356f;
            detunes_[3] = 1;
            detunes_[4] = 1 + detune_ * 0.01991221f;
            detunes_[5] = 1 + detune_ * 0.06216538f;
            detunes_[6] = 1 + detune_ * 0.10745242f;
        }

        value = Clamp(value * 0.5f, 0.005f, 0.7f); // Increased max from 0.5f to 0.7f for more detune

        float y = -0.73764f * fast_powf(value, 2.f) + 1.2841f * value + 0.044372f;
        volumes_[0] = y;
        volumes_[1] = y;
        volumes_[2] = y;
        volumes_[3] = -0.55366f * value + 0.99785f;
        volumes_[4] = y;
        volumes_[5] = y;
        volumes_[6] = y;
    }

    static SuperSaw* create(float sampleRate)
    {
        return new SuperSaw(sampleRate);
    }

    static void destroy(SuperSaw* obj)
    {
        delete obj;
    }

    void Process(float freq, FloatArray output, float targetVol)
    {
        size_t size = output.getSize();

        // Analog pitch drift - thermal VCO instability
        driftCounter_ += size;
        if (driftCounter_ >= (int)(oscs_[0]->getSampleRate() * kOscDriftUpdateSec))
        {
            driftTarget_ = RandomFloat(-1.f, 1.f);
            driftCounter_ = 0;
        }
        ONE_POLE(driftState_, driftTarget_, kOscDriftSmoothCoeff);
        float driftCents = driftState_ * kOscDriftCentsMax;
        float driftFactor = fast_powf(2.f, driftCents / 1200.f);
        freq *= driftFactor;

        ParameterInterpolator freqParam(&oldFreq_, freq, size, ParameterInterpolator::BY_SIZE);
        ParameterInterpolator volParam(&oldVol_, targetVol, size, ParameterInterpolator::BY_SIZE);

        for (size_t i = 0; i < size; i++)
        {
            float currentFreq = freqParam.Next();
            SetFreq(currentFreq);
            float v = volParam.Next();
            for (size_t j = 0; j < 7; j++)
            {
                output[i] += oscs_[j]->generate() * volumes_[j] * v;
            }
            // Sub-oscillator: square wave one octave down
            subOsc_->setFrequency(currentFreq * 0.5f);
            output[i] += subOsc_->generate() * kOscSubMix * v;
            // Gentle analog saturation
            output[i] = SoftClip(output[i]);
        }

        output.multiply(0.3f * (1.4f - detune_));
    }
};

class StereoSuperSaw
{
private:
    PatchCtrls* patchCtrls_;
    PatchCvs* patchCvs_;
    PatchState* patchState_;
    SuperSaw* saws_[2];

    void SetDetune(float value, bool minor = false)
    {
        for (int i = 0; i < 2; i++)
        {
            saws_[i]->SetDetune(value);
        }
    }

public:
    StereoSuperSaw(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState)
    {
        patchCtrls_ = patchCtrls;
        patchCvs_ = patchCvs;
        patchState_ = patchState;

        for (int i = 0; i < 2; i++)
        {
            saws_[i] = SuperSaw::create(patchState_->sampleRate);
        }
    }
    ~StereoSuperSaw()
    {
        for (int i = 0; i < 2; i++)
        {
            SuperSaw::destroy(saws_[i]);
        }
    }

    static StereoSuperSaw* create(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState)
    {
        return new StereoSuperSaw(patchCtrls, patchCvs, patchState);
    }

    static void destroy(StereoSuperSaw* obj)
    {
        delete obj;
    }

    void Process(AudioBuffer &output)
    {
        float u = patchCtrls_->oscUnison;
        if (patchCtrls_->oscUnison < 0)
        {
            u *= 0.5f;
        }

        float f = Modulate(patchCtrls_->oscPitch + patchCtrls_->oscPitch * u, patchCtrls_->oscPitchModAmount, patchState_->modValue, 0, 0, kOscFreqMin, kOscFreqMax, patchState_->modAttenuverters, patchState_->cvAttenuverters);

        float d = Modulate(patchCtrls_->oscDetune, patchCtrls_->oscDetuneModAmount, patchState_->modValue, patchCtrls_->oscDetuneCvAmount, patchCvs_->oscDetune, -1.f, 1.f, patchState_->modAttenuverters, patchState_->cvAttenuverters);
        SetDetune(d);

        float vol = Modulate(patchCtrls_->osc2Vol, patchCtrls_->osc2VolModAmount, patchState_->modValue, patchCtrls_->osc2VolCvAmount, patchCvs_->osc2Vol, 0.f, 1.f, patchState_->modAttenuverters, patchState_->cvAttenuverters);
        float targetVol = vol * kOScSuperSawGain;

        saws_[LEFT_CHANNEL]->Process(f, output.getSamples(LEFT_CHANNEL), targetVol);
        saws_[RIGHT_CHANNEL]->Process(f, output.getSamples(RIGHT_CHANNEL), targetVol);
    }
};
