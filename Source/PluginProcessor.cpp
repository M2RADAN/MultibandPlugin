#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <vector>
#include <memory>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

//==============================================================================
// Используем старый синтаксис конструктора AudioParameterFloat (JUCE 6)
juce::AudioProcessorValueTreeState::ParameterLayout MBRPAudioProcessor::createParameterLayout()
{
    APVTS::ParameterLayout layout;
    using namespace juce;

    // Определяем ID параметров
    const ParameterID lowMidCrossoverParamID{ "lowMidCrossover", 1 };
    const ParameterID midHighCrossoverParamID{ "midHighCrossover", 1 };
    const ParameterID lowPanParamID{ "lowPan", 1 };
    const ParameterID midPanParamID{ "midPan", 1 };
    const ParameterID highPanParamID{ "highPan", 1 };

    // Определяем диапазоны
    auto lowMidRange = NormalisableRange<float>(20.0f, 2000.0f, 1.0f, 0.25f);
    auto midHighRange = NormalisableRange<float>(500.0f, 20000.0f, 1.0f, 0.25f);
    auto panRange = NormalisableRange<float>(-1.0f, 1.0f, 0.01f);

    // Определяем функции преобразования текста
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
    auto panTextToValue = [](const String& text) {
        if (text.compareIgnoreCase("C") == 0 || text.isEmpty()) return 0.0f;
        if (text.startsWithIgnoreCase("L")) return juce::jlimit(-1.0f, 0.0f, -text.substring(1).getFloatValue() / 100.0f);
        if (text.startsWithIgnoreCase("R")) return juce::jlimit(0.0f, 1.0f, text.substring(1).getFloatValue() / 100.0f);
        float val = text.getFloatValue(); // Убрали &isNumeric
        return juce::jlimit(-1.0f, 1.0f, val);
        };

    // Добавляем параметры
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

    // Если нужен Output Gain, раскомментируйте и настройте:
    // const ParameterID outGainParamID { "OutGain", 1 };
    // auto gainRange = NormalisableRange<float>(-24.0f, 24.0f, 0.1f, 1.0f);
    // auto gainValueToText = [](float v, int) { return String(v, 1) + " dB"; };
    // auto gainTextToValue = [](const String& t) { return t.getFloatValue(); };
    // layout.add(std::make_unique<AudioParameterFloat>(
    //     outGainParamID, "Output Gain", gainRange, 0.0f,
    //     "dB", AudioProcessorParameter::genericParameter, gainValueToText, gainTextToValue
    // ));

    return layout;
}

MBRPAudioProcessor::MBRPAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties() // Инициализация базового класса
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    ) // <-- ЗАПЯТАЯ УБРАНА
#else // Для случая, когда JucePlugin_PreferredChannelConfigurations определен
    : AudioProcessor() // <-- Инициализация базового класса для этого случая (пример)
    // Убедитесь, что это правильная инициализация для вашего случая
#endif
    , forwardFFT(fftOrder),
    window(fftSize, juce::dsp::WindowingFunction<float>::hann) // Используем Hann окно
{ // Тело конструктора начинается здесь
    // Инициализация APVTS
    apvts.reset(new juce::AudioProcessorValueTreeState(*this, nullptr, "Parameters", createParameterLayout()));

    // Получаем указатели на параметры
    lowMidCrossover = dynamic_cast<juce::AudioParameterFloat*>(apvts->getParameter("lowMidCrossover"));
    midHighCrossover = dynamic_cast<juce::AudioParameterFloat*>(apvts->getParameter("midHighCrossover"));
    jassert(lowMidCrossover != nullptr && midHighCrossover != nullptr);
    // Если добавили OutGain:
    // prmOutputGain = dynamic_cast<std::atomic<float>*>(apvts->getRawParameterValue("OutGain"));
    // jassert(prmOutputGain != nullptr);
    // apvts->addParameterListener("OutGain", this);
    fftInternalBuffer.fill(0.0f);
    fftMagnitudes.fill(0.0f);
}

MBRPAudioProcessor::~MBRPAudioProcessor()
{
    // Если добавили OutGain:
    // if (prmOutputGain != nullptr) // Проверка на случай, если параметр не был найден
    //    apvts->removeParameterListener("OutGain", this);
}


