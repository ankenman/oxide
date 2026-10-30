#include "PluginEditor.h"

using namespace OxideColours;

namespace
{
    juce::Font captionFont (float height)
    {
        return juce::Font (juce::FontOptions (height, juce::Font::bold))
                   .withExtraKerningFactor (0.18f);
    }

    juce::Font readoutFont (float height)
    {
        return juce::Font (juce::FontOptions (height));
    }
}

//==============================================================================
OxideLookAndFeel::OxideLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Label::textColourId, text);
    setColour (juce::TooltipWindow::backgroundColourId, faceDark);
}

juce::Font OxideLookAndFeel::getLabelFont (juce::Label& l)
{
    return l.getFont();
}

void OxideLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                         float pos, float startAngle, float endAngle,
                                         juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h)
                            .reduced (6.0f);
    const float size   = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto  square = bounds.withSizeKeepingCentre (size, size);
    const auto  centre = square.getCentre();
    const float radius = size * 0.5f;

    // --- scale ticks around the knob -------------------------------------
    {
        const int numTicks = 11;
        for (int i = 0; i < numTicks; ++i)
        {
            const float t     = (float) i / (float) (numTicks - 1);
            const float angle = startAngle + t * (endAngle - startAngle);
            const bool  major = (i == 0 || i == numTicks - 1 || i == numTicks / 2);
            const float inner = radius * 0.90f;
            const float outer = radius * (major ? 1.00f : 0.96f);
            juce::Line<float> tick (centre.getPointOnCircumference (inner, angle),
                                    centre.getPointOnCircumference (outer, angle));
            g.setColour (t <= pos ? accent.withAlpha (0.85f) : textDim.withAlpha (0.6f));
            g.drawLine (tick, major ? 2.0f : 1.2f);
        }
    }

    // --- shadow ------------------------------------------------------------
    const float knobR = radius * 0.82f;
    const auto knobArea = juce::Rectangle<float> (knobR * 2, knobR * 2).withCentre (centre);
    {
        juce::DropShadow shadow (juce::Colours::black.withAlpha (0.6f), (int) (knobR * 0.35f),
                                 { 0, (int) (knobR * 0.12f) });
        juce::Path p; p.addEllipse (knobArea);
        shadow.drawForPath (g, p);
    }

    // --- rim (brushed metal) ----------------------------------------------
    {
        juce::ColourGradient rim (knobRim.brighter (0.6f), centre.x, knobArea.getY(),
                                  knobRim.darker (0.7f),   centre.x, knobArea.getBottom(), false);
        g.setGradientFill (rim);
        g.fillEllipse (knobArea);

        // fine concentric "machined" lines
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        for (float r = knobR * 0.88f; r < knobR; r += 2.0f)
            g.drawEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (centre), 0.6f);
    }

    // --- cap -----------------------------------------------------------------
    const float capR = knobR * 0.84f;
    const auto  capArea = juce::Rectangle<float> (capR * 2, capR * 2).withCentre (centre);
    {
        juce::ColourGradient cap (knobCap.brighter (0.35f), centre.x - capR * 0.4f, centre.y - capR * 0.6f,
                                  knobCap.darker (0.4f),    centre.x + capR * 0.5f, centre.y + capR * 0.8f, true);
        g.setGradientFill (cap);
        g.fillEllipse (capArea);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawEllipse (capArea.reduced (0.5f), 1.0f);
    }

    // --- pointer -----------------------------------------------------------
    {
        const float angle = startAngle + pos * (endAngle - startAngle);
        juce::Path pointer;
        const float pw = juce::jmax (2.5f, capR * 0.08f);
        pointer.addRoundedRectangle (-pw * 0.5f, -capR * 0.92f, pw, capR * 0.55f, pw * 0.5f);
        pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre));

        g.setColour (accent.withAlpha (0.35f));
        g.strokePath (pointer, juce::PathStrokeType (3.0f));
        g.setColour (slider.isEnabled() ? accent : textDim);
        g.fillPath (pointer);
    }
}

