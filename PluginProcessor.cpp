#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

SculptBusAudioProcessor::SculptBusAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    for (auto& m : resMeters)  m.store (0.0f);
    for (auto& m : compMeters) m.store (0.0f);
    for (auto& m : satMeters)  m.store (0.0f);
}

juce::AudioProcessorValueTreeState::ParameterLayout
SculptBusAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto addMacro = [&params] (const char* id, const char* name)
    {
        params.push_back (std::make_unique<juce::AudioParameterFloat>(
            id, name,
            juce::NormalisableRange<float> { -100.0f, 100.0f, 0.1f },
            0.0f, "%"));
    };

    addMacro ("low",      "Low");
    addMacro ("mid",      "Mid");
    addMacro ("high",     "High");
    addMacro ("presence", "Presence");

    params.push_back (std::make_unique<juce::AudioParameterBool>(
        "variation", "Variation", false));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "density", "Density",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f },
        0.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "body", "Body",
        juce::NormalisableRange<float> { -100.0f, 100.0f, 0.1f },
        0.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "detail", "Detail",
        juce::NormalisableRange<float> { -100.0f, 100.0f, 0.1f },
        0.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "glue", "Glue",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f },
        0.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "punch", "Punch",
        juce::NormalisableRange<float> { -100.0f, 100.0f, 0.1f },
        0.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "space", "Space",
        juce::NormalisableRange<float> { -100.0f, 100.0f, 0.1f },
        0.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "mix", "Mix",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f },
        100.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "output", "Output",
        juce::NormalisableRange<float> { -18.0f, 6.0f, 0.01f },
        0.0f, "dB"));

    return { params.begin(), params.end() };
}

bool SculptBusAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (in != out)
        return false;

    return out == juce::AudioChannelSet::mono()
        || out == juce::AudioChannelSet::stereo();
}

void SculptBusAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    baseSampleRate = sampleRate;

    const auto channels = static_cast<size_t> (
        juce::jmax (1, getTotalNumOutputChannels()));

    oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        channels,
        2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true,
        true);

    oversampler->initProcessing (static_cast<size_t> (samplesPerBlock));
    oversampler->reset();

    setLatencySamples (
        static_cast<int> (
            std::round (oversampler->getLatencyInSamples())));

    internalSampleRate = sampleRate * 4.0;
    maxInternalBlockSize = samplesPerBlock * 4 + 64;

    juce::dsp::ProcessSpec spec {
        internalSampleRate,
        static_cast<juce::uint32> (maxInternalBlockSize),
        static_cast<juce::uint32> (channels)
    };

    for (auto& band : macroBands)
    {
        band.filter.prepare (spec);
        band.filter.reset();

        band.band.setSize (
            static_cast<int> (channels),
            maxInternalBlockSize,
            false, false, true);

        band.originalBand.setSize (
            static_cast<int> (channels),
            maxInternalBlockSize,
            false, false, true);

        band.compEnv = 0.0f;
        band.compGain = 1.0f;
        band.sootheGain = 1.0f;
    }

    for (auto& detector : resDetectors)
    {
        detector.filter.prepare (spec);
        detector.filter.reset();

        detector.work.setSize (
            static_cast<int> (channels),
            maxInternalBlockSize,
            false, false, true);

        detector.slowEnergy = 0.0f;
    }

    bodyFilter.filter.prepare (spec);
    bodyFilter.filter.reset();
    bodyFilter.filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    bodyFilter.filter.setCutoffFrequency (170.0f);
    bodyFilter.filter.setResonance (0.52f);
    bodyFilter.work.setSize ((int) channels, maxInternalBlockSize, false, false, true);

    detailFilter.filter.prepare (spec);
    detailFilter.filter.reset();
    detailFilter.filter.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    detailFilter.filter.setCutoffFrequency (5200.0f);
    detailFilter.filter.setResonance (0.52f);
    detailFilter.work.setSize ((int) channels, maxInternalBlockSize, false, false, true);

    fineDry.setSize ((int) channels, maxInternalBlockSize, false, false, true);
    busCompEnv = 0.0f;
    busCompGain = 1.0f;
    punchFastEnv = 0.0f;
    punchSlowEnv = 0.0f;
    busCompMeter.store (0.0f);

    outputGain.reset (sampleRate, 0.04);
    outputGain.setCurrentAndTargetValue (1.0f);

    soothePressure.fill (0.0f);

    configureMacroFilters();
    configureResDetectors();
    computeDetectorWeights();
}

