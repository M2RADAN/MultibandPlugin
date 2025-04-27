#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <vector>
#include <memory> // Include for std::unique_ptr

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

//==============================================================================
// --- ОБНОВЛЕНО: Используем Attributes для создания параметров ---
juce::AudioProcessorValueTreeState::ParameterLayout MBRPAudioProcessor::createParameterLayout()
{
    APVTS::ParameterLayout layout;
    using namespace juce;

    // Define ParameterIDs (use constants or constexpr for better safety)
    const ParameterID lowMidCrossoverParamID{ "lowMidCrossover", 1 };
    const ParameterID midHighCrossoverParamID{ "midHighCrossover", 1 };
    const ParameterID lowPanParamID{ "lowPan", 1 };
    const ParameterID midPanParamID{ "midPan", 1 };
    const ParameterID highPanParamID{ "highPan", 1 };

    // Define ranges
    auto lowMidRange = NormalisableRange<float>(20.0f, 2000.0f, 1.0f, 0.25f);
    auto midHighRange = NormalisableRange<float>(500.0f, 20000.0f, 1.0f, 0.25f);
    auto panRange = NormalisableRange<float>(-1.0f, 1.0f, 0.01f);

    // Text conversion functions remain mostly the same
    auto freqValueToText = [](float v, int /*maxLength*/) { return juce::String(v, 0) + " Hz"; };
    auto kHzValueToText = [](float v, int /*maxLength*/) {
        return (v < 1000.f) ? juce::String(v, 0) + " Hz"
            : juce::String(v / 1000.f, 1) + " kHz";
        };
    auto panValueToText = [](float v, int /*maxLength*/) {
        if (std::abs(v) < 0.01f) return String("C");
        float percentage = std::abs(v) * 100.0f;
        if (v < 0.0f) return String("L") + String(percentage, 0);
        return String("R") + String(percentage, 0);
        };

    // --- ИЗМЕНЕНО: Лямбда panTextToValue ---
    auto panTextToValue = [](const String& text) {
        if (text.compareIgnoreCase("C") == 0 || text.isEmpty()) return 0.0f;
        if (text.startsWithIgnoreCase("L")) return juce::jlimit(-1.0f, 0.0f, -text.substring(1).getFloatValue() / 100.0f);
        if (text.startsWithIgnoreCase("R")) return juce::jlimit(0.0f, 1.0f, text.substring(1).getFloatValue() / 100.0f);

        // --- ИЗМЕНЕНО: Убираем аргумент у getFloatValue ---
        // Попытка преобразовать строку напрямую во float
        float val = text.getFloatValue();
        // getFloatValue вернет 0.0, если не удалось преобразовать.
        // Мы можем просто ограничить результат, т.к. 0.0 ("C") - валидное значение.
        // Для более строгой проверки можно добавить .containsOnlyChars(...) перед getFloatValue
        return juce::jlimit(-1.0f, 1.0f, val);
        // -----------------------------------------------
        };
    // --- Конец изменений в panTextToValue ---


    // Add parameters using the JUCE 6 constructor style
    layout.add(std::make_unique<AudioParameterFloat>(
        lowMidCrossoverParamID, "Low-Mid Freq", lowMidRange, 500.0f,
        "Hz", AudioProcessorParameter::genericParameter, freqValueToText, nullptr
    ));

    layout.add(std::make_unique<AudioParameterFloat>(
        midHighCrossoverParamID, "Mid-High Freq", midHighRange, 2000.0f,
        "Hz/kHz", AudioProcessorParameter::genericParameter, kHzValueToText, nullptr
    ));

    layout.add(std::make_unique<AudioParameterFloat>(
        lowPanParamID, "Low Pan", panRange, 0.0f,
        "%L/R", AudioProcessorParameter::genericParameter, panValueToText, panTextToValue
    ));

    layout.add(std::make_unique<AudioParameterFloat>(
        midPanParamID, "Mid Pan", panRange, 0.0f,
        "%L/R", AudioProcessorParameter::genericParameter, panValueToText, panTextToValue
    ));

    layout.add(std::make_unique<AudioParameterFloat>(
        highPanParamID, "High Pan", panRange, 0.0f,
        "%L/R", AudioProcessorParameter::genericParameter, panValueToText, panTextToValue
    ));

    return layout;
}
// -------------------------------------------------------

MBRPAudioProcessor::MBRPAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    ),
#else
    : // Constructor initializer list starts here if JucePlugin_PreferredChannelConfigurations is defined
#endif
    // Initialize FFT and Window here
