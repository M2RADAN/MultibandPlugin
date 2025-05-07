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

    const ParameterID lowMidCrossoverParamID{ "lowMidCrossover", 1 };
    const ParameterID midHighCrossoverParamID{ "midHighCrossover", 1 };
    const ParameterID lowPanParamID{ "lowPan", 1 };
    const ParameterID midPanParamID{ "midPan", 1 };
    const ParameterID highPanParamID{ "highPan", 1 };
    const ParameterID bypassParamID{ "bypass", 1 }; // ID для байпаса

        // --- НОВЫЕ ID параметров реверба ---
    const ParameterID lowWetParamID{ "lowWet", 1 };
    const ParameterID lowSpaceParamID{ "lowSpace", 1 };
    const ParameterID lowDistanceParamID{ "lowDistance", 1 };
    const ParameterID lowDelayParamID{ "lowDelay", 1 };
    const ParameterID midWetParamID{ "midWet", 1 };
    const ParameterID midSpaceParamID{ "midSpace", 1 };
    const ParameterID midDistanceParamID{ "midDistance", 1 };
    const ParameterID midDelayParamID{ "midDelay", 1 };
    const ParameterID highWetParamID{ "highWet", 1 };
    const ParameterID highSpaceParamID{ "highSpace", 1 };
    const ParameterID highDistanceParamID{ "highDistance", 1 };
    const ParameterID highDelayParamID{ "highDelay", 1 };
    // ----------------------------------


    auto lowMidRange = NormalisableRange<float>(20.0f, 2000.0f, 1.0f, 0.25f);
    auto midHighRange = NormalisableRange<float>(500.0f, 20000.0f, 1.0f, 0.25f);
    auto panRange = NormalisableRange<float>(-1.0f, 1.0f, 0.01f);
    auto wetRange = NormalisableRange<float>(0.0f, 1.0f, 0.01f);
    auto spaceRange = NormalisableRange<float>(0.0f, 1.0f, 0.01f);
    auto distanceRange = NormalisableRange<float>(0.0f, 1.0f, 0.01f);
    auto delayRange = NormalisableRange<float>(0.0f, 1000.0f, 1.0f); // 0-1000 ms
    auto bypassStrings = StringArray{ "Off", "On" }; // Или "Active", "Bypassed"

    auto freqValueToText = [](float v, int ) { return juce::String(v, 0) + " Hz"; };
    auto kHzValueToText = [](float v, int ) {
        return (v < 1000.f) ? juce::String(v, 0) + " Hz"
            : juce::String(v / 1000.f, 1) + " kHz";
        };
    auto panValueToText = [](float v, int ) {
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
    auto percentValueToText = [](float v, int) { return String(v * 100.0f, 0) + " %"; }; // Для Wet/Space/Distance
    auto percentTextToValue = [](const String& text) { return text.getFloatValue() / 100.0f; };


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

    layout.add(std::make_unique<AudioParameterBool>(
        bypassParamID,          // ID
        "Bypass",               // Имя параметра
        false,                  // Значение по умолчанию
        juce::AudioParameterBoolAttributes().withLabel("Bypass").withStringFromValueFunction([bypassStrings](bool v, int) { return bypassStrings[v ? 1 : 0]; }) // Используем атрибуты для label и strings
        .withValueFromStringFunction([bypassStrings](const String& text) { return bypassStrings.indexOf(text) == 1; }) // Добавляем обратное преобразование
    ));

    // --- НОВОЕ: Добавление параметров реверба ---
    auto addReverbParams = [&](const String& prefix, const ParameterID& wetID, const ParameterID& spaceID, const ParameterID& distID, const ParameterID& delayID) {
        layout.add(std::make_unique<AudioParameterFloat>(wetID, prefix + " Wet", wetRange, 0.0f, "%", AudioProcessorParameter::genericParameter, percentValueToText, percentTextToValue));
        layout.add(std::make_unique<AudioParameterFloat>(spaceID, prefix + " Space", spaceRange, 0.5f, "%", AudioProcessorParameter::genericParameter, percentValueToText, percentTextToValue));
        layout.add(std::make_unique<AudioParameterFloat>(distID, prefix + " Distance", distanceRange, 0.5f, "%", AudioProcessorParameter::genericParameter, percentValueToText, percentTextToValue));
        layout.add(std::make_unique<AudioParameterFloat>(delayID, prefix + " Pre-Delay", delayRange, 0.0f, "ms", AudioProcessorParameter::genericParameter, delayMsValueToText, nullptr));
    };

    addReverbParams("Low", lowWetParamID, lowSpaceParamID, lowDistanceParamID, lowDelayParamID);
    addReverbParams("Mid", midWetParamID, midSpaceParamID, midDistanceParamID, midDelayParamID);
    addReverbParams("High", highWetParamID, highSpaceParamID, highDistanceParamID, highDelayParamID);
    // ------------------------------------------

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


    lowMidCrossover = dynamic_cast<juce::AudioParameterFloat*>(apvts->getParameter("lowMidCrossover"));
    midHighCrossover = dynamic_cast<juce::AudioParameterFloat*>(apvts->getParameter("midHighCrossover"));
    jassert(lowMidCrossover != nullptr && midHighCrossover != nullptr);

    bypassParameter = dynamic_cast<juce::AudioParameterBool*>(apvts->getParameter("bypass"));
    jassert(bypassParameter != nullptr); // Убедимся, что параметр найден
    // -------------------------------------------------

    lowWetParam = apvts->getRawParameterValue("lowWet");
    lowSpaceParam = apvts->getRawParameterValue("lowSpace");
    lowDistanceParam = apvts->getRawParameterValue("lowDistance");
    lowDelayParam = apvts->getRawParameterValue("lowDelay");
    midWetParam = apvts->getRawParameterValue("midWet");
    midSpaceParam = apvts->getRawParameterValue("midSpace");
    midDistanceParam = apvts->getRawParameterValue("midDistance");
    midDelayParam = apvts->getRawParameterValue("midDelay");
    highWetParam = apvts->getRawParameterValue("highWet");
    highSpaceParam = apvts->getRawParameterValue("highSpace");
    highDistanceParam = apvts->getRawParameterValue("highDistance");
    highDelayParam = apvts->getRawParameterValue("highDelay");

    // Добавляем Listener (если еще не добавлен)
    apvts->addParameterListener("bypass", this);
    apvts->addParameterListener("lowMidCrossover", this); // Добавьте листенеры и для других параметров, если нужно
    apvts->addParameterListener("midHighCrossover", this);
    apvts->addParameterListener("lowPan", this);
    apvts->addParameterListener("midPan", this);
    apvts->addParameterListener("highPan", this);

}

