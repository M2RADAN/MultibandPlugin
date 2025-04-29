#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <memory>
#include <array> // Для std::array

//==============================================================================
class MBRPAudioProcessor : public juce::AudioProcessor, public juce::AudioProcessorValueTreeState::Listener
{
public:
    MBRPAudioProcessor();
    ~MBRPAudioProcessor() override;

    // --- Стандартные методы ---
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    const juce::String getName() const override;
    bool acceptsMidi() const override; bool producesMidi() const override; bool isMidiEffect() const override;
    double getTailLengthSeconds() const override; int getNumPrograms() override; int getCurrentProgram() override;
    void setCurrentProgram(int index) override; const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;
    double getSampleRate() const { return lastSampleRate; }


    // --- APVTS и Параметры ---
    using APVTS = juce::AudioProcessorValueTreeState;
    static APVTS::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState& getAPVTS() { return *apvts; }

    juce::AudioParameterFloat* lowMidCrossover{ nullptr };
    juce::AudioParameterFloat* midHighCrossover{ nullptr };
    void parameterChanged(const juce::String& parameterID, float newValue) override;


    // === ЛОГИКА FFT ПРОЦЕССОРА ===
    static constexpr int fftOrder = 11; // 2048
    static constexpr int fftSize = 1 << fftOrder;

    // Методы доступа для анализатора
    std::atomic<bool> nextFFTBlockReady{ false }; // Используем этот флаг
    // FIFO для входного сигнала (перед обработкой)
    juce::AbstractFifo abstractFifoInput{ fftSize * 2 }; // Размер FIFO (например, 2 блока FFT)
    juce::AudioBuffer<float> audioFifoInput;           // Буфер данных для FIFO

    // FIFO для выходного сигнала (после обработки)
    juce::AbstractFifo abstractFifoOutput{ fftSize * 2 };
    juce::AudioBuffer<float> audioFifoOutput;

    // Метод для управления копированием данных в FIFO (вызывается из Editor)
    void setCopyToFifo(bool _copyToFifo);
    // ==========================================

    // --- Члены для размера редактора ---
    juce::Point<int> getSavedEditorSize() const { return editorSize; }
    void setSavedEditorSize(const juce::Point<int>& size) { editorSize = size; }

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState> apvts;

    // --- Фильтры и обработка ---
    using Filter = juce::dsp::LinkwitzRileyFilter<float>;
    Filter leftLowMidLPF, rightLowMidLPF;
    Filter leftMidHighLPF, rightMidHighLPF;
    std::atomic<float> leftLowGain{ 1.f }, rightLowGain{ 1.f };
    std::atomic<float> leftMidGain{ 1.f }, rightMidGain{ 1.f };
    std::atomic<float> leftHighGain{ 1.f }, rightHighGain{ 1.f };
    juce::AudioBuffer<float> intermediateBuffer1, lowBandBuffer, midBandBuffer, highBandBuffer;

    //// === ЧЛЕНЫ ДЛЯ FFT ПРОЦЕССОРА ===
    //juce::dsp::FFT forwardFFT;
    //juce::dsp::WindowingFunction<float> window;
    //std::array<float, fftSize * 2> fftInternalBuffer; // Буфер для Real->Complex FFT
    //std::array<float, fftSize / 2> fftMagnitudes;     // Буфер магнитуд для редактора
    //std::atomic<bool> isFftDataReady{ false };       // Флаг готовности магнитуд
    //// ================================

    std::atomic<bool> copyToFifo{ false };
    void pushNextSampleToFifo(const juce::AudioBuffer<float>& buffer, int startChannel, int numChannels,
        juce::AbstractFifo& absFifo, juce::AudioBuffer<float>& fifo);
    // ------------------------------

    float lastSampleRate = 44100.0f;
    juce::Point<int> editorSize = { 900, 700 };

    // --- Внутренние методы ---
    void updateParameters();
    //void processFFT(const juce::AudioBuffer<float>& inputBuffer); // Метод обработки FFT

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MBRPAudioProcessor)
};