forwardFFT(fftOrder),
window(fftSize, juce::dsp::WindowingFunction<float>::blackmanHarris)
{
    // Initialize APVTS using std::unique_ptr in the constructor body
    apvts = std::make_unique<juce::AudioProcessorValueTreeState>(*this, nullptr, "Parameters", createParameterLayout());

    // Get parameter pointers AFTER apvts is initialized
    lowMidCrossover = dynamic_cast<juce::AudioParameterFloat*>(apvts->getParameter("lowMidCrossover"));
    midHighCrossover = dynamic_cast<juce::AudioParameterFloat*>(apvts->getParameter("midHighCrossover"));
    jassert(lowMidCrossover != nullptr && midHighCrossover != nullptr); // Verify pointers

    // Initialize FFT buffers
    fftInputData.fill(0.0f);
    fftInternalBuffer.fill(0.0f);
}

MBRPAudioProcessor::~MBRPAudioProcessor()
{
    // No manual cleanup needed for std::unique_ptr or std::array
}

//==============================================================================
const juce::String MBRPAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool MBRPAudioProcessor::acceptsMidi() const { return false; }
bool MBRPAudioProcessor::producesMidi() const { return false; }
bool MBRPAudioProcessor::isMidiEffect() const { return false; }
double MBRPAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int MBRPAudioProcessor::getNumPrograms() { return 1; }
int MBRPAudioProcessor::getCurrentProgram() { return 0; }
void MBRPAudioProcessor::setCurrentProgram(int index) { juce::ignoreUnused(index); }
const juce::String MBRPAudioProcessor::getProgramName(int index) { juce::ignoreUnused(index); return {}; }
void MBRPAudioProcessor::changeProgramName(int index, const juce::String& newName) { juce::ignoreUnused(index, newName); }

//==============================================================================
void MBRPAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    lastSampleRate = static_cast<float>(sampleRate); // Store sample rate

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock); // Use uint32
    spec.numChannels = 1; // Filters process mono

    // Prepare filters
    leftLowMidLPF.prepare(spec); rightLowMidLPF.prepare(spec);
    leftMidHighLPF.prepare(spec); rightMidHighLPF.prepare(spec);

    // Reset filters
    leftLowMidLPF.reset(); rightLowMidLPF.reset();
    leftMidHighLPF.reset(); rightMidHighLPF.reset();

    // Resize intermediate buffers
    int numOutputChannels = getTotalNumOutputChannels();
    intermediateBuffer1.setSize(numOutputChannels, samplesPerBlock, false, true, true);
    lowBandBuffer.setSize(numOutputChannels, samplesPerBlock, false, true, true);
    midBandBuffer.setSize(numOutputChannels, samplesPerBlock, false, true, true);
    highBandBuffer.setSize(numOutputChannels, samplesPerBlock, false, true, true);

    // Reset FFT data flag
    isFftDataReady = false;

    // Update parameters based on current state (important after potential state load)
    updateParameters();
    DBG("Processor Prepared. Sample Rate: " << sampleRate << ", Block Size: " << samplesPerBlock);
}

void MBRPAudioProcessor::releaseResources()
{
    // Usually nothing needed here for this kind of plugin
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool MBRPAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Require stereo input and output
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo() ||
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}
#endif

void MBRPAudioProcessor::updateParameters()
{
    // Get current parameter values
    float lowMidFreq = lowMidCrossover->get();
    float midHighFreq = midHighCrossover->get();

    // Atomically load raw parameter values for panning
    float lowPanParam = apvts->getRawParameterValue("lowPan")->load();
    float midPanParam = apvts->getRawParameterValue("midPan")->load();
    float highPanParam = apvts->getRawParameterValue("highPan")->load();

    // Ensure crossover frequencies are in order
    midHighFreq = std::max(midHighFreq, lowMidFreq + 1.0f); // Mid/High must be > Low/Mid

    // Update filter cutoff frequencies
    leftLowMidLPF.setCutoffFrequency(lowMidFreq);
    rightLowMidLPF.setCutoffFrequency(lowMidFreq);
    leftMidHighLPF.setCutoffFrequency(midHighFreq);
    rightMidHighLPF.setCutoffFrequency(midHighFreq);

    // Calculate constant power panning gains
    auto calculatePanGains = [](float panParam, std::atomic<float>& leftGain, std::atomic<float>& rightGain)
        {
            constexpr float piOverTwo = static_cast<float>(M_PI) * 0.5f;
            // Map panParam [-1, 1] to angle [0, pi/2]
            float angle = (panParam * 0.5f + 0.5f) * piOverTwo;
            // Use atomic store for thread safety
            leftGain.store(std::cos(angle));
            rightGain.store(std::sin(angle));
        };
    calculatePanGains(lowPanParam, leftLowGain, rightLowGain);
    calculatePanGains(midPanParam, leftMidGain, rightMidGain);
    calculatePanGains(highPanParam, leftHighGain, rightHighGain);
}

