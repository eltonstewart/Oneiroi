// Unit tests for Oneiroi Commons.h utility functions
// Compile: g++ -std=c++17 -I.. test_commons.cpp -o test_commons && ./test_commons

#include <iostream>
#include <cmath>
#include <cassert>

// Include mocks before Commons.h to override OWL types
#include "owl_mocks.h"

// Stub Patch.h include (Commons.h includes it)
#define Patch_h

// Now include the actual code under test
#include "../ParameterInterpolator.h"

// Re-include just the functions we want to test (extracted from Commons.h)
// We can't include full Commons.h due to complex dependencies, so we test the pure functions

constexpr float kEps = 0.0001f;
constexpr float kCvMinThreshold = 0.007f;

inline float Max(float a, float b) { return (a > b) ? a : b; }
inline float Min(float a, float b) { return (a < b) ? a : b; }
inline float Clamp(float in, float min = 0.f, float max = 1.f) { return Min(Max(in, min), max); }

inline float Map(float value, float aMin, float aMax, float bMin, float bMax) {
    float k = fabs(bMax - bMin) / fabs(aMax - aMin) * (bMax > bMin ? 1 : -1);
    return bMin + k * (value - aMin);
}

inline float MapExpo(float value, float aMin = 0.f, float aMax = 1.f, float bMin = 0.f, float bMax = 1.f) {
    value = (value - aMin) / (aMax - aMin);
    return bMin + (value * value) * (bMax - bMin);
}

inline float CenterMap(float value, float min = -1.f, float max = 1.f, float center = 0.55f) {
    if (value < center) {
        value = Map(value, 0.0f, center, min, 0.0f);
    } else {
        value = Map(value, center, 0.99f, 0.0f, max);
    }
    return value;
}

float SoftLimit(float x) {
    return x * (27.f + x * x) / (27.f + 9.f * x * x);
}

float SoftClip(float x) {
    if (x <= -3.f) return -1.f;
    else if (x >= 3.f) return 1.f;
    else return SoftLimit(x);
}

float HardClip(float x, float limit = 1.f) {
    return Clamp(x, -limit, limit);
}

inline float LinearCrossFade(float a, float b, float pos) {
    return a * (1.f - pos) + b * pos;
}

inline float M2F(float m) {
    return powf(2.f, (m - 69) / 12.f) * 440.f;
}

inline float F2S(float freq, float sampleRate = 48000.f) {
    return freq == 0.f ? 0.f : sampleRate / freq;
}

#define CONSTRAIN(var, min, max) if (var < (min)) { var = (min); } else if (var > (max)) { var = (max); }

float Modulate(
    float baseValue, float modAmount, float modValue,
    float cvAmount = 0, float cvValue = 0,
    float minValue = -1.f, float maxValue = 1.f,
    bool modAttenuverters = false, bool cvAttenuverters = false
) {
    if (modAttenuverters) {
        modAmount = CenterMap(modAmount);
        if (modAmount >= -0.1f && modAmount <= 0.1f) modAmount = 0.f;
    }
    if (cvAttenuverters) {
        cvAmount = CenterMap(cvAmount);
        if (cvAmount >= -0.1f && cvAmount <= 0.1f) cvAmount = 0.f;
    }
    if (cvValue >= -kCvMinThreshold && cvValue <= kCvMinThreshold) {
        cvValue = kCvMinThreshold;
    }
    baseValue += modAmount * modValue + cvAmount * cvValue;
    CONSTRAIN(baseValue, minValue, maxValue);
    return baseValue;
}

// Test utilities
int tests_passed = 0;
int tests_failed = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "  " << #name << "... "; \
    try { test_##name(); tests_passed++; std::cout << "PASSED\n"; } \
    catch (...) { tests_failed++; std::cout << "FAILED\n"; } \
} while(0)

#define ASSERT_EQ(a, b) do { if ((a) != (b)) { std::cerr << "Expected " << (b) << " got " << (a); throw 1; } } while(0)
#define ASSERT_NEAR(a, b, eps) do { if (fabs((a) - (b)) > (eps)) { std::cerr << "Expected ~" << (b) << " got " << (a); throw 1; } } while(0)
#define ASSERT_TRUE(x) do { if (!(x)) { std::cerr << "Assertion failed"; throw 1; } } while(0)

// ============ TESTS ============

TEST(clamp_within_range) {
    ASSERT_NEAR(Clamp(0.5f, 0.f, 1.f), 0.5f, kEps);
}

TEST(clamp_below_min) {
    ASSERT_NEAR(Clamp(-0.5f, 0.f, 1.f), 0.f, kEps);
}

TEST(clamp_above_max) {
    ASSERT_NEAR(Clamp(1.5f, 0.f, 1.f), 1.f, kEps);
}

TEST(map_linear_0_to_100) {
    ASSERT_NEAR(Map(0.5f, 0.f, 1.f, 0.f, 100.f), 50.f, kEps);
}

