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

    // ID параметров для громкости, Bypass, Solo, Mute ---
    const ParameterID lowGainID{ "lowGain", 1 };
    const ParameterID lowBypassID{ "lowBypass", 1 };
    const ParameterID lowSoloID{ "lowSolo", 1 };
    const ParameterID lowMuteID{ "lowMute", 1 };

    const ParameterID lowMidGainID{ "lowMidGain", 1 };
    const ParameterID lowMidBypassID{ "lowMidBypass", 1 };
    const ParameterID lowMidSoloID{ "lowMidSolo", 1 };
    const ParameterID lowMidMuteID{ "lowMidMute", 1 };

    const ParameterID midHighGainID{ "midHighGain", 1 };
    const ParameterID midHighBypassID{ "midHighBypass", 1 };
    const ParameterID midHighSoloID{ "midHighSolo", 1 };
    const ParameterID midHighMuteID{ "midHighMute", 1 };

    const ParameterID highGainID{ "highGain", 1 };
    const ParameterID highBypassID{ "highBypass", 1 };
    const ParameterID highSoloID{ "highSolo", 1 };
    const ParameterID highMuteID{ "highMute", 1 };

    // Диапазоны
    auto lowMidRange = NormalisableRange<float>(20.0f, 2000.0f, 1.0f, 0.25f);    // Low/LowMid: 20Hz - 2kHz
    auto midRange = NormalisableRange<float>(100.0f, 5000.0f, 1.0f, 0.25f);      // LowMid/MidHigh: 100Hz - 5kHz
    auto midHighRange = NormalisableRange<float>(500.0f, 20000.0f, 1.0f, 0.25f); // MidHigh/High: 500Hz - 20kHz

    auto panRange = NormalisableRange<float>(-1.0f, 1.0f, 0.01f);
    auto wetRange = NormalisableRange<float>(0.0f, 1.0f, 0.01f);
    auto spaceRange = NormalisableRange<float>(0.0f, 1.0f, 0.01f);
    auto distanceRange = NormalisableRange<float>(0.0f, 1.0f, 0.01f);
    auto delayRange = NormalisableRange<float>(0.0f, 1000.0f, 1.0f); // 0-1000 ms
    auto bypassStrings = StringArray{ "Off", "On" }; // Для общего Bypass и Bypass полос
    auto soloStrings = StringArray{ "Off", "S" };    // Для Solo
    auto muteStrings = StringArray{ "Off", "M" };    // Для Mute

    // Диапазон для громкости (например, -24dB до +12dB) ---
    auto gainRange = NormalisableRange<float>(-24.0f, 12.0f, 0.1f, 1.0f); // Шаг 0.1dB, линейное поведение для слайдера

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

    // Функция для отображения громкости в dB ---
    auto gainDbToText = [](float val, int /*maxLen*/) { return String(val, 1) + " dB"; };
    auto textToGainDb = [](const String& text) { return text.getFloatValue(); }; // Простое преобразование для примера

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

    // Общий Bypass
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

    // Параметры громкости, Bypass, Solo, Mute для полос ---
    auto addBandControlParams =
        [&](const String& prefix, const ParameterID& gainID, const ParameterID& bypassBandID,
            const ParameterID& soloID, const ParameterID& muteID)
    {
        // Громкость
        layout.add(std::make_unique<AudioParameterFloat>(
            gainID, prefix + " Gain", gainRange, 0.0f, // 0dB по умолчанию
            "dB", AudioProcessorParameter::genericParameter, gainDbToText, textToGainDb));

        // Bypass полосы
        layout.add(std::make_unique<AudioParameterBool>(
            bypassBandID, prefix + " Bypass", false, // По умолчанию не в bypass
            AudioParameterBoolAttributes().withLabel("Bypass").withStringFromValueFunction([bypassStrings](bool v, int) { return bypassStrings[v ? 1 : 0]; })
            .withValueFromStringFunction([bypassStrings](const String& text) { return bypassStrings.indexOf(text) == 1; })));
        // Solo
        layout.add(std::make_unique<AudioParameterBool>(
            soloID, prefix + " Solo", false,
            AudioParameterBoolAttributes().withLabel("Solo").withStringFromValueFunction([soloStrings](bool v, int) { return soloStrings[v ? 1 : 0]; })
            .withValueFromStringFunction([soloStrings](const String& text) { return soloStrings.indexOf(text) == 1; })));
        // Mute
        layout.add(std::make_unique<AudioParameterBool>(
            muteID, prefix + " Mute", false,
            AudioParameterBoolAttributes().withLabel("Mute").withStringFromValueFunction([muteStrings](bool v, int) { return muteStrings[v ? 1 : 0]; })
            .withValueFromStringFunction([muteStrings](const String& text) { return muteStrings.indexOf(text) == 1; })));
    };

    addBandControlParams("Low", lowGainID, lowBypassID, lowSoloID, lowMuteID);
    addBandControlParams("Low-Mid", lowMidGainID, lowMidBypassID, lowMidSoloID, lowMidMuteID);
    addBandControlParams("Mid-High", midHighGainID, midHighBypassID, midHighSoloID, midHighMuteID);
    addBandControlParams("High", highGainID, highBypassID, highSoloID, highMuteID);

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

    // Инициализация указателей на параметры громкости, Bypass, Solo, Mute ---
    lowGainParam = apvts->getRawParameterValue("lowGain");
    lowBypassParam = dynamic_cast<juce::AudioParameterBool*>(apvts->getParameter("lowBypass"));
    lowSoloParam = dynamic_cast<juce::AudioParameterBool*>(apvts->getParameter("lowSolo"));
    lowMuteParam = dynamic_cast<juce::AudioParameterBool*>(apvts->getParameter("lowMute"));
    jassert(lowGainParam && lowBypassParam && lowSoloParam && lowMuteParam);

    lowMidGainParam = apvts->getRawParameterValue("lowMidGain");
    lowMidBypassParam = dynamic_cast<juce::AudioParameterBool*>(apvts->getParameter("lowMidBypass"));
    lowMidSoloParam = dynamic_cast<juce::AudioParameterBool*>(apvts->getParameter("lowMidSolo"));
    lowMidMuteParam = dynamic_cast<juce::AudioParameterBool*>(apvts->getParameter("lowMidMute"));
    jassert(lowMidGainParam && lowMidBypassParam && lowMidSoloParam && lowMidMuteParam);

    midHighGainParam = apvts->getRawParameterValue("midHighGain");
    midHighBypassParam = dynamic_cast<juce::AudioParameterBool*>(apvts->getParameter("midHighBypass"));
    midHighSoloParam = dynamic_cast<juce::AudioParameterBool*>(apvts->getParameter("midHighSolo"));
    midHighMuteParam = dynamic_cast<juce::AudioParameterBool*>(apvts->getParameter("midHighMute"));
    jassert(midHighGainParam && midHighBypassParam && midHighSoloParam && midHighMuteParam);

    highGainParam = apvts->getRawParameterValue("highGain");
    highBypassParam = dynamic_cast<juce::AudioParameterBool*>(apvts->getParameter("highBypass"));
    highSoloParam = dynamic_cast<juce::AudioParameterBool*>(apvts->getParameter("highSolo"));
    highMuteParam = dynamic_cast<juce::AudioParameterBool*>(apvts->getParameter("highMute"));
    jassert(highGainParam && highBypassParam && highSoloParam && highMuteParam);

    // Добавление слушателей параметров
    apvts->addParameterListener("bypass", this);
    apvts->addParameterListener("lowMidCrossover", this);
    apvts->addParameterListener("midCrossover", this);
    apvts->addParameterListener("midHighCrossover", this);

    apvts->addParameterListener("lowPan", this);
    apvts->addParameterListener("lowMidPan", this);
    apvts->addParameterListener("midHighPan", this);
    apvts->addParameterListener("highPan", this);

    // Слушатели для параметров Solo и Mute (Bypass и Gain обновляются в updateParameters) ---
    apvts->addParameterListener("lowSolo", this); apvts->addParameterListener("lowMute", this);
    apvts->addParameterListener("lowMidSolo", this); apvts->addParameterListener("lowMidMute", this);
    apvts->addParameterListener("midHighSolo", this); apvts->addParameterListener("midHighMute", this);
    apvts->addParameterListener("highSolo", this); apvts->addParameterListener("highMute", this);
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

    // Удаление слушателей для Solo и Mute ---
    apvts->removeParameterListener("lowSolo", this); apvts->removeParameterListener("lowMute", this);
    apvts->removeParameterListener("lowMidSolo", this); apvts->removeParameterListener("lowMidMute", this);
    apvts->removeParameterListener("midHighSolo", this); apvts->removeParameterListener("midHighMute", this);
    apvts->removeParameterListener("highSolo", this); apvts->removeParameterListener("highMute", this);
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

    // Подготовка DSP для громкости ---
    lowBandGainDSP.prepare(spec); lowBandGainDSP.reset();
    lowMidBandGainDSP.prepare(spec); lowMidBandGainDSP.reset();
    midHighBandGainDSP.prepare(spec); midHighBandGainDSP.reset();
    highBandGainDSP.prepare(spec); highBandGainDSP.reset();

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
    calculatePanGains(lowPanVal, leftLowPanGain, rightLowPanGain);
    calculatePanGains(lowMidPanVal, leftLowMidPanGain, rightLowMidPanGain);
    calculatePanGains(midHighPanVal, leftMidHighPanGain, rightMidHighPanGain);
    calculatePanGains(highPanVal, leftHighPanGain, rightHighPanGain);

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

    // Обновление DSP громкости ---
    // Коэффициент сглаживания для громкости может быть другим, если нужно более быстрое реагирование
    // Здесь используется Decibels::decibelsToGain для преобразования dB в линейный множитель
    if (lowGainParam) lowBandGainDSP.setGainDecibels(lowGainParam->load());
    if (lowMidGainParam) lowMidBandGainDSP.setGainDecibels(lowMidGainParam->load());
    if (midHighGainParam) midHighBandGainDSP.setGainDecibels(midHighGainParam->load());
    if (highGainParam) highBandGainDSP.setGainDecibels(highGainParam->load());
    // --------------------------------------

    // Обновление состояний Solo (вызывается из parameterChanged, но здесь для консистентности при запуске)
    low_isSoloed.store(lowSoloParam ? lowSoloParam->get() : false);
    lowMid_isSoloed.store(lowMidSoloParam ? lowMidSoloParam->get() : false);
    midHigh_isSoloed.store(midHighSoloParam ? midHighSoloParam->get() : false);
    high_isSoloed.store(highSoloParam ? highSoloParam->get() : false);
    anySoloActive.store(low_isSoloed.load() || lowMid_isSoloed.load() || midHigh_isSoloed.load() || high_isSoloed.load());
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

    if (bypassParameter != nullptr && bypassParameter->get()) // Общий Bypass плагина
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

        auto inputBlock = juce::dsp::AudioBlock<float>(buffer);
        auto lowBandBlock = juce::dsp::AudioBlock<float>(lowBandBuffer).getSubBlock(0, numSamples);
        auto lowMidBandBlock = juce::dsp::AudioBlock<float>(lowMidBandBuffer).getSubBlock(0, numSamples);
        auto midHighBandBlock = juce::dsp::AudioBlock<float>(midHighBandBuffer).getSubBlock(0, numSamples);
        auto highBandBlock = juce::dsp::AudioBlock<float>(highBandBuffer).getSubBlock(0, numSamples);
        auto temp1Block = juce::dsp::AudioBlock<float>(tempFilterBuffer1).getSubBlock(0, numSamples);
        auto temp2Block = juce::dsp::AudioBlock<float>(tempFilterBuffer2).getSubBlock(0, numSamples);

        // 1. Разделение на 4 "сырых" полосы
        for (int ch = 0; ch < totalNumInputChannels; ++ch)
        {
            // ... (код разделения на полосы, как и раньше) ...
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

            lowChannelBlock.copyFrom(inputChannelBlock);
            juce::dsp::ProcessContextReplacing<float> lowCtx(lowChannelBlock);
            lpfLowMid.process(lowCtx);

            temp1ChannelBlock.copyFrom(inputChannelBlock);
            juce::dsp::ProcessContextReplacing<float> temp1Ctx(temp1ChannelBlock);
            lpfMid.process(temp1Ctx);

            lowMidChannelBlock.copyFrom(temp1ChannelBlock);
            lowMidChannelBlock.subtract(lowChannelBlock);

            temp2ChannelBlock.copyFrom(inputChannelBlock);
            juce::dsp::ProcessContextReplacing<float> temp2Ctx(temp2ChannelBlock);
            lpfMidHigh.process(temp2Ctx);

            midHighChannelBlock.copyFrom(temp2ChannelBlock);
            midHighChannelBlock.subtract(temp1ChannelBlock);

            highChannelBlock.copyFrom(inputChannelBlock);
            highChannelBlock.subtract(temp2ChannelBlock);
        }

        // 2. Применение SOLO и MUTE к "сырым" полосам
        auto applySoloMuteToRawBand =
            [&](juce::dsp::AudioBlock<float>& rawBandBlock,
                std::atomic<bool>& isSoloed, juce::AudioParameterBool* muteParam)
        {
            bool isCurrentlyMuted = (muteParam && muteParam->get());
            bool shouldBeSilentDueToSolo = anySoloActive.load() && !isSoloed.load();
            if (isCurrentlyMuted || shouldBeSilentDueToSolo) {
                rawBandBlock.clear();
            }
        };
        applySoloMuteToRawBand(lowBandBlock, low_isSoloed, lowMuteParam);
        applySoloMuteToRawBand(lowMidBandBlock, lowMid_isSoloed, lowMidMuteParam);
        applySoloMuteToRawBand(midHighBandBlock, midHigh_isSoloed, midHighMuteParam);
        applySoloMuteToRawBand(highBandBlock, high_isSoloed, highMuteParam);

        // 3. Обработка каждой полосы (Реверб, Гейн, Панорама с учетом Bypass)

        // Лямбда для реверберации (без изменений)
        auto applyReverbToBand =
            [&](juce::dsp::AudioBlock<float>& bandBlock,
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

        // Массивы для удобного доступа к DSP объектам и параметрам в цикле
        juce::dsp::AudioBlock<float>* bandBlocks[] = { &lowBandBlock, &lowMidBandBlock, &midHighBandBlock, &highBandBlock };
        juce::AudioParameterBool* bypassParams[] = { lowBypassParam, lowMidBypassParam, midHighBypassParam, highBypassParam };
        juce::dsp::Reverb* reverbs[] = { &lowReverb, &lowMidReverb, &midHighReverb, &highReverb };
        juce::dsp::DryWetMixer<float>* mixers[] = { &lowMixer, &lowMidMixer, &midHighMixer, &highMixer };
        juce::dsp::DelayLine<float>* delayLines[] = { &lowDelayLine, &lowMidDelayLine, &midHighDelayLine, &highDelayLine };
        juce::dsp::Gain<float>* gainDSPs[] = { &lowBandGainDSP, &lowMidBandGainDSP, &midHighBandGainDSP, &highBandGainDSP };

        // Для панорамы нам понадобятся указатели на atomic<float> для левого и правого каналов
        struct PanGains { std::atomic<float>* left; std::atomic<float>* right; };
        PanGains panGainAtomics[] = {
            {&leftLowPanGain, &rightLowPanGain},
            {&leftLowMidPanGain, &rightLowMidPanGain},
            {&leftMidHighPanGain, &rightMidHighPanGain},
            {&leftHighPanGain, &rightHighPanGain}
        };


        buffer.clear(); // Очищаем выходной буфер перед суммированием

        for (int i = 0; i < 4; ++i) // Итерация по 4 полосам
        {
            juce::dsp::AudioBlock<float>& currentBandBlock = *bandBlocks[i];
            bool isBandBypassed = (bypassParams[i] && bypassParams[i]->get());

            // --- Применение эффектов, если полоса НЕ в байпасе ---
            if (!isBandBypassed)
            {
                // 3.1 Применяем реверберацию
                applyReverbToBand(currentBandBlock, *mixers[i], *delayLines[i], *reverbs[i]);
            }
            // Если в байпасе, реверберация пропускается. Сигнал остается "сухим" (после S/M).

            // 3.2 Применяем Гейн (громкость) в любом случае (к "сухому" или "мокрому" сигналу)
            juce::dsp::ProcessContextReplacing<float> gainCtx(currentBandBlock);
            gainDSPs[i]->process(gainCtx);

            // 3.3 Суммирование в выходной буфер с применением панорамы
            // Панорама применяется только если полоса НЕ в байпасе.
            // Если в байпасе, то сигнал добавляется "как есть" (центральная панорама, т.е. L=R=сигнал_полосы).
            const auto* leftBandIn = currentBandBlock.getChannelPointer(0);
            const auto* rightBandIn = currentBandBlock.getChannelPointer(1);
            auto* leftOut = buffer.getWritePointer(0);
            auto* rightOut = buffer.getWritePointer(1);

            float currentLeftPanGain = 1.0f; // По умолчанию для байпаса или если нет панорамы
            float currentRightPanGain = 1.0f;

            if (!isBandBypassed) {
                currentLeftPanGain = panGainAtomics[i].left->load();
                currentRightPanGain = panGainAtomics[i].right->load();
            }

            for (int sample = 0; sample < numSamples; ++sample)
            {
                leftOut[sample] += leftBandIn[sample] * currentLeftPanGain;
                rightOut[sample] += rightBandIn[sample] * currentRightPanGain;
            }
        }
    }
    else // Обработка для других конфигураций каналов
    {
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

    // Логика для Solo ---
    // Если изменился один из параметров Solo
    if (parameterID.contains("Solo")) {
        // Обновляем состояния solo для DSP
        low_isSoloed.store(lowSoloParam->get());
        lowMid_isSoloed.store(lowMidSoloParam->get());
        midHigh_isSoloed.store(midHighSoloParam->get());
        high_isSoloed.store(highSoloParam->get());

        anySoloActive.store(low_isSoloed.load() || lowMid_isSoloed.load() || midHigh_isSoloed.load() || high_isSoloed.load());

        // Если активировали Solo на одной полосе, нужно убедиться, что другие Solo выключены,
        // если мы хотим эксклюзивное солирование.
        // Для простоты, текущая реализация позволяет несколько Solo одновременно,
        // но в GUI это обычно радио-кнопки или логика, отключающая другие.
        // Если нужно эксклюзивное Solo, здесь можно добавить логику для сброса других solo параметров.
        // Например, если lowSoloParam стал true, то lowMidSoloParam, midHighSoloParam, highSoloParam -> false.
        // Это потребует isInternallySettingParameter флага, чтобы избежать рекурсии.
        // Пока оставляем так: несколько Solo могут быть активны, и anySoloActive это учитывает.
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
