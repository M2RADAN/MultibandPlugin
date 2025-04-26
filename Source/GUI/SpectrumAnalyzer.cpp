#include "SpectrumAnalyzer.h"
#include "Utilities.h"
#include "LookAndFeel.h"

namespace MBRP_GUI
{
    // --- ���������� SpectrumAnalyzer ---
    SpectrumAnalyzer::SpectrumAnalyzer(MBRPAudioProcessor& processor, // ��������� MBRPAudioProcessor
        SimpleFifo& leftFifo,
        SimpleFifo& rightFifo) :
        audioProcessor(processor), // ��������� ������ �� ���������
        sampleRate(processor.getSampleRate()), // �������� ��������� sampleRate
        leftPathProducer(leftFifo), // �������������� PathProducers
        rightPathProducer(rightFifo)
    {
        leftPathProducer.updateNegativeInfinity(NEG_INFINITY);
        rightPathProducer.updateNegativeInfinity(NEG_INFINITY);
        startTimerHz(60);
    }

    void SpectrumAnalyzer::timerCallback()
    {
        if (shouldShowFFTAnalysis) {
            auto bounds = getLocalBounds();
            auto fftBounds = SpectrumAnalyzerUtils::getAnalysisArea(bounds).toFloat();
            sampleRate = audioProcessor.getSampleRate();
            leftPathProducer.process(fftBounds, sampleRate);
            rightPathProducer.process(fftBounds, sampleRate);
        }
        repaint();
    }

    void SpectrumAnalyzer::paint(juce::Graphics& g)
    {
        using namespace juce;
        g.fillAll(Colours::black);
        auto bounds = getLocalBounds();
        drawBackgroundGrid(g, bounds);
        drawTextLabels(g, bounds);
        if (shouldShowFFTAnalysis) {
            drawFFTAnalysis(g, bounds);
        }
    }

    void SpectrumAnalyzer::resized()
    {
        using namespace juce;
        auto bounds = getLocalBounds();
        auto fftBounds = SpectrumAnalyzerUtils::getAnalysisArea(bounds).toFloat();
        auto negInf = mapY(NEG_INFINITY, fftBounds.getBottom(), fftBounds.getY());
        leftPathProducer.updateNegativeInfinity(negInf);
        rightPathProducer.updateNegativeInfinity(negInf);
    }

    void SpectrumAnalyzer::drawFFTAnalysis(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        using namespace juce;
        auto analysisArea = SpectrumAnalyzerUtils::getAnalysisArea(bounds);
        Graphics::ScopedSaveState state(g); // ��������� ��������� �������
        g.reduceClipRegion(analysisArea); // ������������ ��������� ���� ��������

        auto leftPath = leftPathProducer.getPath();
        auto rightPath = rightPathProducer.getPath();
        // --- ������� ��������� ���� ---
        DBG("DRAW FFT: Left Path is empty = " << (leftPath.isEmpty() ? "Yes" : "No")
            << ", Right Path is empty = " << (rightPath.isEmpty() ? "Yes" : "No"));
        // -------------------------------

        leftPath.applyTransform(AffineTransform().translation(analysisArea.getX(), 0));
        g.setColour(ColorScheme::getInputSignalColor());
        g.strokePath(leftPath, PathStrokeType(1.f));

        rightPath.applyTransform(AffineTransform().translation(analysisArea.getX(), 0));
        g.setColour(ColorScheme::getOutputSignalColor());
        g.strokePath(rightPath, PathStrokeType(1.f));
    }

    // --- ���������� ��������������� ������� ��������� ---

    std::vector<float> SpectrumAnalyzer::getFrequencies()
    {
        // ���������� ����������� ����� ������ ��� �����/�����
        return { 20, /*30, 40,*/ 50, 100, 200, /*300, 400,*/ 500, 1000, 2000, /*3000, 4000,*/ 5000, 10000, 20000 };
    }

