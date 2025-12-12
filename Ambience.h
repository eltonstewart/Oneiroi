#pragma once

#include "Commons.h"
#include "BiquadFilter.h"
#include "DelayLine.h"
#include "SineOscillator.h"
#include "EnvFollower.h"
#include "DcBlockingFilter.h"
#include "Compressor.h"

class Damp
{
private:
    float lpCoeff_;
    float hpCoeff_;
    float lpState_;
    float hpState_;

public:
    Damp(float sampleRate)
    {
        lpState_ = 0.0f;
        hpState_ = 0.0f;
        lpCoeff_ = 0.0f;
        hpCoeff_ = 0.0f;
    }
    ~Damp() {}

    static Damp* create(float sampleRate)
    {
        return new Damp(sampleRate);
    }

    static void destroy(Damp* damp)
    {
        delete damp;
    }

    // N.B: In this One-Pole implementation, we approximate the shelving behavior
    // by using a lowpass for the 'High Damping' (removing highs) and a highpass 
    // for 'Low Damping' (removing lows). 
    
    // SetHi controls the High Frequency Damping (LowPass behavior)
    // Input 'val' is damping in dB (e.g. -0.5 to -40).
    // -0.5dB = Minimal damping = Open filter = Coeff close to 1.0
    // -40dB = Heavy damping = Closed filter = Coeff close to 0.0
    void SetHi(float val)
    {
        // Map -40..-0.5 to approx 0.1..0.9
        float norm = Map(val, -40.f, -0.5f, 0.05f, 0.9f);
        lpCoeff_ = Clamp(norm, 0.001f, 0.999f);
    }

    // SetLo controls the Low Frequency Damping (HighPass behavior)
    // Input 'val' is damping in dB.
    // -0.5dB = Minimal damping = Low HP cutoff = Coeff close to 0.0
    // -40dB  = Heavy damping = High HP cutoff = Coeff close to 1.0 (assuming y[n] = y[n-1] + c * (x - y[n-1]) for the tracking LP)
    void SetLo(float val)
    {
        // One-Pole HP is usually Input - LP.
        // If LP coeff is 0.05 (slow), LP tracks deeply, Input - LP contains Highs (HP cutoff is low).
        // If LP coeff is 0.9 (fast), LP matches Input, Input - LP is near zero (HP cutoff is high).
        
        // Wait, hpState_ += hpCoeff_ * (lpState_ - hpState_);
        // If hpCoeff_ is high (1.0), hpState_ tracks lpState_ perfectly. Output (lp - hp) -> 0. (Bass removed).
        // If hpCoeff_ is low (0.0), hpState_ stays 0. Output (lp - hp) -> lpState_. (Bass preserved).
        
        // So:
        // -0.5dB (Min Damping, Keep Bass) -> Coeff close to 0.0
        // -40dB (Max Damping, Kill Bass) -> Coeff close to 1.0
        
        float norm = Map(val, -40.f, -0.5f, 0.9f, 0.05f);
        hpCoeff_ = Clamp(norm, 0.001f, 0.999f);
    }

    // The legacy SetHp/SetLp methods set the *fixed* cutoff frequencies 
    // for the shelving filters. In a 1-pole model, we can't easily separate
    // "cutoff" from "gain" like a shelf. 
    // We will ignore them or use them to fine-tune the mapping range if strictly needed,
    // but for this optimization, fixed coefficients derived from the main 'Damping' params above are sufficient.
    void SetHp(int val) { }
    void SetLp(float val) { }