void SculptBusAudioProcessor::configureMacroFilters()
{
    auto setup = [] (auto& filter,
                     juce::dsp::StateVariableTPTFilterType type,
                     float frequency,
                     float q)
    {
        filter.setType (type);
        filter.setCutoffFrequency (frequency);
        filter.setResonance (q);
    };

    setup (macroBands[0].filter,
           juce::dsp::StateVariableTPTFilterType::lowpass,
           220.0f, 0.50f);

    setup (macroBands[1].filter,
           juce::dsp::StateVariableTPTFilterType::bandpass,
           760.0f, 0.58f);

    setup (macroBands[2].filter,
           juce::dsp::StateVariableTPTFilterType::bandpass,
           3300.0f, 0.56f);

    setup (macroBands[3].filter,
           juce::dsp::StateVariableTPTFilterType::highpass,
           6200.0f, 0.52f);
}

void SculptBusAudioProcessor::configureResDetectors()
{
    constexpr float detectorQ = 3.1f;

    for (int i = 0; i < numResBands; ++i)
    {
        auto& f = resDetectors[static_cast<size_t> (i)].filter;

        f.setType (juce::dsp::StateVariableTPTFilterType::bandpass);

        f.setCutoffFrequency (
            juce::jmin (
                resFrequencies[static_cast<size_t> (i)],
                static_cast<float> (internalSampleRate * 0.44)));

        f.setResonance (detectorQ);
    }
}

void SculptBusAudioProcessor::computeDetectorWeights()
{
    constexpr std::array<float, numMacroBands> centres {
        95.0f, 700.0f, 3500.0f, 10000.0f
    };

    constexpr std::array<float, numMacroBands> widths {
        1.45f, 1.30f, 1.20f, 1.28f
    };

    constexpr std::array<float, 6> variationCentres {
        200.0f, 500.0f, 1000.0f, 3000.0f, 5000.0f, 8000.0f
    };

    for (int b = 0; b < numResBands; ++b)
    {
        const float f =
            resFrequencies[static_cast<size_t> (b)];

        float total = 0.0f;

        for (int m = 0; m < numMacroBands; ++m)
        {
            const float d =
                std::log2 (
                    f / centres[static_cast<size_t> (m)]);

            const float w =
                std::exp (
                    -0.5f
                    * (d * d)
                    / (widths[static_cast<size_t> (m)]
                       * widths[static_cast<size_t> (m)]));

            detectorWeights[static_cast<size_t> (b)][static_cast<size_t> (m)] = w;
            total += w;
        }

        if (total > 0.0001f)
        {
            for (int m = 0; m < numMacroBands; ++m)
                detectorWeights[static_cast<size_t> (b)][static_cast<size_t> (m)] /= total;
        }

        float focus = 0.0f;

        for (const auto target : variationCentres)
        {
            const float d =
                std::log2 (f / target);

            const float local =
                std::exp (
                    -0.5f
                    * (d * d)
                    / (0.20f * 0.20f));

            focus = juce::jmax (focus, local);
        }

        // VAR now only changes detector sensitivity.
        // It does NOT insert or subtract any additional filter from the audio path.
        variationSensitivity[static_cast<size_t> (b)] =
            1.0f + 0.30f * focus;
    }
}

float SculptBusAudioProcessor::curve (float magnitude) const noexcept
{
    magnitude = juce::jlimit (0.0f, 1.0f, magnitude);

    return juce::jlimit (
        0.0f, 1.0f,
        0.46f * magnitude
        + 0.54f * std::pow (magnitude, 0.82f));
}

float SculptBusAudioProcessor::getBlockRMS (
    const juce::AudioBuffer<float>& buffer) const noexcept
{
    if (buffer.getNumChannels() <= 0 || buffer.getNumSamples() <= 0)
        return 0.0f;

    double sum = 0.0;
    const double count =
        static_cast<double> (
            buffer.getNumChannels()
            * buffer.getNumSamples());

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const auto* p = buffer.getReadPointer (ch);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const double s = p[i];
            sum += s * s;
        }
    }

    return static_cast<float> (
        std::sqrt (
            sum / juce::jmax (1.0, count)));
}