TEST(map_linear_edges) {
    ASSERT_NEAR(Map(0.f, 0.f, 1.f, 20.f, 20000.f), 20.f, kEps);
    ASSERT_NEAR(Map(1.f, 0.f, 1.f, 20.f, 20000.f), 20000.f, kEps);
}

TEST(map_inverted_range) {
    // When bMax < bMin, should map inversely
    ASSERT_NEAR(Map(0.f, 0.f, 1.f, 100.f, 0.f), 100.f, kEps);
    ASSERT_NEAR(Map(1.f, 0.f, 1.f, 100.f, 0.f), 0.f, kEps);
}

TEST(map_expo_center) {
    // Exponential mapping: 0.5 input should produce 0.25 output (squared)
    ASSERT_NEAR(MapExpo(0.5f, 0.f, 1.f, 0.f, 1.f), 0.25f, kEps);
}

TEST(soft_clip_within_range) {
    // Values within ±3 should be soft limited (not hard clipped)
    float result = SoftClip(1.0f);
    // SoftLimit(1.0) = 1 * (27 + 1) / (27 + 9) = 28/36 = 0.777...
    ASSERT_TRUE(result > 0.7f && result < 0.85f);
}

TEST(soft_clip_hard_limit) {
    // Values beyond ±3 should hard clip to ±1
    ASSERT_NEAR(SoftClip(5.0f), 1.0f, kEps);
    ASSERT_NEAR(SoftClip(-5.0f), -1.0f, kEps);
}

TEST(hard_clip_limits) {
    ASSERT_NEAR(HardClip(2.0f, 1.0f), 1.0f, kEps);
    ASSERT_NEAR(HardClip(-2.0f, 1.0f), -1.0f, kEps);
    ASSERT_NEAR(HardClip(0.5f, 1.0f), 0.5f, kEps);
}

TEST(linear_crossfade) {
    ASSERT_NEAR(LinearCrossFade(0.f, 1.f, 0.f), 0.f, kEps);
    ASSERT_NEAR(LinearCrossFade(0.f, 1.f, 1.f), 1.f, kEps);
    ASSERT_NEAR(LinearCrossFade(0.f, 1.f, 0.5f), 0.5f, kEps);
}

TEST(m2f_a4_is_440) {
    // MIDI note 69 = A4 = 440 Hz
    ASSERT_NEAR(M2F(69), 440.f, 0.01f);
}

TEST(m2f_octave_doubles) {
    // One octave up (12 semitones) should double frequency
    ASSERT_NEAR(M2F(81), 880.f, 0.1f);  // A5
    ASSERT_NEAR(M2F(57), 220.f, 0.1f);  // A3
}

TEST(f2s_conversion) {
    // 440 Hz at 48kHz sample rate = ~109 samples per period
    ASSERT_NEAR(F2S(440.f, 48000.f), 109.09f, 0.1f);
}

TEST(f2s_zero_freq) {
    ASSERT_NEAR(F2S(0.f, 48000.f), 0.f, kEps);
}

TEST(modulate_basic) {
    // Base 0.5 + mod (0.5 * 1.0) = 1.0
    float result = Modulate(0.5f, 0.5f, 1.0f, 0.f, 0.f, 0.f, 1.f);
    ASSERT_NEAR(result, 1.0f, kEps);
}

TEST(modulate_clamped) {
    // Should clamp to max
    float result = Modulate(0.9f, 0.5f, 1.0f, 0.f, 0.f, 0.f, 1.f);
    ASSERT_NEAR(result, 1.0f, kEps);
}

TEST(modulate_negative_mod) {
    // Base 0.5 + mod (0.5 * -1.0) = 0.0
    float result = Modulate(0.5f, 0.5f, -1.0f, 0.f, 0.f, 0.f, 1.f);
    ASSERT_NEAR(result, 0.0f, kEps);
}

// ============ PARAMETER INTERPOLATOR TESTS ============

TEST(param_interp_ramp_up) {
    float state = 0.0f;
    {
        ParameterInterpolator interp(&state, 1.0f, 4, ParameterInterpolator::BY_SIZE);
        ASSERT_NEAR(interp.Next(), 0.25f, kEps);
        ASSERT_NEAR(interp.Next(), 0.50f, kEps);
        ASSERT_NEAR(interp.Next(), 0.75f, kEps);
        ASSERT_NEAR(interp.Next(), 1.00f, kEps);
    }
    // After destructor, state should be final value
    ASSERT_NEAR(state, 1.0f, kEps);
}

TEST(param_interp_ramp_down) {
    float state = 1.0f;
    {
        ParameterInterpolator interp(&state, 0.0f, 4, ParameterInterpolator::BY_SIZE);
        ASSERT_NEAR(interp.Next(), 0.75f, kEps);
        ASSERT_NEAR(interp.Next(), 0.50f, kEps);
        ASSERT_NEAR(interp.Next(), 0.25f, kEps);
        ASSERT_NEAR(interp.Next(), 0.00f, kEps);
    }
    ASSERT_NEAR(state, 0.0f, kEps);
}