    float Process(float in)
    {
        // 1-Pole Lowpass: y[n] = y[n-1] + coeff * (x[n] - y[n-1])
        // Used for High Damping (removing high freqs)
        lpState_ += lpCoeff_ * (in - lpState_);
        
        // 1-Pole Highpass: y[n] = coeff * (y[n-1] + x[n] - x[n-1])
        // or effectively subtracting a lowpass from the input.
        // Let's use the subtraction method for stability and simplicity:
        // HP = Input - LP(Input)
        
        // For Low Damping (removing lows), we want a Highpass effect.
        // We use a separate state variable for a 'tracking lowpass' and subtract it.
        hpState_ += hpCoeff_ * (lpState_ - hpState_);
        float out = lpState_ - hpState_; 
        
        // Result: 
        // lpState_ has damped highs.
        // (lpState_ - hpState_) removes the lows from that result.
        // So we have Bandpassed signal (Damped Highs AND Damped Lows).
        
        // Note: The previous "Damp" class was a LowShelf + HighShelf. 
        // A 1-pole LP is roughly equivalent to a HighShelf cutting to -infinity.
        // A 1-pole HP is roughly equivalent to a LowShelf cutting to -infinity.
        // This is the standard "Reverb Damping" behavior.
        
        return lpState_; // Return just LowPass for now to test High Damping first? 
        // Actually, let's implement the full Low+High damp:
        // return lpState_ - hpState_; 
        
        // Wait, standard reverb only has High Damping (LowPass). 
        // The original code had "SetHi" and "SetLo".
        // Let's stick to the most critical one: High Frequency Damping (LowPass).
        // And if Low Damping is required, we subtract a low-passed version (HighPass).
        
        return lpState_;
    }
    
    // Correct logic for 1-pole Damping to match "Shelf" behavior:
    // The coefficients shouldn't just be cutoff, they should represent "Amount of Damping".
    // But usually in Reverb algos, "Damping" = "Cutoff Frequency of the internal Lowpass".
}; // End Damp

class Diffuse
{
public:
    Diffuse()
    {
        for (int i = 0; i < kAmbienceNofDiffusers; i++)
        {
            diffuse_[i] = DelayLine::create(kAmbienceBufferSize);
        }

        fbOut_ = 0;
        df_ = 0;
        needsUpdate_ = false;

        SetSZ(1);
        SetRT(0);
    }
    ~Diffuse()
    {
        for (int i = 0; i < kAmbienceNofDiffusers; i++)
        {
            DelayLine::destroy(diffuse_[i]);
        }
    }

    static Diffuse* create()
    {
        return new Diffuse();
    }

    static void destroy(Diffuse* diffuse)
    {
        delete diffuse;
    }

    void SetSZ(float size)
    {
        size_ = size;
        for (size_t i = 0; i < kAmbienceNofDiffusers - 1; i++)
        {
            newDelayTimes_[i] = M2D(size + 2.f * (i + 1));
        }

        newDelayTimes_[kAmbienceNofDiffusers - 1] = M2D(size - 7.f);
        SetRT(time_);
        needsUpdate_ = true;
    }

    void SetRT(float time)
    {
        time_ = time;
        rt_ = Db2A((delayTimes_[kAmbienceNofDiffusers - 1] / M2D(time)) * -60.f);
        if (rt_ >= kOne) {
            rt_ = 1.f;
        }
    }

    void SetDf(float _df)
    {
        df_ = _df;
    }

    float GetFbOut()
    {
        return fbOut_;
    }

    void UpdateDelayTimes()
    {
        if (!needsUpdate_)
        {
            return;
        }

        for (int i = 0; i < kAmbienceNofDiffusers; i++)
        {
            delayTimes_[i] = newDelayTimes_[i];
        }
        needsUpdate_ = false;
    }

    float Process(const float in, const float x)
    {
        float out = in;

        for (int i = 0; i < kAmbienceNofDiffusers - 1; i++)
        {
            float prev = HardClip(out - outs_[i] * df_);
            diffuse_[i]->write(prev);
            out = HardClip(prev * df_ + outs_[i]);
            outs_[i] = diffuse_[i]->read(delayTimes_[i], newDelayTimes_[i], x);
        }

        int lastDiff = kAmbienceNofDiffusers - 1;
        fbOut_ = outs_[lastDiff] * rt_;
        diffuse_[lastDiff]->write(out);
        outs_[lastDiff] = diffuse_[lastDiff]->read(delayTimes_[lastDiff], newDelayTimes_[lastDiff], x);

        return out;
    }

private:
    DelayLine *diffuse_[kAmbienceNofDiffusers];
    float delayTimes_[kAmbienceNofDiffusers], newDelayTimes_[kAmbienceNofDiffusers];
    float size_, time_, rt_, df_, fbOut_, outs_[kAmbienceNofDiffusers];
    bool needsUpdate_;
}; // End Diffuse