const juce::String MBRPAudioProcessor::getName() const { return JucePlugin_Name; }
bool MBRPAudioProcessor::acceptsMidi() const { return false; }
bool MBRPAudioProcessor::producesMidi() const { return false; }
bool MBRPAudioProcessor::isMidiEffect() const { return false; }
double MBRPAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int MBRPAudioProcessor::getNumPrograms() { return 1; }
int MBRPAudioProcessor::getCurrentProgram() { return 0; }
void MBRPAudioProcessor::setCurrentProgram(int index) { juce::ignoreUnused(index); }
const juce::String MBRPAudioProcessor::getProgramName(int index) { juce::ignoreUnused(index); return {}; }
void MBRPAudioProcessor::changeProgramName(int index, const juce::String& newName) { juce::ignoreUnused(index, newName); }

void MBRPAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    lastSampleRate = static_cast<float>(sampleRate);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 1; // Фильтры моно

    leftLowMidLPF.prepare(spec); rightLowMidLPF.prepare(spec);
    leftMidHighLPF.prepare(spec); rightMidHighLPF.prepare(spec);
    leftLowMidLPF.reset(); rightLowMidLPF.reset();
    leftMidHighLPF.reset(); rightMidHighLPF.reset();

    int numOutputChannels = getTotalNumOutputChannels();
    intermediateBuffer1.setSize(numOutputChannels, samplesPerBlock, false, true, true);
    lowBandBuffer.setSize(numOutputChannels, samplesPerBlock, false, true, true);
    midBandBuffer.setSize(numOutputChannels, samplesPerBlock, false, true, true);
    highBandBuffer.setSize(numOutputChannels, samplesPerBlock, false, true, true);

    // Инициализируем FIFO через setCopyToFifo
    isFftDataReady.store(false);
    fftInternalBuffer.fill(0.0f); // Сброс буферов
    fftMagnitudes.fill(0.0f);

    updateParameters();
}

void MBRPAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool MBRPAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo() ||
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}
#endif

void MBRPAudioProcessor::updateParameters()
{
    float lowMidFreq = lowMidCrossover->get();
    float midHighFreq = midHighCrossover->get();

    float lowPanParam = apvts->getRawParameterValue("lowPan")->load();
    float midPanParam = apvts->getRawParameterValue("midPan")->load();
    float highPanParam = apvts->getRawParameterValue("highPan")->load();

    midHighFreq = std::max(midHighFreq, lowMidFreq + 1.0f);

    leftLowMidLPF.setCutoffFrequency(lowMidFreq);
    rightLowMidLPF.setCutoffFrequency(lowMidFreq);
    leftMidHighLPF.setCutoffFrequency(midHighFreq);
    rightMidHighLPF.setCutoffFrequency(midHighFreq);

    auto calculatePanGains = [](float panParam, std::atomic<float>& leftGain, std::atomic<float>& rightGain) {
        constexpr float piOverTwo = static_cast<float>(M_PI) * 0.5f;
        float angle = (panParam * 0.5f + 0.5f) * piOverTwo;
        leftGain.store(std::cos(angle));
        rightGain.store(std::sin(angle));
        };
    calculatePanGains(lowPanParam, leftLowGain, rightLowGain);
    calculatePanGains(midPanParam, leftMidGain, rightMidGain);
    calculatePanGains(highPanParam, leftHighGain, rightHighGain);
}

