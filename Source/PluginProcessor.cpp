#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <vector>
#include <memory>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

juce::AudioProcessorValueTreeState::ParameterLayout MBRPAudioProcessor::createParameterLayout()
{
    APVTS::ParameterLayout layout;
    using namespace juce;

    // ID параметров кроссовера
    const ParameterID lowMidCrossoverParamID{ "lowMidCrossover", 1 };    // Low / LowMid
    const ParameterID midCrossoverParamID{ "midCrossover", 1 };          // LowMid / MidHigh
    const ParameterID midHighCrossoverParamID{ "midHighCrossover", 1 };  // MidHigh / High

    // ID параметров панорамы
    const ParameterID lowPanParamID{ "lowPan", 1 };
    const ParameterID lowMidPanParamID{ "lowMidPan", 1 };
    const ParameterID midHighPanParamID{ "midHighPan", 1 };
    const ParameterID highPanParamID{ "highPan", 1 };

    // ID параметра Bypass
    const ParameterID bypassParamID{ "bypass", 1 };

    // ID параметров реверба
    const ParameterID lowWetParamID{ "lowWet", 1 };
    const ParameterID lowSpaceParamID{ "lowSpace", 1 };
    const ParameterID lowDistanceParamID{ "lowDistance", 1 };
    const ParameterID lowDelayParamID{ "lowDelay", 1 };

    const ParameterID lowMidWetParamID{ "lowMidWet", 1 };
    const ParameterID lowMidSpaceParamID{ "lowMidSpace", 1 };
    const ParameterID lowMidDistanceParamID{ "lowMidDistance", 1 };
    const ParameterID lowMidDelayParamID{ "lowMidDelay", 1 };

    const ParameterID midHighWetParamID{ "midHighWet", 1 };
    const ParameterID midHighSpaceParamID{ "midHighSpace", 1 };
    const ParameterID midHighDistanceParamID{ "midHighDistance", 1 };
    const ParameterID midHighDelayParamID{ "midHighDelay", 1 };

    const ParameterID highWetParamID{ "highWet", 1 };
    const ParameterID highSpaceParamID{ "highSpace", 1 };
    const ParameterID highDistanceParamID{ "highDistance", 1 };
    const ParameterID highDelayParamID{ "highDelay", 1 };

    // Диапазоны
    auto lowMidRange = NormalisableRange<float>(20.0f, 2000.0f, 1.0f, 0.25f);    // Low/LowMid: 20Hz - 2kHz
    auto midRange = NormalisableRange<float>(100.0f, 5000.0f, 1.0f, 0.25f);      // LowMid/MidHigh: 100Hz - 5kHz
    auto midHighRange = NormalisableRange<float>(500.0f, 20000.0f, 1.0f, 0.25f); // MidHigh/High: 500Hz - 20kHz

    auto panRange = NormalisableRange<float>(-1.0f, 1.0f, 0.01f);
    auto wetRange = NormalisableRange<float>(0.0f, 1.0f, 0.01f);
    auto spaceRange = NormalisableRange<float>(0.0f, 1.0f, 0.01f);
    auto distanceRange = NormalisableRange<float>(0.0f, 1.0f, 0.01f);
    auto delayRange = NormalisableRange<float>(0.0f, 1000.0f, 1.0f); // 0-1000 ms
    auto bypassStrings = StringArray{ "Off", "On" };

    // Функции преобразования значений в текст и обратно
    auto freqValueToText = [](float v, int) { return juce::String(v, 0) + " Hz"; };
    auto kHzValueToText = [](float v, int) {
        return (v < 1000.f) ? juce::String(v, 0) + " Hz"
            : juce::String(v / 1000.f, 1) + " kHz";
    };
    auto panValueToText = [](float v, int) {
        if (std::abs(v) < 0.01f) return String("C");
        float percentage = std::abs(v) * 100.0f;
        if (v < 0.0f) return String("L") + String(percentage, 0);
        return String("R") + String(percentage, 0);
    };
    auto panTextToValue = [](const String& text) {
        if (text.compareIgnoreCase("C") == 0 || text.isEmpty()) return 0.0f;
        if (text.startsWithIgnoreCase("L")) return juce::jlimit(-1.0f, 0.0f, -text.substring(1).getFloatValue() / 100.0f);
        if (text.startsWithIgnoreCase("R")) return juce::jlimit(0.0f, 1.0f, text.substring(1).getFloatValue() / 100.0f);
        float val = text.getFloatValue();
        return juce::jlimit(-1.0f, 1.0f, val);
    };
    auto delayMsValueToText = [](float v, int) { return String(v, 0) + " ms"; };
    auto percentValueToText = [](float v, int) { return String(v * 100.0f, 0) + " %"; };
    auto percentTextToValue = [](const String& text) { return text.getFloatValue() / 100.0f; };

    // Параметры кроссовера
    layout.add(std::make_unique<AudioParameterFloat>(
        lowMidCrossoverParamID, "Low / Low-Mid Freq", lowMidRange, 200.0f,
        "Hz", AudioProcessorParameter::genericParameter, freqValueToText, nullptr
        ));
    layout.add(std::make_unique<AudioParameterFloat>(
        midCrossoverParamID, "Low-Mid / Mid-High Freq", midRange, 1000.0f,
        "Hz/kHz", AudioProcessorParameter::genericParameter, kHzValueToText, nullptr
        ));
    layout.add(std::make_unique<AudioParameterFloat>(
        midHighCrossoverParamID, "Mid-High / High Freq", midHighRange, 5000.0f,
        "Hz/kHz", AudioProcessorParameter::genericParameter, kHzValueToText, nullptr
        ));

    // Параметры панорамы
    layout.add(std::make_unique<AudioParameterFloat>(
        lowPanParamID, "Low Pan", panRange, 0.0f,
        "%L/R", AudioProcessorParameter::genericParameter, panValueToText, panTextToValue
        ));
    layout.add(std::make_unique<AudioParameterFloat>(
        lowMidPanParamID, "Low-Mid Pan", panRange, 0.0f,
        "%L/R", AudioProcessorParameter::genericParameter, panValueToText, panTextToValue
        ));
    layout.add(std::make_unique<AudioParameterFloat>(
        midHighPanParamID, "Mid-High Pan", panRange, 0.0f,
        "%L/R", AudioProcessorParameter::genericParameter, panValueToText, panTextToValue
        ));
    layout.add(std::make_unique<AudioParameterFloat>(
        highPanParamID, "High Pan", panRange, 0.0f,
        "%L/R", AudioProcessorParameter::genericParameter, panValueToText, panTextToValue
        ));

    // Параметр Bypass
    layout.add(std::make_unique<AudioParameterBool>(
        bypassParamID, "Bypass", false,
        juce::AudioParameterBoolAttributes().withLabel("Bypass").withStringFromValueFunction([bypassStrings](bool v, int) { return bypassStrings[v ? 1 : 0]; })
        .withValueFromStringFunction([bypassStrings](const String& text) { return bypassStrings.indexOf(text) == 1; })
        ));

    // Параметры реверба
    auto addReverbParams = [&](const String& prefix, const ParameterID& wetID, const ParameterID& spaceID, const ParameterID& distID, const ParameterID& delayID) {
        layout.add(std::make_unique<AudioParameterFloat>(wetID, prefix + " Wet", wetRange, 0.0f, "%", AudioProcessorParameter::genericParameter, percentValueToText, percentTextToValue));
        layout.add(std::make_unique<AudioParameterFloat>(spaceID, prefix + " Space", spaceRange, 0.5f, "%", AudioProcessorParameter::genericParameter, percentValueToText, percentTextToValue));
        layout.add(std::make_unique<AudioParameterFloat>(distID, prefix + " Distance", distanceRange, 0.5f, "%", AudioProcessorParameter::genericParameter, percentValueToText, percentTextToValue));
        layout.add(std::make_unique<AudioParameterFloat>(delayID, prefix + " Pre-Delay", delayRange, 0.0f, "ms", AudioProcessorParameter::genericParameter, delayMsValueToText, nullptr));
    };

    addReverbParams("Low", lowWetParamID, lowSpaceParamID, lowDistanceParamID, lowDelayParamID);
    addReverbParams("Low-Mid", lowMidWetParamID, lowMidSpaceParamID, lowMidDistanceParamID, lowMidDelayParamID);
    addReverbParams("Mid-High", midHighWetParamID, midHighSpaceParamID, midHighDistanceParamID, midHighDelayParamID);
    addReverbParams("High", highWetParamID, highSpaceParamID, highDistanceParamID, highDelayParamID);

    return layout;
}

