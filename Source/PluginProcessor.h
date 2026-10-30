#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "TapeSaturator.h"

namespace ParamID
{
    inline constexpr const char* drive = "drive";
    inline constexpr const char* trim  = "trim";
    inline constexpr const char* tone  = "tone";
}

class OxideAudioProcessor : public juce::AudioProcessor
{
public:
    OxideAudioProcessor();
    ~OxideAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // --- metering (read by the editor) --------------------------------------
    // VU-ballistic envelopes of the input and output (linear, mean-abs style),
    // and a sticky output peak the editor clears after displaying it.
    std::atomic<float> inVuLevel  { 0.0f };
    std::atomic<float> outVuLevel { 0.0f };
    std::atomic<float> outPeak    { 0.0f };

private:
    struct VuEnvelope
    {
        float coef = 0.0f, state = 0.0f;
        void prepare (double fs)
        {
            // Classic VU: 300 ms to 99 % => tau = 300 ms / ln(100)
            const double tau = 0.3 / std::log (100.0);
            coef  = (float) (1.0 - std::exp (-1.0 / (tau * fs)));
            state = 0.0f;
        }
        inline void push (float absSample) noexcept { state += (absSample - state) * coef; }
    };
    VuEnvelope inEnv, outEnv;
    void meterBlock (const juce::AudioBuffer<float>&, int numCh, VuEnvelope&, std::atomic<float>& out,
                     std::atomic<float>* peak);

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    static constexpr int kOversampleOrder = 2; // 4x

    std::atomic<float>* driveParam = nullptr;
    std::atomic<float>* trimParam  = nullptr;
    std::atomic<float>* toneParam  = nullptr;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> driveGain { 1.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> trimGain  { 1.0f };

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    std::vector<oxide::TapeChannel> channels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OxideAudioProcessor)
};