// --- FFT Processing Function ---
void MBRPAudioProcessor::processFFT(const juce::AudioBuffer<float>& inputBuffer)
{
    // 1. Copy samples from the input buffer (e.g., left channel) into the internal FFT buffer.
    //    We need fftSize samples.
    auto numInputSamples = inputBuffer.getNumSamples();
    auto samplesToCopy = std::min(numInputSamples, fftSize);
    const float* leftChannelData = inputBuffer.getReadPointer(0); // Use left channel for analysis

    // Zero out the beginning of the internal buffer
    std::fill(fftInternalBuffer.begin(), fftInternalBuffer.begin() + fftSize, 0.0f);
    // Copy available samples
    std::copy(leftChannelData, leftChannelData + samplesToCopy, fftInternalBuffer.begin());

    // 2. Apply the window function to the data in the internal buffer
    window.multiplyWithWindowingTable(fftInternalBuffer.data(), fftSize);

    // 3. Perform the FFT. The result (complex numbers) will be in fftInternalBuffer.
    forwardFFT.performRealOnlyForwardTransform(fftInternalBuffer.data(), true);
    // 'true' means the result layout is [Re(0), Re(N/2), Re(1), Im(1), Re(2), Im(2), ..., Re(N/2-1), Im(N/2-1)]

    // 4. Calculate magnitudes and store them in fftInputData
    int numBins = fftSize / 2;
    const float* fftResult = fftInternalBuffer.data();

    // Magnitude for DC (bin 0) - use absolute value
    fftInputData[0] = std::abs(fftResult[0]);

    // Magnitudes for other bins
    for (int i = 1; i < numBins; ++i)
    {
        // Indices for real and imaginary parts in the FFT result
        float realPart = fftResult[size_t(2 * i)];     // size_t cast for safety
        float imagPart = fftResult[size_t(2 * i + 1)]; // size_t cast for safety
        fftInputData[i] = std::sqrt(realPart * realPart + imagPart * imagPart);
    }

    // 5. Set the flag indicating that FFT data is ready
    isFftDataReady = true;
}
// -------------------------------------

void MBRPAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    // Mark MIDI buffer as unused to avoid compiler warnings
    juce::ignoreUnused(midiMessages);

    juce::ScopedNoDenormals noDenormals; // Prevent denormal numbers
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    auto numSamples = buffer.getNumSamples();

    // Assert stereo configuration
    jassert(totalNumInputChannels == 2 && totalNumOutputChannels == 2);
    if (totalNumInputChannels != 2 || totalNumOutputChannels != 2) return; // Early exit if not stereo

    // Update filter and pan parameters based on GUI controls/state
    updateParameters();

    // Process FFT using the input buffer *before* processing/filtering
    processFFT(buffer);

    // --- DSP Processing: Band Splitting using Subtraction Method ---
    auto inputBlock = juce::dsp::AudioBlock<float>(buffer);
    auto leftInputBlock = inputBlock.getSingleChannelBlock(0);
    auto rightInputBlock = inputBlock.getSingleChannelBlock(1);

    auto lowBandBlockOut = juce::dsp::AudioBlock<float>(lowBandBuffer);
    auto midBandBlockOut = juce::dsp::AudioBlock<float>(midBandBuffer);
    auto highBandBlockOut = juce::dsp::AudioBlock<float>(highBandBuffer);
    auto leftLowBlockOut = lowBandBlockOut.getSingleChannelBlock(0);
    auto rightLowBlockOut = lowBandBlockOut.getSingleChannelBlock(1);
    auto leftMidBlockOut = midBandBlockOut.getSingleChannelBlock(0);
    auto rightMidBlockOut = midBandBlockOut.getSingleChannelBlock(1);
    auto leftHighBlockOut = highBandBlockOut.getSingleChannelBlock(0);
    auto rightHighBlockOut = highBandBlockOut.getSingleChannelBlock(1);

    auto lpf2InBuffer = juce::dsp::AudioBlock<float>(intermediateBuffer1); // Temporary buffer for LPF2 output
    auto leftLpf2InBlock = lpf2InBuffer.getSingleChannelBlock(0);
    auto rightLpf2InBlock = lpf2InBuffer.getSingleChannelBlock(1);

    // 1. LPF1 => Low Band (result in lowBandBuffer)
    leftLowBlockOut.copyFrom(leftInputBlock);
    rightLowBlockOut.copyFrom(rightInputBlock);
    juce::dsp::ProcessContextReplacing<float> leftLowCtx(leftLowBlockOut);
    juce::dsp::ProcessContextReplacing<float> rightLowCtx(rightLowBlockOut);
    leftLowMidLPF.process(leftLowCtx);
    rightLowMidLPF.process(rightLowCtx);

    // 2. LPF2 => Low + Mid Bands (result in intermediateBuffer1)
    leftLpf2InBlock.copyFrom(leftInputBlock);
    rightLpf2InBlock.copyFrom(rightInputBlock);
    juce::dsp::ProcessContextReplacing<float> leftLpf2Ctx(leftLpf2InBlock);
    juce::dsp::ProcessContextReplacing<float> rightLpf2Ctx(rightLpf2InBlock);
    leftMidHighLPF.process(leftLpf2Ctx);
    rightMidHighLPF.process(rightLpf2Ctx);

    // 3. Mid Band = LPF2 - LPF1 (result in midBandBuffer)
    leftMidBlockOut.copyFrom(leftLpf2InBlock);
    rightMidBlockOut.copyFrom(rightLpf2InBlock);
    leftMidBlockOut.subtract(leftLowBlockOut);
    rightMidBlockOut.subtract(rightLowBlockOut);

    // 4. High Band = Input - LPF2 (result in highBandBuffer)
    leftHighBlockOut.copyFrom(leftInputBlock);
    rightHighBlockOut.copyFrom(rightInputBlock);
    leftHighBlockOut.subtract(leftLpf2InBlock);
    rightHighBlockOut.subtract(rightLpf2InBlock);

    // --- Apply Panning and Sum Bands ---
    buffer.clear(); // Clear the output buffer before summing

    // Get write pointers for the output buffer
    auto* leftOut = buffer.getWritePointer(0);
    auto* rightOut = buffer.getWritePointer(1);

    // Get read pointers for the processed band buffers
    const auto* leftLowIn = lowBandBuffer.getReadPointer(0);
    const auto* rightLowIn = lowBandBuffer.getReadPointer(1);
    const auto* leftMidIn = midBandBuffer.getReadPointer(0);
    const auto* rightMidIn = midBandBuffer.getReadPointer(1);
    const auto* leftHighIn = highBandBuffer.getReadPointer(0);
    const auto* rightHighIn = highBandBuffer.getReadPointer(1);

    // Load atomic pan gains (thread-safe read)
    float llg = leftLowGain.load(); float rlg = rightLowGain.load();
    float lmg = leftMidGain.load(); float rmg = rightMidGain.load();
    float lhg = leftHighGain.load(); float rhg = rightHighGain.load();

    // Per-sample loop to apply panning and sum
    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Apply panning gains to each band
        float lowL = leftLowIn[sample] * llg;
        float lowR = rightLowIn[sample] * rlg;
        float midL = leftMidIn[sample] * lmg;
        float midR = rightMidIn[sample] * rmg;
        float highL = leftHighIn[sample] * lhg;
        float highR = rightHighIn[sample] * rhg;

        // Sum the panned bands into the output buffer
        leftOut[sample] = lowL + midL + highL;
        rightOut[sample] = lowR + midR + highR;
    }
}

//==============================================================================
bool MBRPAudioProcessor::hasEditor() const
{
    return true; // Plugin has an editor
}

juce::AudioProcessorEditor* MBRPAudioProcessor::createEditor()
{
    // Create and return the editor instance
    return new MBRPAudioProcessorEditor(*this);
}

//==============================================================================
void MBRPAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Save the APVTS state to a MemoryBlock
    juce::MemoryOutputStream mos(destData, false);
    apvts->state.writeToStream(mos); // Use -> with unique_ptr
}

void MBRPAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // Load the APVTS state from a MemoryBlock
    auto tree = juce::ValueTree::readFromData(data, size_t(sizeInBytes));
    if (tree.isValid())
    {
        apvts->replaceState(tree); // Use -> with unique_ptr
        // Update internal parameters after loading state
        updateParameters();
    }
}

//==============================================================================
// Factory function called by the host to create the plugin instance
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MBRPAudioProcessor();
}