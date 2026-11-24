#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstring>

namespace { constexpr const char* names[] = {"KICK","SNARE","HAT","CLAP","BASS","LEAD"}; }

juce::AudioProcessorValueTreeState::ParameterLayout YouGlitchSMP1024AudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    p.add(std::make_unique<juce::AudioParameterFloat>("bpm","BPM",80.0f,220.0f,150.0f));
    p.add(std::make_unique<juce::AudioParameterFloat>("density","Density",0.0f,1.0f,0.35f));
    p.add(std::make_unique<juce::AudioParameterFloat>("chaos","Chaos",0.0f,1.0f,0.15f));
    p.add(std::make_unique<juce::AudioParameterFloat>("swing","Swing",0.0f,1.0f,0.0f));
    p.add(std::make_unique<juce::AudioParameterBool>("masterFx","Master FX",false));
    for (int i=0;i<trackCount;++i) { auto id=juce::String(i+1); p.add(std::make_unique<juce::AudioParameterFloat>("vol"+id,juce::String(names[i])+" Volume",0.0f,2.0f,1.0f)); p.add(std::make_unique<juce::AudioParameterFloat>("pan"+id,juce::String(names[i])+" Pan",-1.0f,1.0f,0.0f)); p.add(std::make_unique<juce::AudioParameterBool>("mute"+id,juce::String(names[i])+" Mute",false)); p.add(std::make_unique<juce::AudioParameterBool>("solo"+id,juce::String(names[i])+" Solo",false)); p.add(std::make_unique<juce::AudioParameterInt>("sound"+id,juce::String(names[i])+" Sound",0,417,0)); p.add(std::make_unique<juce::AudioParameterFloat>("start"+id,"Sample Start",0.0f,0.99f,0.0f)); p.add(std::make_unique<juce::AudioParameterFloat>("end"+id,"Sample End",0.01f,1.0f,1.0f)); p.add(std::make_unique<juce::AudioParameterFloat>("pitch"+id,"Sample Pitch",-24.0f,24.0f,0.0f)); p.add(std::make_unique<juce::AudioParameterBool>("loop"+id,"Sample Loop",false)); p.add(std::make_unique<juce::AudioParameterBool>("reverse"+id,"Sample Reverse",false)); p.add(std::make_unique<juce::AudioParameterBool>("gate"+id,"MIDI Gate",false)); p.add(std::make_unique<juce::AudioParameterFloat>("attack"+id,"Sample Attack",0.0f,2.0f,0.005f)); p.add(std::make_unique<juce::AudioParameterFloat>("decay"+id,"Sample Decay",0.0f,2.0f,0.08f)); p.add(std::make_unique<juce::AudioParameterFloat>("sustain"+id,"Sample Sustain",0.0f,1.0f,0.8f)); p.add(std::make_unique<juce::AudioParameterFloat>("release"+id,"Sample Release",0.0f,4.0f,0.08f)); }
    return p;
}