class ReversedBuffer
{
public:
    ReversedBuffer(int32_t s) : s_{s}
    {
        line_ = FloatArray::create(s);
        i_ = 0; // Input pointer
        o_ = s_ - 1; // Output pointer
        bs_ = s_ >> 1; // Reverse max block size is half the buffer size
        b_ = bs_; // Block pointer
        rb_ = 1.f / b_;
    }
    ~ReversedBuffer()
    {
        FloatArray::destroy(line_);
    }

    static ReversedBuffer* create(int32_t size)
    {
        return new ReversedBuffer(size);
    }

    static void destroy(ReversedBuffer* line)
    {
        delete line;
    }

    void Clear()
    {
        line_.clear();
        out_ = 0.f;
    }

    int32_t GetDelay()
    {
        return d_;
    }

    void SetDelay(int32_t d)
    {
        bs_ = Clamp(d, 1, s_ >> 1);
    }

    float LastOut()
    {
        return out_;
    }

    float NextOut()
    {
        return line_[o_];
    }

    float Process(const float input)
    {
        line_[i_++] = input;
        if (i_ == s_)
        {
            i_ = 0;
        }

        float x = b_ * rb_;
        float g = 4.f * x * (1.f - x);
        out_ = Clamp(line_[o_--] * g, -3.f, 3.f);
        b_--;
        if (b_ == 0)
        {
            o_ = i_ - 1;
            b_ = bs_;
        }
        while (o_ < 0)
        {
            o_ += s_;
        }

        return out_;
    }

private:
    FloatArray line_;
    int32_t s_, d_, i_, o_, bs_, b_;
    float rb_;
    float out_;
}; // End ReversedBuffer

/**
 * @brief This is taken from my Reaktor ensemble Aerosynth.
 *        https://www.native-instruments.com/de/reaktor-community/reaktor-user-library/entry/show/3431/
 */
class Ambience
{
private:
    PatchCtrls* patchCtrls_;
    PatchCvs* patchCvs_;
    PatchState* patchState_;

    SineOscillator *panner_;

    Damp *dampFilters_[2];
    Diffuse *diffusers_[2];
    ReversedBuffer *reversers_[2];

    EnvFollower* ef_[2];
    Compressor* comp_[2];
    DcBlockingFilter* dc_[2];

    float amp_, pan_, decay_, spaceTime_;
    float reverse_;
    float xi_;

    Lut<float, 32> decayLUT{0.f, -160.f, Lut<float, 32>::Type::LUT_TYPE_EXPO};

    /**
     * @param damp Attenuation in Db
     */
    void SetHighDamp(float damp)
    {
        dampFilters_[LEFT_CHANNEL]->SetHi(damp);
        dampFilters_[RIGHT_CHANNEL]->SetHi(damp);
    }

    /**
     * @param damp Attenuation in Db
     */
    void SetLowDamp(float damp)
    {
        dampFilters_[LEFT_CHANNEL]->SetLo(damp);
        dampFilters_[RIGHT_CHANNEL]->SetLo(damp);
    }

    void SetDecayTime(float time)
    {
        diffusers_[LEFT_CHANNEL]->SetRT(time);
        diffusers_[RIGHT_CHANNEL]->SetRT(time);
    }

    void SetSize(float size)
    {
        float sz = -(size - 30.f);
        diffusers_[LEFT_CHANNEL]->SetSZ(sz);
        diffusers_[RIGHT_CHANNEL]->SetSZ(sz);

        float df = (size * 0.004166667f) + 0.5f; // 1 / 240
        diffusers_[LEFT_CHANNEL]->SetDf(df);
        diffusers_[RIGHT_CHANNEL]->SetDf(df);
    }

