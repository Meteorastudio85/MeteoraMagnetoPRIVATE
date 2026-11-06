#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "TapeEcho.h"

class MagnetoProcessor : public juce::AudioProcessor
{
public:
    MagnetoProcessor();
    ~MagnetoProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static float outputKnobToDb (float knob);
    static int numSyncDivisions();
    static int syncIndexFromNorm (float norm);
    static double syncBeats (int index);
    static juce::String syncName (int index);

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float> outLevelDb { -100.0f };
    std::atomic<float> currentDelayMs { 350.0f };
    std::atomic<float> hostBpm { 120.0f };

private:
    void updateGainTargets();

    TapeEcho echo;
    juce::SmoothedValue<float> wetGain, dryGain, outGain, powerMix;
    bool needsReset = false;
    double lastBpm = 120.0;

    std::atomic<float>* pPower = nullptr;
    std::atomic<float>* pVolume = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pTime = nullptr;
    std::atomic<float>* pRepeat = nullptr;
    std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pEchoBypass = nullptr;
    std::atomic<float>* pStereo = nullptr;
    std::atomic<float>* pSync = nullptr;
    std::atomic<float>* pHiss = nullptr;
    juce::RangedAudioParameter* timeParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MagnetoProcessor)
};