void SculptBusAudioProcessor::analyseResonance (
    const juce::AudioBuffer<float>& source,
    const std::array<float, numMacroBands>& macros,
    bool variationEnabled)
{
    std::array<float, numResBands> bandDb {};
    std::array<float, numMacroBands> pressure {};
    pressure.fill (0.0f);

    for (int b = 0; b < numResBands; ++b)
    {
        auto& detector =
            resDetectors[static_cast<size_t> (b)];

        detector.work.setSize (
            source.getNumChannels(),
            source.getNumSamples(),
            false, false, true);

        detector.work.makeCopyOf (
            source, true);

        juce::dsp::AudioBlock<float> block (
            detector.work);

        juce::dsp::ProcessContextReplacing<float> context (
            block);

        detector.filter.process (
            context);

        const float rms =
            getBlockRMS (
                detector.work);

        bandDb[static_cast<size_t> (b)] =
            juce::Decibels::gainToDecibels (
                rms + 1.0e-8f,
                -120.0f);

        const float blockSeconds =
            static_cast<float> (
                source.getNumSamples()
                / internalSampleRate);

        const float tc =
            juce::jmap (
                resFrequencies[static_cast<size_t> (b)],
                20.0f, 20000.0f,
                0.34f, 0.13f);

        const float coeff =
            std::exp (
                -blockSeconds
                / juce::jmax (0.05f, tc));

        if (detector.slowEnergy <= 1.0e-7f)
            detector.slowEnergy = rms;
        else
            detector.slowEnergy =
                coeff * detector.slowEnergy
                + (1.0f - coeff) * rms;
    }

    for (int b = 0; b < numResBands; ++b)
    {
        float macroAmount = 0.0f;

        for (int m = 0; m < numMacroBands; ++m)
        {
            const float knob =
                std::abs (
                    macros[static_cast<size_t> (m)]
                    / 100.0f);

            macroAmount +=
                knob
                * detectorWeights[static_cast<size_t> (b)][static_cast<size_t> (m)];
        }

        const float activity =
            curve (
                juce::jlimit (0.0f, 1.0f, macroAmount));

        if (activity < 0.08f)
            continue;

        float neighbourDb = 0.0f;
        float weightSum = 0.0f;

        for (int off = -2; off <= 2; ++off)
        {
            if (off == 0)
                continue;

            const int n = b + off;

            if (n < 0 || n >= numResBands)
                continue;

            const float w =
                std::abs (off) == 1
                    ? 1.0f
                    : 0.55f;

            neighbourDb +=
                bandDb[static_cast<size_t> (n)]
                * w;

            weightSum += w;
        }

        if (weightSum > 0.0f)
            neighbourDb /= weightSum;
        else
            neighbourDb = bandDb[static_cast<size_t> (b)];

        const float slowDb =
            juce::Decibels::gainToDecibels (
                resDetectors[static_cast<size_t> (b)].slowEnergy + 1.0e-8f,
                -120.0f);

        const float varBoost =
            variationEnabled
                ? variationSensitivity[static_cast<size_t> (b)]
                : 1.0f;

        const float spectralExcess =
            juce::jmax (
                0.0f,
                bandDb[static_cast<size_t> (b)]
                - neighbourDb
                - (5.0f / varBoost));

        const float temporalExcess =
            juce::jmax (
                0.0f,
                bandDb[static_cast<size_t> (b)]
                - slowDb
                - (4.0f / varBoost));

        const float resonanceScore =
            juce::jlimit (
                0.0f, 1.0f,
                (spectralExcess * 0.16f
                 + temporalExcess * 0.07f)
                * activity
                * varBoost);

        for (int m = 0; m < numMacroBands; ++m)
        {
            pressure[static_cast<size_t> (m)] =
                juce::jmax (
                    pressure[static_cast<size_t> (m)],
                    resonanceScore
                    * detectorWeights[static_cast<size_t> (b)][static_cast<size_t> (m)]);
        }
    }

    // Smooth only four macro damping values.
    // This is why VAR can no longer create comb-filter artefacts.
    for (int m = 0; m < numMacroBands; ++m)
    {
        const float target =
            juce::jlimit (
                0.0f, 1.0f,
                pressure[static_cast<size_t> (m)]);

        soothePressure[static_cast<size_t> (m)] =
            0.82f * soothePressure[static_cast<size_t> (m)]
            + 0.18f * target;

        resMeters[static_cast<size_t> (m)].store (
            soothePressure[static_cast<size_t> (m)]);
    }
}

