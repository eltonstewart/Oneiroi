#pragma once

#include "Commons.h"
#include "EnvFollower.h"
#include <algorithm>

enum PlaybackDirection
{
    PLAYBACK_STALLED,
    PLAYBACK_FORWARD,
    PLAYBACK_BACKWARDS = -1
};

class WriteHead
{
private:
    enum WriteStatus
    {
        WRITE_STATUS_INACTIVE,
        WRITE_STATUS_FADE_IN,
        WRITE_STATUS_FADE_OUT,
        WRITE_STATUS_ACTIVE,
    };

    FloatArray* buffer_;

    WriteStatus status_;

    uint32_t index_;
    uint32_t start_;
    uint32_t end_;

    int fadeIndex_;

    bool doFade_;

public:
    WriteHead(FloatArray* buffer)
    {
        buffer_ = buffer;

        status_ = WRITE_STATUS_INACTIVE;

        fadeIndex_ = 0;

        doFade_ = false;
    }
    ~WriteHead() {}

    static WriteHead* create(FloatArray* buffer)
    {
        return new WriteHead(buffer);
    }

    static void destroy(WriteHead* obj)
    {
        delete obj;
    }

    inline bool IsWriting()
    {
        return WRITE_STATUS_INACTIVE != status_;
    }

    inline void Start()
    {
        if (WRITE_STATUS_INACTIVE == status_)
        {
            status_ = WRITE_STATUS_FADE_IN;
            doFade_ = true;
            fadeIndex_ = 0;
        }
    }

    inline void Stop()
    {
        if (WRITE_STATUS_ACTIVE == status_)
        {
            status_ = WRITE_STATUS_FADE_OUT;
            doFade_ = true;
            fadeIndex_ = 0;
        }
    }

    inline void Write(uint32_t position, float value)
    {
        while (position >= kLooperTotalBufferLength)
        {
            position -= kLooperTotalBufferLength;
        }
        while (position < 0)
        {
            position += kLooperTotalBufferLength;
        }

        if (doFade_)
        {
            float x = fadeIndex_ * kLooperFadeSamplesR;
            if (WRITE_STATUS_FADE_IN == status_)
            {
                x = 1.f - x;
            }
            fadeIndex_++;
            if (fadeIndex_ == kLooperFadeSamples)
            {
                x = WRITE_STATUS_FADE_OUT == status_;
                doFade_ = false;
                status_ = (WRITE_STATUS_FADE_IN == status_ ? WRITE_STATUS_ACTIVE : WRITE_STATUS_INACTIVE);
            }
            value = CheapEqualPowerCrossFade(value, buffer_->getElement(position), x);
        }

        if (WRITE_STATUS_INACTIVE != status_)
        {
            buffer_->setElement(position, value);
        }
    }
};

class LooperBuffer
{
private:
    FloatArray buffer_;

    float* clearBlock_;

    WriteHead* writeHeads_[2];

public:
    LooperBuffer()
    {
        buffer_ = FloatArray::create(kLooperTotalBufferLength);
        buffer_.noise();
        buffer_.multiply(kLooperNoiseLevel); // Tame the noise a bit

        clearBlock_ = buffer_.getData();

        for (size_t i = 0; i < 2; i++)
        {
            writeHeads_[i] = WriteHead::create(&buffer_);
        }
    }
    ~LooperBuffer()
    {
        for (size_t i = 0; i < 2; i++)
        {
            WriteHead::destroy(writeHeads_[i]);
        }
    }

    static LooperBuffer* create()
    {
        return new LooperBuffer();
    }

    static void destroy(LooperBuffer* obj)
    {
        delete obj;
    }

    FloatArray* GetBuffer()
    {
        return &buffer_;
    }

    inline bool Clear()
    {
        if (clearBlock_ == buffer_.getData() + kLooperTotalBufferLength)
        {
            clearBlock_ =  buffer_.getData();

            return true;
        }

        memset(clearBlock_, 0, kLooperClearBlockTypeSize);
        clearBlock_ += kLooperClearBlockSize;

        return false;
    }

