#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
OxideAudioProcessor::OxideAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    driveParam = apvts.getRawParameterValue (ParamID::drive);
    trimParam  = apvts.getRawParameterValue (ParamID::trim);
    toneParam  = apvts.getRawParameterValue (ParamID::tone);
}

juce::AudioProcessorValueTreeState::ParameterLayout OxideAudioProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::drive, 1 }, "Drive",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 25.0f,
        AudioParameterFloatAttributes().withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) { return String (v, 1); })));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::trim, 1 }, "Trim",
        NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int)
            {
                return (v > 0 ? "+" : "") + String (v, 1);
            })));

    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamID::tone, 1 }, "Tone",
        StringArray { "Normal", "Medium", "Bright" }, 0));

    return { params.begin(), params.end() };
}

//==============================================================================
bool OxideAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void OxideAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto numCh = (size_t) juce::jmax (1, getTotalNumInputChannels());

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) numCh, (size_t) kOversampleOrder,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    oversampler->initProcessing ((size_t) samplesPerBlock);
    oversampler->reset();

    const double osRate = sampleRate * (1 << kOversampleOrder);
    channels.assign (numCh, {});
    for (auto& c : channels)
        c.prepare (osRate);

    // Smoothing runs at the oversampled rate (we advance per OS sample).
    driveGain.reset (osRate, 0.02);
    trimGain .reset (osRate, 0.02);
    driveGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (
        oxide::TapeChannel::driveToDb (driveParam->load() * 0.01f)));
    trimGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (trimParam->load()));

    setLatencySamples ((int) std::lround (oversampler->getLatencyInSamples()));

    inEnv.prepare (sampleRate);
    outEnv.prepare (sampleRate);
    inVuLevel.store (0.0f); outVuLevel.store (0.0f); outPeak.store (0.0f);
}

void OxideAudioProcessor::meterBlock (const juce::AudioBuffer<float>& buffer, int numCh,
                                      VuEnvelope& env, std::atomic<float>& out,
                                      std::atomic<float>* peak)
{
    const int n = buffer.getNumSamples();
    float pk = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        float a = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
            a = juce::jmax (a, std::abs (buffer.getReadPointer (ch)[i]));
        env.push (a);
        pk = juce::jmax (pk, a);
    }
    out.store (env.state, std::memory_order_relaxed);
    if (peak != nullptr)
    {
        // sticky max: the editor resets it to 0 after reading
        float cur = peak->load (std::memory_order_relaxed);
        while (pk > cur && ! peak->compare_exchange_weak (cur, pk, std::memory_order_relaxed)) {}
    }
}

void OxideAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (buffer.getNumChannels(), (int) channels.size());
    for (int ch = numCh; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());
    if (numCh == 0 || oversampler == nullptr) return;

    meterBlock (buffer, numCh, inEnv, inVuLevel, nullptr);

    driveGain.setTargetValue (juce::Decibels::decibelsToGain (
        oxide::TapeChannel::driveToDb (driveParam->load() * 0.01f)));
    trimGain.setTargetValue (juce::Decibels::decibelsToGain (trimParam->load()));

    const auto tone = static_cast<oxide::Tone> ((int) std::lround (toneParam->load()));
    for (auto& c : channels) c.setTone (tone);

    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh,
                                        (size_t) buffer.getNumSamples());
    auto osBlock = oversampler->processSamplesUp (block);

    const int osSamples = (int) osBlock.getNumSamples();
    for (int i = 0; i < osSamples; ++i)
    {
        const float dg = driveGain.getNextValue();
        const float tg = trimGain.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* d = osBlock.getChannelPointer ((size_t) ch);
            d[i] = channels[(size_t) ch].process (d[i], dg, tg);
        }
    }

    oversampler->processSamplesDown (block);

    meterBlock (buffer, numCh, outEnv, outVuLevel, &outPeak);
}

//==============================================================================
void OxideAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void OxideAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* OxideAudioProcessor::createEditor()
{
    return new OxideAudioProcessorEditor (*this);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OxideAudioProcessor();
}
