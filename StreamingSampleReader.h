#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <memory>

// Background disk reader prototype. The worker owns the AudioFormatReader;
// the audio thread only calls popSample() and never performs file I/O or locks.
class StreamingSampleReader final : private juce::Thread
{
public:
    static constexpr int channelCount = 2;
    static constexpr int capacity = 1 << 16; // 65536 frames per channel
    static constexpr int readChunk = 1024;

    StreamingSampleReader();
    ~StreamingSampleReader() override;

    // Call from a non-audio thread. Stops and replaces the previous source.
    bool open(const juce::File& file);
    void close();

    // Real-time safe command: request a new playback position without seeking on the audio thread.
    // The worker performs the actual disk seek; stale frames are discarded by generation.
    uint32_t requestPlayback(juce::int64 startFrame) noexcept;

    // Real-time safe single-consumer API. Returns false on underrun or EOF.
    bool popSample(float& left, float& right) noexcept;
    int getAvailableFrames() const noexcept;
    // Copies up to maxFrames into caller-owned preallocated buffers. Audio-thread safe.
    int popBlock(float* left, float* right, int maxFrames) noexcept;
    juce::int64 getSourceLength() const noexcept { return sourceLength.load(std::memory_order_acquire); }
    double getSourceSampleRate() const noexcept { return publishedSourceRate.load(std::memory_order_acquire); }
    int getUnderrunCount() const noexcept { return underruns.load(std::memory_order_relaxed); }
    bool isReady() const noexcept { return ready.load(std::memory_order_acquire); }
    bool hasReachedEnd() const noexcept { return eof.load(std::memory_order_acquire); }

private:
    void run() override;
    bool pushFrame(float left, float right, uint32_t generation) noexcept;

    juce::AudioFormatManager formats;
    std::unique_ptr<juce::AudioFormatReader> reader; // worker-thread ownership after open()
    juce::AudioBuffer<float> decodeBuffer { channelCount, readChunk };
    std::array<float, capacity> ringL{};
    std::array<float, capacity> ringR{};
    std::array<uint32_t, capacity> ringGeneration{};
    std::atomic<uint32_t> writeIndex{0};
    std::atomic<uint32_t> readIndex{0};
    std::atomic<juce::int64> requestedStartFrame{0};
    std::atomic<uint32_t> requestedGeneration{1};
    std::atomic<uint32_t> consumerGeneration{1};
    std::atomic<int> underruns{0};
    std::atomic<juce::int64> sourceLength{0};
    std::atomic<double> publishedSourceRate{0.0};
    std::atomic<bool> ready{false};
    std::atomic<bool> eof{false};
    juce::int64 filePosition = 0; // worker thread only
    juce::int64 fileLength = 0;   // worker thread only
    double sourceRate = 0.0;
};
