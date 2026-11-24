#include "PluginEditor.h"

void YouGlitchWaveformView::paint(juce::Graphics& g)
{
    auto area=getLocalBounds().toFloat().reduced(2.0f);
    g.fillAll(juce::Colour(0xff101218));
    g.setColour(juce::Colour(0xff353945));
    g.drawRoundedRectangle(area,5.0f,1.0f);
    auto inner=area.reduced(8.0f,7.0f);
    if(inner.isEmpty()) return;
    auto peaks=processor.getWaveformPeaks(track,slot,juce::jmax(1,(int)inner.getWidth()));
    g.setColour(juce::Colour(0xff252a33));
    g.drawHorizontalLine((int)inner.getCentreY(),inner.getX(),inner.getRight());
    if(peaks.empty())
    {
        g.setColour(juce::Colour(0xff858b98));
        g.setFont(13.0f);
        g.drawFittedText("IMPORTER UN SAMPLE POUR AFFICHER LA FORME D'ONDE",inner.toNearestInt(),juce::Justification::centred,1);
        return;
    }
    const float xStart=inner.getX()+start*inner.getWidth();
    const float xEnd=inner.getX()+end*inner.getWidth();
    g.setColour(juce::Colour(0x4438a6ff));
    g.fillRect(juce::Rectangle<float>(xStart,inner.getY(),juce::jmax(0.0f,xEnd-xStart),inner.getHeight()));
    g.setColour(juce::Colour(0xffff713f));
    juce::Path wave;
    const float mid=inner.getCentreY();
    for(size_t i=0;i<peaks.size();++i)
    {
        const float x=inner.getX()+(float)i/(float)juce::jmax((size_t)1,peaks.size()-1)*inner.getWidth();
        const float amp=peaks[i]*inner.getHeight()*0.47f;
        wave.startNewSubPath(x,mid-amp);
        wave.lineTo(x,mid+amp);
    }
    g.strokePath(wave,juce::PathStrokeType(1.0f));
    g.setColour(juce::Colour(0xff57b7ff));
    g.drawVerticalLine((int)xStart,inner.getY(),inner.getBottom());
    g.drawVerticalLine((int)xEnd,inner.getY(),inner.getBottom());
    g.setColour(juce::Colour(0x668b93a5));
    for(int i=1;i<slices;++i)
    {
        const float x=inner.getX()+inner.getWidth()*(float)i/(float)slices;
        g.drawVerticalLine((int)x,inner.getY()+2.0f,inner.getBottom()-2.0f);
    }
}
void YouGlitchWaveformView::mouseDown(const juce::MouseEvent& e)
{
    auto inner=getLocalBounds().toFloat().reduced(10.0f,9.0f);
    if(inner.getWidth()<=0.0f) return;
    const float pos=juce::jlimit(0.0f,1.0f,(e.position.x-inner.getX())/inner.getWidth());
    dragHandle=std::abs(pos-start)<=std::abs(pos-end)?1:2;
    updateRegion(e.position.x);
}
void YouGlitchWaveformView::mouseDrag(const juce::MouseEvent& e) { updateRegion(e.position.x); }
void YouGlitchWaveformView::updateRegion(float x)
{
    auto inner=getLocalBounds().toFloat().reduced(10.0f,9.0f);
    if(inner.getWidth()<=0.0f) return;
    const float pos=juce::jlimit(0.0f,1.0f,(x-inner.getX())/inner.getWidth());
    if(dragHandle==1) start=juce::jmin(pos,end-0.01f);
    else end=juce::jmax(pos,start+0.01f);
    if(onRegionChanged) onRegionChanged(start,end);
    repaint();
}