YouGlitchSMP1024AudioProcessor::YouGlitchSMP1024AudioProcessor() : AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)), parameters(*this,nullptr,"YouGlitchState",createParameterLayout())
{
    bpm=parameters.getRawParameterValue("bpm"); density=parameters.getRawParameterValue("density"); chaos=parameters.getRawParameterValue("chaos"); swing=parameters.getRawParameterValue("swing"); masterFx=parameters.getRawParameterValue("masterFx");
    for(int i=0;i<trackCount;++i){auto id=juce::String(i+1);volume[i]=parameters.getRawParameterValue("vol"+id);pan[i]=parameters.getRawParameterValue("pan"+id);mute[i]=parameters.getRawParameterValue("mute"+id);solo[i]=parameters.getRawParameterValue("solo"+id);}
    midiTargetParams[0]=parameters.getRawParameterValue("bpm");
    midiTargetParams[1]=parameters.getRawParameterValue("density");
    midiTargetParams[2]=parameters.getRawParameterValue("chaos");
    midiTargetParams[3]=parameters.getRawParameterValue("swing");
    midiTargetParams[4]=parameters.getRawParameterValue("masterFx");
    for(int i=0;i<trackCount;++i) midiTargetParams[(size_t)(5+i)]=parameters.getRawParameterValue("vol"+juce::String(i+1));
    for(int i=0;i<11;++i){midiCC[(size_t)i].store(-1);midiMinimum[(size_t)i].store(0.0f);midiMaximum[(size_t)i].store(1.0f);midiInverted[(size_t)i].store(false);midiCurve[(size_t)i].store(0);}
    formatManager.registerBasicFormats();
    for(int i=0;i<trackCount;++i){ streamingPools[(size_t)i]=std::make_unique<StreamingVoicePool>(); streamingEnabled[(size_t)i].store(false); streamingReaders[(size_t)i].store(0); streamingEpoch[(size_t)i].store(1); sound[i]=parameters.getRawParameterValue("sound"+juce::String(i+1)); selectedSampleSlot[i]=0; sliceCounts[i]=8; pendingSlice[i]=0; sliceTriggerRequested[i]=false; }
    for(int r=0;r<trackCount;++r)
    {
        for(int i=0;i<stepCount;++i){ stepProbability[(size_t)r][(size_t)i].store(1.0f); stepRatchet[(size_t)r][(size_t)i].store(1); stepMicroShift[(size_t)r][(size_t)i].store(0.0f); }
    }
    audioRandom.setSeedRandomly(); uiRandom.setSeedRandomly();
}
void YouGlitchSMP1024AudioProcessor::setMidiRange(int target,float minimum,float maximum,bool inverted) noexcept
{
    if(!juce::isPositiveAndBelow(target,11)) return;
    const float lo=juce::jlimit(0.0f,1.0f,minimum);
    const float hi=juce::jlimit(lo,1.0f,maximum);
    midiMinimum[(size_t)target].store(lo);
    midiMaximum[(size_t)target].store(hi);
    midiInverted[(size_t)target].store(inverted);
}
void YouGlitchSMP1024AudioProcessor::setMidiCurve(int target,int curve) noexcept
{
    if(juce::isPositiveAndBelow(target,11))
        midiCurve[(size_t)target].store(juce::jlimit(0,2,curve));
}
void YouGlitchSMP1024AudioProcessor::prepareToPlay(double sr,int)
{
    sampleRate=sr;
    delayBuffer.setSize(2,juce::jmax(1,(int)(sr*1.5)));
    delayBuffer.clear(); delayWrite=0;
    glitchBuffer.setSize(2,juce::jmax(1,(int)(sr*2.0)));
    glitchBuffer.clear(); glitchWrite=0; previousGlitch=-1;
    tapeStopRemaining=0; tapeStopTotal=juce::jmax(1,(int)(sr*0.35)); tapeSpeed=1.0;
    bitCrushCounter=0; bitCrushHoldL=bitCrushHoldR=0.0f;
    samplesToStep=0; currentStep=0; dcInputL=dcInputR=dcOutputL=dcOutputR=0.0f; updateStepLength();
    for(auto& row:voices) for(auto& v:row) v={};
    for(auto& row:pendingDelay) row.fill(-1);
}
void YouGlitchSMP1024AudioProcessor::releaseResources(){playing=false;}
void YouGlitchSMP1024AudioProcessor::updateStepLength() noexcept {stepSamples=sampleRate*15.0/juce::jlimit(80.0f,220.0f,bpm->load());}
void YouGlitchSMP1024AudioProcessor::setPlaying(bool on) noexcept
{
    playing.store(on);
    // Defer voice/transport mutation to processBlock, which owns these objects.
    resetPlaybackRequested.store(true);
}
void YouGlitchSMP1024AudioProcessor::setStep(int r,int s,uint8_t v){if(juce::isPositiveAndBelow(r,trackCount)&&juce::isPositiveAndBelow(s,stepCount))pattern[(size_t)r][(size_t)s].store((uint8_t)juce::jlimit(0,3,(int)v),std::memory_order_relaxed);}
uint8_t YouGlitchSMP1024AudioProcessor::getStep(int r,int s)const{return juce::isPositiveAndBelow(r,trackCount)&&juce::isPositiveAndBelow(s,stepCount)?pattern[(size_t)r][(size_t)s].load(std::memory_order_relaxed):0;}
void YouGlitchSMP1024AudioProcessor::setStepProbability(int r,int s,float v) noexcept
{
    if(juce::isPositiveAndBelow(r,trackCount)&&juce::isPositiveAndBelow(s,stepCount))
        stepProbability[(size_t)r][(size_t)s].store(juce::jlimit(0.0f,1.0f,v),std::memory_order_relaxed);
}
float YouGlitchSMP1024AudioProcessor::getStepProbability(int r,int s) const noexcept
{
    return juce::isPositiveAndBelow(r,trackCount)&&juce::isPositiveAndBelow(s,stepCount)?stepProbability[(size_t)r][(size_t)s].load(std::memory_order_relaxed):1.0f;
}
void YouGlitchSMP1024AudioProcessor::setStepRatchet(int r,int s,int v) noexcept
{
    if(juce::isPositiveAndBelow(r,trackCount)&&juce::isPositiveAndBelow(s,stepCount))
        stepRatchet[(size_t)r][(size_t)s].store((uint8_t)juce::jlimit(1,4,v),std::memory_order_relaxed);
}
int YouGlitchSMP1024AudioProcessor::getStepRatchet(int r,int s) const noexcept
{
    return juce::isPositiveAndBelow(r,trackCount)&&juce::isPositiveAndBelow(s,stepCount)?stepRatchet[(size_t)r][(size_t)s].load(std::memory_order_relaxed):1;
}
void YouGlitchSMP1024AudioProcessor::setStepMicroShift(int r,int s,float v) noexcept
{
    if(juce::isPositiveAndBelow(r,trackCount)&&juce::isPositiveAndBelow(s,stepCount))
        stepMicroShift[(size_t)r][(size_t)s].store(juce::jlimit(0.0f,0.45f,v),std::memory_order_relaxed);
}
float YouGlitchSMP1024AudioProcessor::getStepMicroShift(int r,int s) const noexcept
{
    return juce::isPositiveAndBelow(r,trackCount)&&juce::isPositiveAndBelow(s,stepCount)?stepMicroShift[(size_t)r][(size_t)s].load(std::memory_order_relaxed):0.0f;
}
void YouGlitchSMP1024AudioProcessor::setGlitch(int s,uint8_t v){if(juce::isPositiveAndBelow(s,stepCount))glitchLane[(size_t)s].store((uint8_t)juce::jlimit(0,15,(int)v),std::memory_order_relaxed);}
uint8_t YouGlitchSMP1024AudioProcessor::getGlitch(int s)const{return juce::isPositiveAndBelow(s,stepCount)?glitchLane[(size_t)s].load(std::memory_order_relaxed):0;}
bool YouGlitchSMP1024AudioProcessor::loadSample(int track,const juce::File& file)
{
    if(!juce::isPositiveAndBelow(track,trackCount)) return false;
    int slot=-1;
    for(int i=0;i<samplesPerTrack;++i)
        if(!std::atomic_load(&sampleBuffers[(size_t)track][(size_t)i])) { slot=i; break; }
    if(slot<0) slot=selectedSampleSlot[(size_t)track].load();
    if (!loadSampleIntoSlot(track,slot,file)) return false;
    setSampleSlot(track,slot);
    return true;
}
bool YouGlitchSMP1024AudioProcessor::loadSampleIntoSlot(int track,int slot,const juce::File& file)
{
    if (!juce::isPositiveAndBelow(track, trackCount) || !juce::isPositiveAndBelow(slot, samplesPerTrack) || !file.existsAsFile()) return false;
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->lengthInSamples > (juce::int64)reader->sampleRate * 60 * 10) return false;
    const int channels=(int)juce::jlimit((juce::int64)1,(juce::int64)2,(juce::int64)reader->numChannels);
    const int length=(int)reader->lengthInSamples;
    const juce::int64 decodedBytes=(juce::int64)length*channels*(juce::int64)sizeof(float);
    if(decodedBytes > maxRetainedSampleBytes) return false;
    auto decoded=std::make_shared<juce::AudioBuffer<float>>(channels,length);
    if(!reader->read(decoded.get(),0,length,0,true,channels>1)) return false;
    if(!juce::isPositiveAndBelow(slot,samplesPerTrack)) return false;
    std::shared_ptr<const juce::AudioBuffer<float>> immutable=decoded;
    {
        const juce::ScopedLock lock(sampleInfoLock);
        // Retained buffers are intentionally not reclaimed while audio may hold
        // a Voice::sample reference. The cap bounds this safety trade-off.
        if(retainedSampleBytes + decodedBytes > maxRetainedSampleBytes) return false;
        retainedSampleBuffers.push_back(immutable);
        retainedSampleBytes += decodedBytes;
        sampleNames[(size_t)track][(size_t)slot]=file.getFileName();
        sampleFiles[(size_t)track][(size_t)slot]=file;
        sampleSourceRates[(size_t)track][(size_t)slot].store(reader->sampleRate, std::memory_order_relaxed);
    }
    std::atomic_store(&sampleBuffers[(size_t)track][(size_t)slot],immutable);
    selectedSampleSlot[(size_t)track]=slot;
    return true;
}
bool YouGlitchSMP1024AudioProcessor::beginStreamingAccess(int track) noexcept
{
    if (!juce::isPositiveAndBelow(track, trackCount)
        || !streamingEnabled[(size_t)track].load(std::memory_order_acquire))
        return false;

    streamingReaders[(size_t)track].fetch_add(1, std::memory_order_acq_rel);
    if (!streamingEnabled[(size_t)track].load(std::memory_order_acquire))
    {
        streamingReaders[(size_t)track].fetch_sub(1, std::memory_order_release);
        return false;
    }
    return true;
}

