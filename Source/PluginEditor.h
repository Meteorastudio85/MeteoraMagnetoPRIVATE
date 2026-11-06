#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include "PluginProcessor.h"

class MagnetoEditor : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit MagnetoEditor (MagnetoProcessor&);
    ~MagnetoEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    juce::Point<float> toImage (juce::Point<float> p) const;
    int knobAt (juce::Point<float> ip) const;
    bool isClickable (juce::Point<float> ip) const;

    bool getBool (const juce::String& id) const;
    void setBool (const juce::String& id, bool v);
    void setParamNorm (const juce::String& id, float norm);
    void stepMode (int delta);
    void handleTap();
    juce::String valueText (int knobIndex) const;

    void drawPointer (juce::Graphics&, float cx, float cy, float norm) const;
    void drawRing (juce::Graphics&, float cx, float cy, float r, juce::Colour) const;
    static float vuToDegrees (float vu);
    void drawTapeTransport (juce::Graphics&) const;

    MagnetoProcessor& proc;

    juce::Image background, bgCache, ledPowerOn, ledTapOn, toggleOff, modeKnob;
    std::vector<juce::Image> rollerImages;
    float cacheScale = 0.0f;

    int activeKnob = -1;
    float dragStartNorm = 0.0f;
    float dragStartY = 0.0f;
    bool tapPressed = false;

    float needleDeg = -46.0f;

    // animazione del nastro
    double tapePos = 0.0;        // posizione del nastro in pixel dell'immagine
    double tapeSpeed = 0.0;      // pixel/secondo
    double animTime = 0.0;
    float jitterX = 0.0f, jitterY = 0.0f;
    double lastTimerMs = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MagnetoEditor)
};
