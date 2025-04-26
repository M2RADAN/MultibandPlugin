#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>          // Для M_PI, cos, sin, std::max
#include <vector>         // Для возможного использования std::vector

// Определяем M_PI, если не определен (для MSVC)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

//==============================================================================
// --- ОПРЕДЕЛЕНИЕ ФУНКЦИИ СОЗДАНИЯ ЛЕЙАУТА ПАРАМЕТРОВ ---
juce::AudioProcessorValueTreeState::ParameterLayout MBRPAudioProcessor::createParameterLayout()
{
    APVTS::ParameterLayout layout;
    using namespace juce;

    // Определяем ParameterID для каждого параметра
    const ParameterID lowMidCrossoverParamID{ "lowMidCrossover", 1 };
    const ParameterID midHighCrossoverParamID{ "midHighCrossover", 1 };
    const ParameterID lowPanParamID{ "lowPan", 1 };
    const ParameterID midPanParamID{ "midPan", 1 };
    const ParameterID highPanParamID{ "highPan", 1 };

    // Добавляем параметры в layout, передавая все аргументы конструктора внутрь make_unique
    layout.add(std::make_unique<AudioParameterFloat>(
        lowMidCrossoverParamID,
        "Low-Mid Freq",                         // Имя параметра
        NormalisableRange<float>(20.0f, 2000.0f, 1.0f, 0.25f), // Диапазон
        500.0f,                                 // Значение по умолч.
        String(),                               // parameterLabel (пусто)
        juce::AudioProcessorParameter::genericParameter, // Категория
        [](float v, int) { return juce::String(v, 0) + " Hz"; }, // valueToText
        nullptr                                                 // textToValue
    ));

    layout.add(std::make_unique<AudioParameterFloat>(
        midHighCrossoverParamID,
        "Mid-High Freq",
        NormalisableRange<float>(500.0f, 20000.0f, 1.0f, 0.25f),
        2000.0f,
        String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float v, int) { return (v < 1000.f) ? juce::String(v, 0) + " Hz" : juce::String(v / 1000.f, 1) + " kHz"; },
        nullptr
    ));

    // Используем более короткий конструктор для параметров панорамы
    layout.add(std::make_unique<AudioParameterFloat>(
        lowPanParamID,
        "Low Pan",
        NormalisableRange<float>(-1.0f, 1.0f), // Простая нормализуемая область
        0.0f                                   // Значение по умолчанию
    ));

    layout.add(std::make_unique<AudioParameterFloat>(
        midPanParamID,
        "Mid Pan",
        NormalisableRange<float>(-1.0f, 1.0f),
        0.0f
    ));

    layout.add(std::make_unique<AudioParameterFloat>(
        highPanParamID,
        "High Pan",
        NormalisableRange<float>(-1.0f, 1.0f),
        0.0f
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
    )
    // НЕ инициализируем apvts здесь
#else
// НЕ инициализируем apvts здесь
#endif
{
    // --- ИНИЦИАЛИЗИРУЕМ APVTS В ТЕЛЕ КОНСТРУКТОРА ---
    apvts.reset(new juce::AudioProcessorValueTreeState(*this, nullptr, "Parameters", createParameterLayout()));
    // -----------------------------------------------

    // --- Получаем указатели на параметры ПОСЛЕ инициализации APVTS ---
    lowMidCrossover = dynamic_cast<juce::AudioParameterFloat*>(apvts->getParameter("lowMidCrossover"));
    midHighCrossover = dynamic_cast<juce::AudioParameterFloat*>(apvts->getParameter("midHighCrossover"));
    jassert(lowMidCrossover != nullptr && midHighCrossover != nullptr); // Проверка, что параметры найдены
    // ---------------------------------------------------------------
}

MBRPAudioProcessor::~MBRPAudioProcessor()
{
    // Деструктор пуст, ScopedPointer сам удалит apvts
}

//==============================================================================
const juce::String MBRPAudioProcessor::getName() const
{
    return JucePlugin_Name; // Имя из Projucer
}

// --- Стандартные методы AudioProcessor ---
bool MBRPAudioProcessor::acceptsMidi() const { return false; }
bool MBRPAudioProcessor::producesMidi() const { return false; }
bool MBRPAudioProcessor::isMidiEffect() const { return false; }
double MBRPAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int MBRPAudioProcessor::getNumPrograms() { return 1; }
int MBRPAudioProcessor::getCurrentProgram() { return 0; }
void MBRPAudioProcessor::setCurrentProgram(int index) { juce::ignoreUnused(index); }
const juce::String MBRPAudioProcessor::getProgramName(int index) { juce::ignoreUnused(index); return {}; }
void MBRPAudioProcessor::changeProgramName(int index, const juce::String& newName) { juce::ignoreUnused(index, newName); }
// -----------------------------------------

//==============================================================================
void MBRPAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    lastSampleRate = (float)sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1; // Фильтры обрабатывают моно

    // Подготовка всех фильтров
    leftLowMidLPF.prepare(spec); rightLowMidLPF.prepare(spec);
    leftLowMidHPF.prepare(spec); rightLowMidHPF.prepare(spec);
    leftMidHighLPF.prepare(spec); rightMidHighLPF.prepare(spec);
    leftMidHighHPF.prepare(spec); rightMidHighHPF.prepare(spec);

    // Сброс состояния всех фильтров
    leftLowMidLPF.reset(); rightLowMidLPF.reset();
    leftLowMidHPF.reset(); rightLowMidHPF.reset();
    leftMidHighLPF.reset(); rightMidHighLPF.reset();
    leftMidHighHPF.reset(); rightMidHighHPF.reset();

    // Выделение/изменение размера временных буферов
    int numOutputChannels = getTotalNumOutputChannels();
    intermediateBuffer1.setSize(numOutputChannels, samplesPerBlock, false, true, true);
    lowBandBuffer.setSize(numOutputChannels, samplesPerBlock, false, true, true);
    midBandBuffer.setSize(numOutputChannels, samplesPerBlock, false, true, true);
    highBandBuffer.setSize(numOutputChannels, samplesPerBlock, false, true, true);

    // Подготовка FIFO буферов для анализатора
    leftChannelFifo.prepare(samplesPerBlock);
    rightChannelFifo.prepare(samplesPerBlock);

    // Первичное обновление параметров
    updateParameters();
}