void YouGlitchSMP1024AudioProcessor::endStreamingAccess(int track) noexcept
{
    if (juce::isPositiveAndBelow(track, trackCount))
        streamingReaders[(size_t)track].fetch_sub(1, std::memory_order_release);
}

int YouGlitchSMP1024AudioProcessor::getStreamingUnderruns() const noexcept
{
    int total=0;
    for (const auto& pool : streamingPools)
        if (pool != nullptr) total += pool->getTotalUnderruns();
    return total;
}
int YouGlitchSMP1024AudioProcessor::getLoadedSampleCount(int track) const
{
    if(!juce::isPositiveAndBelow(track,trackCount)) return 0;
    int count=0; for(int i=0;i<samplesPerTrack;++i) if(std::atomic_load(&sampleBuffers[(size_t)track][(size_t)i])) ++count;
    return count;
}
void YouGlitchSMP1024AudioProcessor::setSampleSlot(int track,int slot) noexcept
{
    if(!juce::isPositiveAndBelow(track,trackCount))
        return;

    const int bounded=juce::jlimit(0,samplesPerTrack-1,slot);
    selectedSampleSlot[(size_t)track].store(bounded, std::memory_order_release);

    // Disable new streaming calls first. Existing calls finish before the
    // non-real-time thread replaces reader objects and their worker threads.
    streamingEnabled[(size_t)track].store(false, std::memory_order_release);
    while(streamingReaders[(size_t)track].load(std::memory_order_acquire) != 0)
        juce::Thread::sleep(1);

    // Invalidate all voices that were reading the previous slot. They retain
    // their RAM snapshot and will stay on that snapshot after the new pool opens.
    streamingEpoch[(size_t)track].fetch_add(1, std::memory_order_acq_rel);

    juce::File file;
    { const juce::ScopedLock lock(sampleInfoLock); file=sampleFiles[(size_t)track][(size_t)bounded]; }

    bool opened = false;
    if(file.existsAsFile() && streamingPools[(size_t)track] != nullptr)
        opened = streamingPools[(size_t)track]->open(file);

    streamingEnabled[(size_t)track].store(opened, std::memory_order_release);
}
int YouGlitchSMP1024AudioProcessor::getSliceCount(int track) const noexcept
{
    return juce::isPositiveAndBelow(track,trackCount) ? sliceCounts[(size_t)track].load() : 1;
}
void YouGlitchSMP1024AudioProcessor::setSliceCount(int track,int count) noexcept
{
    if(juce::isPositiveAndBelow(track,trackCount)) sliceCounts[(size_t)track]=juce::jlimit(1,32,count);
}
void YouGlitchSMP1024AudioProcessor::triggerSlice(int track,int slice) noexcept
{
    if(juce::isPositiveAndBelow(track,trackCount))
    {
        pendingSlice[(size_t)track]=juce::jlimit(0,getSliceCount(track)-1,slice);
        sliceTriggerRequested[(size_t)track]=true;
    }
}
juce::String YouGlitchSMP1024AudioProcessor::getSampleName(int track) const
{
    if(!juce::isPositiveAndBelow(track,trackCount)) return {};
    const juce::ScopedLock lock(sampleInfoLock);
    const int slot=selectedSampleSlot[(size_t)track].load();
    return sampleNames[(size_t)track][(size_t)slot];
}
juce::String YouGlitchSMP1024AudioProcessor::getSampleName(int track,int slot) const
{
    if(!juce::isPositiveAndBelow(track,trackCount) || !juce::isPositiveAndBelow(slot,samplesPerTrack)) return {};
    const juce::ScopedLock lock(sampleInfoLock);
    return sampleNames[(size_t)track][(size_t)slot];
}
std::vector<float> YouGlitchSMP1024AudioProcessor::getWaveformPeaks(int track,int slot,int points) const
{
    std::vector<float> peaks;
    if(!juce::isPositiveAndBelow(track,trackCount) || !juce::isPositiveAndBelow(slot,samplesPerTrack) || points<=0)
        return peaks;
    const auto buffer=std::atomic_load(&sampleBuffers[(size_t)track][(size_t)slot]);
    if(!buffer || buffer->getNumSamples()<=0) return peaks;
    const int length=buffer->getNumSamples();
    const int channels=buffer->getNumChannels();
    peaks.resize((size_t)points,0.0f);
    for(int x=0;x<points;++x)
    {
        const int begin=(int)(static_cast<juce::int64>(x)*length/points);
        const int finish=juce::jmax(begin+1,(int)(static_cast<juce::int64>(x+1)*length/points));
        float peak=0.0f;
        for(int i=begin;i<juce::jmin(length,finish);++i)
            for(int c=0;c<channels;++c)
                peak=juce::jmax(peak,std::abs(buffer->getSample(c,i)));
        peaks[(size_t)x]=juce::jlimit(0.0f,1.0f,peak);
    }
    return peaks;
}
void YouGlitchSMP1024AudioProcessor::clearPattern(){for(int r=0;r<trackCount;++r)for(int i=0;i<stepCount;++i){pattern[(size_t)r][(size_t)i].store(0,std::memory_order_relaxed);stepProbability[(size_t)r][(size_t)i].store(1.0f,std::memory_order_relaxed);stepRatchet[(size_t)r][(size_t)i].store(1,std::memory_order_relaxed);stepMicroShift[(size_t)r][(size_t)i].store(0.0f,std::memory_order_relaxed);}for(auto& g:glitchLane)g.store(0,std::memory_order_relaxed);}
void YouGlitchSMP1024AudioProcessor::randomisePattern(){const float d=density->load();for(int bar=0;bar<64;++bar)for(int s=0;s<16;++s){int i=bar*16+s;pattern[0][i].store((uint8_t)(uiRandom.nextFloat()<(s%16==0?.9f:.04f)*(0.35f+d)),std::memory_order_relaxed);pattern[1][i].store((uint8_t)(uiRandom.nextFloat()<((s==4||s==12)?.85f:.025f)*(0.35f+d)),std::memory_order_relaxed);pattern[2][i].store((uint8_t)(uiRandom.nextFloat()<(s%2==0?.65f:.38f)*d+.08f),std::memory_order_relaxed);pattern[3][i].store((uint8_t)(uiRandom.nextFloat()<.07f*d),std::memory_order_relaxed);pattern[4][i].store((uint8_t)((s%4==0&&uiRandom.nextFloat()<d)?1:0),std::memory_order_relaxed);pattern[5][i].store((uint8_t)(uiRandom.nextFloat()<.12f*d?1:0),std::memory_order_relaxed);}for(auto& g:glitchLane)g.store(uiRandom.nextFloat()<d*.12f?(uint8_t)uiRandom.nextInt(15)+1:0,std::memory_order_relaxed);}
void YouGlitchSMP1024AudioProcessor::trigger(int r,int step,float vel,int midiNote)noexcept{int slot=voiceCursor[(size_t)r]++%4;auto& v=voices[(size_t)r][(size_t)slot];v={};v.active=true;v.velocity=vel;v.midiNote=midiNote;v.gateHeld=(midiNote>=0 && parameters.getRawParameterValue("gate"+juce::String(r+1))->load()>0.5f);v.remaining=v.total=(int)(sampleRate*(r==0?.22:r==2?.08:.32));int note=r==4?36+(step%7)*2:r==5?60+(step%12):60;v.frequency=(float)(440.0*std::pow(2.0,(note-69)/12.0));const int sampleSlot=selectedSampleSlot[(size_t)r].load();
    v.sample=std::atomic_load(&sampleBuffers[(size_t)r][(size_t)sampleSlot]);
    if(v.sample){
        const auto id=juce::String(r+1);
        const float start=parameters.getRawParameterValue("start"+id)->load();
        const float end=parameters.getRawParameterValue("end"+id)->load();
        const float pitch=parameters.getRawParameterValue("pitch"+id)->load();
        const double fullLength=(double)v.sample->getNumSamples();
        const int slices=juce::jlimit(1,32,sliceCounts[(size_t)r].load());
        const int slice=juce::jlimit(0,slices-1,pendingSlice[(size_t)r].exchange(0));
        const double sliceStart=fullLength*(double)slice/(double)slices;
        const double sliceEnd=fullLength*(double)(slice+1)/(double)slices;
        const double regionStart=sliceStart+(sliceEnd-sliceStart)*juce::jlimit(0.0f,0.99f,start);
        const double regionEnd=juce::jmax(regionStart+1.0,sliceStart+(sliceEnd-sliceStart)*juce::jlimit(start+0.01f,1.0f,end));
        v.sampleStart=regionStart;
        v.sampleEnd=regionEnd;
        v.sampleRatio=sampleSourceRates[(size_t)r][(size_t)sampleSlot].load(std::memory_order_relaxed)/sampleRate*std::pow(2.0,pitch/12.0);
        v.loop=parameters.getRawParameterValue("loop"+id)->load()>0.5f;
        const bool reverse=parameters.getRawParameterValue("reverse"+id)->load()>0.5f;
        v.sampleDirection=reverse?-1.0:1.0;
        v.samplePosition=reverse?juce::jmax(regionStart,regionEnd-1.0):regionStart;
        v.attack=parameters.getRawParameterValue("attack"+id)->load();
        v.decay=parameters.getRawParameterValue("decay"+id)->load();
        v.sustain=parameters.getRawParameterValue("sustain"+id)->load();
        v.release=parameters.getRawParameterValue("release"+id)->load();
        v.total=v.remaining=juce::jmax(1,(int)((regionEnd-regionStart)/juce::jmax(0.01,v.sampleRatio)));
        // Streaming path supports forward, unity-pitch playback and slices. Unsupported
        // pitch/reverse combinations deliberately retain the established RAM path.
        const bool streamCompatible = !reverse && std::abs(v.sampleRatio-1.0) < 0.0001
            && streamingPools[(size_t)r] != nullptr
            && beginStreamingAccess(r);
        if (streamCompatible)
        {
            auto* pool = streamingPools[(size_t)r].get();
            const bool ready = pool != nullptr && pool->isReady(slot);
            if (ready)
            {
                v.streaming=true;
                v.streamEpoch=streamingEpoch[(size_t)r].load(std::memory_order_acquire);
                v.streamVoice=slot;
                v.streamFramesRemaining=juce::jmax(1,(int)std::ceil(regionEnd-regionStart));
                pool->requestPlayback(slot,(juce::int64)regionStart);
            }
            endStreamingAccess(r);
        }
    }v.sound=(int)(sound[(size_t)r]?sound[(size_t)r]->load():0.0f);}