MBRPAudioProcessor::~MBRPAudioProcessor()
{
    //----//
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
    spec.numChannels = 1; 

    leftLowMidLPF.prepare(spec); rightLowMidLPF.prepare(spec);
    leftMidHighLPF.prepare(spec); rightMidHighLPF.prepare(spec);
    leftLowMidLPF.reset(); rightLowMidLPF.reset();
    leftMidHighLPF.reset(); rightMidHighLPF.reset();

    int numOutputChannels = getTotalNumOutputChannels();
    intermediateBuffer1.setSize(numOutputChannels, samplesPerBlock, false, true, true);
    lowBandBuffer.setSize(numOutputChannels, samplesPerBlock, false, true, true);
    midBandBuffer.setSize(numOutputChannels, samplesPerBlock, false, true, true);
    highBandBuffer.setSize(numOutputChannels, samplesPerBlock, false, true, true);

    setCopyToFifo(copyToFifo.load());

    // --- НОВОЕ: Подготовка DSP объектов реверберации ---
    spec.numChannels = getTotalNumOutputChannels(); // Возвращаем кол-во каналов для реверба/микшера/задержки

    lowReverb.prepare(spec); lowReverb.reset();
    midReverb.prepare(spec); midReverb.reset();
    highReverb.prepare(spec); highReverb.reset();

    lowDelayLine.prepare(spec); lowDelayLine.reset();
    midDelayLine.prepare(spec); midDelayLine.reset();
    highDelayLine.prepare(spec); highDelayLine.reset();

    lowMixer.prepare(spec); lowMixer.reset();
    midMixer.prepare(spec); midMixer.reset();
    highMixer.prepare(spec); highMixer.reset();

    // Устанавливаем начальные параметры реверба (100% WET внутри)
    auto setupReverbParams = [&](juce::dsp::Reverb::Parameters& params, std::atomic<float>* space, std::atomic<float>* distance) {
        params.wetLevel = 1.0f;
        params.dryLevel = 0.0f;
        params.width = 1.0f;
        params.freezeMode = 0.0f;
        params.roomSize = space ? space->load() : 0.5f;     // Загружаем начальное значение
        params.damping = distance ? distance->load() : 0.5f; // Загружаем начальное значение
    };
    setupReverbParams(lowReverbParams, lowSpaceParam, lowDistanceParam); lowReverb.setParameters(lowReverbParams);
    setupReverbParams(midReverbParams, midSpaceParam, midDistanceParam); midReverb.setParameters(midReverbParams);
    setupReverbParams(highReverbParams, highSpaceParam, highDistanceParam); highReverb.setParameters(highReverbParams);

    // Инициализируем сглаженные значения
    currentLowWet = lowWetParam->load(); currentLowSpace = lowSpaceParam->load(); currentLowDistance = lowDistanceParam->load(); currentLowDelayMs = lowDelayParam->load();
    currentMidWet = midWetParam->load(); currentMidSpace = midSpaceParam->load(); currentMidDistance = midDistanceParam->load(); currentMidDelayMs = midDelayParam->load();
    currentHighWet = highWetParam->load(); currentHighSpace = highSpaceParam->load(); currentHighDistance = highDistanceParam->load(); currentHighDelayMs = highDelayParam->load();

    // Устанавливаем начальные значения для задержек и микшеров
    lowDelayLine.setDelay((currentLowDelayMs / 1000.0f) * lastSampleRate); lowMixer.setWetMixProportion(currentLowWet);
    midDelayLine.setDelay((currentMidDelayMs / 1000.0f) * lastSampleRate); midMixer.setWetMixProportion(currentMidWet);
    highDelayLine.setDelay((currentHighDelayMs / 1000.0f) * lastSampleRate); highMixer.setWetMixProportion(currentHighWet);

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
    // Читаем актуальные значения параметров кроссовера.
    // lowMidCrossover и midHighCrossover - это juce::AudioParameterFloat*
    float lowMidFreq = lowMidCrossover->get();
    float midHighFreq = midHighCrossover->get();

    // Читаем актуальные значения параметров панорамы.
    // apvts->getRawParameterValue(...) возвращает std::atomic<float>*, поэтому ->load()
    float lowPanParam = apvts->getRawParameterValue("lowPan")->load();
    float midPanParam = apvts->getRawParameterValue("midPan")->load();
    float highPanParam = apvts->getRawParameterValue("highPan")->load();

    // Гарантируем, что частота mid/high кроссовера всегда выше low/mid
    // с минимальным разделением. Эта коррекция применяется ТОЛЬКО к локальным переменным,
    // используемым для настройки DSP, и НЕ изменяет сами объекты AudioParameterFloat.
    // Основная логика изменения параметров находится в parameterChanged().
    midHighFreq = std::max(midHighFreq, lowMidFreq + MIN_CROSSOVER_SEPARATION);

    // Применяем (возможно, скорректированные локально) значения к фильтрам кроссовера
    leftLowMidLPF.setCutoffFrequency(lowMidFreq);
    rightLowMidLPF.setCutoffFrequency(lowMidFreq);
    leftMidHighLPF.setCutoffFrequency(midHighFreq); // Используем midHighFreq, который прошел через std::max
    rightMidHighLPF.setCutoffFrequency(midHighFreq);

    // Рассчитываем гейны для панорамы
    auto calculatePanGains = [](float panParam, std::atomic<float>& leftGain, std::atomic<float>& rightGain) {
        constexpr float piOverTwo = juce::MathConstants<float>::pi * 0.5f; // Используем juce константу
        float angle = (panParam * 0.5f + 0.5f) * piOverTwo; // Нормализуем panParam [-1, 1] -> [0, 1] для угла
        leftGain.store(std::cos(angle));
        rightGain.store(std::sin(angle));
    };
    calculatePanGains(lowPanParam, leftLowGain, rightLowGain);
    calculatePanGains(midPanParam, leftMidGain, rightMidGain);
    calculatePanGains(highPanParam, leftHighGain, rightHighGain);

    // --- Обновление и сглаживание параметров реверба ---
    const float smoothingFactor = 0.02f; // Коэффициент сглаживания

    // Лямбда для сглаживания параметров
    auto smoothAndUpdateParam = [&](float& currentValue, std::atomic<float>* targetParameterAtomicPtr) {
        if (targetParameterAtomicPtr) { // Проверяем, что указатель не нулевой
            currentValue = currentValue + smoothingFactor * (targetParameterAtomicPtr->load() - currentValue);
        }
    };

    // Сглаживаем параметры реверба для каждой полосы
    // lowWetParam, lowSpaceParam и т.д. - это std::atomic<float>* из PluginProcessor.h
    smoothAndUpdateParam(currentLowWet, lowWetParam);
    smoothAndUpdateParam(currentLowSpace, lowSpaceParam);
    smoothAndUpdateParam(currentLowDistance, lowDistanceParam);
    smoothAndUpdateParam(currentLowDelayMs, lowDelayParam);

    smoothAndUpdateParam(currentMidWet, midWetParam);
    smoothAndUpdateParam(currentMidSpace, midSpaceParam);
    smoothAndUpdateParam(currentMidDistance, midDistanceParam);
    smoothAndUpdateParam(currentMidDelayMs, midDelayParam);

    smoothAndUpdateParam(currentHighWet, highWetParam);
    smoothAndUpdateParam(currentHighSpace, highSpaceParam);
    smoothAndUpdateParam(currentHighDistance, highDistanceParam);
    smoothAndUpdateParam(currentHighDelayMs, highDelayParam);

    // Лямбда для обновления DSP объектов ревербации
    auto updateReverbDSP = [&](juce::dsp::Reverb& reverb, juce::dsp::Reverb::Parameters& reverbModuleParams,
        float smoothedSpace, float smoothedDistance,
        juce::dsp::DelayLine<float>& delayLine, float smoothedDelayMs,
        juce::dsp::DryWetMixer<float>& mixer, float smoothedWet)
    {
        // Устанавливаем параметры для модуля Reverb
        // reverbModuleParams.wetLevel и dryLevel должны быть 1.0f и 0.0f соответственно,
        // т.к. смешивание выполняется DryWetMixer. Это настраивается в prepareToPlay.
        reverbModuleParams.roomSize = smoothedSpace;
        reverbModuleParams.damping = smoothedDistance;
        reverb.setParameters(reverbModuleParams);

        // Устанавливаем задержку для DelayLine
        // lastSampleRate - это член класса MBRPAudioProcessor
        float delayInSamples = juce::jlimit(0.0f,
            (float)delayLine.getMaximumDelayInSamples(),
            (smoothedDelayMs / 1000.0f) * lastSampleRate);
        delayLine.setDelay(delayInSamples);

        // Устанавливаем пропорцию смешивания для DryWetMixer
        mixer.setWetMixProportion(smoothedWet);
    };

    // Обновляем DSP объекты ревербации для каждой полосы, используя сглаженные значения
    updateReverbDSP(lowReverb, lowReverbParams, currentLowSpace, currentLowDistance, lowDelayLine, currentLowDelayMs, lowMixer, currentLowWet);
    updateReverbDSP(midReverb, midReverbParams, currentMidSpace, currentMidDistance, midDelayLine, currentMidDelayMs, midMixer, currentMidWet);
    updateReverbDSP(highReverb, highReverbParams, currentHighSpace, currentHighDistance, highDelayLine, currentHighDelayMs, highMixer, currentHighWet);
}