MBRPAudioProcessor::MBRPAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    )
#else 
    : AudioProcessor()
#endif
{
    apvts.reset(new juce::AudioProcessorValueTreeState(*this, nullptr, "Parameters", createParameterLayout()));

    // Инициализация указателей на параметры кроссовера
    lowMidCrossover = dynamic_cast<juce::AudioParameterFloat*>(apvts->getParameter("lowMidCrossover"));
    midCrossover = dynamic_cast<juce::AudioParameterFloat*>(apvts->getParameter("midCrossover"));
    midHighCrossover = dynamic_cast<juce::AudioParameterFloat*>(apvts->getParameter("midHighCrossover"));
    jassert(lowMidCrossover != nullptr && midCrossover != nullptr && midHighCrossover != nullptr);

    // Инициализация указателя на параметр Bypass
    bypassParameter = dynamic_cast<juce::AudioParameterBool*>(apvts->getParameter("bypass"));
    jassert(bypassParameter != nullptr);

    // Инициализация атомарных указателей на параметры реверба
    lowWetParam = apvts->getRawParameterValue("lowWet");
    lowSpaceParam = apvts->getRawParameterValue("lowSpace");
    lowDistanceParam = apvts->getRawParameterValue("lowDistance");
    lowDelayParam = apvts->getRawParameterValue("lowDelay");

    lowMidWetParam = apvts->getRawParameterValue("lowMidWet");
    lowMidSpaceParam = apvts->getRawParameterValue("lowMidSpace");
    lowMidDistanceParam = apvts->getRawParameterValue("lowMidDistance");
    lowMidDelayParam = apvts->getRawParameterValue("lowMidDelay");

    midHighWetParam = apvts->getRawParameterValue("midHighWet");
    midHighSpaceParam = apvts->getRawParameterValue("midHighSpace");
    midHighDistanceParam = apvts->getRawParameterValue("midHighDistance");
    midHighDelayParam = apvts->getRawParameterValue("midHighDelay");

    highWetParam = apvts->getRawParameterValue("highWet");
    highSpaceParam = apvts->getRawParameterValue("highSpace");
    highDistanceParam = apvts->getRawParameterValue("highDistance");
    highDelayParam = apvts->getRawParameterValue("highDelay");

    // Добавление слушателей параметров
    apvts->addParameterListener("bypass", this);
    apvts->addParameterListener("lowMidCrossover", this);
    apvts->addParameterListener("midCrossover", this);
    apvts->addParameterListener("midHighCrossover", this);

    apvts->addParameterListener("lowPan", this);
    apvts->addParameterListener("lowMidPan", this);
    apvts->addParameterListener("midHighPan", this);
    apvts->addParameterListener("highPan", this);

    // Слушатели для параметров реверба (можно добавить при необходимости, но updateParameters будет их обновлять)
}