float YouGlitchSMP1024AudioProcessor::renderVoice(int r,Voice& v)noexcept
{
    if(!v.active||v.remaining<=0)return 0.0f;
    if (v.streaming)
    {
        if (v.streamEpoch != streamingEpoch[(size_t)r].load(std::memory_order_acquire))
            v.streaming = false;
        if (v.streaming && !beginStreamingAccess(r))
        {
            // A sample-slot change is in progress. Keep this voice alive and
            // fall back to its retained RAM copy instead of touching a reader
            // that may be getting replaced on the UI thread.
            v.streaming = false;
        }
        if (v.streaming)
        {
            float left=0.0f, right=0.0f;
            auto* pool=streamingPools[(size_t)r].get();
            const bool popped = pool != nullptr && v.streamVoice >= 0
                && pool->popSample(v.streamVoice,left,right);
            const bool ended = pool == nullptr || v.streamVoice < 0
                || (pool->hasReachedEnd(v.streamVoice) && !popped);
            if (popped)
            {
                --v.streamFramesRemaining;
                if(v.streamFramesRemaining<=0)
                {
                    if(v.loop)
                    {
                        pool->requestPlayback(v.streamVoice,(juce::int64)v.sampleStart);
                        v.streamFramesRemaining=juce::jmax(1,(int)std::ceil(v.sampleEnd-v.sampleStart));
                    }
                    else { v.active=false; }
                }
            }
            endStreamingAccess(r);

            if (!popped)
            {
                // Underruns are silent but recoverable; EOF ends non-looping voices.
                if (ended && !v.loop) v.active=false;
                return 0.0f;
            }

            const float sample=0.5f*(left+right);
            const float elapsed=(float)(v.total-v.remaining)/(float)juce::jmax(1,v.total);
            if (v.loop) v.remaining=1; else --v.remaining;
            const float attackGain=v.attack<=0.0001f?1.0f:juce::jlimit(0.0f,1.0f,elapsed/v.attack);
            const float decayProgress=v.decay<=0.0001f?1.0f:juce::jlimit(0.0f,1.0f,(elapsed-v.attack)/v.decay);
            const float sustainGain=1.0f+(v.sustain-1.0f)*decayProgress;
            float envelope=elapsed<v.attack?attackGain:sustainGain;
            float gateGain=1.0f;
            if(v.gateReleaseRemaining>0 && v.gateReleaseTotal>0)
            { gateGain=(float)v.gateReleaseRemaining/(float)v.gateReleaseTotal; if(--v.gateReleaseRemaining<=0) v.active=false; }
            return sample*v.velocity*0.7f*envelope*gateGain;
        }
    }
    if (v.sample != nullptr)
    {
        const int sourceLength = v.sample->getNumSamples();
        if(v.loop && v.sampleDirection>0.0 && v.samplePosition>=v.sampleEnd) v.samplePosition=v.sampleStart;
        if(v.loop && v.sampleDirection<0.0 && v.samplePosition<v.sampleStart) v.samplePosition=v.sampleEnd-1.0;
        if (sourceLength <= 0 || v.samplePosition < v.sampleStart || v.samplePosition >= juce::jmin((double)sourceLength,v.sampleEnd)) { v.active=false; return 0.0f; }
        const int channels = v.sample->getNumChannels();
        const int index = (int)v.samplePosition;
        const int next = juce::jmin(index + 1, sourceLength - 1);
        const float frac = (float)(v.samplePosition - index);
        const float a = v.sample->getSample(0, index) + (v.sample->getSample(0, next) - v.sample->getSample(0, index)) * frac;
        float sample = a;
        if (channels > 1)
        {
            const float b = v.sample->getSample(1, index) + (v.sample->getSample(1, next) - v.sample->getSample(1, index)) * frac;
            sample = 0.5f * (a + b);
        }
        v.samplePosition += v.sampleRatio*v.sampleDirection;
        if(v.loop) v.remaining=1;
        else if (--v.remaining <= 0 || (v.sampleDirection>0.0 && v.samplePosition>=juce::jmin((double)sourceLength,v.sampleEnd)) || (v.sampleDirection<0.0 && v.samplePosition<v.sampleStart)) v.active=false;
        const float elapsed=(float)(v.total-v.remaining)/(float)juce::jmax(1,v.total);
        const float duration=(float)v.total/(float)sampleRate;
        const float attackGain=v.attack<=0.0001f?1.0f:juce::jlimit(0.0f,1.0f,elapsed/v.attack);
        const float decayProgress=v.decay<=0.0001f?1.0f:juce::jlimit(0.0f,1.0f,(elapsed-v.attack)/v.decay);
        const float sustainGain=1.0f+(v.sustain-1.0f)*decayProgress;
        float envelope=elapsed<v.attack?attackGain:sustainGain;
        if(!v.loop && v.release>0.0001f)
        {
            const float remainingSeconds=(float)v.remaining/(float)sampleRate;
            if(remainingSeconds<v.release) envelope*=juce::jlimit(0.0f,1.0f,remainingSeconds/v.release);
        }
        juce::ignoreUnused(duration);
        float gateGain=1.0f;
        if(v.gateReleaseRemaining>0 && v.gateReleaseTotal>0)
        {
            gateGain=(float)v.gateReleaseRemaining/(float)v.gateReleaseTotal;
            if(--v.gateReleaseRemaining<=0) v.active=false;
        }
        return sample * v.velocity * 0.7f * envelope * gateGain;
    }
    const float t=(float)(v.total-v.remaining)/(float)juce::jmax(1,v.total);
    // Sound index chooses a deterministic patch family within the original track's bank.
    const int patch=v.sound;
    const float variation=0.78f+0.22f*std::sin((float)(patch+1)*1.618f);
    const float decayRate=(r==0?8.0f:r==2?18.0f:6.5f)*(0.55f+(patch%17)*0.035f);
    const float decay=std::exp(-t*decayRate);
    auto sine=[](double p){return(float)std::sin(p);};
    auto wave=[&](double p)
    {
        const int shape=(patch/3)%4;
        const double phase=p/juce::MathConstants<double>::twoPi;
        const double frac=phase-std::floor(phase);
        if(shape==1)return(float)(1.0-4.0*std::abs(frac-0.5));
        if(shape==2)return frac<0.5?1.0f:-1.0f;
        if(shape==3)return(float)(2.0*frac-1.0);
        return sine(p);
    };
    float out=0.0f;
    const float f=v.frequency*(0.72f+(patch%29)*0.024f);
    if(r==0)
    {
        const float start=65.0f+(patch%37)*7.5f;
        const float end=28.0f+(patch%11)*3.1f;
        const float freq=end+(start-end)*std::exp(-t*(16.0f+(patch%13)*1.7f));
        v.phase+=juce::MathConstants<double>::twoPi*freq/sampleRate;
        const float click=(audioRandom.nextFloat()*2.0f-1.0f)*std::exp(-t*(80.0f+(patch%19)*12.0f));
        out=(wave(v.phase)*0.82f+click*0.18f)*decay;
    }
    else if(r==1||r==3)
    {
        const float noise=audioRandom.nextFloat()*2.0f-1.0f;
        v.phase+=juce::MathConstants<double>::twoPi*(r==1?120.0f+(patch%23)*21.0f:500.0f+(patch%31)*63.0f)/sampleRate;
        const float noiseMix=r==1?0.35f+(patch%7)*0.07f:0.55f+(patch%5)*0.08f;
        out=(noise*noiseMix+wave(v.phase)*(1.0f-noiseMix))*decay;
    }
    else if(r==2)
    {
        const float noise=audioRandom.nextFloat()*2.0f-1.0f;
        const float brightness=0.12f+(patch%23)*0.035f;
        filterStateL+=brightness*(noise-filterStateL);
        out=(noise-filterStateL)*decay*variation;
    }
    else if(r==4)
    {
        v.phase+=juce::MathConstants<double>::twoPi*f/sampleRate;
        const float cutoff=0.35f+0.65f*std::exp(-t*(4.0f+(patch%17)));
        out=(wave(v.phase)*0.72f+sine(v.phase*0.5)*0.28f)*decay*cutoff;
    }
    else
    {
        const float ratio=0.5f+(patch%65)*0.1f;
        v.phase+=juce::MathConstants<double>::twoPi*f/sampleRate;
        v.phase2+=juce::MathConstants<double>::twoPi*(f*ratio)/sampleRate;
        out=sine(v.phase+sine(v.phase2)*(0.4f+(patch%13)*0.16f))*decay;
    }
    if(--v.remaining<=0)v.active=false;
    float gateGain=1.0f;
    if(v.gateReleaseRemaining>0 && v.gateReleaseTotal>0)
    {
        gateGain=(float)v.gateReleaseRemaining/(float)v.gateReleaseTotal;
        if(--v.gateReleaseRemaining<=0) v.active=false;
    }
    return out*v.velocity*(r==2?0.22f:0.30f)*variation*gateGain;
}
void YouGlitchSMP1024AudioProcessor::scheduleStep() noexcept
{
    const int step=currentStep;
    playhead=step;
    activeGlitch=glitchLane[(size_t)step].load(std::memory_order_relaxed);
    const float chaosAmount=chaos->load();
    const float jitter=(audioRandom.nextFloat()-0.5f)*chaosAmount*0.018f;
    juce::ignoreUnused(jitter);
    bool anySolo=false;
    for(auto* p:solo) anySolo|=p->load()>.5f;
    for(int r=0;r<trackCount;++r)
    {
        for(auto& delay:pendingDelay[(size_t)r]) delay=-1;
        const int hits=pattern[(size_t)r][(size_t)step].load(std::memory_order_relaxed);
        if(hits==0 || mute[(size_t)r]->load()>.5f || (anySolo && solo[(size_t)r]->load()<.5f))
            continue;
        const float probability=stepProbability[(size_t)r][(size_t)step].load(std::memory_order_relaxed);
        if(audioRandom.nextFloat()>=probability) continue;
        const int ratchets=juce::jlimit(1,4,juce::jmax((int)stepRatchet[(size_t)r][(size_t)step].load(std::memory_order_relaxed),hits));
        const int lateOffset=(int)(stepSamples*stepMicroShift[(size_t)r][(size_t)step].load(std::memory_order_relaxed));
        const double interval=juce::jmax(1.0,stepSamples-(double)lateOffset)/(double)ratchets;
        for(int k=0;k<ratchets;++k)
        {
            pendingDelay[(size_t)r][(size_t)k]=juce::jmax(0,lateOffset+(int)(interval*k));
            pendingVelocity[(size_t)r][(size_t)k]=1.0f-k*0.12f;
            pendingStep[(size_t)r][(size_t)k]=step;
        }
    }
    currentStep=(currentStep+1)%stepCount;
}
void YouGlitchSMP1024AudioProcessor::processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer& midi){
    juce::ScopedNoDenormals noDenormals;
    const int ch=b.getNumChannels(), n=b.getNumSamples();
    if(ch==0) return;
    if(resetPlaybackRequested.exchange(false)) {
        currentStep=0; samplesToStep=0.0; playhead.store(-1);
        for(auto& row:voices) for(auto& voice:row) voice.active=false;
        for(auto& row:pendingDelay) row.fill(-1);
    }
    updateStepLength();bool fx=masterFx->load()>.5f;for(int r=0;r<trackCount;++r)if(sliceTriggerRequested[(size_t)r].exchange(false))trigger(r,currentStep,1.0f);juce::MidiBuffer::Iterator midiIterator(midi);
    juce::MidiMessage midiMessage; int midiEventPosition=-1;
    bool hasMidiEvent=midiIterator.getNextEvent(midiMessage,midiEventPosition);
    bool anySolo=false;for(auto* p:solo)anySolo|=p->load()>.5f;for(int i=0;i<n;++i){
        while(hasMidiEvent && midiEventPosition<=i)
        {
            if(midiMessage.isController())
            {
                const int cc=midiMessage.getControllerNumber();
                const float value=(float)midiMessage.getControllerValue()/127.0f;
                const int learning=midiLearnTarget.exchange(-1);
                if(learning>=0 && learning<11)
                    midiCC[(size_t)learning].store(cc);
                else
                {
                    for(int target=0;target<11;++target)
                        if(midiCC[(size_t)target].load()==cc && midiTargetParams[(size_t)target]!=nullptr)
                        {
                            const float lo=midiMinimum[(size_t)target].load();
                            const float hi=midiMaximum[(size_t)target].load();
                            float shaped=midiInverted[(size_t)target].load()?1.0f-value:value;
                            const int curve=midiCurve[(size_t)target].load();
                            if(curve==1) shaped=shaped*shaped;
                            else if(curve==2) shaped=std::sqrt(juce::jmax(0.0f,shaped));
                            const float normalized=lo+(hi-lo)*shaped;
                            const float mapped=target==0?80.0f+normalized*140.0f:target==4?(normalized>=0.5f?1.0f:0.0f):target>=5?normalized*2.0f:normalized;
                            midiTargetParams[(size_t)target]->store(mapped);
                        }
                }
            }
            else if(midiMessage.isNoteOn())
            {
                const int note=midiMessage.getNoteNumber();
                const int trackIndex=note-36;
                if(juce::isPositiveAndBelow(trackIndex,trackCount))
                    trigger(trackIndex,currentStep,midiMessage.getFloatVelocity(),note);
            }
            else if(midiMessage.isNoteOff() || (midiMessage.isNoteOn() && midiMessage.getVelocity()==0))
            {
                const int note=midiMessage.getNoteNumber();
                for(int tr=0;tr<trackCount;++tr)
                    for(auto& voice:voices[(size_t)tr])
                        if(voice.active && voice.midiNote==note && voice.gateHeld)
                        {
                            voice.gateHeld=false;
                            voice.gateReleaseTotal=juce::jmax(1,(int)(voice.release*sampleRate));
                            voice.gateReleaseRemaining=voice.gateReleaseTotal;
                            if(voice.loop) voice.loop=false;
                        }
            }
            hasMidiEvent=midiIterator.getNextEvent(midiMessage,midiEventPosition);
        }
        if(playing.load()){if(samplesToStep<=0){scheduleStep();samplesToStep+=stepSamples;}samplesToStep-=1;}float l=0,rgt=0;for(int r=0;r<trackCount;++r){
            for(int k=0;k<4;++k){auto& delay=pendingDelay[(size_t)r][(size_t)k];if(delay>=0&&--delay<=0){trigger(r,pendingStep[(size_t)r][(size_t)k],pendingVelocity[(size_t)r][(size_t)k]);delay=-1;}}
            if(mute[(size_t)r]->load()>.5f||(anySolo&&solo[(size_t)r]->load()<.5f))continue;
            float x=0;for(auto& voice:voices[(size_t)r])x+=renderVoice(r,voice);x*=volume[(size_t)r]->load();float p=pan[(size_t)r]->load();l+=x*std::sqrt(.5f*(1-p));rgt+=x*std::sqrt(.5f*(1+p));}// Keep a rolling pre-effect buffer for freeze, stutter and tape-stop.
            const int glitchBufferSize=glitchBuffer.getNumSamples();
            if(glitchBufferSize>0)
            {
                if(activeGlitch!=previousGlitch)
                {
                    previousGlitch=activeGlitch;
                    if(activeGlitch==1 || activeGlitch==2)
                    {
                        glitchLoopLength=juce::jlimit(32,glitchBufferSize/2,(int)(sampleRate*(activeGlitch==1?0.18:0.09)));
                        glitchLoopStart=(glitchWrite-glitchLoopLength+glitchBufferSize)%glitchBufferSize;
                        glitchRead=glitchLoopStart;
                    }
                    if(activeGlitch==12)
                    {
                        tapeStopTotal=juce::jmax(1,(int)(sampleRate*0.35));
                        tapeStopRemaining=tapeStopTotal;
                        tapeReadPosition=(double)((glitchWrite-(int)(sampleRate*0.22)+glitchBufferSize)%glitchBufferSize);
                        tapeSpeed=1.0;
                    }
                }
                glitchBuffer.setSample(0,glitchWrite,l);
                glitchBuffer.setSample(1,glitchWrite,rgt);
                if(fx && (activeGlitch==1 || activeGlitch==2))
                {
                    l=glitchBuffer.getSample(0,glitchRead);
                    rgt=glitchBuffer.getSample(1,glitchRead);
                    glitchRead=(glitchRead+1)%glitchBufferSize;
                    if(glitchRead>=glitchWrite && glitchRead<((glitchWrite-glitchLoopLength+glitchBufferSize)%glitchBufferSize))
                        glitchLoopStart=(glitchWrite-glitchLoopLength+glitchBufferSize)%glitchBufferSize;
                        glitchRead=glitchLoopStart;
                }
                else if(fx && activeGlitch==11)
                {
                    if(bitCrushCounter--<=0)
                    {
                        bitCrushCounter=3;
                        bitCrushHoldL=std::round(l*15.0f)/15.0f;
                        bitCrushHoldR=std::round(rgt*15.0f)/15.0f;
                    }
                    l=bitCrushHoldL; rgt=bitCrushHoldR;
                }
                else if(fx && activeGlitch==12 && tapeStopRemaining>0)
                {
                    const int readIndex=((int)tapeReadPosition%glitchBufferSize+glitchBufferSize)%glitchBufferSize;
                    l=glitchBuffer.getSample(0,readIndex);
                    rgt=glitchBuffer.getSample(1,readIndex);
                    tapeReadPosition=std::fmod(tapeReadPosition+tapeSpeed,(double)glitchBufferSize);
                    tapeSpeed=juce::jmax(0.0,tapeSpeed-1.0/(double)tapeStopTotal);
                    --tapeStopRemaining;
                }
            }
            if(fx && activeGlitch==4) l=rgt=0.0f;
            else if(fx && activeGlitch==6) { l=std::tanh(l*4.0f); rgt=std::tanh(rgt*4.0f); }
            else if(fx && activeGlitch==5) { const float ring=std::sin((float)((double)i*juce::MathConstants<double>::twoPi*440.0/sampleRate)); l*=ring; rgt*=ring; }
            if(delayBuffer.getNumSamples()>0)
            {
                const int rd=(delayWrite+delayBuffer.getNumSamples()-(int)(sampleRate*.22))%delayBuffer.getNumSamples();
                const float dl=delayBuffer.getSample(0,rd),dr=delayBuffer.getSample(1,rd);
                delayBuffer.setSample(0,delayWrite,l+dl*.28f);
                delayBuffer.setSample(1,delayWrite,rgt+dr*.28f);
                if(fx&&(activeGlitch==9||activeGlitch==10)){l+=dl*.35f;rgt+=dr*.35f;}
                delayWrite=(delayWrite+1)%delayBuffer.getNumSamples();
            }
            // First-order DC blocker followed by smooth saturation. This avoids abrupt
            // hard clipping at the output while keeping all state audio-thread-owned.
            const float dcCoeff = (float)std::exp(-2.0 * juce::MathConstants<double>::pi * 8.0 / sampleRate);
            const float cleanL = l - dcInputL + dcCoeff * dcOutputL;
            dcInputL = l; dcOutputL = cleanL;
            const float cleanR = rgt - dcInputR + dcCoeff * dcOutputR;
            dcInputR = rgt; dcOutputR = cleanR;
            b.setSample(0,i,std::tanh(cleanL));
            if(ch>1)b.setSample(1,i,std::tanh(cleanR));for(int c=2;c<ch;++c)b.setSample(c,i,0);}midi.clear();}
