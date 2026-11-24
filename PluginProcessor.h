#pragma once
#include <JuceHeader.h>
#include <array>
#include <memory>
#include <vector>
#include "StreamingVoicePool.h"

class YouGlitchSMP1024AudioProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int trackCount = 6, stepCount = 1024, pageCount = 16;
    // Public count used by the editor to populate the sample-slot selector.
    static constexpr int sampleSlotCount = 300;
    using Pattern = std::array<std::array<std::atomic<uint8_t>, stepCount>, trackCount>;
    YouGlitchSMP1024AudioProcessor();
    ~YouGlitchSMP1024AudioProcessor() override = default;
    void prepareToPlay(double, int) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    void setPlaying(bool) noexcept;
    void beginMidiLearn(int target) noexcept { midiLearnTarget.store(juce::jlimit(0,10,target)); }
    int getMidiLearnTarget() const noexcept { return midiLearnTarget.load(); }
    int getMidiCC(int target) const noexcept { return juce::isPositiveAndBelow(target,11)?midiCC[(size_t)target].load():-1; }
    void setMidiRange(int target,float minimum,float maximum,bool inverted) noexcept;
    float getMidiMinimum(int target) const noexcept { return juce::isPositiveAndBelow(target,11)?midiMinimum[(size_t)target].load():0.0f; }
    float getMidiMaximum(int target) const noexcept { return juce::isPositiveAndBelow(target,11)?midiMaximum[(size_t)target].load():1.0f; }
    bool isMidiInverted(int target) const noexcept { return juce::isPositiveAndBelow(target,11)?midiInverted[(size_t)target].load():false; }
    void setMidiCurve(int target,int curve) noexcept;
    int getMidiCurve(int target) const noexcept { return juce::isPositiveAndBelow(target,11)?midiCurve[(size_t)target].load():0; }
    bool isPlaying() const noexcept { return playing.load(); }
    void setPage(int p) noexcept { visiblePage.store(juce::jlimit(0, 15, p)); }
    int getPage() const noexcept { return visiblePage.load(); }
    void setStep(int track, int step, uint8_t value);
    uint8_t getStep(int track, int step) const;
    void setStepProbability(int track, int step, float value) noexcept;
    float getStepProbability(int track, int step) const noexcept;
    void setStepRatchet(int track, int step, int value) noexcept;
    int getStepRatchet(int track, int step) const noexcept;
    void setStepMicroShift(int track, int step, float value) noexcept;
    float getStepMicroShift(int track, int step) const noexcept;
    void randomisePattern();
    bool loadSample(int track, const juce::File& file);
    bool loadSampleIntoSlot(int track, int slot, const juce::File& file);
    int getStreamingUnderruns() const noexcept;
    int getLoadedSampleCount(int track) const;
    int getSliceCount(int track) const noexcept;
    void setSliceCount(int track, int count) noexcept;
    void triggerSlice(int track, int slice) noexcept;
    void setSampleSlot(int track, int slot) noexcept;
    juce::String getSampleName(int track) const;
    juce::String getSampleName(int track, int slot) const;
    std::vector<float> getWaveformPeaks(int track, int slot, int points) const;
    void clearPattern();
    void setGlitch(int step, uint8_t value);
    uint8_t getGlitch(int step) const;
    int getPlayheadStep() const noexcept { return playhead.load(); }
    juce::AudioProcessorValueTreeState parameters;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
