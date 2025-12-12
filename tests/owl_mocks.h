// Minimal OWL SDK mocks for Oneiroi unit tests
// These stubs allow Commons.h and other headers to compile on desktop
#pragma once

#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <cstring>

// Stub for randf() used in RandomFloat
inline float randf() {
    return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
}

// Stub for fast math functions
inline float fast_logf(float x) { return logf(x); }
inline float fast_expf(float x) { return expf(x); }
inline float fast_powf(float x, float y) { return powf(x, y); }

// Minimal FloatArray stub
class FloatArray {
private:
    float* data_;
    size_t size_;
    bool owned_;

public:
    FloatArray() : data_(nullptr), size_(0), owned_(false) {}
    FloatArray(float* data, size_t size) : data_(data), size_(size), owned_(false) {}

    static FloatArray create(size_t size) {
        FloatArray arr;
        arr.data_ = new float[size]();
        arr.size_ = size;
        arr.owned_ = true;
        return arr;
    }

    static void destroy(FloatArray& arr) {
        if (arr.owned_ && arr.data_) {
            delete[] arr.data_;
            arr.data_ = nullptr;
        }
    }

    float* getData() { return data_; }
    size_t getSize() const { return size_; }
    float getElement(size_t i) const { return data_[i]; }
    void setElement(size_t i, float v) { data_[i] = v; }
    void clear() { if (data_) memset(data_, 0, size_ * sizeof(float)); }
    void copyFrom(const FloatArray& other) {
        size_t n = size_ < other.size_ ? size_ : other.size_;
        memcpy(data_, other.data_, n * sizeof(float));
    }
    void add(const FloatArray& other) {
        for (size_t i = 0; i < size_; i++) data_[i] += other.data_[i];
    }
    void multiply(float scalar) {
        for (size_t i = 0; i < size_; i++) data_[i] *= scalar;
    }
    void multiply(float scalar, FloatArray& dest) {
        for (size_t i = 0; i < size_; i++) dest.data_[i] = data_[i] * scalar;
    }
    float getMean() const {
        float sum = 0;
        for (size_t i = 0; i < size_; i++) sum += data_[i];
        return size_ > 0 ? sum / size_ : 0;
    }
};

// Minimal AudioBuffer stub
class AudioBuffer {
private:
    FloatArray channels_[2];
    size_t size_;

public:
    AudioBuffer() : size_(0) {}

    static AudioBuffer* create(int numChannels, size_t size) {
        AudioBuffer* buf = new AudioBuffer();
        buf->size_ = size;
        buf->channels_[0] = FloatArray::create(size);
        buf->channels_[1] = FloatArray::create(size);
        return buf;
    }

    static void destroy(AudioBuffer* buf) {
        if (buf) {
            FloatArray::destroy(buf->channels_[0]);
            FloatArray::destroy(buf->channels_[1]);
            delete buf;
        }
    }

    size_t getSize() const { return size_; }
    FloatArray getSamples(int channel) { return channels_[channel]; }
    void clear() {
        channels_[0].clear();
        channels_[1].clear();
    }
    void copyFrom(const AudioBuffer& other) {
        channels_[0].copyFrom(other.channels_[0]);
        channels_[1].copyFrom(other.channels_[1]);
    }
    void add(const AudioBuffer& other) {
        channels_[0].add(other.channels_[0]);
        channels_[1].add(other.channels_[1]);
    }
    void multiply(float scalar) {
        channels_[0].multiply(scalar);
        channels_[1].multiply(scalar);
    }
};

// Stub for TapTempo
class TapTempo {
public:
    static TapTempo* create(float sr, size_t limit) { return new TapTempo(); }
    static void destroy(TapTempo* t) { delete t; }
    void trigger(bool on, uint16_t samples = 0) {}
    float getFrequency() const { return 2.0f; } // 120 BPM
};

// Stub Patch class
class Patch {
public:
    float getSampleRate() { return 48000.f; }
    float getBlockRate() { return 750.f; }
    int getBlockSize() { return 64; }
};

// Channel constants
#define LEFT_CHANNEL 0
#define RIGHT_CHANNEL 1