TEST(param_interp_no_change) {
    float state = 0.5f;
    {
        ParameterInterpolator interp(&state, 0.5f, 4, ParameterInterpolator::BY_SIZE);
        ASSERT_NEAR(interp.Next(), 0.5f, kEps);
        ASSERT_NEAR(interp.Next(), 0.5f, kEps);
    }
    ASSERT_NEAR(state, 0.5f, kEps);
}

TEST(param_interp_subsample) {
    float state = 0.0f;
    ParameterInterpolator interp(&state, 1.0f, 4, ParameterInterpolator::BY_SIZE);
    // subsample at t=0.5 should give halfway between current and next
    ASSERT_NEAR(interp.subsample(0.5f), 0.125f, kEps);
}

// ============ HYSTERESIS QUANTIZER TESTS ============

// Minimal HysteresisQuantizer implementation for testing
class HysteresisQuantizer {
public:
    void Init(int num_steps, float hysteresis, bool symmetric) {
        num_steps_ = num_steps;
        hysteresis_ = hysteresis;
        scale_ = static_cast<float>(symmetric ? num_steps - 1 : num_steps);
        offset_ = symmetric ? 0.0f : -0.5f;
        quantized_value_ = 0;
    }

    int Process(float value) {
        value *= scale_;
        value += offset_;
        float hysteresis_sign = value > static_cast<float>(quantized_value_) ? -1.0f : +1.0f;
        int q = static_cast<int>(value + hysteresis_sign * hysteresis_ + 0.5f);
        if (q < 0) q = 0;
        if (q > num_steps_ - 1) q = num_steps_ - 1;
        quantized_value_ = q;
        return q;
    }

    int quantized_value() const { return quantized_value_; }

private:
    int num_steps_;
    float hysteresis_;
    float scale_;
    float offset_;
    int quantized_value_;
};

TEST(quantizer_basic_steps) {
    HysteresisQuantizer q;
    q.Init(4, 0.0f, false);  // 4 steps, no hysteresis
    ASSERT_EQ(q.Process(0.0f), 0);
    ASSERT_EQ(q.Process(0.25f), 1);
    ASSERT_EQ(q.Process(0.5f), 2);
    ASSERT_EQ(q.Process(0.75f), 3);
}

TEST(quantizer_with_hysteresis) {
    HysteresisQuantizer q;
    q.Init(4, 0.2f, false);  // 4 steps, 0.2 hysteresis
    
    // Start at step 0
    q.Process(0.0f);
    ASSERT_EQ(q.quantized_value(), 0);
    
    // Moving up slightly shouldn't change due to hysteresis
    q.Process(0.2f);
    ASSERT_EQ(q.quantized_value(), 0);  // Still 0 due to hysteresis
    
    // Moving up more should trigger change
    q.Process(0.4f);
    ASSERT_EQ(q.quantized_value(), 1);  // Now 1
}

TEST(quantizer_clamps_bounds) {
    HysteresisQuantizer q;
    q.Init(4, 0.0f, false);
    ASSERT_EQ(q.Process(-0.5f), 0);   // Clamps to min
    ASSERT_EQ(q.Process(1.5f), 3);    // Clamps to max
}

// ============ MAIN ============

int main() {
    std::cout << "\n=== Oneiroi Unit Tests ===\n\n";

    std::cout << "Clamp:\n";
    RUN_TEST(clamp_within_range);
    RUN_TEST(clamp_below_min);
    RUN_TEST(clamp_above_max);

    std::cout << "\nMap:\n";
    RUN_TEST(map_linear_0_to_100);
    RUN_TEST(map_linear_edges);
    RUN_TEST(map_inverted_range);
    RUN_TEST(map_expo_center);

    std::cout << "\nClipping:\n";
    RUN_TEST(soft_clip_within_range);
    RUN_TEST(soft_clip_hard_limit);
    RUN_TEST(hard_clip_limits);

    std::cout << "\nCrossfade:\n";
    RUN_TEST(linear_crossfade);

    std::cout << "\nMIDI/Frequency:\n";
    RUN_TEST(m2f_a4_is_440);
    RUN_TEST(m2f_octave_doubles);
    RUN_TEST(f2s_conversion);
    RUN_TEST(f2s_zero_freq);

    std::cout << "\nModulate:\n";
    RUN_TEST(modulate_basic);
    RUN_TEST(modulate_clamped);
    RUN_TEST(modulate_negative_mod);

    std::cout << "\nParameterInterpolator:\n";
    RUN_TEST(param_interp_ramp_up);
    RUN_TEST(param_interp_ramp_down);
    RUN_TEST(param_interp_no_change);
    RUN_TEST(param_interp_subsample);

    std::cout << "\nHysteresisQuantizer:\n";
    RUN_TEST(quantizer_basic_steps);
    RUN_TEST(quantizer_with_hysteresis);
    RUN_TEST(quantizer_clamps_bounds);

    std::cout << "\n=== Results: " << tests_passed << " passed, " << tests_failed << " failed ===\n\n";

    return tests_failed > 0 ? 1 : 0;
}