private:
    struct Voice { double phase=0, phase2=0, samplePosition=0; float velocity=0, frequency=110; int remaining=0, total=0, sound=0; double sampleRatio=1.0; double sampleStart=0.0; double sampleEnd=0.0; double sampleDirection=1.0; float attack=0.005f, decay=0.08f, sustain=0.8f, release=0.08f; int midiNote=-1, gateReleaseRemaining=0, gateReleaseTotal=0; bool gateHeld=false, loop=false, streaming=false; uint32_t streamEpoch=0; int streamVoice=-1; int streamFramesRemaining=0; std::shared_ptr<const juce::AudioBuffer<float>> sample; bool active=false; };
    void trigger(int track, int step, float velocity, int midiNote=-1) noexcept;
    float renderVoice(int track, Voice&) noexcept;
    void scheduleStep() noexcept;
    void updateStepLength() noexcept;
    std::array<std::atomic<float>*, trackCount> volume{}, pan{};
    std::array<std::atomic<float>*,11> midiTargetParams{};
    std::array<std::atomic<int>,11> midiCC{};
    std::array<std::atomic<float>,11> midiMinimum{}, midiMaximum{};
    std::array<std::atomic<bool>,11> midiInverted{};
    std::array<std::atomic<int>,11> midiCurve{};
    std::atomic<int> midiLearnTarget{-1};
    std::array<std::atomic<float>*, trackCount> mute{}, solo{}, sound{};
    static constexpr int samplesPerTrack = sampleSlotCount;
    std::array<std::array<std::shared_ptr<const juce::AudioBuffer<float>>, samplesPerTrack>, trackCount> sampleBuffers{};
    // One four-voice streaming pool per track. Pools are opened/replaced only from non-audio UI/state-loading paths.
    std::array<std::unique_ptr<StreamingVoicePool>, trackCount> streamingPools{};
    // Prevent UI sample-slot replacement from closing readers while the audio
    // thread is inside a streaming call. The second enabled check closes the
    // race where the audio thread is pre-empted just before incrementing.
    std::array<std::atomic<bool>, trackCount> streamingEnabled{};
    std::array<std::atomic<int>, trackCount> streamingReaders{};
    std::array<std::atomic<uint32_t>, trackCount> streamingEpoch{};
    bool beginStreamingAccess(int track) noexcept;
    void endStreamingAccess(int track) noexcept;
    std::array<std::array<juce::File, samplesPerTrack>, trackCount> sampleFiles{};
    std::array<std::array<juce::String, samplesPerTrack>, trackCount> sampleNames{};
    // Keep every published immutable buffer alive until processor destruction.
    // This prevents the final shared_ptr release (and heap deallocation) from
    // happening on the real-time audio thread when a slot/voice is replaced.
    std::vector<std::shared_ptr<const juce::AudioBuffer<float>>> retainedSampleBuffers;
    juce::int64 retainedSampleBytes = 0;
    static constexpr juce::int64 maxRetainedSampleBytes = (juce::int64)512 * 1024 * 1024;
    std::array<std::array<std::atomic<double>, samplesPerTrack>, trackCount> sampleSourceRates{};
    std::array<std::atomic<int>, trackCount> selectedSampleSlot{};
    std::array<std::atomic<int>, trackCount> sliceCounts{};
    std::array<std::atomic<int>, trackCount> pendingSlice{};
    std::array<std::atomic<bool>, trackCount> sliceTriggerRequested{};
    juce::AudioFormatManager formatManager;
    mutable juce::CriticalSection sampleInfoLock;
    std::atomic<float>* bpm=nullptr; std::atomic<float>* density=nullptr; std::atomic<float>* chaos=nullptr; std::atomic<float>* swing=nullptr; std::atomic<float>* masterFx=nullptr;
    double sampleRate=44100, samplesToStep=0, stepSamples=0;
    int currentStep=0, delayWrite=0, activeGlitch=0;
    std::atomic<int> playhead{-1}, visiblePage{0};
    std::atomic<bool> playing{false};
    // Audio-thread-owned reset request: UI must never mutate active voices directly.
    std::atomic<bool> resetPlaybackRequested{false};
    Pattern pattern{};
    std::array<std::array<std::atomic<float>,stepCount>,trackCount> stepProbability{};
    std::array<std::array<std::atomic<uint8_t>,stepCount>,trackCount> stepRatchet{};
    std::array<std::array<std::atomic<float>,stepCount>,trackCount> stepMicroShift{};
    std::array<std::atomic<uint8_t>,stepCount> glitchLane{}; std::array<std::array<Voice,4>,trackCount> voices{};
    std::array<int,trackCount> voiceCursor{};
    float filterStateL=0.0f;
    std::array<std::array<int,4>,trackCount> pendingDelay{};
    std::array<std::array<float,4>,trackCount> pendingVelocity{};
    std::array<std::array<int,4>,trackCount> pendingStep{};
    // Separate RNG ownership: audio thread and UI thread never share a generator.
    juce::Random audioRandom, uiRandom;
    // DC blocker state is owned exclusively by processBlock.
    float dcInputL=0.0f, dcInputR=0.0f, dcOutputL=0.0f, dcOutputR=0.0f;
    juce::AudioBuffer<float> delayBuffer, glitchBuffer;
    int glitchWrite=0, previousGlitch=-1, glitchRead=0, glitchLoopStart=0, glitchLoopLength=1;
    int tapeStopRemaining=0, tapeStopTotal=1, bitCrushCounter=0;
    double tapeReadPosition=0.0, tapeSpeed=1.0;
    float bitCrushHoldL=0.0f, bitCrushHoldR=0.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(YouGlitchSMP1024AudioProcessor)
};