float SculptBusAudioProcessor::saturate (
    float sample,
    float satAmount,
    int bandIndex) const noexcept
{
    satAmount =
        juce::jlimit (
            0.0f, 1.0f,
            satAmount);

    float maxDrive = 3.2f;
    float maxBlend = 0.34f;
    float asym = 0.007f;

    switch (bandIndex)
    {
        case 0:
            maxDrive = 3.8f;
            maxBlend = 0.40f;
            asym = 0.018f;
            break;

        case 1:
            maxDrive = 3.6f;
            maxBlend = 0.38f;
            asym = 0.015f;
            break;

        case 2:
            maxDrive = 3.15f;
            maxBlend = 0.32f;
            asym = 0.010f;
            break;

        default:
            maxDrive = 2.8f;
            maxBlend = 0.27f;
            asym = 0.007f;
            break;
    }

    const float drive =
        1.0f
        + (maxDrive - 1.0f)
          * satAmount;

    const float biased =
        sample
        + asym
          * satAmount
          * sample * sample
          * (sample >= 0.0f ? 1.0f : -1.0f);

    const float stage1 =
        std::tanh (
            biased * drive);

    const float stage2 =
        std::tanh (
            stage1
            * (1.0f + 0.28f * satAmount));

    const float shaped =
        stage1
        + (stage2 - stage1)
          * (0.28f * satAmount);

    const float blend =
        maxBlend
        * satAmount;

    return sample
        + (shaped - sample)
          * blend;
}

