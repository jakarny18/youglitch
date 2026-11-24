#pragma once
#include "StreamingSampleReader.h"
#include <array>
#include <memory>

// Fixed-size pool: one independent disk reader/ring per overlapping voice.
// open() and close() are non-real-time operations. popSample() is intended
// for the audio thread and never performs file I/O, allocation, or locking.
class StreamingVoicePool final
{
public:
    static constexpr int voiceCount = 4;
    StreamingVoicePool();
    ~StreamingVoicePool();
    StreamingVoicePool(const StreamingVoicePool&) = delete;
    StreamingVoicePool& operator=(const StreamingVoicePool&) = delete;

    bool open(const juce::File& file);
    void close();
    uint32_t requestPlayback(int voice, juce::int64 startFrame) noexcept;
    bool popSample(int voice, float& left, float& right) noexcept;
    int popBlock(int voice, float* left, float* right, int maxFrames) noexcept;
    int getAvailableFrames(int voice) const noexcept;
    juce::int64 getSourceLength(int voice) const noexcept;
    double getSourceSampleRate(int voice) const noexcept;
    bool isReady(int voice) const noexcept;
    bool hasReachedEnd(int voice) const noexcept;
    int getUnderruns(int voice) const noexcept;
    int getTotalUnderruns() const noexcept;

private:
    std::array<std::unique_ptr<StreamingSampleReader>, voiceCount> readers;
};
