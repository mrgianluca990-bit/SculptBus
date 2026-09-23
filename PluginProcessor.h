#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <memory>

class SculptBusAudioProcessor final : public juce::AudioProcessor
{
public:
    SculptBusAudioProcessor();
    ~SculptBusAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    float getResMeter (int band) const noexcept;
    float getCompMeter (int band) const noexcept;
    float getSatMeter (int band) const noexcept;
    float getInputMeter() const noexcept  { return inputMeter.load(); }
    float getOutputMeter() const noexcept { return outputMeter.load(); }

private:
    static constexpr int numMacroBands = 4;
    static constexpr int numResBands = 32;

    struct MacroBandState
    {
        juce::dsp::StateVariableTPTFilter<float> filter;
        juce::AudioBuffer<float> band;
        juce::AudioBuffer<float> originalBand;

        float compEnv = 0.0f;
        float compGain = 1.0f;
        float sootheGain = 1.0f;
    };

    struct ResDetectorState
    {
        juce::dsp::StateVariableTPTFilter<float> filter;
        juce::AudioBuffer<float> work;
        float slowEnergy = 0.0f;
    };

    juce::AudioProcessorValueTreeState apvts;

    std::array<MacroBandState, numMacroBands> macroBands;
    std::array<ResDetectorState, numResBands> resDetectors;

    std::array<std::array<float, numMacroBands>, numResBands> detectorWeights {};
    std::array<float, numResBands> variationSensitivity {};

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    double baseSampleRate = 44100.0;
    double internalSampleRate = 176400.0;
    int maxInternalBlockSize = 2048;

    juce::SmoothedValue<float> outputGain;

    std::array<float, numMacroBands> soothePressure {};

    std::array<std::atomic<float>, numMacroBands> resMeters;
    std::array<std::atomic<float>, numMacroBands> compMeters;
    std::array<std::atomic<float>, numMacroBands> satMeters;

    std::atomic<float> inputMeter  { 0.0f };
    std::atomic<float> outputMeter { 0.0f };

    static constexpr std::array<float, numResBands> resFrequencies
    {
        20.0f, 25.0f, 31.5f, 40.0f, 50.0f, 63.0f, 80.0f, 100.0f,
        125.0f, 160.0f, 200.0f, 250.0f, 315.0f, 400.0f, 500.0f, 630.0f,
        800.0f, 1000.0f, 1250.0f, 1600.0f, 2000.0f, 2500.0f, 3000.0f, 3500.0f,
        4000.0f, 5000.0f, 6300.0f, 8000.0f, 10000.0f, 12500.0f, 16000.0f, 20000.0f
    };

    void configureMacroFilters();
    void configureResDetectors();
    void computeDetectorWeights();

    void processInternal (juce::AudioBuffer<float>&);
    void analyseResonance (const juce::AudioBuffer<float>&,
                           const std::array<float, numMacroBands>& macros,
                           bool variationEnabled);

    void processMacroBand (juce::AudioBuffer<float>&,
                           int bandIndex,
                           float macroValue,
                           float sootheAmount);

    float saturate (float sample,
                    float satAmount,
                    int bandIndex) const noexcept;

    float getBlockRMS (const juce::AudioBuffer<float>&) const noexcept;
    float curve (float magnitude) const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SculptBusAudioProcessor)
};
