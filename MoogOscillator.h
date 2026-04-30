#pragma once

#include "Commons.h"
#include "RampOscillator.h"
#include "SquareWaveOscillator.h"

class MoogOscillator
{
private:
    AntialiasedRampOscillator* mainOsc_;
    AntialiasedSquareWaveOscillator* subOsc_;
    float sampleRate_;
    float driftState_;
    float driftTarget_;
    int driftCounter_;
    float subMix_;
    float drive_;

public:
    MoogOscillator(float sampleRate)
    {
        sampleRate_ = sampleRate;
        mainOsc_ = AntialiasedRampOscillator::create(sampleRate);
        subOsc_ = AntialiasedSquareWaveOscillator::create(sampleRate);
        driftState_ = 0.f;
        driftTarget_ = 0.f;
        driftCounter_ = 0;
        subMix_ = 0.3f;
        drive_ = 1.0f;
    }

    ~MoogOscillator()
    {
        AntialiasedRampOscillator::destroy(mainOsc_);
        AntialiasedSquareWaveOscillator::destroy(subOsc_);
    }

    static MoogOscillator* create(float sampleRate)
    {
        return new MoogOscillator(sampleRate);
    }

    static void destroy(MoogOscillator* osc)
    {
        delete osc;
    }

    void setSubMix(float mix)
    {
        subMix_ = mix;
    }

    void setDrive(float drive)
    {
        drive_ = drive;
    }

    void setFrequency(float freq)
    {
        mainOsc_->setFrequency(freq);
        subOsc_->setFrequency(freq * 0.5f);
    }

    float generate()
    {
        float main = mainOsc_->generate();
        float sub = subOsc_->generate();
        
        // Moog-style saturation: drive into soft clip
        float mix = main + sub * subMix_;
        return SoftClip(mix * drive_);
    }
};
