#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
class YouGlitchWaveformView final : public juce::Component
{
public:
    explicit YouGlitchWaveformView(YouGlitchSMP1024AudioProcessor& p) : processor(p) {}
    void setTrack(int t) noexcept { track=t; repaint(); }
    void setSlot(int s) noexcept { slot=s; repaint(); }
    void setSlices(int n) noexcept { const int next=juce::jlimit(1,32,n); if(slices!=next){slices=next;repaint();} }
    void setRegion(float a,float b) noexcept { const float nextStart=juce::jlimit(0.0f,0.99f,a); const float nextEnd=juce::jlimit(nextStart+0.01f,1.0f,b); if(std::abs(start-nextStart)>0.0001f||std::abs(end-nextEnd)>0.0001f){start=nextStart;end=nextEnd;repaint();} }
    std::function<void(float,float)> onRegionChanged;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
private:
    void updateRegion(float x);
    YouGlitchSMP1024AudioProcessor& processor;
    int track=0,slot=0,slices=8,dragHandle=0;
    float start=0.0f,end=1.0f;
};

class YouGlitchSMP1024AudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit YouGlitchSMP1024AudioProcessorEditor(YouGlitchSMP1024AudioProcessor&);
    void paint(juce::Graphics&) override; void resized() override;
private:
    void timerCallback() override; void chooseSample(); void searchSamples(); void toggleFavorite(); void refreshSampleCount(); void refreshGrid(); void refreshStepControls(); void selectPage(int);
    YouGlitchSMP1024AudioProcessor& processor; YouGlitchWaveformView waveform;
    juce::Label title,status,pageLabel,sampleStatus,midiLearnStatus,midiMinLabel,midiMaxLabel,glitchInfo; juce::TextButton searchSampleButton{"CHERCHER"}, previewSampleButton{"▶ ÉCOUTER"}, midiLearnButton{"APPRENDRE CC"}, play{"▶ PLAY"},random{"ALÉA PATTERN"},clear{"EFFACER"},prev{"◀ PAGE"},next{"PAGE ▶"},loadSampleButton{"IMPORTER SAMPLE"}; juce::OwnedArray<juce::TextButton> sliceButtons;
    juce::Slider midiMin,midiMax,bpm,density,chaos,swing,sampleStart,sampleEnd,samplePitch,sliceCount,stepProbability,stepRatchet,stepMicroShift,sampleAttack,sampleDecay,sampleSustain,sampleRelease; juce::ToggleButton midiInvert{"INVERSER CC"}, sampleLoop{"BOUCLE"}, sampleReverse{"INVERSÉ"}, sampleGate{"MIDI GATE"}; juce::Label bpmL,densityL,chaosL,swingL,soundL,startL,endL,pitchL,sliceL,probabilityL,ratchetL,microL,attackL,decayL,sustainL,releaseL; juce::ComboBox track,soundSelect,sampleSlotSelect,midiTargetSelect,midiCurveSelect; juce::TextEditor sampleSearch; juce::ComboBox sampleCategory; juce::TextButton favoriteButton{"★ FAVORI"}, favoritesOnlyButton{"FAVORIS SEULS"}; std::array<std::array<bool,300>,6> sampleFavorites{}; bool favoritesOnly=false;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> soundAttachment;
    juce::OwnedArray<juce::TextButton> cells;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,11> attachments; std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> loopAttachment, reverseAttachment, gateAttachment;
    int page=0,selectedTrack=0,selectedStep=0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(YouGlitchSMP1024AudioProcessorEditor)
};
