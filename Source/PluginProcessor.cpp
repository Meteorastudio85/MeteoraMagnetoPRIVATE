#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    struct SyncDiv { const char* name; double beats; };

    // in sincro la manopola DELAY TIME scatta solo su questi valori (T = terzina, D = puntato)
    const SyncDiv syncDivs[] = {
        { "1/16T", 1.0 / 6.0 }, { "1/16", 0.25 }, { "1/8T", 1.0 / 3.0 },
        { "1/16D", 0.375 },     { "1/8",  0.5 },  { "1/4T", 2.0 / 3.0 },
        { "1/8D",  0.75 },      { "1/4",  1.0 },  { "1/4D", 1.5 }
    };
    constexpr int numSyncDivs = (int) (sizeof (syncDivs) / sizeof (syncDivs[0]));
}

MagnetoProcessor::MagnetoProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MAGNETO", createLayout())
{
    pPower      = apvts.getRawParameterValue ("power");
    pVolume     = apvts.getRawParameterValue ("volume");
    pMix        = apvts.getRawParameterValue ("mix");
    pTime       = apvts.getRawParameterValue ("time");
    pRepeat     = apvts.getRawParameterValue ("repeat");
    pMode       = apvts.getRawParameterValue ("mode");
    pOutput     = apvts.getRawParameterValue ("output");
    pEchoBypass = apvts.getRawParameterValue ("echoBypass");
    pStereo     = apvts.getRawParameterValue ("stereo");
    pSync       = apvts.getRawParameterValue ("sync");
    pHiss       = apvts.getRawParameterValue ("hiss");
    timeParam   = apvts.getParameter ("time");
}

juce::AudioProcessorValueTreeState::ParameterLayout MagnetoProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto knob = [] (const char* id, const char* name, float def)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                                                      NormalisableRange<float> (0.0f, 10.0f, 0.01f), def);
    };

    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "power", 1 }, "Power", true));
    p.push_back (knob ("volume",  "Drive", 4.0f));
    p.push_back (knob ("mix",     "Dry/Wet", 3.5f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "time", 1 }, "Delay Time",
                                                        NormalisableRange<float> (40.0f, 2000.0f, 0.1f, 0.4f), 350.0f));
    p.push_back (knob ("repeat",  "Repeat",  4.0f));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode (Tape)",
                                                         StringArray { "1 - Nuovo", "2 - Studio", "3 - Usurato", "4 - Morente" }, 1));
    p.push_back (knob ("output",  "Output",  5.0f));
    p.push_back (knob ("hiss",    "Hiss",    3.0f));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "echoBypass", 1 }, "Echo Bypass", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "stereo", 1 }, "Input 2 (Stereo)", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "sync", 1 }, "MIDI Clock Sync", false));

    return { p.begin(), p.end() };
}

float MagnetoProcessor::outputKnobToDb (float v)
{
    if (v <= 0.001f) return -100.0f;
    if (v >= 5.0f)   return (v - 5.0f) * 2.4f;                 // fino a +12 dB
    return -40.0f * std::pow (1.0f - v / 5.0f, 1.5f);          // da -40 dB a 0 dB
}

int MagnetoProcessor::syncIndexFromNorm (float norm)
{
    return juce::jlimit (0, numSyncDivs - 1, juce::roundToInt (norm * (float) (numSyncDivs - 1)));
}

int MagnetoProcessor::numSyncDivisions()
{
    return numSyncDivs;
}

double MagnetoProcessor::syncBeats (int index)
{
    return syncDivs[juce::jlimit (0, numSyncDivs - 1, index)].beats;
}

juce::String MagnetoProcessor::syncName (int index)
{
    return syncDivs[juce::jlimit (0, numSyncDivs - 1, index)].name;
}