void MBRPAudioProcessor::pushNextSampleToFifo(const juce::AudioBuffer<float>& buffer, const int startChannel,
    const int numChannels, juce::AbstractFifo& absFifo,
    juce::AudioBuffer<float>& fifo)
{
    const int numSamples = buffer.getNumSamples();
    const int fifoSize = fifo.getNumSamples();

    if (absFifo.getFreeSpace() < numSamples) return;

    int start1, block1, start2, block2;
    absFifo.prepareToWrite(numSamples, start1, block1, start2, block2);

    if (block1 > 0) fifo.copyFrom(0, start1 % fifoSize, buffer.getReadPointer(startChannel), block1);
    if (block2 > 0) fifo.copyFrom(0, start2 % fifoSize, buffer.getReadPointer(startChannel, block1), block2);
    for (int channel = startChannel + 1; channel < startChannel + numChannels; ++channel) {
        if (block1 > 0) fifo.addFrom(0, start1 % fifoSize, buffer.getReadPointer(channel), block1);
        if (block2 > 0) fifo.addFrom(0, start2 % fifoSize, buffer.getReadPointer(channel, block1), block2);
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
        // Если в байпасе, просто копируем вход на выход (если каналы совпадают)
        // Или обрабатываем разницу каналов по необходимости
        // В данном случае просто выходим, так как буфер уже содержит входной сигнал
        // Но нужно убедиться, что мы не очищаем выходные каналы далее по коду,
        // которых нет во входном сигнале (если конфигурация не стерео->стерео)

        // Важно: Анализатор все равно получит данные (необработанные)
        if (copyToFifo.load())
        {
            // Отправляем входной (теперь и выходной) сигнал в FIFO
            pushNextSampleToFifo(buffer, 0, totalNumInputChannels, abstractFifoInput, audioFifoInput);
            // Для чистоты можно отправлять и в output fifo тот же сигнал
            pushNextSampleToFifo(buffer, 0, totalNumOutputChannels, abstractFifoOutput, audioFifoOutput);
        }
        return; // Выход из processBlock, минуя обработку
    }
    jassert(totalNumInputChannels >= 1 && totalNumOutputChannels >= 1);


    updateParameters();


    if (copyToFifo.load())
        pushNextSampleToFifo(buffer, 0, totalNumInputChannels, abstractFifoInput, audioFifoInput);

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

        auto applyReverbToBand = [&](juce::dsp::AudioBlock<float>& bandBlock,
            juce::dsp::DryWetMixer<float>& mixer,
            juce::dsp::DelayLine<float>& delayLine,
            juce::dsp::Reverb& reverb)
        {
            // 1. Сохраняем Dry сигнал
            mixer.pushDrySamples(bandBlock);
            // 2. Создаем контекст для Wet пути
            juce::dsp::ProcessContextReplacing<float> wetContext(bandBlock);
            // 3. Применяем Pre-Delay
            delayLine.process(wetContext);
            // 4. Применяем Reverb (100% Wet)
            reverb.process(wetContext);
            // 5. Смешиваем Dry и Wet обратно в bandBlock
            mixer.mixWetSamples(bandBlock);
        };

        applyReverbToBand(lowBandBlockOut, lowMixer, lowDelayLine, lowReverb);
        applyReverbToBand(midBandBlockOut, midMixer, midDelayLine, midReverb);
        applyReverbToBand(highBandBlockOut, highMixer, highDelayLine, highReverb);

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
    else
    {
        // Обработка для других конфигураций каналов (если нужна)
        // Например, простое копирование или очистка
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
            editorSize.setX(apvts->state.getProperty("editorSizeX", 900));
            editorSize.setY(apvts->state.getProperty("editorSizeY", 700));
            if (auto* editor = getActiveEditor()) editor->setSize(editorSize.x, editorSize.y);
            updateParameters();
        }
    }
}

void MBRPAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused(parameterID, newValue);
}

void MBRPAudioProcessor::setCopyToFifo(bool _copyToFifo)
{
    if (_copyToFifo != copyToFifo.load()) {
        copyToFifo.store(_copyToFifo);
        if (_copyToFifo) {
            const int fifoSizeSamples = fftSize * 2;
            abstractFifoInput.setTotalSize(fifoSizeSamples);
            abstractFifoOutput.setTotalSize(fifoSizeSamples);
            audioFifoInput.setSize(1, fifoSizeSamples);
            audioFifoOutput.setSize(1, fifoSizeSamples);
            abstractFifoInput.reset();
            abstractFifoOutput.reset();
            audioFifoInput.clear();
            audioFifoOutput.clear();
            DBG("FIFO Copying Enabled. FIFO size: " << fifoSizeSamples);
        }
        else {
            DBG("FIFO Copying Disabled.");
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new MBRPAudioProcessor();
}