    std::vector<float> SpectrumAnalyzer::getGains()
    {
        // ���������� ������ ��� �����/����� � ����� ��������� dB
        std::vector<float> values;
        float increment = 12.f; // ��� �������� �����
        // ��������� ������� ���� �� ����� ����
        for (auto db = MAX_DB; db >= NEG_INFINITY; db -= increment)
        {
            values.push_back(db);
        }
        // ��������, ��� 0 dB � ������ ������� ������������
        if (std::find_if(values.begin(), values.end(), [](float db) { return std::abs(db) < 0.01f; }) == values.end())
            values.push_back(0.0f);
        if (values.back() > NEG_INFINITY + 0.1f)
            values.push_back(NEG_INFINITY);

        // ����� �������� ������������� ������� (��������, ������� 6)
        for (auto db = MAX_DB - increment / 2.0f; db >= NEG_INFINITY; db -= increment)
        {
            if (std::find_if(values.begin(), values.end(), [db](float v) { return std::abs(v - db) < 0.01f; }) == values.end())
                values.push_back(db);
        }

        std::sort(values.begin(), values.end()); // ��������� ��� ����������� �����������
        return values;
    }

    std::vector<float> SpectrumAnalyzer::getXs(const std::vector<float>& freqs, float left, float width)
    {
        // ��������� X-���������� ��� �������� ������
        std::vector<float> xs;
        for (auto f : freqs)
        {
            // ���������� mapX �� Utilities
            xs.push_back(mapX(f, juce::Rectangle<float>(left, 0, width, 0)));
        }
        return xs;
    }

    void SpectrumAnalyzer::drawBackgroundGrid(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        using namespace juce;
        auto freqs = getFrequencies();
        auto analysisArea = SpectrumAnalyzerUtils::getAnalysisArea(bounds);
        auto left = analysisArea.getX();
        auto right = analysisArea.getRight();
        auto top = analysisArea.getY();
        auto bottom = analysisArea.getBottom();
        auto width = analysisArea.getWidth();

        auto xs = getXs(freqs, left, width);
        g.setColour(ColorScheme::getAnalyzerGridColor()); // ���� �����

        // ������ ������������ �����
        for (auto x : xs) { g.drawVerticalLine(roundToInt(x), top, bottom); }

        auto gains = getGains();
        // ������ �������������� �����
        for (auto gainDb : gains)
        {
            // ���������� mapY �� Utilities
            auto y = mapY(gainDb, (float)bottom, (float)top);
            // �������� ����� 0 �� ������
            bool isZeroDb = std::abs(gainDb) < 0.01f;
            // ������ �������� ������� (������� 12 ��� 0) ���� ��������
            bool isMajorTick = isZeroDb || (int(gainDb) % 12 == 0);
            g.setColour(isZeroDb ? ColorScheme::getSliderRangeTextColor().withAlpha(0.75f)
                : (isMajorTick ? ColorScheme::getAnalyzerGridColor() : ColorScheme::getTickColor())); // ���� �����
            g.drawHorizontalLine(roundToInt(y), left, right);
        }
    }