    inline void Write(uint32_t i, float left, float right)
    {
        writeHeads_[LEFT_CHANNEL]->Write(i, left);
        writeHeads_[RIGHT_CHANNEL]->Write(i + kLooperChannelBufferLength, right);
    }

    inline bool IsRecording()
    {
        return writeHeads_[LEFT_CHANNEL]->IsWriting() && writeHeads_[RIGHT_CHANNEL]->IsWriting();
    }

    inline void StartRecording()
    {
        writeHeads_[LEFT_CHANNEL]->Start();
        writeHeads_[RIGHT_CHANNEL]->Start();
    }

    inline void StopRecording()
    {
        writeHeads_[LEFT_CHANNEL]->Stop();
        writeHeads_[RIGHT_CHANNEL]->Stop();
    }

    inline float ReadLeft(int32_t position)
    {
        while (position >= kLooperChannelBufferLength)
        {
            position -= kLooperChannelBufferLength;
        }
        while (position < 0)
        {
            position += kLooperChannelBufferLength;
        }

        return buffer_[position];
    }

    inline float ReadRight(int32_t position)
    {
        position += kLooperChannelBufferLength;

        while (position >= kLooperTotalBufferLength)
        {
            position -= kLooperChannelBufferLength;
        }
        while (position < kLooperChannelBufferLength)
        {
            position += kLooperChannelBufferLength;
        }

        return buffer_[position];
    }

    // Helper to read 4 points for Hermite interpolation
    // p: integer position
    // dir: direction (1 or -1)
    inline void Read4Left(int32_t p, int32_t dir, float& xm1, float& x0, float& x1, float& x2)
    {
        xm1 = ReadLeft(p - dir);
        x0  = ReadLeft(p);
        x1  = ReadLeft(p + dir);
        x2  = ReadLeft(p + dir * 2);
    }

    inline void Read4Right(int32_t p, int32_t dir, float& xm1, float& x0, float& x1, float& x2)
    {
        xm1 = ReadRight(p - dir);
        x0  = ReadRight(p);
        x1  = ReadRight(p + dir);
        x2  = ReadRight(p + dir * 2);
    }

    inline void Read(float p, float &left, float &right, PlaybackDirection direction = PLAYBACK_FORWARD)
    {
        if (direction == PLAYBACK_STALLED) {
            left = 0.f; 
            right = 0.f;
            return;
        }

        int32_t i = int32_t(p);
        float f = p - i;

        // Cubic Hermite Interpolation
        // Requires 4 points: y[-1], y[0], y[1], y[2]
        // In our case, relative to direction: x(i-1), x(i), x(i+1), x(i+2)
        
        float lm1, l0, l1, l2;
        float rm1, r0, r1, r2;
        int32_t dir = (int32_t)direction;

        Read4Left(i, dir, lm1, l0, l1, l2);
        Read4Right(i, dir, rm1, r0, r1, r2);

        // Hermite implementation (Catmull-Rom)
        // c0 = x0
        // c1 = 0.5 * (x1 - xm1)
        // c2 = xm1 - 2.5*x0 + 2*x1 - 0.5*x2
        // c3 = 0.5*(x2 - xm1) + 1.5*(x0 - x1)
        // out = ((c3*f + c2)*f + c1)*f + c0

        // Left
        float c1 = 0.5f * (l1 - lm1);
        float c2 = lm1 - 2.5f * l0 + 2.f * l1 - 0.5f * l2;
        float c3 = 0.5f * (l2 - lm1) + 1.5f * (l0 - l1);
        left = ((c3 * f + c2) * f + c1) * f + l0;

        // Right
        c1 = 0.5f * (r1 - rm1);
        c2 = rm1 - 2.5f * r0 + 2.f * r1 - 0.5f * r2;
        c3 = 0.5f * (r2 - rm1) + 1.5f * (r0 - r1);
        right = ((c3 * f + c2) * f + c1) * f + r0;
    }
};