MBRPAudioProcessor::~MBRPAudioProcessor()
{
    apvts->removeParameterListener("bypass", this);
    apvts->removeParameterListener("lowMidCrossover", this);
    apvts->removeParameterListener("midCrossover", this);
    apvts->removeParameterListener("midHighCrossover", this);
    apvts->removeParameterListener("lowPan", this);
    apvts->removeParameterListener("lowMidPan", this);
    apvts->removeParameterListener("midHighPan", this);
    apvts->removeParameterListener("highPan", this);
}


const juce::String MBRPAudioProcessor::getName() const { return JucePlugin_Name; }
bool MBRPAudioProcessor::acceptsMidi() const { return false; }
bool MBRPAudioProcessor::producesMidi() const { return false; }
bool MBRPAudioProcessor::isMidiEffect() const { return false; }
double MBRPAudioProcessor::getTailLengthSeconds() const { return 2.0; } // Ревербератор может иметь хвост
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
    spec.numChannels = 1; // Фильтры кроссовера обрабатывают каждый канал отдельно

    // Подготовка фильтров кроссовера
    leftLowMidLPF.prepare(spec); rightLowMidLPF.prepare(spec);
    leftMidLPF.prepare(spec);    rightMidLPF.prepare(spec);
    leftMidHighLPF.prepare(spec); rightMidHighLPF.prepare(spec);

    leftLowMidLPF.reset(); rightLowMidLPF.reset();
    leftMidLPF.reset();    rightMidLPF.reset();
    leftMidHighLPF.reset(); rightMidHighLPF.reset();

    // Подготовка буферов
    int numOutputChannels = getTotalNumOutputChannels();
    lowBandBuffer.setSize(numOutputChannels, samplesPerBlock, false, true, true);
    lowMidBandBuffer.setSize(numOutputChannels, samplesPerBlock, false, true, true);
    midHighBandBuffer.setSize(numOutputChannels, samplesPerBlock, false, true, true);
    highBandBuffer.setSize(numOutputChannels, samplesPerBlock, false, true, true);

    tempFilterBuffer1.setSize(numOutputChannels, samplesPerBlock, false, true, true);
    tempFilterBuffer2.setSize(numOutputChannels, samplesPerBlock, false, true, true);


    setCopyToFifo(copyToFifo.load()); // Инициализация FIFO, если нужно

    // Подготовка DSP объектов реверберации
    spec.numChannels = getTotalNumOutputChannels(); // Реверб/микшер/задержка работают с полным количеством каналов

    lowReverb.prepare(spec); lowReverb.reset();
    lowMidReverb.prepare(spec); lowMidReverb.reset();
    midHighReverb.prepare(spec); midHighReverb.reset();
    highReverb.prepare(spec); highReverb.reset();

    lowDelayLine.prepare(spec); lowDelayLine.reset();
    lowMidDelayLine.prepare(spec); lowMidDelayLine.reset();
    midHighDelayLine.prepare(spec); midHighDelayLine.reset();
    highDelayLine.prepare(spec); highDelayLine.reset();

    lowMixer.prepare(spec); lowMixer.reset();
    lowMidMixer.prepare(spec); lowMidMixer.reset();
    midHighMixer.prepare(spec); midHighMixer.reset();
    highMixer.prepare(spec); highMixer.reset();

    // Установка начальных параметров реверба (100% Wet внутри модуля реверба)
    auto setupReverbParams = [&](juce::dsp::Reverb::Parameters& params, std::atomic<float>* space, std::atomic<float>* distance) {
        params.wetLevel = 1.0f;
        params.dryLevel = 0.0f;
        params.width = 1.0f;
        params.freezeMode = 0.0f;
        // Загружаем начальные значения из атомарных указателей, если они уже инициализированы
        params.roomSize = (space && space->load() >= 0.0f) ? space->load() : 0.5f;
        params.damping = (distance && distance->load() >= 0.0f) ? distance->load() : 0.5f;
    };
    setupReverbParams(lowReverbParams, lowSpaceParam, lowDistanceParam); lowReverb.setParameters(lowReverbParams);
    setupReverbParams(lowMidReverbParams, lowMidSpaceParam, lowMidDistanceParam); lowMidReverb.setParameters(lowMidReverbParams);
    setupReverbParams(midHighReverbParams, midHighSpaceParam, midHighDistanceParam); midHighReverb.setParameters(midHighReverbParams);
    setupReverbParams(highReverbParams, highSpaceParam, highDistanceParam); highReverb.setParameters(highReverbParams);

    // Инициализация сглаженных значений
    currentLowWet = lowWetParam ? lowWetParam->load() : 0.0f;
    currentLowSpace = lowSpaceParam ? lowSpaceParam->load() : 0.5f;
    currentLowDistance = lowDistanceParam ? lowDistanceParam->load() : 0.5f;
    currentLowDelayMs = lowDelayParam ? lowDelayParam->load() : 0.0f;

    currentLowMidWet = lowMidWetParam ? lowMidWetParam->load() : 0.0f;
    currentLowMidSpace = lowMidSpaceParam ? lowMidSpaceParam->load() : 0.5f;
    currentLowMidDistance = lowMidDistanceParam ? lowMidDistanceParam->load() : 0.5f;
    currentLowMidDelayMs = lowMidDelayParam ? lowMidDelayParam->load() : 0.0f;

    currentMidHighWet = midHighWetParam ? midHighWetParam->load() : 0.0f;
    currentMidHighSpace = midHighSpaceParam ? midHighSpaceParam->load() : 0.5f;
    currentMidHighDistance = midHighDistanceParam ? midHighDistanceParam->load() : 0.5f;
    currentMidHighDelayMs = midHighDelayParam ? midHighDelayParam->load() : 0.0f;

    currentHighWet = highWetParam ? highWetParam->load() : 0.0f;
    currentHighSpace = highSpaceParam ? highSpaceParam->load() : 0.5f;
    currentHighDistance = highDistanceParam ? highDistanceParam->load() : 0.5f;
    currentHighDelayMs = highDelayParam ? highDelayParam->load() : 0.0f;


    // Установка начальных значений для задержек и микшеров
    lowDelayLine.setDelay((currentLowDelayMs / 1000.0f) * lastSampleRate); lowMixer.setWetMixProportion(currentLowWet);
    lowMidDelayLine.setDelay((currentLowMidDelayMs / 1000.0f) * lastSampleRate); lowMidMixer.setWetMixProportion(currentLowMidWet);
    midHighDelayLine.setDelay((currentMidHighDelayMs / 1000.0f) * lastSampleRate); midHighMixer.setWetMixProportion(currentMidHighWet);
    highDelayLine.setDelay((currentHighDelayMs / 1000.0f) * lastSampleRate); highMixer.setWetMixProportion(currentHighWet);

    updateParameters(); // Применяем все начальные значения параметров
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
    // Чтение актуальных значений параметров кроссовера
    float lmcFreq = lowMidCrossover->get();
    float mcFreq = midCrossover->get();
    float mhcFreq = midHighCrossover->get();

    // Коррекция частот кроссоверов для предотвращения инверсии
    // Эта логика должна быть в parameterChanged, чтобы обновлять сами параметры,
    // а здесь просто используем скорректированные значения.
    // Для простоты пока оставим здесь, но лучше перенести в parameterChanged.
    mcFreq = std::max(mcFreq, lmcFreq + MIN_CROSSOVER_SEPARATION);
    mhcFreq = std::max(mhcFreq, mcFreq + MIN_CROSSOVER_SEPARATION);

    // Применение (возможно, скорректированных локально) значений к фильтрам кроссовера
    leftLowMidLPF.setCutoffFrequency(lmcFreq);   rightLowMidLPF.setCutoffFrequency(lmcFreq);
    leftMidLPF.setCutoffFrequency(mcFreq);       rightMidLPF.setCutoffFrequency(mcFreq);
    leftMidHighLPF.setCutoffFrequency(mhcFreq);  rightMidHighLPF.setCutoffFrequency(mhcFreq);

    // Чтение актуальных значений параметров панорамы
    float lowPanVal = apvts->getRawParameterValue("lowPan")->load();
    float lowMidPanVal = apvts->getRawParameterValue("lowMidPan")->load();
    float midHighPanVal = apvts->getRawParameterValue("midHighPan")->load();
    float highPanVal = apvts->getRawParameterValue("highPan")->load();

    // Расчет гейнов для панорамы
    auto calculatePanGains = [](float panParam, std::atomic<float>& leftGain, std::atomic<float>& rightGain) {
        constexpr float piOverTwo = juce::MathConstants<float>::pi * 0.5f;
        float angle = (panParam * 0.5f + 0.5f) * piOverTwo;
        leftGain.store(std::cos(angle));
        rightGain.store(std::sin(angle));
    };
    calculatePanGains(lowPanVal, leftLowGain, rightLowGain);
    calculatePanGains(lowMidPanVal, leftLowMidGain, rightLowMidGain);
    calculatePanGains(midHighPanVal, leftMidHighGain, rightMidHighGain);
    calculatePanGains(highPanVal, leftHighGain, rightHighGain);

    // Обновление и сглаживание параметров реверба
    const float smoothingFactor = 0.02f;
    auto smoothAndUpdateParam = [&](float& currentValue, std::atomic<float>* targetParameterAtomicPtr) {
        if (targetParameterAtomicPtr) {
            currentValue = currentValue + smoothingFactor * (targetParameterAtomicPtr->load() - currentValue);
        }
    };

    smoothAndUpdateParam(currentLowWet, lowWetParam);
    smoothAndUpdateParam(currentLowSpace, lowSpaceParam);
    smoothAndUpdateParam(currentLowDistance, lowDistanceParam);
    smoothAndUpdateParam(currentLowDelayMs, lowDelayParam);

    smoothAndUpdateParam(currentLowMidWet, lowMidWetParam);
    smoothAndUpdateParam(currentLowMidSpace, lowMidSpaceParam);
    smoothAndUpdateParam(currentLowMidDistance, lowMidDistanceParam);
    smoothAndUpdateParam(currentLowMidDelayMs, lowMidDelayParam);

    smoothAndUpdateParam(currentMidHighWet, midHighWetParam);
    smoothAndUpdateParam(currentMidHighSpace, midHighSpaceParam);
    smoothAndUpdateParam(currentMidHighDistance, midHighDistanceParam);
    smoothAndUpdateParam(currentMidHighDelayMs, midHighDelayParam);

    smoothAndUpdateParam(currentHighWet, highWetParam);
    smoothAndUpdateParam(currentHighSpace, highSpaceParam);
    smoothAndUpdateParam(currentHighDistance, highDistanceParam);
    smoothAndUpdateParam(currentHighDelayMs, highDelayParam);

    // Лямбда для обновления DSP объектов реверберации
    auto updateReverbDSP = [&](juce::dsp::Reverb& reverb, juce::dsp::Reverb::Parameters& reverbModuleParams,
        float smoothedSpace, float smoothedDistance,
        juce::dsp::DelayLine<float>& delayLine, float smoothedDelayMs,
        juce::dsp::DryWetMixer<float>& mixer, float smoothedWet)
    {
        reverbModuleParams.roomSize = smoothedSpace;
        reverbModuleParams.damping = smoothedDistance;
        reverb.setParameters(reverbModuleParams);

        float delayInSamples = juce::jlimit(0.0f,
            (float)delayLine.getMaximumDelayInSamples(),
            (smoothedDelayMs / 1000.0f) * lastSampleRate);
        delayLine.setDelay(delayInSamples);
        mixer.setWetMixProportion(smoothedWet);
    };

    updateReverbDSP(lowReverb, lowReverbParams, currentLowSpace, currentLowDistance, lowDelayLine, currentLowDelayMs, lowMixer, currentLowWet);
    updateReverbDSP(lowMidReverb, lowMidReverbParams, currentLowMidSpace, currentLowMidDistance, lowMidDelayLine, currentLowMidDelayMs, lowMidMixer, currentLowMidWet);
    updateReverbDSP(midHighReverb, midHighReverbParams, currentMidHighSpace, currentMidHighDistance, midHighDelayLine, currentMidHighDelayMs, midHighMixer, currentMidHighWet);
    updateReverbDSP(highReverb, highReverbParams, currentHighSpace, currentHighDistance, highDelayLine, currentHighDelayMs, highMixer, currentHighWet);
}