//==============================================================================
KnobPanel::KnobPanel (juce::RangedAudioParameter& param, const juce::String& caption)
    : parameter (param), captionText (caption), attachment (param, slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                juce::MathConstants<float>::pi * 2.8f, true);
    slider.setDoubleClickReturnValue (true, param.convertFrom0to1 (param.getDefaultValue()));
    slider.setMouseDragSensitivity (220);
    slider.onValueChange = [this] { updateReadout(); };
    addAndMakeVisible (slider);

    readout.setJustificationType (juce::Justification::centred);
    readout.setFont (readoutFont (13.0f));
    readout.setColour (juce::Label::textColourId, text);
    readout.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (readout);

    updateReadout();
}

void KnobPanel::updateReadout()
{
    const float v = (float) slider.getValue();
    readout.setText (parameter.getText (parameter.convertTo0to1 (v), 0) + " " + parameter.getLabel(),
                     juce::dontSendNotification);
}

void KnobPanel::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (22);            // caption
    readout.setBounds (r.removeFromBottom (20));
    slider.setBounds (r);
}

void KnobPanel::paint (juce::Graphics& g)
{
    g.setColour (text);
    g.setFont (captionFont (13.0f));
    g.drawText (captionText, getLocalBounds().removeFromTop (22), juce::Justification::centred);
}

//==============================================================================
ToneSwitch::ToneSwitch (juce::RangedAudioParameter& param)
    : parameter (param),
      attachment (param, [this] (float v)
      {
          index = juce::jlimit (0, 2, (int) std::lround (v));
          repaint();
      })
{
    attachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

juce::Rectangle<float> ToneSwitch::trackBounds() const
{
    auto b = getLocalBounds().toFloat();
    return { b.getX() + 6.0f, b.getY() + 8.0f, 22.0f, b.getHeight() - 16.0f };
}

void ToneSwitch::paint (juce::Graphics& g)
{
    const auto track = trackBounds();

    // slot
    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.fillRoundedRectangle (track, 6.0f);
    g.setColour (bevel.withAlpha (0.6f));
    g.drawRoundedRectangle (track.reduced (0.5f), 6.0f, 1.0f);

    // three detents, labels + LEDs
    const float step = track.getHeight() / 3.0f;
    for (int i = 0; i < 3; ++i)
    {
        const float cy      = track.getY() + step * ((float) i + 0.5f);
        const int   paramIx = 2 - i;             // top row = Bright (index 2)
        const bool  on      = (paramIx == index);

        // LED
        const auto led = juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ track.getRight() + 12.0f, cy });
        if (on)
        {
            g.setColour (accent.withAlpha (0.25f));
            g.fillEllipse (led.expanded (3.0f));
        }
        g.setColour (on ? accent : ledOff);
        g.fillEllipse (led);

        // label
        g.setColour (on ? text : textDim);
        g.setFont (captionFont (10.5f));
        g.drawText (labels[i], juce::Rectangle<float> (track.getRight() + 22.0f, cy - 8.0f, 70.0f, 16.0f),
                    juce::Justification::centredLeft);
    }

    // lever (knurled block) at the active detent
    {
        const int   row = 2 - index;
        const float cy  = track.getY() + step * ((float) row + 0.5f);
        auto lever = juce::Rectangle<float> (track.getWidth() - 4.0f, step - 6.0f).withCentre ({ track.getCentreX(), cy });

        juce::DropShadow (juce::Colours::black.withAlpha (0.5f), 4, { 0, 2 })
            .drawForRectangle (g, lever.toNearestInt());

        juce::ColourGradient grad (knobRim.brighter (0.5f), lever.getX(), lever.getY(),
                                   knobRim.darker (0.6f),   lever.getX(), lever.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (lever, 4.0f);

        g.setColour (juce::Colours::white.withAlpha (0.08f));
        for (float ly = lever.getY() + 4.0f; ly < lever.getBottom() - 3.0f; ly += 3.0f)
            g.drawHorizontalLine ((int) ly, lever.getX() + 4.0f, lever.getRight() - 4.0f);
    }
}

void ToneSwitch::setIndex (int newIndex)
{
    newIndex = juce::jlimit (0, 2, newIndex);
    if (newIndex == index) return;
    attachment.setValueAsCompleteGesture ((float) newIndex);
}

