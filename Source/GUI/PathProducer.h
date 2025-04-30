#pragma once

#include <JuceHeader.h>
#include "FFTDataGenerator.h"
#include "AnalyzerPathGenerator.h"
#include "../PluginProcessor.h" // ��� ������� � SimpleFifo (�������, ��� ���� ����������)

namespace MBRP_GUI
{
    // �����, ���������� �� ��������� ����������� �� FIFO,
    // �������� �� � FFTDataGenerator, ��������� ���������� FFT,
    // �������� ��� � AnalyzerPathGenerator � �������� ���������� ����.
    template<typename BlockType> // BlockType ����� juce::AudioBuffer<float>
    struct PathProducer
    {
        PathProducer(MBRPAudioProcessor::SimpleFifo& scsf) : // ��������� ������ �� ��� SimpleFifo
            singleChannelSampleFifo(&scsf)
        {
            // ������������� ���������� FFT � �������� 2048
            fftDataGenerator.changeOrder(FFTOrder::order2048);
            // ������������� ����-������ ������� �������
            monoBuffer.setSize(1, fftDataGenerator.getFFTSize());
        }

        void process(juce::Rectangle<float> fftBounds, double sampleRate)
        {
            juce::AudioBuffer<float> tempIncomingBuffer;
            int buffersProcessedCount = 0; // ������� ��� �������

            // ������������ ��� ��������� ������ �� FIFO
            while (singleChannelSampleFifo != nullptr && singleChannelSampleFifo->getNumCompleteBuffersAvailable() > 0)
            {
                if (singleChannelSampleFifo->getAudioBuffer(tempIncomingBuffer))
                {
                    buffersProcessedCount++;
                    float fifoPeak = tempIncomingBuffer.getMagnitude(0, tempIncomingBuffer.getNumSamples());
                    DBG("PATH PRODUCER (FIFO->Mono): Pulled buffer #" << buffersProcessedCount << ", Peak = " << juce::Decibels::gainToDecibels(fifoPeak) << " dB");

                    // --- ����������� � ����-����� (���������� �������) ---
                    // ��������, ��� ������� ��������� ��� ����� �� FIFO ������
                    auto inputNumSamples = tempIncomingBuffer.getNumSamples();
                    auto monoNumSamples = monoBuffer.getNumSamples();

                    if (inputNumSamples <= monoNumSamples) {
                        monoBuffer.clear(); // ������� ���� �����
                        // �������� �� ������ 0 ���������� ������
                        monoBuffer.copyFrom(0, 0, tempIncomingBuffer, 0, 0, inputNumSamples);

                        // ������� ���� ����-������
                        float monoPeak = monoBuffer.getMagnitude(0, monoNumSamples);
                        DBG("PATH PRODUCER (Mono->FFT): Calling produceFFT..., Mono Peak = " << juce::Decibels::gainToDecibels(monoPeak) << " dB");

                        // �������� ����-����� � FFT ���������
                        fftDataGenerator.produceFFTDataForRendering(monoBuffer, negativeInfinity);
                    }
                    else {
                        DBG("PathProducer::process Warning: Incoming buffer size (" << inputNumSamples
                            << ") > mono buffer size (" << monoNumSamples << "). Skipping FFT frame.");
                        // jassertfalse; // ����� ����������������� ��� ��������� ���������
                    }
                    // --- ����� ����������� ---
                }
                else
                {
                    DBG("PathProducer::process Error: Failed to pull buffer from FIFO!");
                }
            }
            // if (buffersProcessedCount == 0) { DBG("PATH PRODUCER (FIFO->Mono): No buffers pulled from FIFO this cycle."); }

            // ������������ ��� ��������� ���������� FFT
            int fftBlocksProcessedCount = 0;
            const auto fftSize = fftDataGenerator.getFFTSize();
            const auto binWidth = sampleRate / double(fftSize);

            while (fftDataGenerator.getNumAvailableFFTDataBlocks() > 0)
            {
                std::vector<float> fftData;
                if (fftDataGenerator.getFFTData(fftData))
                {
                    fftBlocksProcessedCount++;
                    float maxDb = NEG_INFINITY; // ���������� ���������
                    if (!fftData.empty()) {
                        maxDb = *std::max_element(fftData.begin(), fftData.end());
                    }
                    DBG("PATH PRODUCER (FFT->PathGen): Pulled FFT Data #" << fftBlocksProcessedCount << ", Max dB = " << maxDb);

                    // �������� ������ FFT � ��������� ����
                    pathProducer.generatePath(fftData, fftBounds, fftSize, static_cast<float>(binWidth), negativeInfinity);
                }
                else
                {
                    DBG("PathProducer::process Error: Failed to pull FFT data from FFTDataGenerator!");
                }
            }
            // if (fftBlocksProcessedCount == 0 && buffersProcessedCount > 0) { DBG("PATH PRODUCER (FFT->PathGen): No FFT data generated or pulled."); }

            // �������� ��������������� ����
            while (pathProducer.getNumPathsAvailable() > 0)
            {
                if (pathProducer.getPath(fftPath)) {
                    // ������� �������� ���� - ����� ����������������� ��� ��������� �������
                     // auto pathBounds = fftPath.getBounds();
                     // DBG("PATH PRODUCER (PathGen->Final): Path pulled, bounds=" << pathBounds.toString() << ", empty=" << fftPath.isEmpty());
                }
                else {
                    DBG("PATH PRODUCER (PathGen->Final): Failed to pull path from PathGenerator!");
                }
            }
        }

        // ����� ��� ��������� ���������� ���������������� ����
        juce::Path getPath() { return fftPath; }

        // ����� ��� ���������� ������� ������� dB (������������ � resized ���������)
        void updateNegativeInfinity(float nf) { negativeInfinity = nf; }

    private:
        // ��������� �� FIFO (���������� � ������������)
        MBRPAudioProcessor::SimpleFifo* singleChannelSampleFifo;

        BlockType monoBuffer; // ����-����� ��� FFT

        // ��������� ������ FFT
        FFTDataGenerator<std::vector<float>> fftDataGenerator;

        // ��������� ���� �� ������ FFT
        AnalyzerPathGenerator<juce::Path> pathProducer;

        juce::Path fftPath; // ��������� ��� ���������� ���������������� ����

        float negativeInfinity{ NEG_INFINITY }; // ���������� ��������� �� ���������
    };

} // ����� namespace MBRP_GUI