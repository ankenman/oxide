#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

//==============================================================================
// Colours / look shared by the editor and its widgets.
namespace OxideColours
{
    const juce::Colour faceDark   { 0xff23252a };
    const juce::Colour faceLight  { 0xff2e3137 };
    const juce::Colour bevel      { 0xff44474f };
    const juce::Colour knobCap    { 0xff15161a };
    const juce::Colour knobRim    { 0xff3a3d44 };
    const juce::Colour text       { 0xffb9bcc4 };
    const juce::Colour textDim    { 0xff6f737c };
    const juce::Colour accent     { 0xffe3a24a }; // warm amber
    const juce::Colour ledOff     { 0xff3a2a16 };
}

//==============================================================================
class OxideLookAndFeel : public juce::LookAndFeel_V4
{
public:
    OxideLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    juce::Font getLabelFont (juce::Label&) override;
};

//==============================================================================
// A big knob with a caption above and a live value readout below.
class KnobPanel : public juce::Component
{
public:
    KnobPanel (juce::RangedAudioParameter& param, const juce::String& caption);

    void resized() override;
    void paint (juce::Graphics&) override;

    juce::Slider& getSlider() { return slider; }

private:
    void updateReadout();

    juce::RangedAudioParameter& parameter;
    juce::String captionText;
    juce::Slider slider;
    juce::Label  readout;
    juce::SliderParameterAttachment attachment;
};

//==============================================================================
// Three-position toggle switch (Normal / Medium / Bright).
class ToneSwitch : public juce::Component
{
public:
    explicit ToneSwitch (juce::RangedAudioParameter& param);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void setFromY (float y);
    void setIndex (int newIndex);
    juce::Rectangle<float> trackBounds() const;

    juce::RangedAudioParameter& parameter;
    juce::ParameterAttachment attachment;
    int index = 0;
    const juce::StringArray labels { "BRIGHT", "MEDIUM", "NORMAL" }; // top to bottom
};

//==============================================================================
// Analogue-style needle VU meter. 0 VU = kZeroVuDbfs, scale −20..+3 VU.
// Feed it the (already VU-integrated) linear level from the processor.
class VuMeter : public juce::Component
{
public:
    static constexpr float kZeroVuDbfs = -18.0f;

    explicit VuMeter (const juce::String& caption);

    void setLevel (float linearMeanAbs);   // VU-envelope level, 0..1+
    void setPeakLit (bool shouldLight);    // peak LED (output meter only)
    void setHasPeakLed (bool b) { hasPeakLed = b; }

    void paint (juce::Graphics&) override;

private:
    float positionForDb (float db) const;
    juce::Point<float> pivot() const;
    float needleLength() const;

    juce::String caption;
    float vuDb    = -30.0f;   // current needle reading
    bool  peakLit = false;
    bool  hasPeakLed = false;
};

//==============================================================================
class OxideAudioProcessorEditor : public juce::AudioProcessorEditor,
                                  private juce::Timer
{
public:
    explicit OxideAudioProcessorEditor (OxideAudioProcessor&);
    ~OxideAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    OxideAudioProcessor& processor;
    OxideLookAndFeel lookAndFeel;

    VuMeter    inMeter  { "IN" };
    VuMeter    outMeter { "OUT" };
    KnobPanel  driveKnob;
    KnobPanel  trimKnob;
    ToneSwitch toneSwitch;

    int peakHoldTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OxideAudioProcessorEditor)
};
