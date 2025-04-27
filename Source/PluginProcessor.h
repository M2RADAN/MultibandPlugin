
#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
// --- �������� �������������� ��������� ---
// �������, ��� ��� ����� ���������� � ���������� ������������ ���� MBRP_GUI
// ���� ��� � ��� �� �����, ���� ����� ���� ������, �������� "Fifo.h"
#include "DSP/Fifo.h"
#include "DSP/SingleChannelSampleFifo.h"
// ---------------------------------------

//==============================================================================
class MBRPAudioProcessor : public juce::AudioProcessor
{
public:
    MBRPAudioProcessor();
    ~MBRPAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // --- APVTS � Layout ---
    using APVTS = juce::AudioProcessorValueTreeState;
    static APVTS::ParameterLayout createParameterLayout(); // ���������� ������� �������� Layout
    // --- ������� ������������ ������������� APVTS ������ ---
    // APVTS apvts{ *this, nullptr, "Parameters", createParameterLayout() };
    // -------------------------------------------------------

    // ����� ������� � APVTS (���������� ��������� ���� apvts)
    juce::AudioProcessorValueTreeState& getAPVTS() { return *apvts; } // ���������� �� ������

    // --- FIFO ��� ����������� ---
    using SimpleFifo = MBRP_GUI::SingleChannelSampleFifo<juce::AudioBuffer<float>>;
    SimpleFifo leftChannelFifo{ MBRP_GUI::Channel::Left };
    SimpleFifo rightChannelFifo{ MBRP_GUI::Channel::Right };
    // ---------------------------

    // --- ��������� �� ��������� ��� ������� � Processor ---
    juce::AudioParameterFloat* lowMidCrossover{ nullptr };
    juce::AudioParameterFloat* midHighCrossover{ nullptr };
    // -----------------------------------------------

private:
    // --- ��������� ���������� ���������� APVTS ����� ---
    juce::ScopedPointer<juce::AudioProcessorValueTreeState> apvts;
    // --------------------------------------------------

    // --- ������� ---
    using Filter = juce::dsp::LinkwitzRileyFilter<float>;
    Filter leftLowMidLPF, rightLowMidLPF;
    Filter leftLowMidHPF, rightLowMidHPF;
    Filter leftMidHighLPF, rightMidHighLPF;
    Filter leftMidHighHPF, rightMidHighHPF;

    // --- ��������� �������� ---
    std::atomic<float> leftLowGain{ 1.f }, rightLowGain{ 1.f };
    std::atomic<float> leftMidGain{ 1.f }, rightMidGain{ 1.f };
    std::atomic<float> leftHighGain{ 1.f }, rightHighGain{ 1.f };

    // --- ��������� ������ ---
    juce::AudioBuffer<float> intermediateBuffer1;
    juce::AudioBuffer<float> lowBandBuffer;
    juce::AudioBuffer<float> midBandBuffer;
    juce::AudioBuffer<float> highBandBuffer;

    float lastSampleRate = 44100.0f;

    // --- ������� ���������� ---
    void updateParameters();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MBRPAudioProcessor)
};
