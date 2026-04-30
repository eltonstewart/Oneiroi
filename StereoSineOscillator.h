#pragma once

#include "Commons.h"
#include "SineOscillator.h"
#include "MoogVCO.h"
#include "Schmitt.h"

class StereoSineOscillator
{
private:
    PatchCtrls* patchCtrls_;
    PatchCvs* patchCvs_;
    PatchState* patchState_;

    // Standard mode: dual sine oscillators
    SineOscillator* oscs_[2];
    
    // Moog mode: Moog VCO (saw + sub)
    MoogVCO* moogVco_;
    bool moogMode_;

    Schmitt trigger_;

    float oldFreqs_[2];
    float oldVol_;

    bool fadeOut_, fadeIn_;
    float sine1Volume_, sine2Volume_;

    void FadeOut()
    {
        sine2Volume_ -= kOscSineFadeInc;
        if (sine2Volume_ <= 0)
        {
            sine2Volume_ = 0;
            fadeOut_ = false;
            fadeIn_ = true;
        }
    }

    void FadeIn()
    {
        sine2Volume_ += kOscSineFadeInc;
        if (sine2Volume_ >= 0.5f)
        {
            sine2Volume_ = 0.5f;
            fadeIn_ = false;
        }
    }

public:
    StereoSineOscillator(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* PatchState, bool moogMode = false)
    {
        patchCtrls_ = patchCtrls;
        patchCvs_ = patchCvs;
        patchState_ = PatchState;
        moogMode_ = moogMode;

        for (size_t i = 0; i < 2; i++)
        {
            oscs_[i] = SineOscillator::create(patchState_->sampleRate);
        }
        
        // OSC1 Moog VCO: 0 cents detune (main voice)
        moogVco_ = MoogVCO::create(patchState_->sampleRate, 0.0f);
        moogVco_->setSubMix(0.3f);  // 30% sub like classic Moog
        moogVco_->setDrive(1.2f);   // Slight drive for warmth

        fadeOut_ = false;
        fadeIn_ = false;
        sine1Volume_ = 0.5f;
        sine2Volume_ = 0.5f;
        oldVol_ = 0.0f;
    }
    ~StereoSineOscillator()
    {
        for (size_t i = 0; i < 2; i++)
        {
            SineOscillator::destroy(oscs_[i]);
        }
        MoogVCO::destroy(moogVco_);
    }

    void setMoogMode(bool moog)
    {
        moogMode_ = moog;
    }

    static StereoSineOscillator* create(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* PatchState, bool moogMode = false)
    {
        return new StereoSineOscillator(patchCtrls, patchCvs, PatchState, moogMode);
    }

    static void destroy(StereoSineOscillator* obj)
    {
        delete obj;
    }

    void Process(AudioBuffer &output)
    {
        size_t size = output.getSize();

        if (moogMode_)
        {
            // Moog mode: single sawtooth + sub-oscillator (OSC1)
            float vol = Modulate(patchCtrls_->osc1Vol, patchCtrls_->osc1VolModAmount, 
                                patchState_->modValue, patchCtrls_->osc1VolCvAmount, 
                                patchCvs_->osc1Vol, 0.f, 1.f, 
                                patchState_->modAttenuverters, patchState_->cvAttenuverters);
            
            float baseFreq = Clamp(patchCtrls_->oscPitch, kOscFreqMin, kOscFreqMax);
            moogVco_->processBlock(baseFreq, output.getSamples(LEFT_CHANNEL), size, vol * kOScSineGain);
            
            // Copy left to right (mono source, same on both channels)
            for (size_t i = 0; i < size; i++)
            {
                float sample = output.getSamples(LEFT_CHANNEL)[i];
                output.getSamples(RIGHT_CHANNEL).setElement(i, sample);
            }
        }
        else
        {
            // Standard mode: dual sine oscillators with unison
            float u;
            if (patchCtrls_->oscUnison < 0)
            {
                u = Map(patchCtrls_->oscUnison, -1.f, 0.f, 0.5f, 1.f);
            }
            else
            {
                u = Map(patchCtrls_->oscUnison, 0.f, 1.f, 1.f, 2.f);
            }

            float f[2];
            f[0] = Clamp(patchCtrls_->oscPitch, kOscFreqMin, kOscFreqMax);
            f[1] = Clamp(f[0] * u, kOscFreqMin, kOscFreqMax);

            ParameterInterpolator freqParams[2] = {
                ParameterInterpolator(&oldFreqs_[0], f[0], size, ParameterInterpolator::BY_SIZE), 
                ParameterInterpolator(&oldFreqs_[1], f[1], size, ParameterInterpolator::BY_SIZE)
            };
            
            float vol = Modulate(patchCtrls_->osc1Vol, patchCtrls_->osc1VolModAmount, patchState_->modValue, patchCtrls_->osc1VolCvAmount, patchCvs_->osc1Vol, 0.f, 1.f, patchState_->modAttenuverters, patchState_->cvAttenuverters);
            ParameterInterpolator volParam(&oldVol_, vol * kOScSineGain, size, ParameterInterpolator::BY_SIZE);

            for (size_t i = 0; i < size; i++)
            {
                for (size_t j = 0; j < 2; j++)
                {
                    oscs_[j]->setFrequency(freqParams[j].Next());
                }

                if (trigger_.Process(patchState_->oscUnisonCenterFlag) && !fadeOut_)
                {
                    fadeOut_ = true;
                }
                if (fadeOut_)
                {
                    FadeOut();
                }
                else if (fadeIn_)
                {
                    oscs_[1]->setPhase(oscs_[0]->getPhase());
                    FadeIn();
                }

                float out = oscs_[0]->generate() * sine1Volume_ + oscs_[1]->generate() * sine2Volume_;

                out = SoftClip(out); // Gentle analog saturation
                out *= volParam.Next();

                output.getSamples(LEFT_CHANNEL).setElement(i, out);
                output.getSamples(RIGHT_CHANNEL).setElement(i, out);
            }
        }
    }
};