void MBRPAudioProcessor::releaseResources()
{
    // Освободить ресурсы, если нужно
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool MBRPAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo() ||
        layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}
#endif

void MBRPAudioProcessor::updateParameters()
{
    // Получаем текущие значения параметров
    float lowMidFreq = lowMidCrossover->get();
    float midHighFreq = midHighCrossover->get();
    // Используем ->getRawParameterValue() для доступа к параметрам панорамы через APVTS
    float lowPanParam = apvts->getRawParameterValue("lowPan")->load();
    float midPanParam = apvts->getRawParameterValue("midPan")->load();
    float highPanParam = apvts->getRawParameterValue("highPan")->load();

    // Гарантируем правильный порядок частот кроссовера
    midHighFreq = std::max(midHighFreq, lowMidFreq + 1.0f); // Mid/High >= Low/Mid

    // Обновляем частоты среза фильтров, используемых в методе вычитания
    leftLowMidLPF.setCutoffFrequency(lowMidFreq);
    rightLowMidLPF.setCutoffFrequency(lowMidFreq);
    leftMidHighLPF.setCutoffFrequency(midHighFreq);
    rightMidHighLPF.setCutoffFrequency(midHighFreq);

    // Обновляем гейны панорамы
    auto calculatePanGains = [](float panParam, std::atomic<float>& leftGain, std::atomic<float>& rightGain)
        {
            constexpr float piOverTwo = (float)M_PI * 0.5f;
            float angle = (panParam * 0.5f + 0.5f) * piOverTwo;
            leftGain.store(std::cos(angle));
            rightGain.store(std::sin(angle));
        };
    calculatePanGains(lowPanParam, leftLowGain, rightLowGain);
    calculatePanGains(midPanParam, leftMidGain, rightMidGain);
    calculatePanGains(highPanParam, leftHighGain, rightHighGain);
}


void MBRPAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals; // Защита от сверхмалых чисел
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    auto numSamples = buffer.getNumSamples();

    // Убеждаемся, что плагин работает в стерео режиме
    jassert(totalNumInputChannels == 2 && totalNumOutputChannels == 2);
    if (totalNumInputChannels != 2 || totalNumOutputChannels != 2) return;

    // Обновляем параметры перед обработкой блока
    updateParameters();

    // --- Отправка данных в FIFO для анализатора (до обработки) ---
    leftChannelFifo.update(buffer);
    rightChannelFifo.update(buffer);
    // ------------------------------------------------------------

    // --- Подготовка блоков и контекстов для DSP ---
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

    // --- Разделение на полосы методом вычитания ---

    // 1. LPF1 => НЧ полоса (результат в lowBandBuffer)
    leftLowBlockOut.copyFrom(leftInputBlock);
    rightLowBlockOut.copyFrom(rightInputBlock);
    juce::dsp::ProcessContextReplacing<float> leftLowCtx(leftLowBlockOut);
    juce::dsp::ProcessContextReplacing<float> rightLowCtx(rightLowBlockOut);
    leftLowMidLPF.process(leftLowCtx);
    rightLowMidLPF.process(rightLowCtx);

    // 2. LPF2 => (НЧ + СЧ) полоса (результат в intermediateBuffer1)
    leftLpf2InBlock.copyFrom(leftInputBlock);
    rightLpf2InBlock.copyFrom(rightInputBlock);
    juce::dsp::ProcessContextReplacing<float> leftLpf2Ctx(leftLpf2InBlock);
    juce::dsp::ProcessContextReplacing<float> rightLpf2Ctx(rightLpf2InBlock);
    leftMidHighLPF.process(leftLpf2Ctx);
    rightMidHighLPF.process(rightLpf2Ctx);

    // 3. СЧ = LPF2 - LPF1 (результат в midBandBuffer)
    leftMidBlockOut.copyFrom(leftLpf2InBlock);
    rightMidBlockOut.copyFrom(rightLpf2InBlock);
    leftMidBlockOut.subtract(leftLowBlockOut);
    rightMidBlockOut.subtract(rightLowBlockOut);

    // 4. ВЧ = Вход - LPF2 (результат в highBandBuffer)
    leftHighBlockOut.copyFrom(leftInputBlock);
    rightHighBlockOut.copyFrom(rightInputBlock);
    leftHighBlockOut.subtract(leftLpf2InBlock);
    rightHighBlockOut.subtract(rightLpf2InBlock);

    // --- Применение панорамы и суммирование ---
    buffer.clear(); // Очищаем выходной буфер перед записью результата

    // Указатели для доступа к данным
    auto* leftOut = buffer.getWritePointer(0);
    auto* rightOut = buffer.getWritePointer(1);
    const auto* leftLowIn = lowBandBuffer.getReadPointer(0);
    const auto* rightLowIn = lowBandBuffer.getReadPointer(1);
    const auto* leftMidIn = midBandBuffer.getReadPointer(0);
    const auto* rightMidIn = midBandBuffer.getReadPointer(1);
    const auto* leftHighIn = highBandBuffer.getReadPointer(0);
    const auto* rightHighIn = highBandBuffer.getReadPointer(1);

    // Загружаем гейны панорамы (атомарно)
    float llg = leftLowGain.load(); float rlg = rightLowGain.load();
    float lmg = leftMidGain.load(); float rmg = rightMidGain.load();
    float lhg = leftHighGain.load(); float rhg = rightHighGain.load();

    // Посемпловый цикл обработки
    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Применяем панораму к каждой полосе
        float lowL = leftLowIn[sample] * llg;
        float lowR = rightLowIn[sample] * rlg;
        float midL = leftMidIn[sample] * lmg;
        float midR = rightMidIn[sample] * rmg;
        float highL = leftHighIn[sample] * lhg;
        float highR = rightHighIn[sample] * rhg;

        // Суммируем обработанные полосы в выходной буфер
        leftOut[sample] = lowL + midL + highL;
        rightOut[sample] = lowR + midR + highR;
    }
}

//==============================================================================
bool MBRPAudioProcessor::hasEditor() const
{
    return true; // Указываем, что плагин имеет GUI редактор
}

juce::AudioProcessorEditor* MBRPAudioProcessor::createEditor()
{
    // Создаем и возвращаем экземпляр нашего редактора
    return new MBRPAudioProcessorEditor(*this);
}

//==============================================================================
void MBRPAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Сохраняем состояние параметров из APVTS в MemoryBlock
    juce::MemoryOutputStream mos(destData, false);
    apvts->state.writeToStream(mos); // Используем -> для доступа к state
}

void MBRPAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // Загружаем состояние параметров из MemoryBlock в APVTS
    auto tree = juce::ValueTree::readFromData(data, size_t(sizeInBytes));
    if (tree.isValid())
    {
        apvts->replaceState(tree); // Используем -> для вызова replaceState
        // После загрузки состояния необходимо обновить внутренние параметры плагина
        updateParameters();
    }
}

//==============================================================================
// Фабричная функция для создания экземпляра плагина хостом
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MBRPAudioProcessor();
}