    void SpectrumAnalyzer::drawTextLabels(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        using namespace juce;
        g.setColour(ColorScheme::getScaleTextColor()); // ���� ������ �����
        const int fontHeight = 10;
        g.setFont(fontHeight);

        auto analysisArea = SpectrumAnalyzerUtils::getAnalysisArea(bounds);
        auto left = analysisArea.getX() + 1;
        auto top = analysisArea.getY();
        auto bottom = analysisArea.getBottom();
        auto width = analysisArea.getWidth();
        auto right = analysisArea.getRight();

        auto freqs = getFrequencies();
        auto xs = getXs(freqs, left, width);

        // ������ ����� ������ ��� ������
        for (size_t i = 0; i < freqs.size(); ++i)
        {
            auto f = freqs[i];
            auto x = xs[i];
            bool addK = false;
            String str;
            if (f > 999.f) { addK = true; f /= 1000.f; }
            str << f; if (addK) str << "k";
            // str << "Hz"; // ����� ������ Hz ��� ������������

            auto textWidth = g.getCurrentFont().getStringWidth(str);
            Rectangle<int> r;
            r.setSize(textWidth, fontHeight);
            r.setCentre(roundToInt(x), bottom + fontHeight / 2 + 2); // ������� ��� ������
            // ������, ������ ���� ��������� ����������
            if (r.getX() >= analysisArea.getX() && r.getRight() <= analysisArea.getRight())
                g.drawFittedText(str, r, juce::Justification::centred, 1);
        }

        auto gain = getGains();
        // ������ ����� ������� ����� � ������ �� �����
        for (auto gDb : gain)
        {
            // ���������� ����� ��� �������� �������
            if (std::abs(gDb) < 0.01f || (int(gDb) != 0 && int(gDb) % 12 == 0) || gDb == MAX_DB || gDb == NEG_INFINITY)
            {
                auto y = mapY(gDb, (float)bottom, (float)top); // ���������� mapY �� Utilities
                String str;
                if (gDb > 0) str << "+";
                str << roundToInt(gDb);

                auto textWidth = g.getCurrentFont().getStringWidth(str);
                Rectangle<int> r;
                r.setSize(textWidth, fontHeight);

                // ����� ������
                r.setX(right + 4); // ������ ������
                r.setCentre(r.getCentreX(), roundToInt(y));
                if (r.getY() >= top && r.getBottom() <= bottom) // �������� �� ������
                    g.drawFittedText(str, r, juce::Justification::centredLeft, 1);

                // ����� �����
                r.setX(left - textWidth - 4); // ������ �����
                r.setCentre(r.getCentreX(), roundToInt(y));
                if (r.getY() >= top && r.getBottom() <= bottom) // �������� �� ������
                    g.drawFittedText(str, r, juce::Justification::centredRight, 1);
            }
        }
    }

    // --- ���������� AnalyzerOverlay ---
    AnalyzerOverlay::AnalyzerOverlay(juce::AudioParameterFloat& lowXover,
        juce::AudioParameterFloat& midXover) :
        lowMidXoverParam(lowXover),
        midHighXoverParam(midXover)
    {
        setInterceptsMouseClicks(false, false); // ������� �� ������ ������������� �����
        startTimerHz(30); // ������ ��� ������� ����������� �����
    }

    void AnalyzerOverlay::paint(juce::Graphics& g)
    {
        // ������ ����� ���������� ������ �����������
        drawCrossoverLines(g, getLocalBounds());
    }

    void AnalyzerOverlay::timerCallback()
    {
        // ������ ����������� �����������, ����� ����� �����������
        repaint();
    }

    void AnalyzerOverlay::drawCrossoverLines(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        using namespace juce;
        // ���������� �� �� �������, ��� � ����������
        bounds = SpectrumAnalyzerUtils::getAnalysisArea(bounds);
        const float top = static_cast<float>(bounds.getY());
        const float bottom = static_cast<float>(bounds.getBottom());

        // �������� ���������� X ��� ������� �������� ����������
        float lowMidX = mapX(lowMidXoverParam.get(), bounds.toFloat()); // ���������� mapX
        float midHighX = mapX(midHighXoverParam.get(), bounds.toFloat()); // ���������� mapX

        g.setColour(Colours::lightblue.withAlpha(0.7f)); // ���� �����
        float lineThickness = 1.5f;

        // ������ �����, ���� ��� �������� � ������� �������
        if (lowMidX >= bounds.getX() && lowMidX <= bounds.getRight())
            g.drawVerticalLine(roundToInt(lowMidX), top, bottom);
        if (midHighX >= bounds.getX() && midHighX <= bounds.getRight())
            g.drawVerticalLine(roundToInt(midHighX), top, bottom);

        // �����������: �������� "�����" ��� ������� �� ������
        // g.setColour(Colours::yellow);
        // g.fillEllipse(lowMidX - 3, top, 6, 6);
        // g.fillEllipse(midHighX - 3, top, 6, 6);
    }

} // ����� namespace MBRP_GUI