#include "PluginProcessor.h" // �������� MBRPAudioProcessor �� PluginProcessor.h
#include "PluginEditor.h"    // �������� MBRPAudioProcessorEditor �� PluginEditor.h
#include <cmath>          // ��� M_PI, cos, sin, std::max
#include <vector>         // ��� ���������� ������������� std::vector

// ���������� M_PI, ���� �� ��������� (��� MSVC)
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
            // �������� ������� ����� ��/�� ����������
            std::make_unique<juce::AudioParameterFloat>("lowMidCrossover",            // parameterID
                                                        "Low-Mid Freq",               // parameter name
                                                        juce::NormalisableRange<float>(20.0f, 2000.0f, 1.0f, 0.25f), // range (log)
                                                        500.0f,                       // default value
                                                        juce::String(),               // parameter label (unit suffix)
                                                        juce::AudioProcessorParameter::genericParameter, // category
                                                        [](float v, int) { return juce::String(v, 0) + " Hz"; }, // value to text lambda
                                                        nullptr),                     // text to value lambda

                                                        // �������� ������� ����� ��/�� ����������
                                                        std::make_unique<juce::AudioParameterFloat>("midHighCrossover",           // parameterID
                                                                                                    "Mid-High Freq",              // parameter name
                                                                                                    juce::NormalisableRange<float>(500.0f, 20000.0f, 1.0f, 0.25f), // range (log)
                                                                                                    2000.0f,                      // default value
                                                                                                    juce::String(),               // parameter label (unit suffix)
                                                                                                    juce::AudioProcessorParameter::genericParameter, // category
                                                                                                    [](float v, int) { return (v < 1000.f) ? juce::String(v, 0) + " Hz" : juce::String(v / 1000.f, 1) + " kHz"; }, // value to text lambda
                                                                                                    nullptr),                     // text to value lambda

                                                                                                    // �������� �������� �� ������
                                                                                                    std::make_unique<juce::AudioParameterFloat>("lowPan",                     // parameterID
                                                                                                                                                "Low Pan",                    // parameter name
                                                                                                                                                -1.0f,                        // min value (Left)
                                                                                                                                                1.0f,                         // max value (Right)
                                                                                                                                                0.0f),                        // default value (Center)

                                                                                                                                                // �������� �������� �� ������
                                                                                                                                                std::make_unique<juce::AudioParameterFloat>("midPan", "Mid Pan", -1.0f, 1.0f, 0.0f),

                                                                                                                                                // �������� �������� �� ������
                                                                                                                                                std::make_unique<juce::AudioParameterFloat>("highPan", "High Pan", -1.0f, 1.0f, 0.0f)
        });

    // ������������� ���� ��������
    leftLPF1.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    rightLPF1.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    leftHPF1.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
    rightHPF1.setType(juce::dsp::LinkwitzRileyFilterType::highpass);

    leftLPF2.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    rightLPF2.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    leftHPF2.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
    rightHPF2.setType(juce::dsp::LinkwitzRileyFilterType::highpass);

    leftAPF2.setType(juce::dsp::LinkwitzRileyFilterType::allpass); // �����! ��� Allpass
    rightAPF2.setType(juce::dsp::LinkwitzRileyFilterType::allpass);
}

MBRPAudioProcessor::~MBRPAudioProcessor()
{
}

//==============================================================================
const juce::String MBRPAudioProcessor::getName() const
{
    return JucePlugin_Name; // �������, ��� ��� � Projucer = "MBRP"
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
    spec.numChannels = 1; // ������� ������������ ����������

    // ���������� ���� ��������
    leftLPF1.prepare(spec); rightLPF1.prepare(spec);
    leftHPF1.prepare(spec); rightHPF1.prepare(spec);
    leftLPF2.prepare(spec); rightLPF2.prepare(spec);
    leftHPF2.prepare(spec); rightHPF2.prepare(spec);
    leftAPF2.prepare(spec); rightAPF2.prepare(spec); // ���������� All-Pass

    // ����� ��������� ��������
    leftLPF1.reset(); rightLPF1.reset();
    leftHPF1.reset(); rightHPF1.reset();
    leftLPF2.reset(); rightLPF2.reset();
    leftHPF2.reset(); rightHPF2.reset();
    leftAPF2.reset(); rightAPF2.reset(); // ����� All-Pass

    // ��������� ������ ��� �������
    int numOutputChannels = getTotalNumOutputChannels(); // ������ 2 ��� ������
    // ������������� ����� ������� ������� ��� ��������� �������
    hpf1OutputBuffer.setSize(numOutputChannels, samplesPerBlock, false, true, true);
    lowBandBuffer.setSize(numOutputChannels, samplesPerBlock, false, true, true);
    midBandBuffer.setSize(numOutputChannels, samplesPerBlock, false, true, true);
    highBandBuffer.setSize(numOutputChannels, samplesPerBlock, false, true, true);

    updateParameters(); // ��������� ��������� ��� ������
}

