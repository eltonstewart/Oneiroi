#pragma once

#include "Commons.h"

class StereoEffect
{
public:
    virtual ~StereoEffect() {}
    virtual void process(AudioBuffer &input, AudioBuffer &output) = 0;

    static void destroy(StereoEffect* obj)
    {
        delete obj;
    }
};
