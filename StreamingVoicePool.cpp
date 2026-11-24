#include "StreamingVoicePool.h"

StreamingVoicePool::StreamingVoicePool()
{
    for (auto& reader : readers)
        reader = std::make_unique<StreamingSampleReader>();
}

StreamingVoicePool::~StreamingVoicePool()
{
    close();
}

bool StreamingVoicePool::open(const juce::File& file)
{
    // Each overlapping voice gets its own reader cursor and ring buffer.
    // This method must only be called from the message/background thread.
    close();
    int opened = 0;
    for (auto& reader : readers)
    {
        if (!reader->open(file))
        {
            for (auto& cleanup : readers)
                cleanup->close();
            return false;
        }
        ++opened;
    }
    juce::ignoreUnused(opened);
    return true;
}

void StreamingVoicePool::close()
{
    for (auto& reader : readers)
        if (reader != nullptr)
            reader->close();
}

uint32_t StreamingVoicePool::requestPlayback(int voice, juce::int64 startFrame) noexcept
{
    if (!juce::isPositiveAndBelow(voice, voiceCount) || readers[(size_t)voice] == nullptr)
        return 0;
    return readers[(size_t)voice]->requestPlayback(startFrame);
}

bool StreamingVoicePool::popSample(int voice, float& left, float& right) noexcept
{
    if (!juce::isPositiveAndBelow(voice, voiceCount) || readers[(size_t)voice] == nullptr)
    {
        left = right = 0.0f;
        return false;
    }
    return readers[(size_t)voice]->popSample(left, right);
}

int StreamingVoicePool::popBlock(int voice, float* left, float* right, int maxFrames) noexcept
{
    if (!juce::isPositiveAndBelow(voice, voiceCount) || readers[(size_t)voice] == nullptr)
    {
        if (left != nullptr && right != nullptr)
            for (int i = 0; i < juce::jmax(0, maxFrames); ++i) left[i] = right[i] = 0.0f;
        return 0;
    }
    return readers[(size_t)voice]->popBlock(left, right, maxFrames);
}

int StreamingVoicePool::getAvailableFrames(int voice) const noexcept
{
    return juce::isPositiveAndBelow(voice, voiceCount) && readers[(size_t)voice] != nullptr
        ? readers[(size_t)voice]->getAvailableFrames() : 0;
}

juce::int64 StreamingVoicePool::getSourceLength(int voice) const noexcept
{
    return juce::isPositiveAndBelow(voice, voiceCount) && readers[(size_t)voice] != nullptr
        ? readers[(size_t)voice]->getSourceLength() : 0;
}

double StreamingVoicePool::getSourceSampleRate(int voice) const noexcept
{
    return juce::isPositiveAndBelow(voice, voiceCount) && readers[(size_t)voice] != nullptr
        ? readers[(size_t)voice]->getSourceSampleRate() : 0.0;
}

int StreamingVoicePool::getTotalUnderruns() const noexcept
{
    int total = 0;
    for (const auto& reader : readers)
        if (reader != nullptr) total += reader->getUnderrunCount();
    return total;
}

bool StreamingVoicePool::isReady(int voice) const noexcept
{
    return juce::isPositiveAndBelow(voice, voiceCount)
        && readers[(size_t)voice] != nullptr
        && readers[(size_t)voice]->isReady();
}

int StreamingVoicePool::getUnderruns(int voice) const noexcept
{
    return juce::isPositiveAndBelow(voice, voiceCount) && readers[(size_t)voice] != nullptr
        ? readers[(size_t)voice]->getUnderrunCount() : 0;
}

bool StreamingVoicePool::hasReachedEnd(int voice) const noexcept
{
    return juce::isPositiveAndBelow(voice, voiceCount)
        && readers[(size_t)voice] != nullptr
        && readers[(size_t)voice]->hasReachedEnd();
}
