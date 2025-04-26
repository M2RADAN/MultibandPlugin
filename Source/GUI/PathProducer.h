#pragma once

#include <JuceHeader.h>
#include "FFTDataGenerator.h"
#include "AnalyzerPathGenerator.h"
#include "../PluginProcessor.h" // Для доступа к SimpleFifo (убедись, что путь правильный)

namespace MBRP_GUI
{
    // Класс, отвечающий за получение аудиоданных из FIFO,
    // передачу их в FFTDataGenerator, получение результата FFT,
    // передачу его в AnalyzerPathGenerator и хранение финального пути.
    template<typename BlockType> // BlockType здесь juce::AudioBuffer<float>
    struct PathProducer
    {
        PathProducer(MBRPAudioProcessor::SimpleFifo& scsf) : // Принимает ссылку на НАШ SimpleFifo
            singleChannelSampleFifo(&scsf)
        {
            // Инициализация генератора FFT с порядком 2048
            fftDataGenerator.changeOrder(FFTOrder::order2048);
            // Инициализация моно-буфера нужного размера
            monoBuffer.setSize(1, fftDataGenerator.getFFTSize());
        }

        void process(juce::Rectangle<float> fftBounds, double sampleRate)
        {
            juce::AudioBuffer<float> tempIncomingBuffer;
            int buffersProcessedCount = 0; // Счетчик для отладки

            // Обрабатываем все доступные буферы из FIFO
            while (singleChannelSampleFifo != nullptr && singleChannelSampleFifo->getNumCompleteBuffersAvailable() > 0)
            {
                if (singleChannelSampleFifo->getAudioBuffer(tempIncomingBuffer))
                {
                    buffersProcessedCount++;
                    float fifoPeak = tempIncomingBuffer.getMagnitude(0, tempIncomingBuffer.getNumSamples());
                    DBG("PATH PRODUCER (FIFO->Mono): Pulled buffer #" << buffersProcessedCount << ", Peak = " << juce::Decibels::gainToDecibels(fifoPeak) << " dB");

                    // --- Копирование в моно-буфер (упрощенный вариант) ---
                    // Убедимся, что размеры совпадают или буфер из FIFO меньше
                    auto inputNumSamples = tempIncomingBuffer.getNumSamples();
                    auto monoNumSamples = monoBuffer.getNumSamples();

                    if (inputNumSamples <= monoNumSamples) {
                        monoBuffer.clear(); // Очищаем моно буфер
                        // Копируем из канала 0 временного буфера
                        monoBuffer.copyFrom(0, 0, tempIncomingBuffer, 0, 0, inputNumSamples);

                        // Отладка пика моно-буфера
                        float monoPeak = monoBuffer.getMagnitude(0, monoNumSamples);
                        DBG("PATH PRODUCER (Mono->FFT): Calling produceFFT..., Mono Peak = " << juce::Decibels::gainToDecibels(monoPeak) << " dB");

                        // Передаем моно-буфер в FFT генератор
                        fftDataGenerator.produceFFTDataForRendering(monoBuffer, negativeInfinity);
                    }
                    else {
                        DBG("PathProducer::process Warning: Incoming buffer size (" << inputNumSamples
                            << ") > mono buffer size (" << monoNumSamples << "). Skipping FFT frame.");
                        // jassertfalse; // Можно раскомментировать для остановки отладчика
                    }
                    // --- Конец копирования ---
                }
                else
                {
                    DBG("PathProducer::process Error: Failed to pull buffer from FIFO!");
                }
            }
            // if (buffersProcessedCount == 0) { DBG("PATH PRODUCER (FIFO->Mono): No buffers pulled from FIFO this cycle."); }

            // Обрабатываем все доступные результаты FFT
            int fftBlocksProcessedCount = 0;
            const auto fftSize = fftDataGenerator.getFFTSize();
            const auto binWidth = sampleRate / double(fftSize);

            while (fftDataGenerator.getNumAvailableFFTDataBlocks() > 0)
            {
                std::vector<float> fftData;
                if (fftDataGenerator.getFFTData(fftData))
                {
                    fftBlocksProcessedCount++;
                    float maxDb = NEG_INFINITY; // Используем константу
                    if (!fftData.empty()) {
                        maxDb = *std::max_element(fftData.begin(), fftData.end());
                    }
                    DBG("PATH PRODUCER (FFT->PathGen): Pulled FFT Data #" << fftBlocksProcessedCount << ", Max dB = " << maxDb);

                    // Передаем данные FFT в генератор пути
                    pathProducer.generatePath(fftData, fftBounds, fftSize, static_cast<float>(binWidth), negativeInfinity);
                }
                else
                {
                    DBG("PathProducer::process Error: Failed to pull FFT data from FFTDataGenerator!");
                }
            }
            // if (fftBlocksProcessedCount == 0 && buffersProcessedCount > 0) { DBG("PATH PRODUCER (FFT->PathGen): No FFT data generated or pulled."); }

            // Получаем сгенерированные пути
            while (pathProducer.getNumPathsAvailable() > 0)
            {
                if (pathProducer.getPath(fftPath)) {
                    // Успешно получили путь - можно раскомментировать для детальной отладки
                     // auto pathBounds = fftPath.getBounds();
                     // DBG("PATH PRODUCER (PathGen->Final): Path pulled, bounds=" << pathBounds.toString() << ", empty=" << fftPath.isEmpty());
                }
                else {
                    DBG("PATH PRODUCER (PathGen->Final): Failed to pull path from PathGenerator!");
                }
            }
        }

        // Метод для получения последнего сгенерированного пути
        juce::Path getPath() { return fftPath; }

        // Метод для обновления нижнего предела dB (используется в resized редактора)
        void updateNegativeInfinity(float nf) { negativeInfinity = nf; }

    private:
        // Указатель на FIFO (передается в конструкторе)
        MBRPAudioProcessor::SimpleFifo* singleChannelSampleFifo;

        BlockType monoBuffer; // Моно-буфер для FFT

        // Генератор данных FFT
        FFTDataGenerator<std::vector<float>> fftDataGenerator;

        // Генератор пути из данных FFT
        AnalyzerPathGenerator<juce::Path> pathProducer;

        juce::Path fftPath; // Хранилище для последнего сгенерированного пути

        float negativeInfinity{ NEG_INFINITY }; // Используем константу по умолчанию
    };

} // Конец namespace MBRP_GUI