juce::AudioProcessorEditor* YouGlitchSMP1024AudioProcessor::createEditor(){return new YouGlitchSMP1024AudioProcessorEditor(*this);}
void YouGlitchSMP1024AudioProcessor::getStateInformation(juce::MemoryBlock& dest){auto state=parameters.copyState();juce::MemoryBlock raw;
    std::array<std::array<uint8_t,stepCount>,trackCount> patternPlain{};
    std::array<uint8_t,stepCount> glitchPlain{};
    std::array<std::array<float,stepCount>,trackCount> probabilityPlain{};
    std::array<std::array<uint8_t,stepCount>,trackCount> ratchetPlain{};
    std::array<std::array<float,stepCount>,trackCount> microPlain{};
    for(int r=0;r<trackCount;++r)for(int i=0;i<stepCount;++i){patternPlain[(size_t)r][(size_t)i]=pattern[(size_t)r][(size_t)i].load(std::memory_order_relaxed);probabilityPlain[(size_t)r][(size_t)i]=stepProbability[(size_t)r][(size_t)i].load(std::memory_order_relaxed);ratchetPlain[(size_t)r][(size_t)i]=stepRatchet[(size_t)r][(size_t)i].load(std::memory_order_relaxed);microPlain[(size_t)r][(size_t)i]=stepMicroShift[(size_t)r][(size_t)i].load(std::memory_order_relaxed);}
    for(int i=0;i<stepCount;++i)glitchPlain[(size_t)i]=glitchLane[(size_t)i].load(std::memory_order_relaxed);
    raw.append(patternPlain.data(),sizeof(patternPlain));raw.append(glitchPlain.data(),sizeof(glitchPlain));raw.append(probabilityPlain.data(),sizeof(probabilityPlain));raw.append(ratchetPlain.data(),sizeof(ratchetPlain));raw.append(microPlain.data(),sizeof(microPlain));state.setProperty("patternBase64",raw.toBase64Encoding(),nullptr);for(int i=0;i<11;++i){state.setProperty("midiCC"+juce::String(i),midiCC[(size_t)i].load(),nullptr);state.setProperty("midiMin"+juce::String(i),midiMinimum[(size_t)i].load(),nullptr);state.setProperty("midiMax"+juce::String(i),midiMaximum[(size_t)i].load(),nullptr);state.setProperty("midiInvert"+juce::String(i),midiInverted[(size_t)i].load(),nullptr);state.setProperty("midiCurve"+juce::String(i),midiCurve[(size_t)i].load(),nullptr);}for(int i=0;i<trackCount;++i){state.setProperty("sliceCount"+juce::String(i+1),sliceCounts[(size_t)i].load(),nullptr);state.setProperty("selectedSlot"+juce::String(i+1),selectedSampleSlot[(size_t)i].load(),nullptr);for(int j=0;j<samplesPerTrack;++j){juce::String path;{const juce::ScopedLock lock(sampleInfoLock);path=sampleFiles[(size_t)i][(size_t)j].getFullPathName();}state.setProperty("samplePath"+juce::String(i+1)+"_"+juce::String(j+1),path,nullptr);}}if(auto xml=state.createXml())copyXmlToBinary(*xml,dest);}
