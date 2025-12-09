#pragma once

#include <stdlib.h>

/// @brief Linearly interpolates a parameter across a buffer to eliminate zipper noise
/// @details Performs sample-accurate smoothing by interpolating from start to target
///          over N samples. Final value is written back to state on destruction.
///
/// @warning Destructor modifies external state. Copy/move deleted to prevent corruption.
///
/// @example Size-based (divide by buffer size):
///   ParameterInterpolator interp(&oldVol, 0.8f, 64, ParameterInterpolator::BY_SIZE);
///   for (size_t i = 0; i < 64; ++i) {
///       buffer[i] *= interp.Next();
///   }
///
/// @example Step-based (multiply by step factor):
///   ParameterInterpolator interp(&oldVol, 0.8f, 0.01f, ParameterInterpolator::BY_STEP);
class ParameterInterpolator
{
private:
    float* state_;
    float value_;
    float increment_;

public:
    /// @brief Tag types for constructor disambiguation
    struct BySizeTag {};
    struct ByStepTag {};

    static constexpr BySizeTag BY_SIZE{};
    static constexpr ByStepTag BY_STEP{};

    /// @brief Default constructor (safe but creates non-functional object)
    ParameterInterpolator() : state_(nullptr), value_(0.f), increment_(0.f) {}

    // Delete copy and move to prevent multiple write-backs
    ParameterInterpolator(const ParameterInterpolator&) = delete;
    ParameterInterpolator& operator=(const ParameterInterpolator&) = delete;
    ParameterInterpolator(ParameterInterpolator&&) = delete;
    ParameterInterpolator& operator=(ParameterInterpolator&&) = delete;

    /// @brief Size-based interpolation: divides delta by buffer size
    /// @param state Pointer to state variable (must not be null)
    /// @param new_value Target value to interpolate towards
    /// @param size Number of samples to interpolate over (must be > 0)
    /// @param tag Disambiguation tag (use BY_SIZE constant)
    ParameterInterpolator(float* state, float new_value, size_t size, BySizeTag)
    {
        state_ = state;
        value_ = (state != nullptr) ? *state : 0.f;
        increment_ = (state != nullptr && size > 0) ? (new_value - *state) / static_cast<float>(size) : 0.f;
    }

    /// @brief Step-based interpolation: multiplies delta by step factor
    /// @param state Pointer to state variable (must not be null)
    /// @param new_value Target value to interpolate towards
    /// @param step Fractional step per sample (typically small, e.g. 0.01)
    /// @param tag Disambiguation tag (use BY_STEP constant)
    ParameterInterpolator(float* state, float new_value, float step, ByStepTag)
    {
        state_ = state;
        value_ = (state != nullptr) ? *state : 0.f;
        increment_ = (state != nullptr) ? (new_value - *state) * step : 0.f;
    }

    /// @brief Convenience Constructor (Legacy support for size_t/int)
    ///        Because size is the standard usage, we can keep this implicit 
    ///        BUT we remove the float/double overload to prevent accidents.
    ParameterInterpolator(float* state, float new_value, int size) 
        : ParameterInterpolator(state, new_value, static_cast<size_t>(size), BY_SIZE) {}

    /// @brief Destructor writes final value back to state
    ~ParameterInterpolator()
    {
        if (state_ != nullptr)
        {
            *state_ = value_;
        }
    }

    /// @brief Get next interpolated value (call once per sample)
    inline float Next()
    {
        value_ += increment_;
        return value_;
    }

    /// @brief Get interpolated value at fractional position
    /// @param t Fractional position within interpolation range
    inline float subsample(float t) const
    {
        return value_ + increment_ * t;
    }
};
