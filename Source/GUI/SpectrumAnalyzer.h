/*
  ==============================================================================

    SpectrumAnalyzer.h
    (����: GUI/SpectrumAnalyzer.h)

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Utilities.h" // <-- �������� �������

// --- �������� ��������� ����������� ����� Utilities ---
#include "../PluginProcessor.h" // ����� ��� MBRPAudioProcessor::SimpleFifo
#include "PathProducer.h"
// --------------------------------------------------

// --- ��������� ���� ����������� �������� (���� ��� �� � Utilities.h) ---
// ��� �������, ��� Utilities.h ������� ����� ���� ������ � PluginEditor.h
// � �������� ��� ��������� � ������������ ���� MBRP_GUI
namespace MBRP_GUI
{
    // --- ���������� ���������� ��������� ---
    struct SpectrumAnalyzerUtils
    {
        // ������ ���������� ����������� �������
        static juce::Rectangle<int> getRenderArea(juce::Rectangle<int> bounds);
        static juce::Rectangle<int> getAnalysisArea(juce::Rectangle<int> bounds);
    };
    // ������ ����������� �������� � MAPX/MAPY ������
    // ������ ����������� SpectrumAnalyzerUtils ������

    struct SpectrumAnalyzer : juce::Component, juce::Timer
    {
        // ���������� ������ ��� ���� �� PluginProcessor.h
        using SimpleFifo = MBRPAudioProcessor::SimpleFifo;
        SpectrumAnalyzer(MBRPAudioProcessor& processor, SimpleFifo& left, SimpleFifo& right);
        ~SpectrumAnalyzer() override = default;

        void timerCallback() override;
        void paint(juce::Graphics& g) override;
        void resized() override;

        void toggleAnalysisEnablement(bool enabled) { shouldShowFFTAnalysis = enabled; }

    private:
        MBRPAudioProcessor& audioProcessor;
        double sampleRate;
        bool shouldShowFFTAnalysis = true;

        // ���������� ������� � ��������� �� Utilities.h (��� ����� �����)
        void drawBackgroundGrid(juce::Graphics& g, juce::Rectangle<int> bounds);
        void drawTextLabels(juce::Graphics& g, juce::Rectangle<int> bounds);
        std::vector<float> getFrequencies();
        std::vector<float> getGains();
        std::vector<float> getXs(const std::vector<float>& freqs, float left, float width);

        PathProducer<juce::AudioBuffer<float>> leftPathProducer, rightPathProducer;

        void drawFFTAnalysis(juce::Graphics& g, juce::Rectangle<int> bounds);
    };

    struct AnalyzerOverlay : juce::Component, juce::Timer
    {
        AnalyzerOverlay(juce::AudioParameterFloat& lowXover,
            juce::AudioParameterFloat& midXover);
        void paint(juce::Graphics& g) override;
        void timerCallback() override;

    private:
        void drawCrossoverLines(juce::Graphics& g, juce::Rectangle<int> bounds);
        juce::AudioParameterFloat& lowMidXoverParam;
        juce::AudioParameterFloat& midHighXoverParam;
    };

} //end namespace MBRP_GUI