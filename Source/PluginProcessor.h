#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <memory> // Для std::unique_ptr

//==============================================================================
class MBRPAudioProcessor : public juce::AudioProcessor, public juce::AudioProcessorValueTreeState::Listener // Добавили Listener
{
public:
    MBRPAudioProcessor();
    ~MBRPAudioProcessor() override;

    // --- Стандартные методы AudioProcessor ---
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
    double getSampleRate() const { return lastSampleRate; }


    // --- APVTS и Параметры ---
    using APVTS = juce::AudioProcessorValueTreeState;
    static APVTS::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState& getAPVTS() { return *apvts; } // Getter

    juce::AudioParameterFloat* lowMidCrossover{ nullptr };
    juce::AudioParameterFloat* midHighCrossover{ nullptr };
    // std::atomic<float>* prmOutputGain {nullptr}; // Закомментировано, т.к. не используется

    // Listener для параметров (если нужен)
    void parameterChanged(const juce::String& parameterID, float newValue) override;


    // --- Члены для Анализатора Спектра (стиль witte) ---
    static constexpr int fftOrder = 11; // Порядок FFT (2048) (1024 при 10)
    static constexpr int fftSize = 1 << fftOrder;

    std::atomic<bool> nextFFTBlockReady{ false }; // Флаг для анализатора

    juce::AbstractFifo abstractFifoInput{ fftSize * 2 }; // Абстрактное FIFO для входного сигнала
    juce::AudioBuffer<float> audioFifoInput;           // Реальный буфер для входного FIFO

    juce::AbstractFifo abstractFifoOutput{ fftSize * 2 };// Абстрактное FIFO для выходного сигнала
    juce::AudioBuffer<float> audioFifoOutput;          // Реальный буфер для выходного FIFO

    // Метод для управления копированием в FIFO
    void setCopyToFifo(bool _copyToFifo);
    // --------------------------------------------------

    // --- Члены для размера редактора (из witte) ---
    juce::Point<int> getSavedEditorSize() const { return editorSize; }
    void setSavedEditorSize(const juce::Point<int>& size) { editorSize = size; }
    // ---------------------------------------------

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState> apvts;

    // --- Фильтры и обработка ---
    using Filter = juce::dsp::LinkwitzRileyFilter<float>;
    Filter leftLowMidLPF, rightLowMidLPF;
    Filter leftMidHighLPF, rightMidHighLPF;

    std::atomic<float> leftLowGain{ 1.f }, rightLowGain{ 1.f };
    std::atomic<float> leftMidGain{ 1.f }, rightMidGain{ 1.f };
    std::atomic<float> leftHighGain{ 1.f }, rightHighGain{ 1.f };

    juce::AudioBuffer<float> intermediateBuffer1;
    juce::AudioBuffer<float> lowBandBuffer;
    juce::AudioBuffer<float> midBandBuffer;
    juce::AudioBuffer<float> highBandBuffer;

    // --- Управление FIFO ---
    std::atomic<bool> copyToFifo{ false };
    void pushNextSampleToFifo(const juce::AudioBuffer<float>& buffer, int startChannel, int numChannels,
        juce::AbstractFifo& absFifo, juce::AudioBuffer<float>& fifo);

    float lastSampleRate = 44100.0f;
    juce::Point<int> editorSize = { 900, 700 };

    void updateParameters();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MBRPAudioProcessor)
};