void SculptBusAudioProcessor::processMacroBand (
    juce::AudioBuffer<float>& source,
    int bandIndex,
    float macroValue,
    float sootheAmount)
{
    auto& state =
        macroBands[static_cast<size_t> (bandIndex)];

    const float normalized =
        juce::jlimit (
            -1.0f, 1.0f,
            macroValue / 100.0f);

    const float positive =
        juce::jmax (0.0f, normalized);

    const float negative =
        juce::jmax (0.0f, -normalized);

    const float magnitude =
        std::abs (normalized);

    const float activity =
        curve (magnitude);

    state.band.setSize (
        source.getNumChannels(),
        source.getNumSamples(),
        false, false, true);

    state.band.makeCopyOf (
        source, true);

    juce::dsp::AudioBlock<float> bandBlock (
        state.band);

    juce::dsp::ProcessContextReplacing<float> filterContext (
        bandBlock);

    state.filter.process (
        filterContext);

    state.originalBand.setSize (
        source.getNumChannels(),
        source.getNumSamples(),
        false, false, true);

    state.originalBand.makeCopyOf (
        state.band, true);

    // EQ: still the first and most obvious sensation.
    const float eqDb =
        positive > 0.0f
            ? 6.2f * activity * positive
            : -7.0f * activity * negative;

    state.band.applyGain (
        juce::Decibels::decibelsToGain (
            eqDb));

    // SOOTHE GUARDRAIL:
    // only four smooth macro dampers, no narrow subtraction in the audio path.
    // max around 2.2 dB at very high detected resonance pressure.
    const float sootheDb =
        -1.35f
        * juce::jlimit (
            0.0f, 1.0f,
            sootheAmount);

    const float targetSootheGain =
        juce::Decibels::decibelsToGain (
            sootheDb);

    const float blockSeconds =
        static_cast<float> (
            source.getNumSamples()
            / internalSampleRate);

    const float sootheCoeff =
        std::exp (
            -blockSeconds / 0.24f);

    state.sootheGain =
        sootheCoeff * state.sootheGain
        + (1.0f - sootheCoeff)
          * targetSootheGain;

    state.band.applyGain (
        state.sootheGain);

    // BUS GLUE: slower, softer stereo-linked compression.
    const float compEntrance =
        juce::jlimit (
            0.0f, 1.0f,
            (positive - 0.18f) / 0.82f);

    const float compAmount =
        positive > 0.0f
            ? 0.26f * std::pow (compEntrance, 1.22f)
            : 0.10f * negative * activity;

    float attackMs = 35.0f;
    float releaseMs = 240.0f;

    switch (bandIndex)
    {
        case 0: attackMs = 55.0f; releaseMs = 320.0f; break;
        case 1: attackMs = 38.0f; releaseMs = 260.0f; break;
        case 2: attackMs = 28.0f; releaseMs = 210.0f; break;
        default: attackMs = 20.0f; releaseMs = 170.0f; break;
    }

    const float attackCoeff =
        std::exp (
            -1.0f
            / static_cast<float> (
                internalSampleRate
                * attackMs * 0.001));

    const float releaseCoeff =
        std::exp (
            -1.0f
            / static_cast<float> (
                internalSampleRate
                * releaseMs * 0.001));

    const float thresholdDb =
        positive > 0.0f
            ? juce::jmap (
                compAmount,
                0.0f, 0.26f,
                -1.0f, -5.0f)
            : -5.0f;

    const float ratio =
        positive > 0.0f
            ? juce::jmap (
                compAmount,
                0.0f, 0.26f,
                1.0f, 1.45f)
            : 1.20f;

    float maxGrDb = 0.0f;

    for (int i = 0; i < state.band.getNumSamples(); ++i)
    {
        float detector = 0.0f;

        for (int ch = 0; ch < state.band.getNumChannels(); ++ch)
        {
            detector =
                juce::jmax (
                    detector,
                    std::abs (
                        state.band.getSample (ch, i)));
        }

        const float envCoeff =
            detector > state.compEnv
                ? attackCoeff
                : releaseCoeff;

        state.compEnv =
            envCoeff * state.compEnv
            + (1.0f - envCoeff)
              * detector;

        const float envDb =
            juce::Decibels::gainToDecibels (
                state.compEnv + 1.0e-8f,
                -120.0f);

        const float overDb =
            juce::jmax (
                0.0f,
                envDb - thresholdDb);

        const float grDb =
            overDb
            * (1.0f
               - 1.0f / juce::jmax (1.0f, ratio));

        maxGrDb =
            juce::jmax (
                maxGrDb,
                grDb);

        const float targetGain =
            juce::Decibels::decibelsToGain (
                -grDb);

        const float gainCoeff =
            targetGain < state.compGain
                ? attackCoeff
                : releaseCoeff;

        state.compGain =
            gainCoeff * state.compGain
            + (1.0f - gainCoeff)
              * targetGain;

        for (int ch = 0; ch < state.band.getNumChannels(); ++ch)
        {
            state.band.setSample (
                ch, i,
                state.band.getSample (ch, i)
                * state.compGain);
        }
    }

    compMeters[static_cast<size_t> (bandIndex)].store (
        juce::jlimit (
            0.0f, 1.0f,
            maxGrDb / 8.0f));

    // BUS SAT: subtle; only becomes obvious near the top of the positive range.
    const float satEntrance =
        juce::jlimit (
            0.0f, 1.0f,
            (positive - 0.58f) / 0.42f);

    const float satAmount =
        std::pow (
            satEntrance,
            1.25f);

    float nonlinear = 0.0f;

    for (int ch = 0; ch < state.band.getNumChannels(); ++ch)
    {
        auto* data =
            state.band.getWritePointer (ch);

        for (int i = 0; i < state.band.getNumSamples(); ++i)
        {
            const float before = data[i];
            const float after =
                saturate (
                    before,
                    satAmount,
                    bandIndex);

            nonlinear +=
                std::abs (
                    after - before);

            data[i] = after;
        }
    }

    const float norm =
        static_cast<float> (
            juce::jmax (
                1,
                state.band.getNumSamples()
                * state.band.getNumChannels()));

    satMeters[static_cast<size_t> (bandIndex)].store (
        juce::jlimit (
            0.0f, 1.0f,
            nonlinear / norm * 8.0f));

    for (int ch = 0; ch < source.getNumChannels(); ++ch)
    {
        auto* dst =
            source.getWritePointer (ch);

        const auto* original =
            state.originalBand.getReadPointer (ch);

        const auto* processed =
            state.band.getReadPointer (ch);

        for (int i = 0; i < source.getNumSamples(); ++i)
        {
            dst[i] +=
                processed[i]
                - original[i];
        }
    }
}