YouGlitchSMP1024AudioProcessorEditor::YouGlitchSMP1024AudioProcessorEditor(YouGlitchSMP1024AudioProcessor& p):AudioProcessorEditor(&p),processor(p),waveform(p){setSize(1000,760);title.setText("YouGlitch SMP 1024",juce::dontSendNotification);title.setFont(juce::Font(juce::FontOptions(25,juce::Font::bold)));addAndMakeVisible(title);auto setup=[this](juce::Slider& s,juce::Label& l,juce::String name){s.setSliderStyle(juce::Slider::LinearHorizontal);s.setTextBoxStyle(juce::Slider::TextBoxRight,false,55,20);l.setText(name,juce::dontSendNotification);addAndMakeVisible(s);addAndMakeVisible(l);};setup(bpm,bpmL,"BPM");setup(density,densityL,"DENSITÉ");setup(chaos,chaosL,"CHAOS");setup(swing,swingL,"SWING");attachments[0]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters,"bpm",bpm);attachments[1]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters,"density",density);attachments[2]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters,"chaos",chaos);attachments[3]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters,"swing",swing);
setup(sampleStart,startL,"DÉBUT");setup(sampleEnd,endL,"FIN");setup(samplePitch,pitchL,"PITCH");
sampleStart.setRange(0.0,0.99,0.001);sampleEnd.setRange(0.01,1.0,0.001);samplePitch.setRange(-24.0,24.0,0.1);
addAndMakeVisible(sampleLoop); addAndMakeVisible(sampleReverse); addAndMakeVisible(sampleGate);
attachments[4]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters,"start1",sampleStart);
attachments[5]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters,"end1",sampleEnd);
attachments[6]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters,"pitch1",samplePitch);
loopAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters,"loop1",sampleLoop); reverseAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters,"reverse1",sampleReverse); gateAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters,"gate1",sampleGate);
auto setupEnvelope=[this,&setup](juce::Slider& slider,juce::Label& label,const juce::String& name,double lo,double hi,double step){setup(slider,label,name);slider.setRange(lo,hi,step);};
setupEnvelope(sampleAttack,attackL,"A",0.0,2.0,0.001);
setupEnvelope(sampleDecay,decayL,"D",0.0,2.0,0.001);
setupEnvelope(sampleSustain,sustainL,"S",0.0,1.0,0.01);
setupEnvelope(sampleRelease,releaseL,"R",0.0,4.0,0.001);
attachments[7]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters,"attack1",sampleAttack);
attachments[8]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters,"decay1",sampleDecay);
attachments[9]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters,"sustain1",sampleSustain);
attachments[10]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters,"release1",sampleRelease);
addAndMakeVisible(waveform);
waveform.onRegionChanged=[this](float a,float b){
    auto* startParam=processor.parameters.getParameter("start"+juce::String(selectedTrack+1));
    auto* endParam=processor.parameters.getParameter("end"+juce::String(selectedTrack+1));
    if(startParam) startParam->setValueNotifyingHost(startParam->convertTo0to1(a));
    if(endParam) endParam->setValueNotifyingHost(endParam->convertTo0to1(b));
};
sliceL.setText("SLICES",juce::dontSendNotification);addAndMakeVisible(sliceL);
sliceCount.setSliderStyle(juce::Slider::IncDecButtons);sliceCount.setTextBoxStyle(juce::Slider::TextBoxRight,false,42,20);sliceCount.setRange(1,32,1);sliceCount.setValue(8);addAndMakeVisible(sliceCount);
sliceCount.onValueChange=[this]{processor.setSliceCount(selectedTrack,(int)sliceCount.getValue());};
auto setupStepControl=[this](juce::Slider& slider,juce::Label& label,const juce::String& name,double min,double max,double interval)
{
    label.setText(name,juce::dontSendNotification);
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,48,18);
    slider.setRange(min,max,interval);
    addAndMakeVisible(label); addAndMakeVisible(slider);
};
setupStepControl(stepProbability,probabilityL,"PROB %",0.0,100.0,1.0);
setupStepControl(stepRatchet,ratchetL,"RATCHET",1.0,4.0,1.0);
setupStepControl(stepMicroShift,microL,"MICRO +%",0.0,45.0,1.0);
stepProbability.onValueChange=[this]{processor.setStepProbability(selectedTrack,selectedStep,(float)(stepProbability.getValue()/100.0));};
stepRatchet.onValueChange=[this]{processor.setStepRatchet(selectedTrack,selectedStep,(int)stepRatchet.getValue());};
stepMicroShift.onValueChange=[this]{processor.setStepMicroShift(selectedTrack,selectedStep,(float)(stepMicroShift.getValue()/100.0));};
for(int i=0;i<8;++i){auto* sb=sliceButtons.add(new juce::TextButton("SL "+juce::String(i+1)));sb->onClick=[this,i]{processor.triggerSlice(selectedTrack,i);};addAndMakeVisible(sb);}
for(auto* b:{&midiLearnButton,&play,&random,&clear,&prev,&next,&loadSampleButton,&searchSampleButton,&previewSampleButton})addAndMakeVisible(*b);play.onClick=[this]{bool on=!processor.isPlaying();processor.setPlaying(on);play.setButtonText(on?"■ STOP":"▶ PLAY");};random.onClick=[this]{processor.randomisePattern();refreshGrid();};clear.onClick=[this]{processor.clearPattern();refreshGrid();};prev.onClick=[this]{selectPage((page+15)%16);};next.onClick=[this]{selectPage((page+1)%16);};track.addItemList({"KICK","SNARE","HAT","CLAP","BASS","LEAD"},1);track.setSelectedId(1,juce::dontSendNotification);addAndMakeVisible(track);loadSampleButton.onClick=[this]{chooseSample();};
sampleSearch.setTextToShowWhenEmpty("Rechercher un sample chargé…",juce::Colours::grey); addAndMakeVisible(sampleSearch);
searchSampleButton.onClick=[this]{searchSamples();};
sampleCategory.addItemList({"TOUTES","PERCUSSIONS","FX","TONAL","VOIX"},1); addAndMakeVisible(sampleCategory);
sampleCategory.onChange=[this]{searchSamples();};
favoriteButton.onClick=[this]{toggleFavorite();};
favoritesOnlyButton.onClick=[this]{favoritesOnly=!favoritesOnly;favoritesOnlyButton.setButtonText(favoritesOnly?"★ FAVORIS ACTIFS":"FAVORIS SEULS");searchSamples();};
previewSampleButton.onClick=[this]{processor.triggerSlice(selectedTrack,0);};
refreshSampleCount();
for(int i=0;i<YouGlitchSMP1024AudioProcessor::sampleSlotCount;++i)sampleSlotSelect.addItem("SAMPLE "+juce::String(i+1),i+1);
addAndMakeVisible(sampleSlotSelect);
sampleSlotSelect.onChange=[this]{processor.setSampleSlot(selectedTrack,sampleSlotSelect.getSelectedItemIndex());waveform.setSlot(sampleSlotSelect.getSelectedItemIndex());sampleStatus.setText(processor.getSampleName(selectedTrack),juce::dontSendNotification);favoriteButton.setButtonText(sampleFavorites[selectedTrack][sampleSlotSelect.getSelectedItemIndex()]?"★ FAVORI":"☆ AJOUTER FAVORI");};
sampleStatus.setText("Aucun sample chargé",juce::dontSendNotification);addAndMakeVisible(sampleStatus);
midiTargetSelect.addItemList({"BPM","DENSITÉ","CHAOS","SWING","MASTER FX","KICK VOL","SNARE VOL","HAT VOL","CLAP VOL","BASS VOL","LEAD VOL"},1);
addAndMakeVisible(midiTargetSelect);midiLearnButton.setButtonText("APPRENDRE CC");midiLearnButton.onClick=[this]{const int target=midiTargetSelect.getSelectedItemIndex();processor.beginMidiLearn(target);midiLearnStatus.setText("En attente d'un contrôleur MIDI…",juce::dontSendNotification);};addAndMakeVisible(midiLearnStatus);
auto setupMidiRange=[this](juce::Slider& slider,juce::Label& label,const juce::String& name){label.setText(name,juce::dontSendNotification);slider.setSliderStyle(juce::Slider::LinearHorizontal);slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,48,18);slider.setRange(0.0,100.0,1.0);addAndMakeVisible(label);addAndMakeVisible(slider);};
setupMidiRange(midiMin,midiMinLabel,"MIN %");setupMidiRange(midiMax,midiMaxLabel,"MAX %");addAndMakeVisible(midiInvert);
auto updateMidiRange=[this]{const int target=midiTargetSelect.getSelectedItemIndex();processor.setMidiRange(target,(float)(midiMin.getValue()/100.0),(float)(midiMax.getValue()/100.0),midiInvert.getToggleState());};
midiMin.onValueChange=updateMidiRange;midiMax.onValueChange=updateMidiRange;midiInvert.onClick=updateMidiRange;
midiTargetSelect.onChange=[this]{const int t=midiTargetSelect.getSelectedItemIndex();midiMin.setValue(processor.getMidiMinimum(t)*100.0,juce::dontSendNotification);midiMax.setValue(processor.getMidiMaximum(t)*100.0,juce::dontSendNotification);midiInvert.setToggleState(processor.isMidiInverted(t),juce::dontSendNotification);};
midiCurveSelect.addItem("LINÉAIRE",1);midiCurveSelect.addItem("EXPONENTIELLE",2);midiCurveSelect.addItem("LOGARITHMIQUE",3);midiCurveSelect.setSelectedId(1,juce::dontSendNotification);addAndMakeVisible(midiCurveSelect);
midiCurveSelect.onChange=[this]{processor.setMidiCurve(midiTargetSelect.getSelectedItemIndex(),midiCurveSelect.getSelectedItemIndex()-1);};
auto oldTargetChanged=midiTargetSelect.onChange;
midiTargetSelect.onChange=[this]{const int t=midiTargetSelect.getSelectedItemIndex();midiMin.setValue(processor.getMidiMinimum(t)*100.0,juce::dontSendNotification);midiMax.setValue(processor.getMidiMaximum(t)*100.0,juce::dontSendNotification);midiInvert.setToggleState(processor.isMidiInverted(t),juce::dontSendNotification);midiCurveSelect.setSelectedId(processor.getMidiCurve(t)+1,juce::dontSendNotification);};
soundL.setText("SON",juce::dontSendNotification);addAndMakeVisible(soundL);
for(int i=0;i<418;++i)soundSelect.addItem("SON "+juce::String(i+1),i+1);
addAndMakeVisible(soundSelect);
auto bindSound=[this]{soundAttachment.reset();soundAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.parameters,"sound"+juce::String(selectedTrack+1),soundSelect);};
track.onChange=[this,bindSound]{selectedTrack=track.getSelectedItemIndex();bindSound();waveform.setTrack(selectedTrack);waveform.setRegion(processor.parameters.getRawParameterValue("start"+juce::String(selectedTrack+1))->load(),processor.parameters.getRawParameterValue("end"+juce::String(selectedTrack+1))->load());sliceCount.setValue(processor.getSliceCount(selectedTrack),juce::dontSendNotification);
attachments[4]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters,"start"+juce::String(selectedTrack+1),sampleStart);
attachments[5]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters,"end"+juce::String(selectedTrack+1),sampleEnd);
attachments[6]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters,"pitch"+juce::String(selectedTrack+1),samplePitch);
loopAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters,"loop"+juce::String(selectedTrack+1),sampleLoop);reverseAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters,"reverse"+juce::String(selectedTrack+1),sampleReverse);gateAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters,"gate"+juce::String(selectedTrack+1),sampleGate);
attachments[7]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters,"attack"+juce::String(selectedTrack+1),sampleAttack);
attachments[8]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters,"decay"+juce::String(selectedTrack+1),sampleDecay);
attachments[9]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters,"sustain"+juce::String(selectedTrack+1),sampleSustain);
attachments[10]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters,"release"+juce::String(selectedTrack+1),sampleRelease);sampleStatus.setText(processor.getSampleName(selectedTrack).isNotEmpty()?processor.getSampleName(selectedTrack):"Aucun sample chargé",juce::dontSendNotification);refreshGrid();refreshStepControls();};
glitchInfo.setText("GLITCH: 1 FREEZE · 2 STUTTER · 4 KILL · 5 RING · 6 DIST · 9/10 DELAY · 11 BITCRUSH · 12 TAPE STOP",juce::dontSendNotification);glitchInfo.setColour(juce::Label::textColourId,juce::Colour(0xffb5bac4));addAndMakeVisible(glitchInfo);
bindSound();selectedStep=0;refreshStepControls();waveform.setTrack(0);waveform.setSlot(0);waveform.setRegion(0.0f,1.0f);addAndMakeVisible(pageLabel);addAndMakeVisible(status);status.setText("Synthèse native · 6 pistes · séquenceur 1024 pas",juce::dontSendNotification);for(int i=0;i<64;++i){auto* b=cells.add(new juce::TextButton());b->onClick=[this,i]{int s=page*64+i;selectedStep=s;processor.setStep(selectedTrack,s,(uint8_t)((processor.getStep(selectedTrack,s)+1)%4));refreshGrid();refreshStepControls();};addAndMakeVisible(b);}startTimerHz(20);selectPage(0);}
void YouGlitchSMP1024AudioProcessorEditor::paint(juce::Graphics& g){g.fillAll(juce::Colour(0xff0d0e11));g.setColour(juce::Colour(0xffff5a2b));g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(8),8,2);g.setColour(juce::Colour(0xff181a1f));g.fillRoundedRectangle(16,155,(float)getWidth()-32,(float)getHeight()-175,8);}
void YouGlitchSMP1024AudioProcessorEditor::resized(){auto a=getLocalBounds().reduced(20);title.setBounds(a.removeFromTop(42));auto row=a.removeFromTop(42);play.setBounds(row.removeFromLeft(110).reduced(2));random.setBounds(row.removeFromLeft(145).reduced(2));clear.setBounds(row.removeFromLeft(100).reduced(2));next.setBounds(row.removeFromRight(100).reduced(2));prev.setBounds(row.removeFromRight(100).reduced(2));pageLabel.setBounds(row.removeFromRight(100));auto sliders=a.removeFromTop(104);auto r=sliders.removeFromTop(26);bpmL.setBounds(r.removeFromLeft(75));bpm.setBounds(r);r=sliders.removeFromTop(26);densityL.setBounds(r.removeFromLeft(75));density.setBounds(r);r=sliders.removeFromTop(26);chaosL.setBounds(r.removeFromLeft(75));chaos.setBounds(r);r=sliders.removeFromTop(26);swingL.setBounds(r.removeFromLeft(75));swing.setBounds(r);auto sampler=a.removeFromTop(32);startL.setBounds(sampler.removeFromLeft(55));sampleStart.setBounds(sampler.removeFromLeft(170));endL.setBounds(sampler.removeFromLeft(40));sampleEnd.setBounds(sampler.removeFromLeft(170));pitchL.setBounds(sampler.removeFromLeft(45));samplePitch.setBounds(sampler.removeFromLeft(170));sampleLoop.setBounds(sampler.removeFromLeft(80));sampleReverse.setBounds(sampler.removeFromLeft(100));sampleGate.setBounds(sampler.removeFromLeft(110));auto env=a.removeFromTop(34);attackL.setBounds(env.removeFromLeft(22));sampleAttack.setBounds(env.removeFromLeft(190));decayL.setBounds(env.removeFromLeft(22));sampleDecay.setBounds(env.removeFromLeft(190));sustainL.setBounds(env.removeFromLeft(22));sampleSustain.setBounds(env.removeFromLeft(190));releaseL.setBounds(env.removeFromLeft(22));sampleRelease.setBounds(env);
waveform.setBounds(a.removeFromTop(112).reduced(1));
auto slices=a.removeFromTop(30);sliceL.setBounds(slices.removeFromLeft(60));sliceCount.setBounds(slices.removeFromLeft(90));for(int i=0;i<sliceButtons.size();++i)sliceButtons[i]->setBounds(slices.removeFromLeft(65).reduced(1));
auto stepControls=a.removeFromTop(34);
probabilityL.setBounds(stepControls.removeFromLeft(58));stepProbability.setBounds(stepControls.removeFromLeft(165));
ratchetL.setBounds(stepControls.removeFromLeft(68));stepRatchet.setBounds(stepControls.removeFromLeft(130));
microL.setBounds(stepControls.removeFromLeft(75));stepMicroShift.setBounds(stepControls.removeFromLeft(165));
auto midiRow=a.removeFromTop(30);midiTargetSelect.setBounds(midiRow.removeFromLeft(150).reduced(1));midiLearnButton.setBounds(midiRow.removeFromLeft(135).reduced(1));midiLearnStatus.setBounds(midiRow);
auto midiRangeRow=a.removeFromTop(30);midiMinLabel.setBounds(midiRangeRow.removeFromLeft(48));midiMin.setBounds(midiRangeRow.removeFromLeft(140));midiMaxLabel.setBounds(midiRangeRow.removeFromLeft(48));midiMax.setBounds(midiRangeRow.removeFromLeft(140));midiInvert.setBounds(midiRangeRow.removeFromLeft(120));midiCurveSelect.setBounds(midiRangeRow.removeFromLeft(150).reduced(1));
glitchInfo.setBounds(a.removeFromTop(24));
auto tr=a.removeFromTop(38);track.setBounds(tr.removeFromLeft(105).reduced(2));loadSampleButton.setBounds(tr.removeFromLeft(125).reduced(2));sampleSearch.setBounds(tr.removeFromLeft(175).reduced(2));searchSampleButton.setBounds(tr.removeFromLeft(90).reduced(2));sampleSlotSelect.setBounds(tr.removeFromLeft(105).reduced(2));previewSampleButton.setBounds(tr.removeFromLeft(105).reduced(2));sampleStatus.setBounds(tr.removeFromLeft(130));status.setBounds(tr);
auto browser=a.removeFromTop(32);sampleCategory.setBounds(browser.removeFromLeft(130).reduced(1));favoriteButton.setBounds(browser.removeFromLeft(135).reduced(1));favoritesOnlyButton.setBounds(browser.removeFromLeft(145).reduced(1));
int gap=4,w=(a.getWidth()-gap*15)/16,h=42;for(int i=0;i<64;++i)cells[i]->setBounds(a.getX()+(i%16)*(w+gap),a.getY()+(i/16)*(h+gap),w,h);}
void YouGlitchSMP1024AudioProcessorEditor::selectPage(int p){page=juce::jlimit(0,15,p);processor.setPage(page);selectedStep=page*64;refreshStepControls();pageLabel.setText("PAGE "+juce::String(page+1)+" / 16",juce::dontSendNotification);refreshGrid();}
void YouGlitchSMP1024AudioProcessorEditor::refreshGrid(){for(int i=0;i<64;++i){auto v=processor.getStep(selectedTrack,page*64+i);cells[i]->setButtonText(v==0?"":v==1?"●":v==2?"●×2":"●×4");cells[i]->setColour(juce::TextButton::buttonColourId,v==0?juce::Colour(0xff2b2d34):v==1?juce::Colour(0xffff5a2b):v==2?juce::Colour(0xffffb21f):juce::Colour(0xffe6c800));cells[i]->setColour(juce::TextButton::textColourOffId,juce::Colours::black);}}
void YouGlitchSMP1024AudioProcessorEditor::refreshStepControls()
{
    stepProbability.setValue(processor.getStepProbability(selectedTrack,selectedStep)*100.0,juce::dontSendNotification);
    stepRatchet.setValue(processor.getStepRatchet(selectedTrack,selectedStep),juce::dontSendNotification);
    stepMicroShift.setValue(processor.getStepMicroShift(selectedTrack,selectedStep)*100.0,juce::dontSendNotification);
}
void YouGlitchSMP1024AudioProcessorEditor::timerCallback()
{
    waveform.setSlices(processor.getSliceCount(selectedTrack));
    waveform.setRegion(processor.parameters.getRawParameterValue("start"+juce::String(selectedTrack+1))->load(),processor.parameters.getRawParameterValue("end"+juce::String(selectedTrack+1))->load());
    const int playingStep=processor.getPlayheadStep();
    for(int i=0;i<64;++i)
    {
        const int absoluteStep=page*64+i;
        const bool isPlayhead=playingStep==absoluteStep;
        const bool isSelected=selectedStep==absoluteStep;
    }
    if(!processor.isPlaying()) play.setButtonText("▶ PLAY");
    const int learning=processor.getMidiLearnTarget();
    if(learning>=0) midiLearnStatus.setText("En attente d'un CC pour "+midiTargetSelect.getText(),juce::dontSendNotification);
    else
    {
        const int target=midiTargetSelect.getSelectedItemIndex();
        const int cc=processor.getMidiCC(target);
        midiLearnStatus.setText(cc>=0?"CC "+juce::String(cc)+" assigné":"Aucun CC assigné",juce::dontSendNotification);
    }
}

