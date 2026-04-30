#pragma once

#include "Commons.h"
#include "RampOscillator.h"
#include "SquareWaveOscillator.h"

/**
 * @brief Moog-style Voltage Controlled Oscillator
 * 
 * Single sawtooth oscillator with built-in sub-oscillator (square, -1 octave).
 * This matches the architecture of Moog Mother-32, Mavis, and Subharmonicon.
 * 
 * Features:
 * - Band-limited sawtooth (antialiased ramp)
 * - Square wave sub-oscillator at 0.5x frequency
 * - Adjustable sub mix level (default 30% like Moog)
 * - Analog drift simulation for warmth
 * - Optional drive/saturation
 */
class MoogVCO
{
private:
    AntialiasedRampOscillator* mainOsc_;
    AntialiasedSquareWaveOscillator* subOsc_;
    float sampleRate_;
    
    // Analog drift
    float driftState_;
    float driftTarget_;
    int driftCounter_;
    
    // Parameters
    float subMix_;
    float drive_;
    float detuneCents_;

public:
    MoogVCO(float sampleRate, float detuneCents = 0.0f)
        : sampleRate_(sampleRate), subMix_(0.3f), drive_(1.0f), detuneCents_(detuneCents)
    {
        mainOsc_ = AntialiasedRampOscillator::create(sampleRate);
        subOsc_ = AntialiasedSquareWaveOscillator::create(sampleRate);
        driftState_ = 0.f;
        driftTarget_ = 0.f;
        driftCounter_ = 0;
    }

    ~MoogVCO()
    {
        AntialiasedRampOscillator::destroy(mainOsc_);
        AntialiasedSquareWaveOscillator::destroy(subOsc_);
    }

    static MoogVCO* create(float sampleRate, float detuneCents = 0.0f)
    {
        return new MoogVCO(sampleRate, detuneCents);
    }

    static void destroy(MoogVCO* osc)
    {
        delete osc;
    }

    void setSubMix(float mix)
    {
        subMix_ = Clamp(mix, 0.0f, 1.0f);
    }

    void setDrive(float drive)
    {
        drive_ = Clamp(drive, 0.5f, 5.0f);
    }

    void setDetune(float cents)
    {
        detuneCents_ = cents;
    }

    void setFrequency(float freq)
    {
        // Apply detune
        float detunedFreq = freq * fast_powf(2.f, detuneCents_ / 1200.f);
        mainOsc_->setFrequency(detunedFreq);
        subOsc_->setFrequency(detunedFreq * 0.5f);
    }

    void processBlock(float freq, FloatArray output, size_t size, float volume)
    {
        // Analog pitch drift
        driftCounter_ += size;
        if (driftCounter_ >= (int)(sampleRate_ * kOscDriftUpdateSec))
        {
            driftTarget_ = RandomFloat(-1.f, 1.f);
            driftCounter_ = 0;
        }
        ONE_POLE(driftState_, driftTarget_, kOscDriftSmoothCoeff);
        float driftCents = driftState_ * kOscDriftCentsMax;
        float driftFactor = fast_powf(2.f, driftCents / 1200.f);
        
        float driftedFreq = freq * driftFactor;
        
        for (size_t i = 0; i < size; i++)
        {
            setFrequency(driftedFreq);
            
            float main = mainOsc_->generate();
            float sub = subOsc_->generate();
            
            // Classic Moog mix: saw + sub-oscillator
            float mix = main + sub * subMix_;
            
            // Apply drive (tanh saturation for warmth)
            float out = SoftClip(mix * drive_);
            
            output[i] = out * volume;
        }
    }

    float generate()
    {
        float main = mainOsc_->generate();
        float sub = subOsc_->generate();
        float mix = main + sub * subMix_;
        return SoftClip(mix * drive_);
    }
};