void MBRPAudioProcessor::releaseResources()
{
    // ����� ����� ���������� ������, ���� ��� ���������� �����������
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool MBRPAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // ������� ������ ���� � ������ �����
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}
#endif

void MBRPAudioProcessor::updateParameters()
{
    // �������� ������� �������� ����������
    float lowMidFreq = apvts->getRawParameterValue("lowMidCrossover")->load();
    float midHighFreq = apvts->getRawParameterValue("midHighCrossover")->load();
    float lowPanParam = apvts->getRawParameterValue("lowPan")->load();
    float midPanParam = apvts->getRawParameterValue("midPan")->load();
    float highPanParam = apvts->getRawParameterValue("highPan")->load();

    // �����������, ��� ������� ���������� �� ������������ �����������
    midHighFreq = std::max(midHighFreq, lowMidFreq + 1.0f); // midHigh ������ ���� ���� lowMid

    // ��������� ������� ����� ��������
    leftLPF1.setCutoffFrequency(lowMidFreq); rightLPF1.setCutoffFrequency(lowMidFreq);
    leftHPF1.setCutoffFrequency(lowMidFreq); rightHPF1.setCutoffFrequency(lowMidFreq);
    leftLPF2.setCutoffFrequency(midHighFreq); rightLPF2.setCutoffFrequency(midHighFreq);
    leftHPF2.setCutoffFrequency(midHighFreq); rightHPF2.setCutoffFrequency(midHighFreq);
    leftAPF2.setCutoffFrequency(midHighFreq); rightAPF2.setCutoffFrequency(midHighFreq); // All-Pass �� ������� 2-�� �����

    // ������������ ����� �������� (Constant Power Panning)
    auto calculatePanGains = [](float panParam, std::atomic<float>& leftGain, std::atomic<float>& rightGain)
        {
            constexpr float piOverTwo = (float)M_PI * 0.5f;
            float angle = (panParam * 0.5f + 0.5f) * piOverTwo; // ����������� -1..+1 � 0..pi/2
            leftGain.store(std::cos(angle));  // ���� ������ = cos(angle)
            rightGain.store(std::sin(angle)); // ���� ������� = sin(angle)
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

    updateParameters(); // ���������� ���������� (����� ��������������)

    // --- ���������� ������ � ���������� ---
    auto inputBlock = juce::dsp::AudioBlock<float>(buffer);
    auto lowBandBlock = juce::dsp::AudioBlock<float>(lowBandBuffer);
    auto midBandBlock = juce::dsp::AudioBlock<float>(midBandBuffer);
    auto highBandBlock = juce::dsp::AudioBlock<float>(highBandBuffer);
    auto hpf1OutputBlock = juce::dsp::AudioBlock<float>(hpf1OutputBuffer);

    // �������� ����� ��� ������� ������
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

    // --- ���������� �� ������ (����� � ������� ������������) ---

    // 1. ������������� ������� ������������ �����
    leftLowBlock.copyFrom(leftInputBlock);
    rightLowBlock.copyFrom(rightInputBlock);
    leftMidBlock.copyFrom(leftInputBlock);      // �� ������ � �����, ����� �������� HPF1
    rightMidBlock.copyFrom(rightInputBlock);
    // � �� ����� ���������� ���� �� ����, �� ����� ���������� �� ������ HPF1

    // 2. ��������� �� ������ (LPF1 -> APF2)
    juce::dsp::ProcessContextReplacing<float> leftLowCtx(leftLowBlock);
    juce::dsp::ProcessContextReplacing<float> rightLowCtx(rightLowBlock);
    leftLPF1.process(leftLowCtx);    // -> leftLowBlock = �� L (����� LPF1)
    rightLPF1.process(rightLowCtx);   // -> rightLowBlock = �� R (����� LPF1)
    leftAPF2.process(leftLowCtx);    // -> leftLowBlock = �� L (����� LPF1 + APF2)
    rightAPF2.process(rightLowCtx);   // -> rightLowBlock = �� R (����� LPF1 + APF2)

    // 3. ��������� �� ������ (HPF1 -> LPF2)
    juce::dsp::ProcessContextReplacing<float> leftMidCtx(leftMidBlock);
    juce::dsp::ProcessContextReplacing<float> rightMidCtx(rightMidBlock);
    leftHPF1.process(leftMidCtx);    // -> leftMidBlock = (��+��) L
    rightHPF1.process(rightMidCtx);   // -> rightMidBlock = (��+��) R
    // �������� ��������� HPF1 �� ��������� ����� � � ����� ��
    leftHpf1OutBlock.copyFrom(leftMidBlock);
    rightHpf1OutBlock.copyFrom(rightMidBlock);
    leftHighBlock.copyFrom(leftHpf1OutBlock); // <--- �������� ���� ��� ��
    rightHighBlock.copyFrom(rightHpf1OutBlock);
    // ��������� LPF2 � (��+��) � ������ ��
    leftLPF2.process(leftMidCtx);    // -> leftMidBlock = �� L
    rightLPF2.process(rightMidCtx);   // -> rightMidBlock = �� R

    // 4. ��������� �� ������ (���������� ������������� ��������� HPF1 � ��������� HPF2)
    juce::dsp::ProcessContextReplacing<float> leftHighCtx(leftHighBlock);
    juce::dsp::ProcessContextReplacing<float> rightHighCtx(rightHighBlock);
    leftHPF2.process(leftHighCtx);   // -> leftHighBlock = �� L
    rightHPF2.process(rightHighCtx);  // -> rightHighBlock = �� R

    // --- ���������� �������� � ������������ ---
    buffer.clear(); // ������� ������������ ����� ��� ������ ����������

    auto* leftOut = buffer.getWritePointer(0);
    auto* rightOut = buffer.getWritePointer(1);
    // ������ �� ��������� ������� �����
    const auto* leftLowIn = lowBandBuffer.getReadPointer(0);
    const auto* rightLowIn = lowBandBuffer.getReadPointer(1);
    const auto* leftMidIn = midBandBuffer.getReadPointer(0);
    const auto* rightMidIn = midBandBuffer.getReadPointer(1);
    const auto* leftHighIn = highBandBuffer.getReadPointer(0);
    const auto* rightHighIn = highBandBuffer.getReadPointer(1);

    // ��������� ��������� �����
    float llg = leftLowGain.load(); float rlg = rightLowGain.load();
    float lmg = leftMidGain.load(); float rmg = rightMidGain.load();
    float lhg = leftHighGain.load(); float rhg = rightHighGain.load();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // ���������������
        float lowL = leftLowIn[sample] * llg;
        float lowR = rightLowIn[sample] * rlg;
        float midL = leftMidIn[sample] * lmg;
        float midR = rightMidIn[sample] * rmg;
        float highL = leftHighIn[sample] * lhg;
        float highR = rightHighIn[sample] * rhg;

        // ������������
        leftOut[sample] = lowL + midL + highL;
        rightOut[sample] = lowR + midR + highR;
    }
}

//==============================================================================
bool MBRPAudioProcessor::hasEditor() const
{
    return true; // ��, �������� ����
}

juce::AudioProcessorEditor* MBRPAudioProcessor::createEditor()
{
    // ������� � ���������� ��������� ������ ���������
    return new MBRPAudioProcessorEditor(*this);
}

//==============================================================================
void MBRPAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // ��������� ��������� APVTS
    juce::MemoryOutputStream mos(destData, false);
    apvts->state.writeToStream(mos);
}

void MBRPAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // ��������� ��������� APVTS
    auto tree = juce::ValueTree::readFromData(data, size_t(sizeInBytes));
    if (tree.isValid())
    {
        apvts->replaceState(tree);
        updateParameters(); // ��������� ��������� ������� ����� �������� ���������
    }
}

//==============================================================================
// ��������� �������, ���������� ������ ��� �������� ���������� �������
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MBRPAudioProcessor();
}