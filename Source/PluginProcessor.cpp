#include "PluginProcessor.h" // Включает MBRPAudioProcessor из PluginProcessor.h
#include "PluginEditor.h"    // Включает MBRPAudioProcessorEditor из PluginEditor.h
#include <cmath>          // Для M_PI, cos, sin, std::max
#include <vector>         // Для возможного использования std::vector

// Определяем M_PI, если не определен (для MSVC)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

//==============================================================================
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
#endif
{
    apvts = new juce::AudioProcessorValueTreeState(*this, nullptr, "Parameters",
        {
            // Параметр частоты среза НЧ/СЧ кроссовера
            std::make_unique<juce::AudioParameterFloat>("lowMidCrossover",            // parameterID
                                                        "Low-Mid Freq",               // parameter name
                                                        juce::NormalisableRange<float>(20.0f, 2000.0f, 1.0f, 0.25f), // range (log)
                                                        500.0f,                       // default value
                                                        juce::String(),               // parameter label (unit suffix)
                                                        juce::AudioProcessorParameter::genericParameter, // category
                                                        [](float v, int) { return juce::String(v, 0) + " Hz"; }, // value to text lambda
                                                        nullptr),                     // text to value lambda

                                                        // Параметр частоты среза СЧ/ВЧ кроссовера
                                                        std::make_unique<juce::AudioParameterFloat>("midHighCrossover",           // parameterID
                                                                                                    "Mid-High Freq",              // parameter name
                                                                                                    juce::NormalisableRange<float>(500.0f, 20000.0f, 1.0f, 0.25f), // range (log)
                                                                                                    2000.0f,                      // default value
                                                                                                    juce::String(),               // parameter label (unit suffix)
                                                                                                    juce::AudioProcessorParameter::genericParameter, // category
                                                                                                    [](float v, int) { return (v < 1000.f) ? juce::String(v, 0) + " Hz" : juce::String(v / 1000.f, 1) + " kHz"; }, // value to text lambda
                                                                                                    nullptr),                     // text to value lambda

                                                                                                    // Параметр панорамы НЧ полосы
                                                                                                    std::make_unique<juce::AudioParameterFloat>("lowPan",                     // parameterID
                                                                                                                                                "Low Pan",                    // parameter name
                                                                                                                                                -1.0f,                        // min value (Left)
                                                                                                                                                1.0f,                         // max value (Right)
                                                                                                                                                0.0f),                        // default value (Center)

                                                                                                                                                // Параметр панорамы СЧ полосы
                                                                                                                                                std::make_unique<juce::AudioParameterFloat>("midPan", "Mid Pan", -1.0f, 1.0f, 0.0f),

                                                                                                                                                // Параметр панорамы ВЧ полосы
                                                                                                                                                std::make_unique<juce::AudioParameterFloat>("highPan", "High Pan", -1.0f, 1.0f, 0.0f)
        });

    // Устанавливаем типы фильтров
    leftLPF1.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    rightLPF1.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    leftHPF1.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
    rightHPF1.setType(juce::dsp::LinkwitzRileyFilterType::highpass);

    leftLPF2.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    rightLPF2.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    leftHPF2.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
    rightHPF2.setType(juce::dsp::LinkwitzRileyFilterType::highpass);

    leftAPF2.setType(juce::dsp::LinkwitzRileyFilterType::allpass); // Важно! Тип Allpass
    rightAPF2.setType(juce::dsp::LinkwitzRileyFilterType::allpass);
}

MBRPAudioProcessor::~MBRPAudioProcessor()
{
}

//==============================================================================
const juce::String MBRPAudioProcessor::getName() const
{
    return JucePlugin_Name; // Убедись, что имя в Projucer = "MBRP"
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
    lastSampleRate = (float)sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1; // Фильтры обрабатывают поканально

    // Подготовка всех фильтров
    leftLPF1.prepare(spec); rightLPF1.prepare(spec);
    leftHPF1.prepare(spec); rightHPF1.prepare(spec);
    leftLPF2.prepare(spec); rightLPF2.prepare(spec);
    leftHPF2.prepare(spec); rightHPF2.prepare(spec);
    leftAPF2.prepare(spec); rightAPF2.prepare(spec); // Подготовка All-Pass

    // Сброс состояния фильтров
    leftLPF1.reset(); rightLPF1.reset();
    leftHPF1.reset(); rightHPF1.reset();
    leftLPF2.reset(); rightLPF2.reset();
    leftHPF2.reset(); rightHPF2.reset();
    leftAPF2.reset(); rightAPF2.reset(); // Сброс All-Pass

    // Выделение памяти для буферов
    int numOutputChannels = getTotalNumOutputChannels(); // Обычно 2 для стерео
    // Устанавливаем флаги очистки буферов при изменении размера
    hpf1OutputBuffer.setSize(numOutputChannels, samplesPerBlock, false, true, true);
    lowBandBuffer.setSize(numOutputChannels, samplesPerBlock, false, true, true);
    midBandBuffer.setSize(numOutputChannels, samplesPerBlock, false, true, true);
    highBandBuffer.setSize(numOutputChannels, samplesPerBlock, false, true, true);

    updateParameters(); // Обновляем параметры при старте
}