void ToneSwitch::setFromY (float y)
{
    const auto track = trackBounds();
    const float rel  = juce::jlimit (0.0f, 0.999f, (y - track.getY()) / track.getHeight());
    const int   row  = (int) (rel * 3.0f);
    setIndex (2 - row);
}

void ToneSwitch::mouseDown (const juce::MouseEvent& e)  { setFromY (e.position.y); }
void ToneSwitch::mouseDrag (const juce::MouseEvent& e)  { setFromY (e.position.y); }

void ToneSwitch::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (wheel.deltaY > 0.0f)      setIndex (index + 1);
    else if (wheel.deltaY < 0.0f) setIndex (index - 1);
}

//==============================================================================
namespace
{
    const juce::Colour vuFace      { 0xffe9dcbd };
    const juce::Colour vuFaceEdge  { 0xffcbbd9a };
    const juce::Colour vuInk       { 0xff2a2520 };
    const juce::Colour vuRed       { 0xffc0352b };
    constexpr float    kVuMinDb    = -20.0f;
    constexpr float    kVuMaxDb    =  3.0f;
    constexpr float    kVuHalfSpan = juce::MathConstants<float>::pi * 0.16f; // ±29°
}

VuMeter::VuMeter (const juce::String& c) : caption (c)
{
    setInterceptsMouseClicks (false, false);
}

void VuMeter::setLevel (float lin)
{
    // Calibrate: a sine whose peak is kZeroVuDbfs has mean-abs = (2/pi)*peak => 0 VU
    const float ref = (2.0f / juce::MathConstants<float>::pi) * juce::Decibels::decibelsToGain (kZeroVuDbfs);
    const float db  = juce::jlimit (-40.0f, 10.0f, juce::Decibels::gainToDecibels (lin / ref, -40.0f));
    if (std::abs (db - vuDb) > 0.05f) { vuDb = db; repaint(); }
}

void VuMeter::setPeakLit (bool b)
{
    if (b != peakLit) { peakLit = b; repaint(); }
}

float VuMeter::positionForDb (float db) const
{
    // A VU scale is linear in voltage, not dB.
    const float aMin = juce::Decibels::decibelsToGain (kVuMinDb);
    const float aMax = juce::Decibels::decibelsToGain (kVuMaxDb);
    const float a    = juce::Decibels::decibelsToGain (juce::jlimit (kVuMinDb - 6.0f, kVuMaxDb + 2.0f, db));
    return (a - aMin) / (aMax - aMin);
}

juce::Point<float> VuMeter::pivot() const
{
    const auto b = getLocalBounds().toFloat();
    return { b.getCentreX(), b.getBottom() + b.getHeight() * 0.55f };
}

float VuMeter::needleLength() const
{
    const auto b = getLocalBounds().toFloat();
    return b.getHeight() * 0.55f + b.getHeight() * 0.78f;
}

