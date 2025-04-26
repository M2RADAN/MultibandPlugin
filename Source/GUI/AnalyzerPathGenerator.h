#pragma once
#include <JuceHeader.h>
#include "Utilities.h" // ��� mapX, mapY, ��������
#include "../DSP/Fifo.h"      // ��� ����������� Fifo

namespace MBRP_GUI
{
    // �����, ���������� �� �������������� ������� ������ FFT (� dB) � ������ juce::Path
    template<typename PathType> // PathType ����� juce::Path
    struct AnalyzerPathGenerator
    {
        /*
         converts 'renderData[]' (������ dB) into a juce::Path
         */
        void generatePath(const std::vector<float>& renderData, // ������ � dB
            juce::Rectangle<float> fftBounds,      // ������� ���������
            int fftSize,                           // ������ FFT
            float binWidth,                        // ������ ��������� �������
            float negativeInfinity)                // ������ ������ dB
        {
            auto top = fftBounds.getY();        // ������� Y ����������
            auto bottom = fftBounds.getBottom(); // ������ Y ����������
            auto width = fftBounds.getWidth();  // ������ �������

            int numBins = fftSize / 2; // ���������� �������� �����

            PathType p; // ������� ����� ����
            p.preallocateSpace(3 * (int)fftBounds.getWidth()); // �����������: �������� ������

            // --- ������� ������� ������ ---
            float maxRenderedDb = negativeInfinity;
            if (!renderData.empty()) {
                maxRenderedDb = *std::max_element(renderData.begin(), renderData.end());
            }
            DBG("PATH GENERATOR: Max dB value received = " << maxRenderedDb);
            // -------------------------------

            // ���������� ������ ����� (��� 0 - DC offset, ������ �� ������ ��� ������ �� ����)
            auto y = mapY(renderData[0], bottom, top); // ���������� mapY
            if (std::isnan(y) || std::isinf(y)) y = bottom; // ������ �� ������������ ��������
            p.startNewSubPath(fftBounds.getX(), y); // �������� ���� �����

            const int pathResolution = 2; // ������ ����� ������ N �������� �� X (��� �����������)
            float minY = bottom, maxY = top; // ��� ������� ��������� Y

            // ��������� �� ����� FFT
            for (int binNum = 1; binNum < numBins; ++binNum)
            {
                y = mapY(renderData[static_cast<size_t>(binNum)], bottom, top); // ���������� mapY

                if (!std::isnan(y) && !std::isinf(y)) // ���������� ������������ ��������
                {
                    // ������������ X ���������� ��� �������� ����
                    auto binFreq = (float)binNum * binWidth; // ������� ����
                    // ���������� mapX ��� ���������������� ����������� �������
                    float binX = mapX(binFreq, fftBounds);

                    // ��������� ����� � ����, ���� ������ ���������� �������� �� X
                    if (std::abs(binX - p.getCurrentPosition().getX()) >= pathResolution)
                    {
                        p.lineTo(binX, y);
                        minY = std::min(minY, y);
                        maxY = std::max(maxY, y);
                    }
                }
            }
            DBG("PATH GENERATOR: Generated Path Y range = [" << minY << ", " << maxY << "]");

            // ��������� ��������������� ���� � FIFO ��� ���������� � paint()
            pathFifo.push(p);
        }

        // ������� ������� ����� �������� ��� ������
        int getNumPathsAvailable() const { return pathFifo.getNumAvailableForReading(); }

        // ������� ������� ���� �� FIFO
        bool getPath(PathType& path) { return pathFifo.pull(path); }

    private:
        // FIFO ��� �������� ��������������� �������� juce::Path
        Fifo<PathType> pathFifo;
    };

} // ����� namespace MBRP_GUI