void SculptBusAudioProcessor::processBusCompressor (juce::AudioBuffer<float>& buffer,
                                                    float glue,
                                                    float punch)
{
    glue = juce::jlimit (0.0f, 1.0f, glue);
    punch = juce::jlimit (-1.0f, 1.0f, punch);

    if (glue <= 0.0001f)
    {
        busCompMeter.store (0.0f);
        return;
    }

    // Stereo-linked classic bus-compressor style stage.
    // 30 ms attack keeps transients, release tightens progressively, ratio rises toward 4:1.
    const float attackMs = juce::jmap (punch, -1.0f, 1.0f, 10.0f, 30.0f);
    const float releaseMs = juce::jmap (glue, 0.0f, 1.0f, 260.0f, 95.0f);
    const float thresholdDb = juce::jmap (glue, 0.0f, 1.0f, -2.0f, -18.0f);
    const float ratio = juce::jmap (glue, 0.0f, 1.0f, 1.5f, 4.0f);

    const float attackCoeff = std::exp (-1.0f / (float) (internalSampleRate * attackMs * 0.001));
    const float releaseCoeff = std::exp (-1.0f / (float) (internalSampleRate * releaseMs * 0.001));

    float maxGrDb = 0.0f;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float detector = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            detector = juce::jmax (detector, std::abs (buffer.getSample (ch, i)));

        const float envCoeff = detector > busCompEnv ? attackCoeff : releaseCoeff;
        busCompEnv = envCoeff * busCompEnv + (1.0f - envCoeff) * detector;

        const float envDb = juce::Decibels::gainToDecibels (busCompEnv + 1.0e-8f, -120.0f);
        const float overDb = juce::jmax (0.0f, envDb - thresholdDb);
        const float grDb = overDb * (1.0f - 1.0f / ratio);
        maxGrDb = juce::jmax (maxGrDb, grDb);

        const float targetGain = juce::Decibels::decibelsToGain (-grDb);
        const float coeff = targetGain < busCompGain ? attackCoeff : releaseCoeff;
        busCompGain = coeff * busCompGain + (1.0f - coeff) * targetGain;

        // Musical partial makeup rather than level-match behaviour.
        const float makeup = juce::Decibels::decibelsToGain (2.8f * glue);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.setSample (ch, i, buffer.getSample (ch, i) * busCompGain * makeup);
    }

    busCompMeter.store (juce::jlimit (0.0f, 1.0f, maxGrDb / 10.0f));
}

