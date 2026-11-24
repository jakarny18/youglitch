#include "StreamingSampleReader.h"

StreamingSampleReader::StreamingSampleReader() : juce::Thread("YouGlitch sample disk reader")
{
    formats.registerBasicFormats();
}

StreamingSampleReader::~StreamingSampleReader()
{
    close();
}

bool StreamingSampleReader::open(const juce::File& file)
{
    // This method is deliberately non-real-time: reader creation and thread
    // replacement may allocate, open files, and wait for the worker to stop.
    signalThreadShouldExit();
    if (!stopThread(2000))
        return false;
    reader.reset();
    ready.store(false, std::memory_order_release);
    eof.store(false, std::memory_order_release);
    underruns.store(0, std::memory_order_relaxed);
    writeIndex.store(0, std::memory_order_relaxed);
    readIndex.store(0, std::memory_order_relaxed);
    requestedStartFrame.store(0, std::memory_order_relaxed);
    requestedGeneration.store(1, std::memory_order_relaxed);
    consumerGeneration.store(1, std::memory_order_relaxed);
    filePosition = 0;
    sourceLength.store(0, std::memory_order_release);
    publishedSourceRate.store(0.0, std::memory_order_release);

    auto candidate = std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(file));
    if (candidate == nullptr || candidate->lengthInSamples <= 0 || candidate->numChannels <= 0)
        return false;

    fileLength = candidate->lengthInSamples;
    sourceRate = candidate->sampleRate;
    sourceLength.store(fileLength, std::memory_order_release);
    publishedSourceRate.store(sourceRate, std::memory_order_release);
    reader = std::move(candidate);
    startThread(juce::Thread::Priority::normal);
    return true;
}

void StreamingSampleReader::close()
{
    signalThreadShouldExit();
    if (!stopThread(2000))
        return; // Never destroy the reader while its worker may still be using it.
    reader.reset();
    ready.store(false, std::memory_order_release);
    eof.store(true, std::memory_order_release);
    sourceLength.store(0, std::memory_order_release);
    publishedSourceRate.store(0.0, std::memory_order_release);}

uint32_t StreamingSampleReader::requestPlayback(juce::int64 startFrame) noexcept
{
    const auto boundedFrame = juce::jlimit<juce::int64>(0, juce::jmax<juce::int64>(0, sourceLength.load(std::memory_order_acquire) - 1), startFrame);
    const uint32_t next = requestedGeneration.load(std::memory_order_relaxed) + 1;
    requestedStartFrame.store(boundedFrame, std::memory_order_relaxed);
    // Publish the generation after the target frame so the worker sees a coherent command.
    requestedGeneration.store(next, std::memory_order_release);
    consumerGeneration.store(next, std::memory_order_release);
    eof.store(false, std::memory_order_release);
    return next;
}

int StreamingSampleReader::getAvailableFrames() const noexcept
{
    const uint32_t w = writeIndex.load(std::memory_order_acquire);
    const uint32_t r = readIndex.load(std::memory_order_relaxed);
    return (int)(w - r);
}

int StreamingSampleReader::popBlock(float* left, float* right, int maxFrames) noexcept
{
    if (left == nullptr || right == nullptr || maxFrames <= 0)
        return 0;

    uint32_t r = readIndex.load(std::memory_order_relaxed);
    const uint32_t w = writeIndex.load(std::memory_order_acquire);
    const uint32_t generation = consumerGeneration.load(std::memory_order_acquire);
    int count = 0;
    while (r != w && count < maxFrames)
    {
        const uint32_t index = r & (capacity - 1);
        if (ringGeneration[index] == generation)
        {
            left[count] = ringL[index];
            right[count] = ringR[index];
            ++count;
        }
        ++r; // Also discard frames from earlier playback commands.
    }
    readIndex.store(r, std::memory_order_release);

    if (count < maxFrames && !eof.load(std::memory_order_acquire))
        underruns.fetch_add(1, std::memory_order_relaxed);
    for (int i = count; i < maxFrames; ++i)
        left[i] = right[i] = 0.0f;
    return count;
}

bool StreamingSampleReader::popSample(float& left, float& right) noexcept
{
    uint32_t r = readIndex.load(std::memory_order_relaxed);
    const uint32_t w = writeIndex.load(std::memory_order_acquire);
    const uint32_t generation = consumerGeneration.load(std::memory_order_acquire);
    while (r != w)
    {
        const uint32_t index = r & (capacity - 1);
        ++r;
        readIndex.store(r, std::memory_order_release);
        if (ringGeneration[index] == generation)
        {
            left = ringL[index];
            right = ringR[index];
            return true;
        }
    }

    left = right = 0.0f;
    if (!eof.load(std::memory_order_acquire))
        underruns.fetch_add(1, std::memory_order_relaxed);
    return false;
}

bool StreamingSampleReader::pushFrame(float left, float right, uint32_t generation) noexcept
{
    const uint32_t w = writeIndex.load(std::memory_order_relaxed);
    const uint32_t r = readIndex.load(std::memory_order_acquire);
    if ((uint32_t)(w - r) >= (uint32_t)capacity)
        return false;

    const uint32_t index = w & (capacity - 1);
    ringL[index] = left;
    ringR[index] = right;
    ringGeneration[index] = generation;
    writeIndex.store(w + 1, std::memory_order_release);
    return true;
}

void StreamingSampleReader::run()
{
    if (reader == nullptr)
        return;

    ready.store(true, std::memory_order_release);
    uint32_t activeGeneration = requestedGeneration.load(std::memory_order_acquire);
    while (!threadShouldExit())
    {
        const uint32_t commandGeneration = requestedGeneration.load(std::memory_order_acquire);
        if (commandGeneration != activeGeneration)
        {
            activeGeneration = commandGeneration;
            filePosition = juce::jlimit<juce::int64>(0, fileLength, requestedStartFrame.load(std::memory_order_relaxed));
            eof.store(false, std::memory_order_release);
        }

        if (filePosition >= fileLength)
        {
            eof.store(true, std::memory_order_release);
            wait(2);
            continue;
        }

        const uint32_t w = writeIndex.load(std::memory_order_relaxed);
        const uint32_t r = readIndex.load(std::memory_order_acquire);
        const int freeFrames = capacity - (int)(uint32_t)(w - r);
        if (freeFrames <= 0)
        {
            wait(2);
            continue;
        }

        const int count = (int)juce::jmin((juce::int64)juce::jmin(readChunk, freeFrames), fileLength - filePosition);
        decodeBuffer.clear();
        if (!reader->read(&decodeBuffer, 0, count, filePosition, true, true))
        {
            eof.store(true, std::memory_order_release);
            wait(2);
            continue;
        }

        // A newer command may arrive while decoding. Tagging the decoded frames with
        // the old generation makes the audio consumer discard them safely.
        for (int i = 0; i < count; ++i)
        {
            const float l = decodeBuffer.getSample(0, i);
            const float rr = reader->numChannels > 1 ? decodeBuffer.getSample(1, i) : l;
            if (!pushFrame(l, rr, activeGeneration))
                break;
        }
        filePosition += count;
    }

    eof.store(true, std::memory_order_release);
}
