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
}

MBRPAudioProcessor::~MBRPAudioProcessor()
{
}

//==============================================================================
const juce::String MBRPAudioProcessor::getName() const
{
    return JucePlugin_Name; // ��� �� Projucer
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
    spec.numChannels = 1; // ������� ������������ �� ������ ������

    // ���������� ��������
    leftLowMidLPF.prepare(spec); rightLowMidLPF.prepare(spec);
    leftLowMidHPF.prepare(spec); rightLowMidHPF.prepare(spec);
    leftMidHighLPF.prepare(spec); rightMidHighLPF.prepare(spec);
    leftMidHighHPF.prepare(spec); rightMidHighHPF.prepare(spec);

    // ����� ��������� ��������
    leftLowMidLPF.reset(); rightLowMidLPF.reset();
    leftLowMidHPF.reset(); rightLowMidHPF.reset();
    leftMidHighLPF.reset(); rightMidHighLPF.reset();
    leftMidHighHPF.reset(); rightMidHighHPF.reset();

    // ��������� ������ ��� �������
    int numOutputChannels = getTotalNumOutputChannels(); // ������ 2 ��� ������
    intermediateBuffer1.setSize(numOutputChannels, samplesPerBlock, false, true, true); // ������� ��� ��������� �������
    lowBandBuffer.setSize(numOutputChannels, samplesPerBlock, false, true, true);
    midBandBuffer.setSize(numOutputChannels, samplesPerBlock, false, true, true);
    highBandBuffer.setSize(numOutputChannels, samplesPerBlock, false, true, true);

    updateParameters(); // ��������� ��������� ��� ������
}

void MBRPAudioProcessor::releaseResources()
{
    // ����������� �������, ���� �����
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

    // ��������� ������� ��������
    leftLowMidLPF.setCutoffFrequency(lowMidFreq); rightLowMidLPF.setCutoffFrequency(lowMidFreq);
    leftLowMidHPF.setCutoffFrequency(lowMidFreq); rightLowMidHPF.setCutoffFrequency(lowMidFreq);
    leftMidHighLPF.setCutoffFrequency(midHighFreq); rightMidHighLPF.setCutoffFrequency(midHighFreq);
    leftMidHighHPF.setCutoffFrequency(midHighFreq); rightMidHighHPF.setCutoffFrequency(midHighFreq);

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

    updateParameters();

    // --- ���������� ������ � ���������� ---
    auto inputBlock = juce::dsp::AudioBlock<float>(buffer);
    auto leftInputBlock = inputBlock.getSingleChannelBlock(0);
    auto rightInputBlock = inputBlock.getSingleChannelBlock(1);

    auto lowBandBlock = juce::dsp::AudioBlock<float>(lowBandBuffer);
    auto midBandBlock = juce::dsp::AudioBlock<float>(midBandBuffer);
    auto highBandBlock = juce::dsp::AudioBlock<float>(highBandBuffer);
    // ��� ��� ��� ����� ����� ��� (��+��)
    auto intermediateBlock = juce::dsp::AudioBlock<float>(intermediateBuffer1);

    auto leftLowBlock = lowBandBlock.getSingleChannelBlock(0);
    auto rightLowBlock = lowBandBlock.getSingleChannelBlock(1);
    auto leftMidBlock = midBandBlock.getSingleChannelBlock(0);
    auto rightMidBlock = midBandBlock.getSingleChannelBlock(1);
    auto leftHighBlock = highBandBlock.getSingleChannelBlock(0);
    auto rightHighBlock = highBandBlock.getSingleChannelBlock(1);
    auto leftInterBlock = intermediateBlock.getSingleChannelBlock(0);
    auto rightInterBlock = intermediateBlock.getSingleChannelBlock(1);

    // --- ���������� �� ������ �� ����� ---

    // 1. �������� �� � (��+��)
    leftLowBlock.copyFrom(leftInputBlock);
    rightLowBlock.copyFrom(rightInputBlock);
    leftInterBlock.copyFrom(leftInputBlock);
    rightInterBlock.copyFrom(rightInputBlock);

    juce::dsp::ProcessContextReplacing<float> leftLowCtx(leftLowBlock);
    juce::dsp::ProcessContextReplacing<float> rightLowCtx(rightLowBlock);
    juce::dsp::ProcessContextReplacing<float> leftInterCtx(leftInterBlock);
    juce::dsp::ProcessContextReplacing<float> rightInterCtx(rightInterBlock);

    leftLowMidLPF.process(leftLowCtx);   // -> leftLowBlock = �� L
    rightLowMidLPF.process(rightLowCtx);  // -> rightLowBlock = �� R
    leftLowMidHPF.process(leftInterCtx);  // -> leftInterBlock = (��+��) L
    rightLowMidHPF.process(rightInterCtx); // -> rightInterBlock = (��+��) R

    // 2. �������� �� � �� �� (��+��)
    // �������� (��+��) � ������ ��� �� � �� ����������
    leftMidBlock.copyFrom(leftInterBlock);
    rightMidBlock.copyFrom(rightInterBlock);
    leftHighBlock.copyFrom(leftInterBlock);
    rightHighBlock.copyFrom(rightInterBlock);

    juce::dsp::ProcessContextReplacing<float> leftMidCtx(leftMidBlock);
    juce::dsp::ProcessContextReplacing<float> rightMidCtx(rightMidBlock);
    juce::dsp::ProcessContextReplacing<float> leftHighCtx(leftHighBlock);
    juce::dsp::ProcessContextReplacing<float> rightHighCtx(rightHighBlock);

    // ��������� ������ ������ ��������
    leftMidHighLPF.process(leftMidCtx);   // -> leftMidBlock = �� L (�� ��+��)
    rightMidHighLPF.process(rightMidCtx);  // -> rightMidBlock = �� R (�� ��+��)
    leftMidHighHPF.process(leftHighCtx);   // -> leftHighBlock = �� L (�� ��+��)
    rightMidHighHPF.process(rightHighCtx);  // -> rightHighBlock = �� R (�� ��+��)

    // --- ���������� �������� � ������������ ---
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

    for (int sample = 0; sample < numSamples; ++sample)
    {
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