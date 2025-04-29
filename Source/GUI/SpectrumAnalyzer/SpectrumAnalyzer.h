#pragma once

#include <JuceHeader.h>
#include "../Source/PluginProcessor.h" // Ïóòü ê âàøåìó ïðîöåññîðó

namespace MBRP_GUI
{

    class SpectrumAnalyzer final : public juce::Component, juce::Timer
    {
    public:
        explicit SpectrumAnalyzer(MBRPAudioProcessor& p);
        ~SpectrumAnalyzer() override = default;

        void paint(juce::Graphics&) override;
        void resized() override;
        void timerCallback() override;

    private:
        MBRPAudioProcessor& processor;

        // --- Äàííûå äëÿ îòîáðàæåíèÿ ---
        std::vector<float> displayData;      // Äàííûå äëÿ îñíîâíîé êðèâîé (ñ EMA)
        std::vector<float> peakHoldLevels;   // Äàííûå äëÿ ëèíèè ïèêîâ (ñ decay)
        std::atomic<float> peakDbLevel{ mindB }; // Ïèê âñåãî ñïåêòðà (ìãíîâåííûé)

        // --- Ôóíêöèè îòðèñîâêè (õåëïåðû) ---
        void drawFrequencyGrid(juce::Graphics& g, const juce::Rectangle<float>& bounds);
        void drawGainScale(juce::Graphics& g, const juce::Rectangle<float>& bounds);
        void drawSpectrumAndPeaks(juce::Graphics& g, const juce::Rectangle<float>& bounds);

        // --- Âñïîìîãàòåëüíûå ôóíêöèè ---
        float frequencyToX(float freq, float width) const;

        // --- Êîíñòàíòû ---
        static constexpr float minFreq = 20.0f;
        static constexpr float maxFreq = 20000.0f;
        static constexpr float mindB = -100.0f;
        static constexpr float maxdB = 30.0f;

        // --- Êîýôôèöèåíòû ---
        static constexpr float smoothingAlpha = 0.2f; // Êîýôôèöèåíò ñãëàæèâàíèÿ EMA
        static constexpr float peakHoldDecayFactor = 0.957f; // Çàòóõàíèå ïèêîâ

        // --- Öâåòà ---
        const juce::Colour backgroundColour{ juce::Colours::black };
        const juce::Colour spectrumFillColour{ juce::Colours::lightblue.withAlpha(0.2f) };
        const juce::Colour spectrumLineColour{ juce::Colours::lightblue };
        const juce::Colour peakHoldLineColour{ juce::Colours::lightgoldenrodyellow.withAlpha(0.7f) };
        // const juce::Colour overZeroDbFillColour   { juce::Colours::red.withAlpha(0.3f) }; // Ïîêà íå èñïîëüçóåòñÿ
        const juce::Colour overZeroDbLineColour{ juce::Colours::red };
        const juce::Colour zeroDbLineColour{ juce::Colours::white.withAlpha(0.5f) };
        const juce::Colour gridLineColour{ juce::Colours::dimgrey.withAlpha(0.3f) };
        const juce::Colour gridTextColour{ juce::Colours::lightgrey.withAlpha(0.7f) };
        const juce::Colour peakTextColour{ juce::Colours::white };


        // === ÄÎÁÀÂËÅÍÛ ÍÅÄÎÑÒÀÞÙÈÅ ×ËÅÍÛ ÈÇ witte/EqEditor ===
        int resizeDebounceInFrames = 0; // Çàäåðæêà ïåðåñ÷åòà ïîñëå ðåñàéçà

        // Îáúåêòû FFT (èñïîëüçóåì ïîðÿäîê èç ïðîöåññîðà)
        juce::dsp::FFT fftInput{ MBRPAudioProcessor::fftOrder };
        juce::dsp::FFT fftOutput{ MBRPAudioProcessor::fftOrder }; // Äëÿ âûõîäíîãî ñèãíàëà (åñëè íóæåí)

        // Îêíî Õàííà (èñïîëüçóåì ðàçìåð èç ïðîöåññîðà)
        juce::dsp::WindowingFunction<float> hannWindow{ static_cast<size_t>(MBRPAudioProcessor::fftSize),
            juce::dsp::WindowingFunction<float>::hann };

        // Âðåìåííûå áóôåðû äëÿ FFT
        juce::AudioBuffer<float> fftBufferInput{ 1, MBRPAudioProcessor::fftSize * 2 };
        juce::AudioBuffer<float> fftBufferOutput{ 1, MBRPAudioProcessor::fftSize * 2 }; // Äëÿ âûõîäíîãî

        // Áóôåðû äëÿ ñêîëüçÿùåãî ñðåäíåãî (óñðåäíåíèå ïî 4 êàäðàì + 1 äëÿ ñóììû)
        juce::AudioBuffer<float> avgInput{ 5, MBRPAudioProcessor::fftSize / 2 };
        juce::AudioBuffer<float> avgOutput{ 5, MBRPAudioProcessor::fftSize / 2 }; // Äëÿ âûõîäíîãî
        int avgInputPtr = 1; // Óêàçàòåëü íà òåêóùèé ñëîò óñðåäíåíèÿ (1..N-1)
        int avgOutputPtr = 1; // Äëÿ âûõîäíîãî

        // Áëîêèðîâêà äëÿ áåçîïàñíîãî äîñòóïà ê ïóòÿì èç ðàçíûõ ïîòîêîâ
        juce::CriticalSection pathCreationLock;
        // Ïóòè óáðàíû, ò.ê. ôîðìèðóþòñÿ ëîêàëüíî â paint/drawSpectrumAndPeaks

        // Ñòðóêòóðà äëÿ ñâÿçè áèíîâ FFT ñ òî÷êàìè X íà ýêðàíå
        struct fftPoint
        {
            int firstBinIndex = 0;
            int lastBinIndex = 1;
            int x = 0;
        };
        int fftPointsSize = 0; // Êîëè÷åñòâî àêòóàëüíûõ òî÷åê â fftPoints
        std::vector<fftPoint> fftPoints; // Âåêòîð òî÷åê ýêðàíà

        // Îáúÿâëåíèå ñòàòè÷åñêîé ôóíêöèè ïîëó÷åíèÿ óðîâíÿ äëÿ òî÷êè
        static float getFftPointLevel(const float* averagedMagnitudes, const fftPoint& point);

        // Îáúÿâëåíèå ïðèâàòíûõ ìåòîäîâ
        void recalculateFftPoints();
        void drawNextFrame(); // Òåïåðü ýòî âíóòðåííèé ìåòîä, âûçûâàåìûé èç timerCallback
        // =========================================================


        // Âñïîìîãàòåëüíàÿ ôóíêöèÿ äëÿ òåêñòà
        static float getTextLayoutWidth(const juce::String& text, const juce::Font& font);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
    };

} // namespace MBRP_GUI