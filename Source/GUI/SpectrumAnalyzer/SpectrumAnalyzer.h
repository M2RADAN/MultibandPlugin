#pragma once

#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"
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


        std::vector<float> displayData;    
        std::vector<float> peakHoldLevels;   
        std::atomic<float> peakDbLevel{ mindB }; 


        void drawFrequencyGrid(juce::Graphics& g, const juce::Rectangle<float>& bounds);
        void drawGainScale(juce::Graphics& g, const juce::Rectangle<float>& bounds);
        void drawSpectrumAndPeaks(juce::Graphics& g, const juce::Rectangle<float>& bounds);
        void drawFrequencyMarkers(juce::Graphics& g, const juce::Rectangle<float>& bounds);


        float frequencyToX(float freq, float width) const;

 
        static constexpr float minFreq = 20.0f;
        static constexpr float maxFreq = 20000.0f;
        static constexpr float mindB = -100.0f;
        static constexpr float maxdB = 30.0f;


        static constexpr float smoothingAlpha = 0.2f; 
        static constexpr float peakHoldDecayFactor = 0.957f; 


        const juce::Colour backgroundColour{ juce::Colours::black };
        const juce::Colour spectrumFillColour{ juce::Colours::lightblue.withAlpha(0.2f) };
        const juce::Colour spectrumLineColour{ juce::Colours::lightblue };
        const juce::Colour peakHoldLineColour{ juce::Colours::lightgoldenrodyellow.withAlpha(0.7f) };

        const juce::Colour overZeroDbLineColour{ juce::Colours::red };
        const juce::Colour zeroDbLineColour{ juce::Colours::white.withAlpha(0.5f) };
        const juce::Colour gridLineColour{ juce::Colours::dimgrey.withAlpha(0.3f) };
        const juce::Colour gridTextColour{ juce::Colours::lightgrey.withAlpha(0.7f) };
        const juce::Colour peakTextColour{ juce::Colours::white };


        int resizeDebounceInFrames = 0; 


        juce::dsp::FFT fftInput{ MBRPAudioProcessor::fftOrder };
        juce::dsp::FFT fftOutput{ MBRPAudioProcessor::fftOrder };


        juce::dsp::WindowingFunction<float> hannWindow{ static_cast<size_t>(MBRPAudioProcessor::fftSize),
            juce::dsp::WindowingFunction<float>::hann };


        juce::AudioBuffer<float> fftBufferInput{ 1, MBRPAudioProcessor::fftSize * 2 };
        juce::AudioBuffer<float> fftBufferOutput{ 1, MBRPAudioProcessor::fftSize * 2 }; 

        juce::AudioBuffer<float> avgInput{ 5, MBRPAudioProcessor::fftSize / 2 };
        juce::AudioBuffer<float> avgOutput{ 5, MBRPAudioProcessor::fftSize / 2 }; 
        int avgInputPtr = 1;
        int avgOutputPtr = 1; 


        juce::CriticalSection pathCreationLock;

        struct fftPoint
        {
            int firstBinIndex = 0;
            int lastBinIndex = 1;
            int x = 0;
        };
        int fftPointsSize = 0; 
        std::vector<fftPoint> fftPoints; 


        static float getFftPointLevel(const float* averagedMagnitudes, const fftPoint& point);

        void recalculateFftPoints();
        void drawNextFrame(); 




        static float getTextLayoutWidth(const juce::String& text, const juce::Font& font);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
    };

} 