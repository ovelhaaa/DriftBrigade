#include "PluginProcessor.h"
#include <iostream>
#include <stdexcept>
#include "AllocationTracker.h"
namespace { void check(bool ok,const char* msg) { if(!ok) throw std::runtime_error(msg); } }
int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI initialiser;
    try {
        DriftProcessor a,b;
        for(std::size_t i=0;i<drift::Count;++i) {
            auto* parameter=a.state.getParameter(drift::parameterSpecs[i].id);
            parameter->setValueNotifyingHost(static_cast<float>(i+1)/10);
            check(std::abs(parameter->convertTo0to1(parameter->convertFrom0to1(0.37f))-0.37f)<1e-5,"normalized round trip");
        }
        juce::MemoryBlock saved; a.getStateInformation(saved); b.setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
        for(const auto& s : drift::parameterSpecs) check(a.state.getRawParameterValue(s.id)->load()==b.state.getRawParameterValue(s.id)->load(),"state round trip");
        const char bad[]="invalid"; b.setStateInformation(bad,sizeof(bad));
        for(double sr : {44100.,48000.,88200.,96000.}) {
            for(bool stereo : {false,true}) {
                const auto set=stereo ? juce::AudioChannelSet::stereo() : juce::AudioChannelSet::mono();
                juce::AudioProcessor::BusesLayout layout; layout.inputBuses.add(set); layout.outputBuses.add(set);
                check(b.setBusesLayout(layout),"mono/stereo routing"); b.prepareToPlay(sr,17);
                for(int n : {0,1,17,127,1024,8192}) {
                    juce::AudioBuffer<float> audio(stereo ? 2 : 1,n); audio.clear();
                    if(n>0) audio.setSample(0,0,1);
                    juce::MidiBuffer midi;
                    const auto before=allocations.load(); b.processBlock(audio,midi);
                    check(allocations.load()==before,"plugin callback allocation");
                    for(int ch=0;ch<audio.getNumChannels();++ch) for(int i=0;i<n;++i) check(std::isfinite(audio.getSample(ch,i)),"plugin finite output");
                }
            }
        }
        juce::AudioProcessor::BusesLayout upmix; upmix.inputBuses.add(juce::AudioChannelSet::mono()); upmix.outputBuses.add(juce::AudioChannelSet::stereo());
        check(b.setBusesLayout(upmix),"mono to stereo routing");
        b.state.getParameter("mix")->setValueNotifyingHost(0); b.prepareToPlay(48000,17);
        juce::AudioBuffer<float> audio(2,17); audio.clear(); audio.setSample(0,0,0.5f); audio.setSample(1,0,-0.8f);
        juce::MidiBuffer midi; b.processBlock(audio,midi); check(audio.getSample(0,0)==0.5f && audio.getSample(1,0)==0.5f,"mono input duplicated before DSP");
        std::unique_ptr<juce::AudioProcessorEditor> editor(b.createEditor()); editor->setSize(540,540); editor->setSize(1000,900);
        if(argc>1) {
            editor->setSize(660,600);
            auto snapshot=editor->createComponentSnapshot(editor->getLocalBounds());
            juce::File destination(juce::String::fromUTF8(argv[1]));
            auto stream=destination.createOutputStream(); check(stream!=nullptr,"snapshot output");
            stream->setPosition(0); check(stream->truncate().wasOk(),"snapshot truncate");
            juce::PNGImageFormat format; check(format.writeImageToStream(snapshot,*stream),"snapshot encode");
        }
        std::cout << "PASS: JUCE state, normalized parameters, editor construction, mono/stereo/upmix, variable blocks and no callback allocations\n";
        return 0;
    } catch(const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