void SculptBusAudioProcessor::processFineTune (juce::AudioBuffer<float>& wet,
                                               const juce::AudioBuffer<float>& postBandsDry)
{
    const float density = apvts.getRawParameterValue ("density")->load() / 100.0f;
    const float body = apvts.getRawParameterValue ("body")->load() / 100.0f;
    const float detail = apvts.getRawParameterValue ("detail")->load() / 100.0f;
    const float glue = apvts.getRawParameterValue ("glue")->load() / 100.0f;
    const float punch = apvts.getRawParameterValue ("punch")->load() / 100.0f;
    const float space = apvts.getRawParameterValue ("space")->load() / 100.0f;
    const float mix = apvts.getRawParameterValue ("mix")->load() / 100.0f;

    // BODY: broad but now clearly audible, up to about +/-5 dB in the low foundation.
    if (std::abs (body) > 0.001f)
    {
        bodyFilter.work.setSize (wet.getNumChannels(), wet.getNumSamples(), false, false, true);
        bodyFilter.work.makeCopyOf (wet, true);
        juce::dsp::AudioBlock<float> block (bodyFilter.work);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        bodyFilter.filter.process (ctx);
        const float g = juce::Decibels::decibelsToGain (5.0f * body) - 1.0f;
        for (int ch = 0; ch < wet.getNumChannels(); ++ch)
            wet.addFrom (ch, 0, bodyFilter.work, ch, 0, wet.getNumSamples(), g);
    }

    // DETAIL: up to about +/-5 dB of broad high-frequency contour.
    if (std::abs (detail) > 0.001f)
    {
        detailFilter.work.setSize (wet.getNumChannels(), wet.getNumSamples(), false, false, true);
        detailFilter.work.makeCopyOf (wet, true);
        juce::dsp::AudioBlock<float> block (detailFilter.work);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        detailFilter.filter.process (ctx);
        const float g = juce::Decibels::decibelsToGain (5.0f * detail) - 1.0f;
        for (int ch = 0; ch < wet.getNumChannels(); ++ch)
            wet.addFrom (ch, 0, detailFilter.work, ch, 0, wet.getNumSamples(), g);
    }

    // DENSITY: noticeably stronger post-band analogue-style thickening.
    if (density > 0.001f)
    {
        const float shaped = std::pow (density, 0.75f);
        const float drive = 1.0f + 4.0f * shaped;
        const float blend = 0.55f * shaped;
        for (int ch = 0; ch < wet.getNumChannels(); ++ch)
        {
            auto* p = wet.getWritePointer (ch);
            for (int i = 0; i < wet.getNumSamples(); ++i)
            {
                const float x = p[i];
                const float y1 = std::tanh (x * drive);
                const float y2 = std::tanh (y1 * (1.0f + 0.8f * shaped));
                const float y = y1 + (y2 - y1) * (0.45f * shaped);
                p[i] = x + (y - x) * blend;
            }
        }
    }

    // GLUE is the dedicated stereo-linked bus compressor.
    processBusCompressor (wet, glue, punch);

    // PUNCH: independent transient fine tuning, so it remains audible even at low Glue.
    if (std::abs (punch) > 0.001f)
    {
        const float fastCoeff = std::exp (-1.0f / (float) (internalSampleRate * 0.004));
        const float slowCoeff = std::exp (-1.0f / (float) (internalSampleRate * 0.045));
        const float amount = 0.42f * punch;

        for (int i = 0; i < wet.getNumSamples(); ++i)
        {
            float detector = 0.0f;
            for (int ch = 0; ch < wet.getNumChannels(); ++ch)
                detector = juce::jmax (detector, std::abs (wet.getSample (ch, i)));

            punchFastEnv = fastCoeff * punchFastEnv + (1.0f - fastCoeff) * detector;
            punchSlowEnv = slowCoeff * punchSlowEnv + (1.0f - slowCoeff) * detector;

            const float transient = juce::jlimit (-1.0f, 1.0f,
                (punchFastEnv - punchSlowEnv) / (punchSlowEnv + 0.02f));
            const float gain = juce::jlimit (0.70f, 1.35f, 1.0f + transient * amount);

            for (int ch = 0; ch < wet.getNumChannels(); ++ch)
                wet.setSample (ch, i, wet.getSample (ch, i) * gain);
        }
    }

    // SPACE: now useful over a wider but still bus-safe range, +/-25% side level.
    if (wet.getNumChannels() >= 2 && std::abs (space) > 0.001f)
    {
        const float width = 1.0f + 0.25f * space;
        auto* l = wet.getWritePointer (0);
        auto* r = wet.getWritePointer (1);
        for (int i = 0; i < wet.getNumSamples(); ++i)
        {
            const float mid = 0.5f * (l[i] + r[i]);
            const float side = 0.5f * (l[i] - r[i]) * width;
            l[i] = mid + side;
            r[i] = mid - side;
        }
    }

    // MIX is now strictly POST-BANDS: 0% = four-band result, 100% = four-band result + Fine Tune.
    if (mix < 0.999f)
    {
        const float dryGain = 1.0f - mix;
        for (int ch = 0; ch < wet.getNumChannels(); ++ch)
        {
            auto* w = wet.getWritePointer (ch);
            const auto* d = postBandsDry.getReadPointer (ch);
            for (int i = 0; i < wet.getNumSamples(); ++i)
                w[i] = w[i] * mix + d[i] * dryGain;
        }
    }
}