bool MagnetoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void MagnetoProcessor::updateGainTargets()
{
    // DRY/WET a potenza costante
    const float m = juce::jlimit (0.0f, 1.0f, pMix->load() / 10.0f);
    const float a = m * juce::MathConstants<float>::halfPi;
    dryGain.setTargetValue (std::cos (a));
    wetGain.setTargetValue (std::sin (a) * 1.25f);
    outGain.setTargetValue (juce::Decibels::decibelsToGain (outputKnobToDb (pOutput->load()), -99.0f));
}

void MagnetoProcessor::prepareToPlay (double sampleRate, int)
{
    echo.prepare (sampleRate);

    wetGain.reset (sampleRate, 0.03);
    dryGain.reset (sampleRate, 0.03);
    outGain.reset (sampleRate, 0.03);
    powerMix.reset (sampleRate, 0.02);

    updateGainTargets();
    wetGain.setCurrentAndTargetValue (wetGain.getTargetValue());
    dryGain.setCurrentAndTargetValue (dryGain.getTargetValue());
    outGain.setCurrentAndTargetValue (outGain.getTargetValue());
    powerMix.setCurrentAndTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    needsReset = false;
}

void MagnetoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    const int totalIn = getTotalNumInputChannels();
    for (int c = totalIn; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);

    const int numCh = juce::jmin (totalIn, buffer.getNumChannels());
    if (numCh <= 0 || n <= 0)
        return;

    // ---- tempo di delay (manuale o sincronizzato all'host)
    float ms = pTime->load();
    if (pSync->load() > 0.5f)
    {
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto bpm = pos->getBpm())
                    if (*bpm > 1.0)
                        lastBpm = *bpm;
        hostBpm.store ((float) lastBpm);

        const int idx = syncIndexFromNorm (timeParam->getValue());
        ms = (float) juce::jlimit (40.0, 2000.0, syncBeats (idx) * 60000.0 / lastBpm);
    }
    currentDelayMs.store (ms);

    // ---- accensione / spegnimento (con dissolvenza, niente click)
    const bool power = pPower->load() > 0.5f;
    powerMix.setTargetValue (power ? 1.0f : 0.0f);

    if (! power && ! powerMix.isSmoothing())
    {
        needsReset = true;
        outLevelDb.store (-100.0f);
        return; // segnale dry intatto
    }

    if (power && needsReset)
    {
        echo.reset();
        needsReset = false;
    }

    echo.setParams (ms,
                    pVolume->load() / 10.0f,   // DRIVE = livello di registrazione sul nastro
                    pRepeat->load() / 10.0f,
                    juce::roundToInt (pMode->load()),
                    pStereo->load() > 0.5f,
                    pEchoBypass->load() > 0.5f,
                    pHiss->load() / 10.0f);
    updateGainTargets();

    float* L = buffer.getWritePointer (0);
    float* R = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
    double sumSq = 0.0;

    for (int i = 0; i < n; ++i)
    {
        const float inL = L[i];
        const float inR = R != nullptr ? R[i] : inL;

        float eL = 0.0f, eR = 0.0f;
        echo.process (inL, inR, eL, eR);

        const float wg = wetGain.getNextValue();
        const float dg = dryGain.getNextValue();
        const float og = outGain.getNextValue();
        const float pm = powerMix.getNextValue();

        float oL = (inL * dg + eL * wg) * og;
        float oR = (inR * dg + eR * wg) * og;
        oL = inL + (oL - inL) * pm;
        oR = inR + (oR - inR) * pm;

        L[i] = oL;
        if (R != nullptr) R[i] = oR;

        const float m = R != nullptr ? 0.5f * (oL + oR) : oL;
        sumSq += (double) m * m;
    }

    const double rms = std::sqrt (sumSq / n);
    outLevelDb.store (power ? (float) (20.0 * std::log10 (rms + 1.0e-9)) : -100.0f);
}

void MagnetoProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml != nullptr)
        copyXmlToBinary (*xml, destData);
}

void MagnetoProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* MagnetoProcessor::createEditor()
{
    return new MagnetoEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MagnetoProcessor();
}