void VuMeter::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (1.0f);
    const auto face = b.reduced (0.0f, 0.0f);
    const auto piv  = pivot();
    const float len = needleLength();

    auto angleFor = [] (float p) { return -kVuHalfSpan + juce::jlimit (-0.03f, 1.03f, p) * 2.0f * kVuHalfSpan; };

    // bezel
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (face.expanded (3.0f).translated (0, 2), 7.0f);
    g.setColour (knobRim.darker (0.3f));
    g.fillRoundedRectangle (face.expanded (3.0f), 7.0f);

    // face
    juce::Path faceClip; faceClip.addRoundedRectangle (face, 5.0f);
    g.saveState();
    g.reduceClipRegion (faceClip);
    {
        juce::ColourGradient grad (vuFace.brighter (0.05f), face.getCentreX(), face.getY(),
                                   vuFaceEdge, face.getCentreX(), face.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRect (face);
        // warm backlight glow in the centre
        juce::ColourGradient glow (juce::Colours::white.withAlpha (0.18f), face.getCentreX(), face.getCentreY() + 10,
                                   juce::Colours::transparentWhite, face.getX(), face.getY(), true);
        g.setGradientFill (glow);
        g.fillRect (face);
    }

    // arc scale
    const float arcR = len * 0.94f;
    {
        juce::Path blackArc, redArc;
        blackArc.addCentredArc (piv.x, piv.y, arcR, arcR, 0.0f, angleFor (0.0f), angleFor (positionForDb (0.0f)), true);
        redArc  .addCentredArc (piv.x, piv.y, arcR, arcR, 0.0f, angleFor (positionForDb (0.0f)), angleFor (1.0f), true);
        g.setColour (vuInk);
        g.strokePath (blackArc, juce::PathStrokeType (1.6f));
        g.setColour (vuRed);
        g.strokePath (redArc, juce::PathStrokeType (3.0f));
    }

    // ticks + labels
    {
        struct Tick { float db; const char* label; bool major; };
        const Tick ticks[] = { { -20, "20", true }, { -15, nullptr, false }, { -10, "10", true },
                               { -8, nullptr, false }, { -7, "7", true }, { -6, nullptr, false },
                               { -5, "5", true }, { -4, nullptr, false }, { -3, "3", true },
                               { -2, "2", true }, { -1, "1", true }, { 0, "0", true },
                               { 1, "1", true }, { 2, "2", true }, { 3, "3", true } };

        g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
        for (const auto& t : ticks)
        {
            const float a   = angleFor (positionForDb (t.db));
            const bool  red = t.db >= 0.0f;
            const float o   = arcR + (t.major ? 7.0f : 4.0f);
            g.setColour (red ? vuRed : vuInk);
            g.drawLine (juce::Line<float> (piv.getPointOnCircumference (arcR, a),
                                           piv.getPointOnCircumference (o, a)), t.major ? 1.4f : 1.0f);
            if (t.label != nullptr)
            {
                const auto lp = piv.getPointOnCircumference (arcR + 15.0f, a);
                g.drawText (juce::String (t.label), juce::Rectangle<float> (22, 12).withCentre (lp),
                            juce::Justification::centred);
            }
        }
        // "+" / "−" markers at the ends
        g.setColour (vuInk);
        g.drawText ("-", juce::Rectangle<float> (14, 12).withCentre (piv.getPointOnCircumference (arcR + 15.0f, angleFor (-0.06f))),
                    juce::Justification::centred);
        g.setColour (vuRed);
        g.drawText ("+", juce::Rectangle<float> (14, 12).withCentre (piv.getPointOnCircumference (arcR + 15.0f, angleFor (1.06f))),
                    juce::Justification::centred);
    }

    // legend
    g.setColour (vuInk.withAlpha (0.85f));
    g.setFont (captionFont (12.0f));
    g.drawText ("VU", face.withTrimmedBottom (face.getHeight() * 0.18f), juce::Justification::centredBottom);
    g.setFont (captionFont (9.0f));
    g.drawText (caption, face.reduced (8.0f, 5.0f), juce::Justification::bottomLeft);

    // needle
    {
        const float a = angleFor (positionForDb (vuDb));
        const auto tip = piv.getPointOnCircumference (len, a);
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.drawLine (juce::Line<float> (piv.translated (1.5f, 2.0f), tip.translated (1.5f, 2.0f)), 2.0f);
        g.setColour (vuInk);
        g.drawLine (juce::Line<float> (piv, tip), 1.6f);
    }

    // peak LED
    if (hasPeakLed)
    {
        const auto led = juce::Rectangle<float> (7.0f, 7.0f)
                             .withCentre ({ face.getRight() - 12.0f, face.getBottom() - 10.0f });
        if (peakLit)
        {
            g.setColour (vuRed.withAlpha (0.35f));
            g.fillEllipse (led.expanded (3.0f));
        }
        g.setColour (peakLit ? vuRed.brighter (0.2f) : vuRed.darker (0.7f).withAlpha (0.6f));
        g.fillEllipse (led);
        g.setColour (vuInk.withAlpha (0.7f));
        g.setFont (captionFont (8.0f));
        g.drawText ("PEAK", juce::Rectangle<float> (40.0f, 10.0f).withCentre (led.getCentre().translated (-28.0f, 0.0f)),
                    juce::Justification::centredRight);
    }
    g.restoreState();

    // glass highlight
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    juce::Path glass;
    glass.addRoundedRectangle (face.withHeight (face.getHeight() * 0.35f), 5.0f);
    g.fillPath (glass);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (face, 5.0f, 1.0f);
}

