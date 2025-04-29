// --- START OF FILE LookAndFeel.h ---
/*
  ==============================================================================

    LookAndFeel.h
    Created: 30 Oct 2021 12:57:21am
    Author:  matkatmusic

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#define USE_LIVE_CONSTANT false

#if USE_LIVE_CONSTANT
#define colorHelper(c) JUCE_LIVE_CONSTANT(c);
#else
#define colorHelper(c) c;
#endif

namespace ColorScheme
{
    
    // --- �������� ����� ������� ���� ---
    inline juce::Colour getBackgroundColor() { return colorHelper(juce::Colours::whitesmoke); } // �������� ��� ��������� (������-�����)
    inline juce::Colour getTextColor() { return colorHelper(juce::Colours::darkgrey); }   // �������� ���� ������
    inline juce::Colour getDarkTextColor() { return colorHelper(juce::Colours::black); }      // ��� ���������� ��� ��������
    inline juce::Colour getSecondaryTextColor() { return colorHelper(juce::Colours::grey); }       // ����� ������ �����

    // --- ����� ����������� (�������������� ��� ������� ���) ---
    inline juce::Colour getAnalyzerBackgroundColor() { return colorHelper(juce::Colours::white); }     // ��� ����������� - �����
    inline juce::Colour getAnalyzerGridBaseColor() { return colorHelper(juce::Colours::lightgrey); } // ����� �����������
    inline juce::Colour getScaleTextColor() { return getTextColor(); }                       // ����� ���� (�����-�����)
    inline juce::Colour getAnalyzerPeakTextColor() { return getTextColor(); }                       // ����� ����
    inline juce::Colour getZeroDbLineBaseColor() { return colorHelper(juce::Colours::darkgrey); }  // ����� 0 �� (������)

    // --- ����� ������� � ����� (����� �������� ������) ---
    inline juce::Colour getInputSignalColor() { return colorHelper(juce::Colour(0xff008cff)); } // ����� ��� �������
    inline juce::Colour getSpectrumFillBaseColor() { return getInputSignalColor(); }
    inline juce::Colour getSpectrumLineColor() { return getInputSignalColor(); }
    inline juce::Colour getPeakHoldLineBaseColor() { return colorHelper(juce::Colours::orange); }    // ��������� ��� �����
    inline juce::Colour getOverZeroDbLineColor() { return colorHelper(juce::Colours::red); }       // ������� ��� ���������� 0 ��

    // --- ����� ����������� (���������) ---
    inline juce::Colour getOrangeBorderColor() { return colorHelper(juce::Colour(255u, 154u, 1u)); } // Low/Mid
    inline juce::Colour getMidHighCrossoverColor() { return colorHelper(juce::Colours::cyan); }        // Mid/High

    // --- ����� ��������� ---
    inline juce::Colour getSliderFillColor() { return colorHelper(juce::Colours::lightgrey); }    // ������� ��������� ��������
    inline juce::Colour getSliderTrackColor() { return colorHelper(juce::Colours::silver); }       // ���� ��������� ��������
    inline juce::Colour getSliderThumbColor() { return colorHelper(juce::Colours::cornflowerblue); } // ����� �������� (�����)
    inline juce::Colour getSliderBorderColor() { return colorHelper(juce::Colours::darkgrey); }     // ������� ��������� ��������

    // --- ����� ������ Low/Mid/High (BandSelectControls) ---
    inline juce::Colour getToggleButtonOffColor() { return colorHelper(juce::Colours::lightgrey); }    // ��� ���������� ������
    inline juce::Colour getToggleButtonOffBorder() { return colorHelper(juce::Colours::grey); }         // ������� ���������� ������
    inline juce::Colour getToggleButtonOffTextColor() { return getTextColor(); }                       // ����� ���������� ������ (�����-�����)
    // ����� ��� �������� ������ ����� ������� �� ������ �����������/�����
    inline juce::Colour getLowBandColor() { return getOrangeBorderColor(); }                 // ���������� ���� Low/Mid ����������
    inline juce::Colour getMidBandColor() { return colorHelper(juce::Colours::lightgreen); } // ������� ��� Mid
    inline juce::Colour getHighBandColor() { return getMidHighCrossoverColor(); }             // ������� ��� High
    inline juce::Colour getToggleButtonOnTextColor() { return colorHelper(juce::Colours::white); }       // ����� �������� ������ (�����)
    
    
    // --- ������������ ����� ---
    inline juce::Colour getGainReductionColor() { return colorHelper(juce::Colour(0xff38c19b)); }
    //inline juce::Colour getInputSignalColor() { return colorHelper(juce::Colour(0xff00e7ff)); } // ���������� ��� ����� �������
    inline juce::Colour getOutputSignalColor() { return colorHelper(juce::Colour(0xff0cff00)); }
    //inline juce::Colour getSliderFillColor() { return colorHelper(juce::Colour(0xff96bbc2)); }
    //inline juce::Colour getOrangeBorderColor() { return colorHelper(juce::Colour(255u, 154u, 1u)); } // ���������� ��� Low/Mid ����������
    inline juce::Colour getSliderRangeTextColor() { return colorHelper(juce::Colour(0u, 172u, 1u)); }
    //inline juce::Colour getSliderBorderColor() { return colorHelper(juce::Colour(0xff0087a2)); }
    inline juce::Colour getThresholdColor() { return colorHelper(juce::Colour(0xffe0760c)); }
    inline juce::Colour getModuleBorderColor() { return colorHelper(juce::Colour(0xff475d9d)); }
    inline juce::Colour getTitleColor() { return colorHelper(juce::Colour(0xff2c92ca)); }
    //inline juce::Colour getAnalyzerGridColor() { return colorHelper(juce::Colour(0xff262626)); } // ������� ���� ����� - ������
    inline juce::Colour getTickColor() { return colorHelper(juce::Colour(0xff313131)); }
    inline juce::Colour getMeterLineColor() { return colorHelper(juce::Colour(0xff3c3c3c)); }
    //inline juce::Colour getScaleTextColor() { return colorHelper(juce::Colours::snow); } // ���������� ��� ������ �� ������
    inline juce::Colour getComponentOutlineColor() { return colorHelper(juce::Colours::darkgrey); }
    // --- ����� ��� ���������������� ����� ��� Analyzer/Overlay ---
    /*inline juce::Colour getAnalyzerBackgroundColor() { return colorHelper(juce::Colours::transparentBlack); }*/
    // ������� ���� ��� ������� ������� (����� ����������� ��� ���������)
    //inline juce::Colour getSpectrumFillBaseColor() { return getInputSignalColor(); /* ��� ������ ������� ���� */ }
    // ���� ����� ������� (��������� � ������� ��� ������� ��� ������ �������� �������)
   /* inline juce::Colour getSpectrumLineColor() { return getInputSignalColor(); }*/
    // ������� ���� ��� ����� ��������� ����� (����� ����������� ��� ���������)
    //inline juce::Colour getPeakHoldLineBaseColor() { return colorHelper(juce::Colours::darkblue); }
    // ���� ����� ���� 0 ��
    //inline juce::Colour getOverZeroDbLineColor() { return colorHelper(juce::Colours::red); }
    // ������� ���� ����� 0 �� (����� ����������� ��� ���������)
    /*inline juce::Colour getZeroDbLineBaseColor() { return colorHelper(juce::Colours::white); }*/
    // ������� ���� ����� ����������� (����� ����������� ��� ���������)
    /*inline juce::Colour getAnalyzerGridBaseColor() { return colorHelper(juce::Colours::dimgrey); }*/
    // ���� ������ �������� ��������
    //inline juce::Colour getAnalyzerPeakTextColor() { return getScaleTextColor(); /* ��� juce::Colours::white */ }
    // ���� ��� Mid/High ����������
    //inline juce::Colour getMidHighCrossoverColor() { return colorHelper(juce::Colours::cyan); }
    inline juce::Colour BackgroundColor() { return colorHelper(juce::Colours::black); }
} // ����� namespace ColorScheme


struct LookAndFeel : juce::LookAndFeel_V4
{
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float minSliderPos, float maxSliderPos,
        const juce::Slider::SliderStyle style, juce::Slider& slider) override;
    void drawRotarySlider(juce::Graphics&,
        int x, int y, int width, int height,
        float sliderPosProportional,
        float rotaryStartAngle,
        float rotaryEndAngle,
        juce::Slider&) override;

    void drawToggleButton(juce::Graphics& g,
        juce::ToggleButton& toggleButton,
        bool shouldDrawButtonAsHighlighted,
        bool shouldDrawButtonAsDown) override;
};

// --- END OF FILE LookAndFeel.h ---