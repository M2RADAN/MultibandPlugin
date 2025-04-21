#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic> // ��� std::atomic

//==============================================================================
/**
    ����� ����� ���������� ��� MBRP (Multiband Panner)
*/
class MBRPAudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    MBRPAudioProcessor();
    ~MBRPAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState& getAPVTS() { return *apvts; }

private:
    //==============================================================================
    juce::ScopedPointer<juce::AudioProcessorValueTreeState> apvts;

    // --- ������� Linkwitz-Riley 2-�� ������� (12 ��/���) ---
    using Filter = juce::dsp::LinkwitzRileyFilter<float>;

    // ������� ��� ���������� �� / (��+��)
    Filter leftLowMidLPF, rightLowMidLPF; // Low Pass < lowMidXover
    Filter leftLowMidHPF, rightLowMidHPF; // High Pass > lowMidXover

    // ������� ��� ���������� �� / �� (����������� � ������ HPF ����)
    Filter leftMidHighLPF, rightMidHighLPF; // Low Pass < midHighXover
    Filter leftMidHighHPF, rightMidHighHPF; // High Pass > midHighXover


    // --- ��������� �������� (��������� ��� ������������������) ---
    std::atomic<float> leftLowGain{ 1.f };
    std::atomic<float> rightLowGain{ 1.f };
    std::atomic<float> leftMidGain{ 1.f };
    std::atomic<float> rightMidGain{ 1.f };
    std::atomic<float> leftHighGain{ 1.f };
    std::atomic<float> rightHighGain{ 1.f };

    // --- ��������� ������ ��� ��������� ---
    juce::AudioBuffer<float> intermediateBuffer1; // ��� (��+��) ����� ������� �������
    juce::AudioBuffer<float> lowBandBuffer;       // ��� ��������� �� ������
    juce::AudioBuffer<float> midBandBuffer;       // ��� ��������� �� ������
    juce::AudioBuffer<float> highBandBuffer;      // ��� ��������� �� ������


    float lastSampleRate = 44100.0f; // ������ sample rate

    // --- ������� ���������� ---
    void updateParameters(); // ��������� ������� � ����� ��������

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MBRPAudioProcessor)
};