//==============================================================================
OxideAudioProcessorEditor::OxideAudioProcessorEditor (OxideAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      driveKnob  (*p.apvts.getParameter (ParamID::drive), "DRIVE"),
      trimKnob   (*p.apvts.getParameter (ParamID::trim),  "TRIM"),
      toneSwitch (*p.apvts.getParameter (ParamID::tone))
{
    setLookAndFeel (&lookAndFeel);
    outMeter.setHasPeakLed (true);
    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);
    addAndMakeVisible (driveKnob);
    addAndMakeVisible (trimKnob);
    addAndMakeVisible (toneSwitch);
    setSize (480, 400);
    startTimerHz (50);
}

OxideAudioProcessorEditor::~OxideAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void OxideAudioProcessorEditor::timerCallback()
{
    inMeter .setLevel (processor.inVuLevel .load (std::memory_order_relaxed));
    outMeter.setLevel (processor.outVuLevel.load (std::memory_order_relaxed));

    const float pk = processor.outPeak.exchange (0.0f, std::memory_order_relaxed);
    if (pk >= juce::Decibels::decibelsToGain (-0.3f))
        peakHoldTicks = 40;                      // ~0.8 s hold
    else if (peakHoldTicks > 0)
        --peakHoldTicks;
    outMeter.setPeakLit (peakHoldTicks > 0);
}

void OxideAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // faceplate
    g.setGradientFill (juce::ColourGradient (faceLight, b.getCentreX(), b.getY(),
                                             faceDark,  b.getCentreX(), b.getBottom(), false));
    g.fillAll();

    // subtle brushed texture
    g.setColour (juce::Colours::white.withAlpha (0.018f));
    for (int y = 0; y < getHeight(); y += 3)
        g.drawHorizontalLine (y, 0.0f, (float) getWidth());

    // bevel
    g.setColour (bevel);
    g.drawRect (b.reduced (0.5f), 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawRect (b.reduced (1.5f), 1.0f);

    // corner screws
    const float s = 5.0f;
    for (auto c : { juce::Point<float> (14, 14), juce::Point<float> (b.getRight() - 14, 14),
                    juce::Point<float> (14, b.getBottom() - 14),
                    juce::Point<float> (b.getRight() - 14, b.getBottom() - 14) })
    {
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (juce::Rectangle<float> (s * 2 + 2, s * 2 + 2).withCentre (c.translated (0, 1)));
        g.setColour (knobRim.brighter (0.2f));
        g.fillEllipse (juce::Rectangle<float> (s * 2, s * 2).withCentre (c));
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawLine (c.x - 3, c.y - 3, c.x + 3, c.y + 3, 1.2f);
        g.drawLine (c.x - 3, c.y + 3, c.x + 3, c.y - 3, 1.2f);
    }

    // wordmark
    auto footer = getLocalBounds().removeFromBottom (34).reduced (28, 0);
    g.setColour (text);
    g.setFont (captionFont (15.0f));
    g.drawText ("OXIDE", footer, juce::Justification::centredLeft);
    g.setColour (textDim);
    g.setFont (readoutFont (11.5f));
    g.drawText ("tape saturator", footer, juce::Justification::centredRight);

    // accent hairline above footer
    g.setColour (accent.withAlpha (0.35f));
    g.drawHorizontalLine (getHeight() - 36, 28.0f, (float) getWidth() - 28.0f);
}

void OxideAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (24, 18);
    r.removeFromBottom (36);

    const int knobW = 180;
    auto meters = r.removeFromTop (118);
    inMeter .setBounds (meters.removeFromLeft  (knobW).reduced (6, 8));
    outMeter.setBounds (meters.removeFromRight (knobW).reduced (6, 8));
    r.removeFromTop (6);

    driveKnob.setBounds (r.removeFromLeft (knobW));
    trimKnob .setBounds (r.removeFromRight (knobW));
    toneSwitch.setBounds (r.withSizeKeepingCentre (110, 130));
}
