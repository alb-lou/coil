#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <atomic>
#include "SpringDsp.h"

class CoilProcessor : public juce::AudioProcessor
{
public:
    CoilProcessor();
    ~CoilProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processChunk (juce::AudioBuffer<float>&);

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Coil"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts;

private:
    std::atomic<float> *pModel, *pDecay, *pTension, *pBright, *pBoingy, *pDrive,
                       *pEq1Gain, *pEq1Freq, *pEq1Width, *pEq2Gain, *pEq2Freq, *pEq2Width,
                       *pDry, *pWet, *pWidth, *pPan;

    spr::SpringReverb reverb;
    std::array<spr::Biquad, 2> eq1, eq2;
    int lastModel = -1;
    double fs = 44100.0;
    int maxBlock = 512;
    juce::SmoothedValue<float> dryS, wetS, widthS, panLS, panRS;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CoilProcessor)
};