void YouGlitchSMP1024AudioProcessor::setStateInformation(const void* data,int size){if(auto xml=getXmlFromBinary(data,size))if(xml->hasTagName(parameters.state.getType())){auto state=juce::ValueTree::fromXml(*xml);parameters.replaceState(state);for(int i=0;i<11;++i){midiCC[(size_t)i].store((int)state.getProperty("midiCC"+juce::String(i),-1));setMidiRange(i,(float)state.getProperty("midiMin"+juce::String(i),0.0f),(float)state.getProperty("midiMax"+juce::String(i),1.0f),(bool)state.getProperty("midiInvert"+juce::String(i),false));setMidiCurve(i,(int)state.getProperty("midiCurve"+juce::String(i),0));}for(int i=0;i<trackCount;++i){sliceCounts[(size_t)i]=juce::jlimit(1,32,(int)state.getProperty("sliceCount"+juce::String(i+1),8));int wantedSlot=juce::jlimit(0,samplesPerTrack-1,(int)state.getProperty("selectedSlot"+juce::String(i+1),0));for(int j=0;j<samplesPerTrack;++j){auto path=state.getProperty("samplePath"+juce::String(i+1)+"_"+juce::String(j+1)).toString();if(path.isNotEmpty()){loadSampleIntoSlot(i,j,juce::File(path));}}selectedSampleSlot[(size_t)i]=wantedSlot;setSampleSlot(i,wantedSlot);}juce::MemoryBlock raw;if(raw.fromBase64Encoding(state.getProperty("patternBase64").toString()))
{
    using PatternPlain=std::array<std::array<uint8_t,stepCount>,trackCount>; using ProbabilityPlain=std::array<std::array<float,stepCount>,trackCount>; using RatchetPlain=std::array<std::array<uint8_t,stepCount>,trackCount>; using MicroPlain=std::array<std::array<float,stepCount>,trackCount>; using GlitchPlain=std::array<uint8_t,stepCount>;
    const auto oldSize=sizeof(PatternPlain)+sizeof(GlitchPlain);
    const auto newSize=oldSize+sizeof(ProbabilityPlain)+sizeof(RatchetPlain)+sizeof(MicroPlain);
    if(raw.getSize()==oldSize || raw.getSize()==newSize)
    {
        const auto* bytes=static_cast<const char*>(raw.getData());
        PatternPlain patternPlain{}; GlitchPlain glitchPlain{};
        std::memcpy(patternPlain.data(),bytes,sizeof(patternPlain));
        std::memcpy(glitchPlain.data(),bytes+sizeof(patternPlain),sizeof(glitchPlain));
        for(int r=0;r<trackCount;++r)for(int i=0;i<stepCount;++i)pattern[(size_t)r][(size_t)i].store(patternPlain[(size_t)r][(size_t)i],std::memory_order_relaxed);
        for(int i=0;i<stepCount;++i)glitchLane[(size_t)i].store(glitchPlain[(size_t)i],std::memory_order_relaxed);
        if(raw.getSize()==newSize)
        {
            auto offset=oldSize; ProbabilityPlain probabilityPlain{}; RatchetPlain ratchetPlain{}; MicroPlain microPlain{};
            std::memcpy(probabilityPlain.data(),bytes+offset,sizeof(probabilityPlain)); offset+=sizeof(probabilityPlain);
            std::memcpy(ratchetPlain.data(),bytes+offset,sizeof(ratchetPlain)); offset+=sizeof(ratchetPlain);
            std::memcpy(microPlain.data(),bytes+offset,sizeof(microPlain));
            for(int r=0;r<trackCount;++r)for(int i=0;i<stepCount;++i){stepProbability[(size_t)r][(size_t)i].store(probabilityPlain[(size_t)r][(size_t)i],std::memory_order_relaxed);stepRatchet[(size_t)r][(size_t)i].store(ratchetPlain[(size_t)r][(size_t)i],std::memory_order_relaxed);stepMicroShift[(size_t)r][(size_t)i].store(microPlain[(size_t)r][(size_t)i],std::memory_order_relaxed);}
        }
        else
        {
            for(int r=0;r<trackCount;++r)for(int i=0;i<stepCount;++i){stepProbability[(size_t)r][(size_t)i].store(1.0f,std::memory_order_relaxed);stepRatchet[(size_t)r][(size_t)i].store(1,std::memory_order_relaxed);stepMicroShift[(size_t)r][(size_t)i].store(0.0f,std::memory_order_relaxed);}
        }
    }
}}}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new YouGlitchSMP1024AudioProcessor();}