void MBRPAudioProcessor::pushNextSampleToFifo(const juce::AudioBuffer<float>& buffer, const int startChannel,
    const int numChannels, juce::AbstractFifo& absFifo,
    juce::AudioBuffer<float>& fifo)
{
    const int numSamples = buffer.getNumSamples();
    // const int fifoSize = fifo.getNumSamples(); // Не используется напрямую

    if (absFifo.getFreeSpace() < numSamples) return;

    int start1, block1, start2, block2;
    absFifo.prepareToWrite(numSamples, start1, block1, start2, block2);

    // Нормализуем start1 и start2 к размеру FIFO буфера, если он меньше чем totalSize abstractFifo
    const int concreteFifoSize = fifo.getNumSamples();
    start1 %= concreteFifoSize;
    start2 %= concreteFifoSize;


    if (block1 > 0) fifo.copyFrom(0, start1, buffer.getReadPointer(startChannel), block1);
    if (block2 > 0) fifo.copyFrom(0, start2, buffer.getReadPointer(startChannel, block1), block2);

    for (int channel = startChannel + 1; channel < startChannel + numChannels; ++channel) {
        if (block1 > 0) fifo.addFrom(0, start1, buffer.getReadPointer(channel), block1);
        if (block2 > 0) fifo.addFrom(0, start2, buffer.getReadPointer(channel, block1), block2);
    }

    absFifo.finishedWrite(block1 + block2);
    nextFFTBlockReady.store(true);
}

void MBRPAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    jassert(totalNumInputChannels >= 1 && totalNumOutputChannels >= 1);

    if (bypassParameter != nullptr && bypassParameter->get())
    {
        if (copyToFifo.load())
        {
            pushNextSampleToFifo(buffer, 0, totalNumInputChannels, abstractFifoInput, audioFifoInput);
            pushNextSampleToFifo(buffer, 0, totalNumOutputChannels, abstractFifoOutput, audioFifoOutput);
        }
        return;
    }

    updateParameters();

    if (copyToFifo.load())
        pushNextSampleToFifo(buffer, 0, totalNumInputChannels, abstractFifoInput, audioFifoInput);

    if (totalNumInputChannels == 2 && totalNumOutputChannels == 2)
    {
        auto numSamples = buffer.getNumSamples();

        // Оборачиваем буферы в AudioBlock для удобства
        auto inputBlock = juce::dsp::AudioBlock<float>(buffer);

        auto lowBandBlock = juce::dsp::AudioBlock<float>(lowBandBuffer).getSubBlock(0, numSamples);
        auto lowMidBandBlock = juce::dsp::AudioBlock<float>(lowMidBandBuffer).getSubBlock(0, numSamples);
        auto midHighBandBlock = juce::dsp::AudioBlock<float>(midHighBandBuffer).getSubBlock(0, numSamples);
        auto highBandBlock = juce::dsp::AudioBlock<float>(highBandBuffer).getSubBlock(0, numSamples);

        auto temp1Block = juce::dsp::AudioBlock<float>(tempFilterBuffer1).getSubBlock(0, numSamples);
        auto temp2Block = juce::dsp::AudioBlock<float>(tempFilterBuffer2).getSubBlock(0, numSamples);

        // Разделение на 4 полосы
        for (int ch = 0; ch < totalNumInputChannels; ++ch)
        {
            auto inputChannelBlock = inputBlock.getSingleChannelBlock(ch);

            auto lowChannelBlock = lowBandBlock.getSingleChannelBlock(ch);
            auto lowMidChannelBlock = lowMidBandBlock.getSingleChannelBlock(ch);
            auto midHighChannelBlock = midHighBandBlock.getSingleChannelBlock(ch);
            auto highChannelBlock = highBandBlock.getSingleChannelBlock(ch);

            auto temp1ChannelBlock = temp1Block.getSingleChannelBlock(ch);
            auto temp2ChannelBlock = temp2Block.getSingleChannelBlock(ch);

            auto& lpfLowMid = (ch == 0) ? leftLowMidLPF : rightLowMidLPF;
            auto& lpfMid = (ch == 0) ? leftMidLPF : rightMidLPF;
            auto& lpfMidHigh = (ch == 0) ? leftMidHighLPF : rightMidHighLPF;

            juce::dsp::ProcessContextReplacing<float> ctx(inputChannelBlock); // Контекст для операций "на месте"

            // 1. Полоса Low
            lowChannelBlock.copyFrom(inputChannelBlock); // Копируем вход
            juce::dsp::ProcessContextReplacing<float> lowCtx(lowChannelBlock);
            lpfLowMid.process(lowCtx); // lowBandBuffer теперь содержит Low = Input * LPF(lowMidCrossover)

            // 2. Полоса Low-Mid
            temp1ChannelBlock.copyFrom(inputChannelBlock); // Копируем вход
            juce::dsp::ProcessContextReplacing<float> temp1Ctx(temp1ChannelBlock);
            lpfMid.process(temp1Ctx); // tempFilterBuffer1 теперь содержит Input * LPF(midCrossover)

            lowMidChannelBlock.copyFrom(temp1ChannelBlock); // Копируем результат LPF(midCrossover)
            lowMidChannelBlock.subtract(lowChannelBlock);   // lowMidBandBuffer = LPF(midCrossover) - LPF(lowMidCrossover)

            // 3. Полоса Mid-High
            temp2ChannelBlock.copyFrom(inputChannelBlock); // Копируем вход
            juce::dsp::ProcessContextReplacing<float> temp2Ctx(temp2ChannelBlock);
            lpfMidHigh.process(temp2Ctx); // tempFilterBuffer2 теперь содержит Input * LPF(midHighCrossover)

            midHighChannelBlock.copyFrom(temp2ChannelBlock); // Копируем результат LPF(midHighCrossover)
            midHighChannelBlock.subtract(temp1ChannelBlock);// midHighBandBuffer = LPF(midHighCrossover) - LPF(midCrossover)

            // 4. Полоса High
            highChannelBlock.copyFrom(inputChannelBlock);    // Копируем вход
            highChannelBlock.subtract(temp2ChannelBlock); // highBandBuffer = Input - LPF(midHighCrossover)
        }

        // Применение реверберации к каждой полосе
        auto applyReverbToBand = [&](juce::dsp::AudioBlock<float>& bandBlock,
            juce::dsp::DryWetMixer<float>& mixer,
            juce::dsp::DelayLine<float>& delayLine,
            juce::dsp::Reverb& reverb)
        {
            mixer.pushDrySamples(bandBlock);
            juce::dsp::ProcessContextReplacing<float> wetContext(bandBlock);
            delayLine.process(wetContext);
            reverb.process(wetContext);
            mixer.mixWetSamples(bandBlock);
        };

        applyReverbToBand(lowBandBlock, lowMixer, lowDelayLine, lowReverb);
        applyReverbToBand(lowMidBandBlock, lowMidMixer, lowMidDelayLine, lowMidReverb);
        applyReverbToBand(midHighBandBlock, midHighMixer, midHighDelayLine, midHighReverb);
        applyReverbToBand(highBandBlock, highMixer, highDelayLine, highReverb);

        // Суммирование полос и применение панорамы
        buffer.clear();
        auto* leftOut = buffer.getWritePointer(0);
        auto* rightOut = buffer.getWritePointer(1);

        const auto* leftLowIn = lowBandBuffer.getReadPointer(0);
        const auto* rightLowIn = lowBandBuffer.getReadPointer(1);
        const auto* leftLowMidIn = lowMidBandBuffer.getReadPointer(0);
        const auto* rightLowMidIn = lowMidBandBuffer.getReadPointer(1);
        const auto* leftMidHighIn = midHighBandBuffer.getReadPointer(0);
        const auto* rightMidHighIn = midHighBandBuffer.getReadPointer(1);
        const auto* leftHighIn = highBandBuffer.getReadPointer(0);
        const auto* rightHighIn = highBandBuffer.getReadPointer(1);

        // Загрузка гейнов панорамы
        float l_lg = leftLowGain.load();     float r_lg = rightLowGain.load();
        float l_lmg = leftLowMidGain.load();  float r_lmg = rightLowMidGain.load();
        float l_mhg = leftMidHighGain.load(); float r_mhg = rightMidHighGain.load();
        float l_hg = leftHighGain.load();    float r_hg = rightHighGain.load();

        for (int sample = 0; sample < numSamples; ++sample) {
            float lowL = leftLowIn[sample] * l_lg;     float lowR = rightLowIn[sample] * r_lg;
            float lmL = leftLowMidIn[sample] * l_lmg;  float lmR = rightLowMidIn[sample] * r_lmg;
            float mhL = leftMidHighIn[sample] * l_mhg; float mhR = rightMidHighIn[sample] * r_mhg;
            float highL = leftHighIn[sample] * l_hg;   float highR = rightHighIn[sample] * r_hg;

            leftOut[sample] = lowL + lmL + mhL + highL;
            rightOut[sample] = lowR + lmR + mhR + highR;
        }
    }
    else
    {
        // Обработка для других конфигураций каналов (если нужна)
        for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
            buffer.clear(i, 0, buffer.getNumSamples());
    }

    if (copyToFifo.load())
        pushNextSampleToFifo(buffer, 0, totalNumOutputChannels, abstractFifoOutput, audioFifoOutput);
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
            editorSize.setX(apvts->state.getProperty("editorSizeX", editorSize.getX()));
            editorSize.setY(apvts->state.getProperty("editorSizeY", editorSize.getY()));
            if (auto* editor = getActiveEditor()) editor->setSize(editorSize.x, editorSize.y);
            updateParameters(); // Применяем загруженные параметры
        }
    }
}

void MBRPAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused(newValue); // newValue уже применен к параметру

    if (isInternallySettingCrossoverParam.load()) return; // Предотвращение рекурсии

    // Логика коррекции частот кроссоверов
    // Вызывается, когда пользователь изменяет один из слайдеров кроссовера
    if (parameterID == "lowMidCrossover" || parameterID == "midCrossover" || parameterID == "midHighCrossover")
    {
        isInternallySettingCrossoverParam.store(true); // Устанавливаем флаг

        float lmcVal = lowMidCrossover->get();
        float mcVal = midCrossover->get();
        float mhcVal = midHighCrossover->get();

        bool changed = false;

        if (parameterID == "lowMidCrossover")
        {
            if (mcVal < lmcVal + MIN_CROSSOVER_SEPARATION) {
                midCrossover->setValueNotifyingHost(midCrossover->getNormalisableRange().convertTo0to1(lmcVal + MIN_CROSSOVER_SEPARATION));
                changed = true;
                // Обновляем mcVal локально, так как он мог измениться
                mcVal = midCrossover->get();
            }
        }

        if (parameterID == "midCrossover")
        {
            if (lmcVal > mcVal - MIN_CROSSOVER_SEPARATION) {
                lowMidCrossover->setValueNotifyingHost(lowMidCrossover->getNormalisableRange().convertTo0to1(mcVal - MIN_CROSSOVER_SEPARATION));
                changed = true;
            }
            if (mhcVal < mcVal + MIN_CROSSOVER_SEPARATION) {
                midHighCrossover->setValueNotifyingHost(midHighCrossover->getNormalisableRange().convertTo0to1(mcVal + MIN_CROSSOVER_SEPARATION));
                changed = true;
            }
        }

        if (parameterID == "midHighCrossover")
        {
            if (mcVal > mhcVal - MIN_CROSSOVER_SEPARATION) {
                midCrossover->setValueNotifyingHost(midCrossover->getNormalisableRange().convertTo0to1(mhcVal - MIN_CROSSOVER_SEPARATION));
                changed = true;
            }
        }
        // Дополнительная проверка после всех изменений, чтобы убедиться в порядке
        // Это может быть избыточным, если логика выше корректна, но для безопасности:
        lmcVal = lowMidCrossover->get(); // Перечитать значения, если они изменились
        mcVal = midCrossover->get();
        mhcVal = midHighCrossover->get();

        if (mcVal < lmcVal + MIN_CROSSOVER_SEPARATION) {
            midCrossover->setValueNotifyingHost(midCrossover->getNormalisableRange().convertTo0to1(lmcVal + MIN_CROSSOVER_SEPARATION));
            mcVal = midCrossover->get(); // Обновить локальное значение
        }
        if (mhcVal < mcVal + MIN_CROSSOVER_SEPARATION) {
            midHighCrossover->setValueNotifyingHost(midHighCrossover->getNormalisableRange().convertTo0to1(mcVal + MIN_CROSSOVER_SEPARATION));
        }


        isInternallySettingCrossoverParam.store(false); // Сбрасываем флаг

        // updateParameters(); // Вызываем обновление, если что-то изменилось программно
        // (но оно и так будет вызвано в processBlock)
    }
}


void MBRPAudioProcessor::setCopyToFifo(bool _copyToFifo)
{
    if (_copyToFifo != copyToFifo.load()) {
        copyToFifo.store(_copyToFifo);
        if (_copyToFifo) {
            const int fifoSizeSamples = fftSize * 2;
            abstractFifoInput.setTotalSize(fifoSizeSamples);
            abstractFifoOutput.setTotalSize(fifoSizeSamples);
            // Размер audioFifoInput/Output должен быть равен fifoSizeSamples, а не 1 каналу
            // Иначе в pushNextSampleToFifo будет выход за пределы, если start1/start2 > 0
            audioFifoInput.setSize(1, fifoSizeSamples); // Канал 1, но размер fifoSizeSamples
            audioFifoOutput.setSize(1, fifoSizeSamples); // Канал 1, но размер fifoSizeSamples

            abstractFifoInput.reset();
            abstractFifoOutput.reset();
            audioFifoInput.clear();
            audioFifoOutput.clear();
            DBG("FIFO Copying Enabled. Abstract FIFO size: " << fifoSizeSamples << ", Concrete FIFO samples: " << audioFifoInput.getNumSamples());
        }
        else {
            DBG("FIFO Copying Disabled.");
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new MBRPAudioProcessor();
}
