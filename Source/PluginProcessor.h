#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
// --- ÂÊËÞ×ÀÅÌ ÀÄÀÏÒÈÐÎÂÀÍÍÛÅ ÇÀÃÎËÎÂÊÈ ---
// Óáåäèñü, ÷òî ýòè ôàéëû ñóùåñòâóþò è èñïîëüçóþò ïðîñòðàíñòâî èìåí MBRP_GUI
// Åñëè îíè â òîé æå ïàïêå, ïóòü ìîæåò áûòü äðóãèì, íàïðèìåð "Fifo.h"
//#include "DSP/Fifo.h"
//#include "DSP/SingleChannelSampleFifo.h"
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

    // --- APVTS è Layout ---
    using APVTS = juce::AudioProcessorValueTreeState;
    static APVTS::ParameterLayout createParameterLayout(); // Îáúÿâëåíèå ôóíêöèè ñîçäàíèÿ Layout
    // --- ÓÁÈÐÀÅÌ ÍÅÏÐÀÂÈËÜÍÓÞ ÈÍÈÖÈÀËÈÇÀÖÈÞ APVTS ÎÒÑÞÄÀ ---
    // APVTS apvts{ *this, nullptr, "Parameters", createParameterLayout() };
    // -------------------------------------------------------

    // Ìåòîä äîñòóïà ê APVTS (èñïîëüçóåò ïðèâàòíûé ÷ëåí apvts)
    juce::AudioProcessorValueTreeState& getAPVTS() { return *apvts; } // Âîçâðàùàåì ïî ññûëêå

    // --- FIFO äëÿ àíàëèçàòîðà ---
    //using SimpleFifo = MBRP_GUI::SingleChannelSampleFifo<juce::AudioBuffer<float>>;
    //SimpleFifo leftChannelFifo{ MBRP_GUI::Channel::Left };
    //SimpleFifo rightChannelFifo{ MBRP_GUI::Channel::Right };
    // ---------------------------

    // --- Óêàçàòåëè íà ïàðàìåòðû äëÿ Îâåðëåÿ è Processor ---
    juce::AudioParameterFloat* lowMidCrossover{ nullptr };
    juce::AudioParameterFloat* midHighCrossover{ nullptr };
    // -----------------------------------------------

     // --- ÄÎÁÀÂËßÅÌ ÊÎÍÑÒÀÍÒÛ È ÎÁÚÅÊÒÛ ÄËß FFT ---
    static constexpr int fftOrder = 11; // Ïîðÿäîê FFT (11 => 2048)
    static constexpr int fftSize = 1 << fftOrder; // Ðàçìåð FFT

    // --- ÄÎÁÀÂËßÅÌ ÌÅÒÎÄÛ ÄÎÑÒÓÏÀ ÄËß ÍÎÂÎÃÎ ÀÍÀËÈÇÀÒÎÐÀ ---
    std::atomic<bool>& getIsFftDataReady() { return isFftDataReady; }
    const std::array<float, fftSize>& getFftData() const { return fftInputData; } // Äîñòóï ê äàííûì
    float getSampleRate() const { return lastSampleRate; } // Ñòàíäàðòíûé ìåòîä
    // ----------------------------------------------------

private:
    // --- ÎÑÒÀÂËßÅÌ ÏÐÀÂÈËÜÍÎÅ ÎÁÚßÂËÅÍÈÅ APVTS ÇÄÅÑÜ ---
    std::unique_ptr<juce::AudioProcessorValueTreeState> apvts;
    // --------------------------------------------------

    // --- Ôèëüòðû ---
    using Filter = juce::dsp::LinkwitzRileyFilter<float>;
    Filter leftLowMidLPF, rightLowMidLPF;
    //Filter leftLowMidHPF, rightLowMidHPF;
    Filter leftMidHighLPF, rightMidHighLPF;
    //Filter leftMidHighHPF, rightMidHighHPF;

    // --- Ïàðàìåòðû ïàíîðàìû ---
    std::atomic<float> leftLowGain{ 1.f }, rightLowGain{ 1.f };
    std::atomic<float> leftMidGain{ 1.f }, rightMidGain{ 1.f };
    std::atomic<float> leftHighGain{ 1.f }, rightHighGain{ 1.f };

    // --- Âðåìåííûå áóôåðû ---
    juce::AudioBuffer<float> intermediateBuffer1;
    juce::AudioBuffer<float> lowBandBuffer;
    juce::AudioBuffer<float> midBandBuffer;
    juce::AudioBuffer<float> highBandBuffer;

    // --- ÄÎÁÀÂËßÅÌ ÎÁÚÅÊÒÛ ÄËß FFT ---
    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;
    // Ìàññèâ äëÿ âõîäíûõ äàííûõ FFT (èñïîëüçóåì ïîëîâèíó)
    std::array<float, fftSize> fftInputData; // Õðàíèò ìàãíèòóäû ïîñëå FFT
    // Áóôåð äëÿ êîïèðîâàíèÿ ñýìïëîâ ïåðåä FFT
    std::array<float, fftSize * 2> fftInternalBuffer; // Íóæåí äëÿ performRealOnlyForwardTransform
    std::atomic<bool> isFftDataReady{ false };
    // -------------------------------

    float lastSampleRate = 44100.0f;

    // --- Ôóíêöèè îáíîâëåíèÿ ---
    void updateParameters();

    // --- ÄÎÁÀÂËÅÍÎ: Ôóíêöèÿ îáðàáîòêè FFT ---
    void processFFT(const juce::AudioBuffer<float>& inputBuffer);
    // ---------------------------------------

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MBRPAudioProcessor)
};
