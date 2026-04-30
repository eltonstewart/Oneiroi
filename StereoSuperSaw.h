#pragma once

#include "Commons.h"
#include "MoogVCO.h"

/**
 * @brief Stereo Moog VCO (OSC2) with slight detune for warmth
 * 
 * Replaces the 7-voice supersaw with a second Moog VCO
detuned -5 cents lower than OSC1.
 * This creates a gentle "beating" effect without phase cancellation
 * issues, adding warmth and thickness typical of dual-VCO Moog synths.
 */
class StereoSuperSaw
{
private:
    PatchCtrls* patchCtrls_;
    PatchCvs* patchCvs_;
    PatchState* patchState_;
    
    // Moog VCO with -5 cents detune for beating warmth
    MoogVCO* vco_;
    
    // Standard mode: dual supersaw (kept for backwards compatibility)
    bool standardMode_;

public:
    StereoSuperSaw(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState, bool standardMode = false)
    {
        patchCtrls_ = patchCtrls;
        patchCvs_ = patchCvs;
        patchState_ = patchState;
        standardMode_ = standardMode;
        
        // OSC2: Moog VCO detuned -5 cents for beating warmth
        vco_ = MoogVCO::create(patchState_->sampleRate, -5.0f);
        vco_->setSubMix(0.25f);   // Slightly less sub than OSC1
        vco_->setDrive(1.1f);     // Slightly less drive
    }
    ~StereoSuperSaw()
    {
        MoogVCO::destroy(vco_);
    }

    static StereoSuperSaw* create(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState, bool standardMode = false)
    {
        return new StereoSuperSaw(patchCtrls, patchCvs, patchState, standardMode);
    }

    static void destroy(StereoSuperSaw* obj)
    {
        delete obj;
    }

    void Process(AudioBuffer &output)
    {
        size_t size = output.getSize();
        
        float freq = Modulate(patchCtrls_->oscPitch, patchCtrls_->oscPitchModAmount, 
                             patchState_->modValue, 0, 0, 
                             kOscFreqMin, kOscFreqMax, 
                             patchState_->modAttenuverters, patchState_->cvAttenuverters);
        
        float vol = Modulate(patchCtrls_->osc2Vol, patchCtrls_->osc2VolModAmount, 
                            patchState_->modValue, patchCtrls_->osc2VolCvAmount, 
                            patchCvs_->osc2Vol, 0.f, 1.f, 
                            patchState_->modAttenuverters, patchState_->cvAttenuverters);
        
        // Slightly lower volume than OSC1 (0.85x) to prevent masking
        float targetVol = vol * kOScSuperSawGain * 0.85f;
        
        // Process block
        vco_->processBlock(freq, output.getSamples(LEFT_CHANNEL), size, targetVol);
        
        // Copy left to right (mono source)
        for (size_t i = 0; i < size; i++)
        {
            float sample = output.getSamples(LEFT_CHANNEL)[i];
            output.getSamples(RIGHT_CHANNEL).setElement(i, sample);
        }
    }
};
