#pragma once

#include "Commons.h"
#include "Interpolator.h"
#include <stdint.h>

class DelayLine
{
private:
    FloatArray buffer_;
    uint32_t size_, writeIndex_, delay_;

public:
    DelayLine(uint32_t size)
    {
        size_ = size;
        buffer_ = FloatArray::create(size_);
        delay_ = size_ - 1;
        writeIndex_ = 0;
    }
    ~DelayLine()
    {
        FloatArray::destroy(buffer_);
    }

    static DelayLine* create(uint32_t size)
    {
        return new DelayLine(size);
    }

    static void destroy(DelayLine* line)
    {
        delete line;
    }

    void clear()
    {
        buffer_.clear();
    }

    void setDelay(uint32_t delay)
    {
        delay_ = delay;
    }

    inline float readAt(int index)
    {
        int size = static_cast<int>(size_);
        int offset = index % size;
        if (offset < 0)
        {
            offset += size;
        }

        int i = static_cast<int>(writeIndex_) - offset - 1;
        if (i < 0)
        {
            i += size;
        }

        return buffer_[i];
    }

    inline float read(float index)
    {
        index = Clamp(index, 0.f, static_cast<float>(size_ - 2));
        size_t idx = (size_t)index;
        float y0 = readAt(idx);
        float y1 = readAt(idx + 1);
        float frac = index - idx;

        return Interpolator::linear(y0, y1, frac);
    }

    inline float readCubic(float index)
    {
        index = Clamp(index, 1.f, static_cast<float>(size_ - 3));
        size_t idx = (size_t)index;
        float frac = index - idx;

        float y0 = readAt(idx - 1);
        float y1 = readAt(idx);
        float y2 = readAt(idx + 1);
        float y3 = readAt(idx + 2);

        return Interpolator::cubic(y0, y1, y2, y3, frac);
    }

    inline float read(float index1, float index2, float x)
    {
        float v = read(index1);
        if (x == 0)
        {
            return v;
        }

        return v * (1.f - x) + read(index2) * x;
    }

    inline void write(float value, int stride = 1)
    {
        buffer_[writeIndex_] = value;
        writeIndex_ += stride;
        if  (writeIndex_ >= size_)
        {
            writeIndex_ -= size_;
        }
    }
};