void MBRPAudioProcessor::processFFT(const juce::AudioBuffer<float>& inputBuffer)
{
    auto numSamples = inputBuffer.getNumSamples();
    auto samplesToCopy = std::min(numSamples, fftSize);
    const float* leftChannelData = inputBuffer.getReadPointer(0);

    std::fill(fftInternalBuffer.begin(), fftInternalBuffer.begin() + fftSize, 0.0f);
    std::copy(leftChannelData, leftChannelData + samplesToCopy, fftInternalBuffer.begin());

    window.multiplyWithWindowingTable(fftInternalBuffer.data(), fftSize);
    forwardFFT.performRealOnlyForwardTransform(fftInternalBuffer.data(), true);

    const int numBins = fftSize / 2;
    const float* fftResult = fftInternalBuffer.data();
    fftMagnitudes[0] = std::abs(fftResult[0]);
    for (int i = 1; i < numBins; ++i) {
        float realPart = fftResult[size_t(2 * i)];
        float imagPart = fftResult[size_t(2 * i + 1)];
        fftMagnitudes[i] = std::sqrt(realPart * realPart + imagPart * imagPart);
    }
    isFftDataReady.store(true); // Устанавливаем флаг
}
void MBRPAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    jassert(totalNumInputChannels >= 1 && totalNumOutputChannels >= 1);

    updateParameters();

    // Копируем ВХОДНЫЕ данные в FIFO (используем вариант 1 - суммирование)
    processFFT(buffer);
    // DSP обработка (разделение на полосы, панорамирование)
    if (totalNumInputChannels == 2 && totalNumOutputChannels == 2)
    {
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
        auto lpf2InBuffer = juce::dsp::AudioBlock<float>(intermediateBuffer1);
        auto leftLpf2InBlock = lpf2InBuffer.getSingleChannelBlock(0);
        auto rightLpf2InBlock = lpf2InBuffer.getSingleChannelBlock(1);

        leftLowBlockOut.copyFrom(leftInputBlock);
        rightLowBlockOut.copyFrom(rightInputBlock);
        juce::dsp::ProcessContextReplacing<float> leftLowCtx(leftLowBlockOut);
        juce::dsp::ProcessContextReplacing<float> rightLowCtx(rightLowBlockOut);
        leftLowMidLPF.process(leftLowCtx);
        rightLowMidLPF.process(rightLowCtx);

        leftLpf2InBlock.copyFrom(leftInputBlock);
        rightLpf2InBlock.copyFrom(rightInputBlock);
        juce::dsp::ProcessContextReplacing<float> leftLpf2Ctx(leftLpf2InBlock);
        juce::dsp::ProcessContextReplacing<float> rightLpf2Ctx(rightLpf2InBlock);
        leftMidHighLPF.process(leftLpf2Ctx);
        rightMidHighLPF.process(rightLpf2Ctx);

        leftMidBlockOut.copyFrom(leftLpf2InBlock);
        rightMidBlockOut.copyFrom(rightLpf2InBlock);
        leftMidBlockOut.subtract(leftLowBlockOut);
        rightMidBlockOut.subtract(rightLowBlockOut);

        leftHighBlockOut.copyFrom(leftInputBlock);
        rightHighBlockOut.copyFrom(rightInputBlock);
        leftHighBlockOut.subtract(leftLpf2InBlock);
        rightHighBlockOut.subtract(rightLpf2InBlock);

        buffer.clear();
        auto* leftOut = buffer.getWritePointer(0);
        auto* rightOut = buffer.getWritePointer(1);
        const auto* leftLowIn = lowBandBuffer.getReadPointer(0);
        const auto* rightLowIn = lowBandBuffer.getReadPointer(1);
        const auto* leftMidIn = midBandBuffer.getReadPointer(0);
        const auto* rightMidIn = midBandBuffer.getReadPointer(1);
        const auto* leftHighIn = highBandBuffer.getReadPointer(0);
        const auto* rightHighIn = highBandBuffer.getReadPointer(1);

        float llg = leftLowGain.load(); float rlg = rightLowGain.load();
        float lmg = leftMidGain.load(); float rmg = rightMidGain.load();
        float lhg = leftHighGain.load(); float rhg = rightHighGain.load();

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
            float lowL = leftLowIn[sample] * llg; float lowR = rightLowIn[sample] * rlg;
            float midL = leftMidIn[sample] * lmg; float midR = rightMidIn[sample] * rmg;
            float highL = leftHighIn[sample] * lhg; float highR = rightHighIn[sample] * rhg;
            leftOut[sample] = lowL + midL + highL;
            rightOut[sample] = lowR + midR + highR;
        }
    }

    // Копируем ВЫХОДНЫЕ данные в FIFO (используем вариант 1 - суммирование)
   
}

bool MBRPAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* MBRPAudioProcessor::createEditor() {
    return new MBRPAudioProcessorEditor(*this);
}

void MBRPAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts->copyState();
    state.setProperty("editorSizeX", editorSize.x, nullptr);
    state.setProperty("editorSizeY", editorSize.y, nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void MBRPAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr) {
        if (xmlState->hasTagName(apvts->state.getType())) {
            apvts->replaceState(juce::ValueTree::fromXml(*xmlState));
            editorSize.setX(apvts->state.getProperty("editorSizeX", 900));
            editorSize.setY(apvts->state.getProperty("editorSizeY", 700));
            if (auto* editor = getActiveEditor()) editor->setSize(editorSize.x, editorSize.y);
            updateParameters();
        }
    }
}

void MBRPAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    // Оставьте пустым, если не используете prmOutputGain или другую логику здесь
    juce::ignoreUnused(parameterID, newValue);
}


juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new MBRPAudioProcessor();
}