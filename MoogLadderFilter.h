#pragma once

#include "Commons.h"

/**
 * @brief Moog transistor ladder filter (24dB/octave)
 * 
 * Implementation of the classic Moog ladder filter with:
 * - 4 cascaded one-pole lowpass stages (24dB/octave)
 * - Resonance feedback from stage 4 back to input
 * - tanh saturation for that warm Moog drive
 * 
 * Based on the topology from:
 * - Moog Minimoog Model D
 * - Moog Mother-32
 * - Moog Mavis
 * 
 * The key characteristics:
 * 1. Steep 24dB rolloff (4 x 6dB stages)
 * 2. Resonance creates a peak before cutoff ("scream")
 * 3. Saturation at each stage creates warmth
 * 4. Self-oscillation at high resonance
 */
class MoogLadderFilter
{
private:
    float stage_[4];      // Four filter stages
    float delay_[4];      // State variables for denormal protection
    float sampleRate_;
    float cutoff_;
    float resonance_;
    float drive_;
    float cutoffFreq_;

    inline void updateCoefficients()
    {
        // Clamp cutoff to Nyquist
        float f = Clamp(cutoff_, 20.0f, sampleRate_ * 0.45f);
        
        // Normalized cutoff frequency (0 to 1)
        // Moog ladder uses a warped frequency to match analog behavior
        float wc = f / sampleRate_;
        
        // Bilinear transform with pre-warping
        // The Moog ladder has a specific non-linear frequency response
        float wd = k2Pi * f;
        float wa = 2.0f * sampleRate_ * tanf(wd / (2.0f * sampleRate_));
        float g = wa / (2.0f * sampleRate_);
        
        // Store the coefficient
        cutoffFreq_ = g;
    }

public:
    MoogLadderFilter(float sampleRate)
        : sampleRate_(sampleRate), cutoff_(1000.0f), 
          resonance_(0.0f), drive_(1.0f), cutoffFreq_(0.0f)
    {
        for (int i = 0; i < 4; i++)
        {
            stage_[i] = 0.0f;
            delay_[i] = 0.0f;
        }
        updateCoefficients();
    }

    static MoogLadderFilter* create(float sampleRate)
    {
        return new MoogLadderFilter(sampleRate);
    }

    static void destroy(MoogLadderFilter* filter)
    {
        delete filter;
    }

    void setCutoff(float cutoff)
    {
        cutoff_ = Clamp(cutoff, 20.0f, 20000.0f);
        updateCoefficients();
    }

    void setResonance(float resonance)
    {
        // Resonance range: 0 to ~4.0 (self-oscillation)
        resonance_ = Clamp(resonance, 0.0f, 4.0f);
    }

    void setDrive(float drive)
    {
        // Drive: 1.0 = clean, up to 5.0 = heavy saturation
        drive_ = Clamp(drive, 0.5f, 5.0f);
    }

    /**
     * @brief Process a single sample through the ladder filter
     * 
     * @param input Input sample
     * @return float Filtered output
     */
    inline float process(float input)
    {
        // Resonance feedback: take output from stage 4, scale by resonance, 
        // subtract from input (negative feedback)
        float feedback = stage_[3] * resonance_;
        
        // Input with feedback
        float x = input - feedback;
        
        // Apply drive saturation to input
        // This is where the Moog "warmth" comes from
        x = tanhf(x * drive_) / drive_;
        
        // Four one-pole lowpass stages
        // Each stage: y = y + g * (x - y) where g is the cutoff coefficient
        for (int i = 0; i < 4; i++)
        {
            stage_[i] += cutoffFreq_ * (x - stage_[i]) + 1e-18f;  // + denormal protection
            x = stage_[i];
        }
        
        return stage_[3];
    }

    /**
     * @brief Process stereo audio buffer
     */
    void process(AudioBuffer& input, AudioBuffer& output)
    {
        FloatArray leftIn = input.getSamples(LEFT_CHANNEL);
        FloatArray rightIn = input.getSamples(RIGHT_CHANNEL);
        FloatArray leftOut = output.getSamples(LEFT_CHANNEL);
        FloatArray rightOut = output.getSamples(RIGHT_CHANNEL);
        size_t size = input.getSize();
        
        for (size_t i = 0; i < size; i++)
        {
            leftOut[i] = process(leftIn[i]);
            rightOut[i] = process(rightIn[i]);
        }
    }

    /**
     * @brief Process mono channel
     */
    void process(float* input, float* output, size_t size)
    {
        for (size_t i = 0; i < size; i++)
        {
            output[i] = process(input[i]);
        }
    }

    void reset()
    {
        for (int i = 0; i < 4; i++)
        {
            stage_[i] = 0.0f;
            delay_[i] = 0.0f;
        }
    }
};