void YouGlitchSMP1024AudioProcessorEditor::chooseSample()
{
    auto chooser = std::make_shared<juce::FileChooser>("Choisir un sample audio", juce::File{}, "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3;*.m4a");
    juce::Component::SafePointer<YouGlitchSMP1024AudioProcessorEditor> safeThis(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safeThis, chooser](const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == juce::File{} || safeThis == nullptr) return;
            if (safeThis->processor.loadSample(safeThis->selectedTrack, file))
            {
                safeThis->sampleSlotSelect.setSelectedItemIndex(juce::jmax(0,safeThis->processor.getLoadedSampleCount(safeThis->selectedTrack)-1),juce::dontSendNotification);
                safeThis->waveform.setSlot(safeThis->sampleSlotSelect.getSelectedItemIndex()); safeThis->refreshSampleCount(); safeThis->sampleStatus.setText(safeThis->processor.getSampleName(safeThis->selectedTrack), juce::dontSendNotification);
            }
            else
                safeThis->sampleStatus.setText("Format non lu / fichier invalide", juce::dontSendNotification);
        });
}

void YouGlitchSMP1024AudioProcessorEditor::refreshSampleCount()
{
    sampleStatus.setText(juce::String(processor.getLoadedSampleCount(selectedTrack))+" / 300 samples",juce::dontSendNotification);
}
void YouGlitchSMP1024AudioProcessorEditor::searchSamples()
{
    const auto query=sampleSearch.getText().trim().toLowerCase();
    const int category=sampleCategory.getSelectedItemIndex();
    for(int slot=0;slot<300;++slot)
    {
        const auto name=processor.getSampleName(selectedTrack,slot);
        if(name.isEmpty()) continue;
        const auto lower=name.toLowerCase();
        if(!query.isEmpty()&&!lower.contains(query)) continue;
        if(favoritesOnly&&!sampleFavorites[selectedTrack][slot]) continue;
        bool categoryMatch=(category==0);
        if(category==1) categoryMatch=lower.contains("kick")||lower.contains("snare")||lower.contains("hat")||lower.contains("clap")||lower.contains("perc")||lower.contains("drum");
        if(category==2) categoryMatch=lower.contains("fx")||lower.contains("noise")||lower.contains("impact")||lower.contains("riser")||lower.contains("sweep");
        if(category==3) categoryMatch=lower.contains("bass")||lower.contains("lead")||lower.contains("synth")||lower.contains("piano")||lower.contains("keys")||lower.contains("tone");
        if(category==4) categoryMatch=lower.contains("voice")||lower.contains("vocal")||lower.contains("vox");
        if(!categoryMatch) continue;
        sampleSlotSelect.setSelectedItemIndex(slot,juce::sendNotification);
        waveform.setSlot(slot);
        sampleStatus.setText("Trouvé : "+name,juce::dontSendNotification);
        return;
    }
    sampleStatus.setText("Aucun résultat",juce::dontSendNotification);
}

void YouGlitchSMP1024AudioProcessorEditor::toggleFavorite()
{
    const int slot=sampleSlotSelect.getSelectedItemIndex();
    if(slot<0||slot>=300||processor.getSampleName(selectedTrack,slot).isEmpty()) return;
    sampleFavorites[selectedTrack][slot]=!sampleFavorites[selectedTrack][slot];
    favoriteButton.setButtonText(sampleFavorites[selectedTrack][slot]?"★ FAVORI":"☆ AJOUTER FAVORI");
    searchSamples();
}