void MBRPAudioProcessor::releaseResources()
{
    // Здесь можно освободить память, если она выделялась динамически
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool MBRPAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Требуем стерео вход и стерео выход
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}
#endif

void MBRPAudioProcessor::updateParameters()
{
    // Получаем текущие значения параметров
    float lowMidFreq = apvts->getRawParameterValue("lowMidCrossover")->load();
    float midHighFreq = apvts->getRawParameterValue("midHighCrossover")->load();
    float lowPanParam = apvts->getRawParameterValue("lowPan")->load();
    float midPanParam = apvts->getRawParameterValue("midPan")->load();
    float highPanParam = apvts->getRawParameterValue("highPan")->load();

    // Гарантируем, что частоты кроссовера не пересекаются некорректно
    midHighFreq = std::max(midHighFreq, lowMidFreq + 1.0f); // midHigh должна быть выше lowMid

    // Обновляем частоты среза фильтров
    leftLPF1.setCutoffFrequency(lowMidFreq); rightLPF1.setCutoffFrequency(lowMidFreq);
    leftHPF1.setCutoffFrequency(lowMidFreq); rightHPF1.setCutoffFrequency(lowMidFreq);
    leftLPF2.setCutoffFrequency(midHighFreq); rightLPF2.setCutoffFrequency(midHighFreq);
    leftHPF2.setCutoffFrequency(midHighFreq); rightHPF2.setCutoffFrequency(midHighFreq);
    leftAPF2.setCutoffFrequency(midHighFreq); rightAPF2.setCutoffFrequency(midHighFreq); // All-Pass на частоте 2-го среза

    // Рассчитываем гейны панорамы (Constant Power Panning)
    auto calculatePanGains = [](float panParam, std::atomic<float>& leftGain, std::atomic<float>& rightGain)
        {
            constexpr float piOverTwo = (float)M_PI * 0.5f;
            float angle = (panParam * 0.5f + 0.5f) * piOverTwo; // Преобразуем -1..+1 в 0..pi/2
            leftGain.store(std::cos(angle));  // Гейн левого = cos(angle)
            rightGain.store(std::sin(angle)); // Гейн правого = sin(angle)
        };

    calculatePanGains(lowPanParam, leftLowGain, rightLowGain);
    calculatePanGains(midPanParam, leftMidGain, rightMidGain);
    calculatePanGains(highPanParam, leftHighGain, rightHighGain);
}


void MBRPAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    auto numSamples = buffer.getNumSamples();

    jassert(totalNumInputChannels == 2 && totalNumOutputChannels == 2);
    if (totalNumInputChannels != 2 || totalNumOutputChannels != 2) return;

    updateParameters(); // Обновление параметров (можно оптимизировать)

    // --- Подготовка блоков и контекстов ---
    auto inputBlock = juce::dsp::AudioBlock<float>(buffer);
    auto lowBandBlock = juce::dsp::AudioBlock<float>(lowBandBuffer);
    auto midBandBlock = juce::dsp::AudioBlock<float>(midBandBuffer);
    auto highBandBlock = juce::dsp::AudioBlock<float>(highBandBuffer);
    auto hpf1OutputBlock = juce::dsp::AudioBlock<float>(hpf1OutputBuffer);

    // Получаем блоки для каждого канала
    auto leftInputBlock = inputBlock.getSingleChannelBlock(0);
    auto rightInputBlock = inputBlock.getSingleChannelBlock(1);
    auto leftLowBlock = lowBandBlock.getSingleChannelBlock(0);
    auto rightLowBlock = lowBandBlock.getSingleChannelBlock(1);
    auto leftMidBlock = midBandBlock.getSingleChannelBlock(0);
    auto rightMidBlock = midBandBlock.getSingleChannelBlock(1);
    auto leftHighBlock = highBandBlock.getSingleChannelBlock(0);
    auto rightHighBlock = highBandBlock.getSingleChannelBlock(1);
    auto leftHpf1OutBlock = hpf1OutputBlock.getSingleChannelBlock(0);
    auto rightHpf1OutBlock = hpf1OutputBlock.getSingleChannelBlock(1);

    // --- Разделение на полосы (метод с фазовой компенсацией) ---

    // 1. Инициализация буферов копированием входа
    leftLowBlock.copyFrom(leftInputBlock);
    rightLowBlock.copyFrom(rightInputBlock);
    leftMidBlock.copyFrom(leftInputBlock);      // СЧ начнем с входа, потом применим HPF1
    rightMidBlock.copyFrom(rightInputBlock);
    // В ВЧ буфер копировать пока не надо, он будет скопирован из выхода HPF1

    // 2. Обработка НЧ полосы (LPF1 -> APF2)
    juce::dsp::ProcessContextReplacing<float> leftLowCtx(leftLowBlock);
    juce::dsp::ProcessContextReplacing<float> rightLowCtx(rightLowBlock);
    leftLPF1.process(leftLowCtx);    // -> leftLowBlock = НЧ L (после LPF1)
    rightLPF1.process(rightLowCtx);   // -> rightLowBlock = НЧ R (после LPF1)
    leftAPF2.process(leftLowCtx);    // -> leftLowBlock = НЧ L (после LPF1 + APF2)
    rightAPF2.process(rightLowCtx);   // -> rightLowBlock = НЧ R (после LPF1 + APF2)

    // 3. Обработка СЧ полосы (HPF1 -> LPF2)
    juce::dsp::ProcessContextReplacing<float> leftMidCtx(leftMidBlock);
    juce::dsp::ProcessContextReplacing<float> rightMidCtx(rightMidBlock);
    leftHPF1.process(leftMidCtx);    // -> leftMidBlock = (СЧ+ВЧ) L
    rightHPF1.process(rightMidCtx);   // -> rightMidBlock = (СЧ+ВЧ) R
    // Копируем результат HPF1 во временный буфер И в буфер ВЧ
    leftHpf1OutBlock.copyFrom(leftMidBlock);
    rightHpf1OutBlock.copyFrom(rightMidBlock);
    leftHighBlock.copyFrom(leftHpf1OutBlock); // <--- Копируем сюда для ВЧ
    rightHighBlock.copyFrom(rightHpf1OutBlock);
    // Применяем LPF2 к (СЧ+ВЧ) в буфере СЧ
    leftLPF2.process(leftMidCtx);    // -> leftMidBlock = СЧ L
    rightLPF2.process(rightMidCtx);   // -> rightMidBlock = СЧ R

    // 4. Обработка ВЧ полосы (используем скопированный результат HPF1 и применяем HPF2)
    juce::dsp::ProcessContextReplacing<float> leftHighCtx(leftHighBlock);
    juce::dsp::ProcessContextReplacing<float> rightHighCtx(rightHighBlock);
    leftHPF2.process(leftHighCtx);   // -> leftHighBlock = ВЧ L
    rightHPF2.process(rightHighCtx);  // -> rightHighBlock = ВЧ R

    // --- Применение панорамы и суммирование ---
    buffer.clear(); // Очищаем оригинальный буфер для записи результата

    auto* leftOut = buffer.getWritePointer(0);
    auto* rightOut = buffer.getWritePointer(1);
    // Читаем из финальных буферов полос
    const auto* leftLowIn = lowBandBuffer.getReadPointer(0);
    const auto* rightLowIn = lowBandBuffer.getReadPointer(1);
    const auto* leftMidIn = midBandBuffer.getReadPointer(0);
    const auto* rightMidIn = midBandBuffer.getReadPointer(1);
    const auto* leftHighIn = highBandBuffer.getReadPointer(0);
    const auto* rightHighIn = highBandBuffer.getReadPointer(1);

    // Загружаем атомарные гейны
    float llg = leftLowGain.load(); float rlg = rightLowGain.load();
    float lmg = leftMidGain.load(); float rmg = rightMidGain.load();
    float lhg = leftHighGain.load(); float rhg = rightHighGain.load();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Панорамирование
        float lowL = leftLowIn[sample] * llg;
        float lowR = rightLowIn[sample] * rlg;
        float midL = leftMidIn[sample] * lmg;
        float midR = rightMidIn[sample] * rmg;
        float highL = leftHighIn[sample] * lhg;
        float highR = rightHighIn[sample] * rhg;

        // Суммирование
        leftOut[sample] = lowL + midL + highL;
        rightOut[sample] = lowR + midR + highR;
    }
}

//==============================================================================
bool MBRPAudioProcessor::hasEditor() const
{
    return true; // Да, редактор есть
}

juce::AudioProcessorEditor* MBRPAudioProcessor::createEditor()
{
    // Создаем и возвращаем экземпляр нашего редактора
    return new MBRPAudioProcessorEditor(*this);
}

//==============================================================================
void MBRPAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Сохраняем состояние APVTS
    juce::MemoryOutputStream mos(destData, false);
    apvts->state.writeToStream(mos);
}

void MBRPAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // Загружаем состояние APVTS
    auto tree = juce::ValueTree::readFromData(data, size_t(sizeInBytes));
    if (tree.isValid())
    {
        apvts->replaceState(tree);
        updateParameters(); // Обновляем параметры плагина после загрузки состояния
    }
}

//==============================================================================
// Фабричная функция, вызываемая хостом для создания экземпляра плагина
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MBRPAudioProcessor();
}