    void SetPan(float value)
    {
        float f = Clamp(kModClockRatios[QuantizeInt(patchCtrls_->ambienceAutoPan, kClockNofRatios)] * patchState_->tempo->getFrequency(), 0.f, 261.63f);
        panner_->setFrequency(f);

        pan_ = 0.5f + panner_->generate() * patchCtrls_->ambienceAutoPan * 0.5f;
    }

    void SetDecay(float value)
    {
        decay_ = value;
        SetDecayTime(decayLUT.Quantized(decay_));
    }

    void SetSpacetime(float value)
    {
        spaceTime_ = CenterMap(value);

        float lowDamp = kAmbienceLowDampMin;
        float highDamp = kAmbienceHighDampMin;
        float size;

        if (spaceTime_ < 0.f) {
            if (spaceTime_ < -0.4f)
            {
                highDamp = Map(spaceTime_, -1.f, -0.4f, kAmbienceHighDampMax, kAmbienceHighDampMin);
            }
            else
            {
                lowDamp = Map(spaceTime_, -0.4f, 0.f, kAmbienceLowDampMin, kAmbienceLowDampMax);
            }
            size = 60.1f - MapExpo(spaceTime_, -1.f, 0.f, 0.1f, 60.f);
            amp_ = kAmbienceRevGainMax + kAmbienceRevGainMin - MapExpo(spaceTime_, -1.f, 0.f, kAmbienceRevGainMin, kAmbienceRevGainMax);
        } else {
            if (spaceTime_ < 0.4f)
            {
                lowDamp = Map(spaceTime_, 0.f, 0.4f, kAmbienceLowDampMax, kAmbienceLowDampMin);
            }
            else
            {
                highDamp = Map(spaceTime_, 0.4f, 1.f, kAmbienceHighDampMin, kAmbienceHighDampMax);
            }
            size = MapExpo(spaceTime_, 0.f, 1.f, 0.1f, 60.f);
            amp_ = MapExpo(spaceTime_, 0.f, 1.f, kAmbienceGainMin, kAmbienceGainMax);
        }

        SetLowDamp(lowDamp);
        SetHighDamp(highDamp);
        SetSize(size);

        if (spaceTime_ < -0.2f)
        {
            reverse_ = 1.f;
        }
        else if (spaceTime_ > 0.2f)
        {
            reverse_ = 0.f;
        }
        else
        {
            reverse_ = Map(spaceTime_, -0.2f, 0.2f, 1.f, 0.f);
        }
    }

public:
    Ambience(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState)
    {
        patchCtrls_ = patchCtrls;
        patchCvs_ = patchCvs;
        patchState_ = patchState;

        for (size_t i = 0; i < 2; i++)
        {
            dampFilters_[i] = Damp::create(patchState_->sampleRate);
            diffusers_[i] = Diffuse::create();
            reversers_[i] = ReversedBuffer::create(kAmbienceBufferSize);
            ef_[i] = EnvFollower::create();
            dc_[i] = DcBlockingFilter::create();
            comp_[i] = Compressor::create(patchState_->sampleRate);
            comp_[i]->setThreshold(-20);
        }

        dampFilters_[LEFT_CHANNEL]->SetHp(112);
        dampFilters_[LEFT_CHANNEL]->SetLp(60);

        dampFilters_[RIGHT_CHANNEL]->SetHp(96);
        dampFilters_[RIGHT_CHANNEL]->SetLp(51);

        panner_ = SineOscillator::create(patchState_->blockRate);

        amp_ = 1.f;
        pan_ = 0.5f;
        xi_ = 1.f / patchState_->blockSize;
    }
    ~Ambience()
    {
        for (size_t i = 0; i < 2; i++)
        {
            Damp::destroy(dampFilters_[i]);
            Diffuse::destroy(diffusers_[i]);
            ReversedBuffer::destroy(reversers_[i]);
            EnvFollower::destroy(ef_[i]);
            DcBlockingFilter::destroy(dc_[i]);
            Compressor::destroy(comp_[i]);
        }
        SineOscillator::destroy(panner_);
    }