void SculptBusAudioProcessor::processInternal (
    juce::AudioBuffer<float>& buffer)
{
    const std::array<float, numMacroBands> macros {
        apvts.getRawParameterValue ("low")->load(),
        apvts.getRawParameterValue ("mid")->load(),
        apvts.getRawParameterValue ("high")->load(),
        apvts.getRawParameterValue ("presence")->load()
    };

    const bool variationEnabled =
        apvts.getRawParameterValue ("variation")->load() > 0.5f;

    analyseResonance (buffer, macros, variationEnabled);

    // The four-band engine is intentionally left untouched.
    for (int band = 0; band < numMacroBands; ++band)
    {
        processMacroBand (
            buffer,
            band,
            macros[static_cast<size_t> (band)],
            soothePressure[static_cast<size_t> (band)]);
    }

    // Fine Tune starts HERE, after the approved four-band sound.
    fineDry.setSize (buffer.getNumChannels(), buffer.getNumSamples(), false, false, true);
    fineDry.makeCopyOf (buffer, true);
    processFineTune (buffer, fineDry);
}

void SculptBusAudioProcessor::processBlock (
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    float inPeak = 0.0f;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        inPeak =
            juce::jmax (
                inPeak,
                buffer.getMagnitude (
                    ch, 0,
                    buffer.getNumSamples()));
    }

    inputMeter.store (
        juce::jlimit (
            0.0f, 1.0f,
            inPeak));

    if (oversampler != nullptr)
    {
        juce::dsp::AudioBlock<float> baseBlock (
            buffer);

        auto upBlock =
            oversampler->processSamplesUp (
                baseBlock);

        std::array<float*, 2> pointers {
            nullptr, nullptr
        };

        for (size_t ch = 0;
             ch < upBlock.getNumChannels()
             && ch < pointers.size();
             ++ch)
        {
            pointers[ch] =
                upBlock.getChannelPointer (ch);
        }

        juce::AudioBuffer<float> internalBuffer (
            pointers.data(),
            static_cast<int> (
                upBlock.getNumChannels()),
            static_cast<int> (
                upBlock.getNumSamples()));

        processInternal (
            internalBuffer);

        oversampler->processSamplesDown (
            baseBlock);
    }
    else
    {
        processInternal (
            buffer);
    }

    const float outputDb =
        apvts.getRawParameterValue ("output")->load();

    outputGain.setTargetValue (
        juce::Decibels::decibelsToGain (
            outputDb));

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float g =
            outputGain.getNextValue();

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            buffer.setSample (
                ch, i,
                buffer.getSample (ch, i)
                * g);
        }
    }

    float outPeak = 0.0f;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        outPeak =
            juce::jmax (
                outPeak,
                buffer.getMagnitude (
                    ch, 0,
                    buffer.getNumSamples()));
    }

    outputMeter.store (
        juce::jlimit (
            0.0f, 1.0f,
            outPeak));
}

float SculptBusAudioProcessor::getResMeter (int band) const noexcept
{
    return resMeters[
        static_cast<size_t> (
            juce::jlimit (
                0, numMacroBands - 1,
                band))].load();
}

float SculptBusAudioProcessor::getCompMeter (int band) const noexcept
{
    return compMeters[
        static_cast<size_t> (
            juce::jlimit (
                0, numMacroBands - 1,
                band))].load();
}

float SculptBusAudioProcessor::getSatMeter (int band) const noexcept
{
    return satMeters[
        static_cast<size_t> (
            juce::jlimit (
                0, numMacroBands - 1,
                band))].load();
}

void SculptBusAudioProcessor::getStateInformation (
    juce::MemoryBlock& destData)
{
    if (auto xml =
        apvts.copyState().createXml())
    {
        copyXmlToBinary (
            *xml, destData);
    }
}

void SculptBusAudioProcessor::setStateInformation (
    const void* data,
    int sizeInBytes)
{
    if (auto xml =
        getXmlFromBinary (
            data,
            sizeInBytes))
    {
        if (xml->hasTagName (
                apvts.state.getType()))
        {
            apvts.replaceState (
                juce::ValueTree::fromXml (
                    *xml));
        }
    }
}

juce::AudioProcessorEditor*
SculptBusAudioProcessor::createEditor()
{
    return new SculptBusAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SculptBusAudioProcessor();
}