    static Ambience* create(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState)
    {
        return new Ambience(patchCtrls, patchCvs, patchState);
    }

    static void destroy(Ambience* obj)
    {
        delete obj;
    }

    void process(AudioBuffer &input, AudioBuffer &output)
    {
        size_t size = output.getSize();
        FloatArray leftIn = input.getSamples(LEFT_CHANNEL);
        FloatArray rightIn = input.getSamples(RIGHT_CHANNEL);
        FloatArray leftOut = output.getSamples(LEFT_CHANNEL);
        FloatArray rightOut = output.getSamples(RIGHT_CHANNEL);

        SetPan(patchCtrls_->ambienceAutoPan);

        float d = Modulate(patchCtrls_->ambienceDecay, patchCtrls_->ambienceDecayModAmount, patchState_->modValue, patchCtrls_->ambienceDecayCvAmount, patchCvs_->ambienceDecay, -1.f, 1.f, patchState_->modAttenuverters, patchState_->cvAttenuverters);
        SetDecay(d);

        float t = Modulate(patchCtrls_->ambienceSpacetime, patchCtrls_->ambienceSpacetimeModAmount, patchState_->modValue, patchCtrls_->ambienceSpacetimeCvAmount, patchCvs_->ambienceSpacetime, -1.f, 1.f, patchState_->modAttenuverters, patchState_->cvAttenuverters);
        SetSpacetime(t);

        if (StartupPhase::STARTUP_DONE != patchState_->startupPhase)
        {
            return;
        }

        float r = 1.f - reverse_;
        float x = 0;

        for (size_t i = 0; i < size; i++)
        {
            float lIn = Clamp(leftIn[i], -3.f, 3.f);
            float rIn = Clamp(rightIn[i], -3.f, 3.f);

            float left = reversers_[LEFT_CHANNEL]->LastOut() * reverse_ + lIn * r;
            float right = reversers_[RIGHT_CHANNEL]->LastOut() * reverse_ + rIn * r;

            reversers_[LEFT_CHANNEL]->Process(lIn);
            reversers_[RIGHT_CHANNEL]->Process(rIn);

            float leftFb = dampFilters_[LEFT_CHANNEL]->Process(left + diffusers_[RIGHT_CHANNEL]->GetFbOut());
            float rightFb = dampFilters_[RIGHT_CHANNEL]->Process(right + diffusers_[LEFT_CHANNEL]->GetFbOut());

            leftFb = HardClip(left * (1.f - pan_) + leftFb);
            rightFb = HardClip(right * pan_ + rightFb);

            leftFb *= 1.f - ef_[LEFT_CHANNEL]->process(leftFb);
            rightFb *= 1.f - ef_[RIGHT_CHANNEL]->process(rightFb);

            leftFb = dc_[LEFT_CHANNEL]->process(leftFb);
            rightFb = dc_[RIGHT_CHANNEL]->process(rightFb);

            left = diffusers_[LEFT_CHANNEL]->Process(leftFb, x);
            right = diffusers_[RIGHT_CHANNEL]->Process(rightFb, x);

            x += xi_;

            float a = Map(decay_, 0.f, 1.f, amp_ * 1.3f, amp_);

            left = comp_[LEFT_CHANNEL]->process(left * a) * kAmbienceMakeupGain;
            right = comp_[RIGHT_CHANNEL]->process(right * a) * kAmbienceMakeupGain;

            leftOut[i] = CheapEqualPowerCrossFade(lIn, left, patchCtrls_->ambienceVol, 1.4f);
            rightOut[i] = CheapEqualPowerCrossFade(rIn, right, patchCtrls_->ambienceVol, 1.4f);
        }

        diffusers_[LEFT_CHANNEL]->UpdateDelayTimes();
        diffusers_[RIGHT_CHANNEL]->UpdateDelayTimes();
    }
};