/*
  ==============================================================================

    FFTDataGenerator.h
    Created: 30 Oct 2021 11:27:47am
    Author:  matkatmusic

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Utilities.h"
#include "../DSP/Fifo.h"

namespace MBRP_GUI
{

template<typename BlockType>
struct FFTDataGenerator
{
    /**
     produces the FFT data from an audio buffer.
     */
    void produceFFTDataForRendering(const juce::AudioBuffer<float>& audioData, const float negativeInfinity)
    {
        const auto fftSize = getFFTSize();

        fftData.assign(fftData.size(), 0); // Очищаем буфер FFT
        auto* readIndex = audioData.getReadPointer(0); // Берем данные из моно буфера
        // Копируем данные из входного моно буфера в буфер для FFT
        std::copy(readIndex, readIndex + fftSize, fftData.begin());

        // 1. Применяем оконную функцию
        window->multiplyWithWindowingTable(fftData.data(), static_cast<size_t>(fftSize));

        // 2. Выполняем FFT (получаем магнитуды)
        forwardFFT->performFrequencyOnlyForwardTransform(fftData.data());

        int numBins = (int)fftSize / 2;

        // --- ИСПРАВЛЕНА НОРМАЛИЗАЦИЯ и КОНВЕРТАЦИЯ В dB ---
        // Коэффициент нормализации (как в MBRP, с компенсацией окна Ханна или Блекмана-Харриса)
        // Окно Блекмана-Харриса имеет когерентное усиление ~0.42. Компенсация = 1/0.42 ≈ 2.38
        // Базовая нормализация 2.0 / fftSize. Итоговая = (2.0 / fftSize) * (1.0 / CoherentGain)
        // Попробуем коэффициент около 4.0 / fftSize, как мы делали раньше, или чуть больше
        const float normalizationFactor = 4.5f / (float)fftSize; // Можно подбирать (4.0, 4.5, 5.0...)

        for (int i = 0; i < numBins; ++i)
        {
            auto magnitude = fftData[static_cast<size_t>(i)];

            // Проверка на NaN/inf
            if (std::isinf(magnitude) || std::isnan(magnitude))
            {
                magnitude = 0.f;
            }

            // Применяем нормализацию
            magnitude *= normalizationFactor;

            // Преобразуем нормализованную магнитуду в dB
            // Используем переданный negativeInfinity как нижний порог
            fftData[static_cast<size_t>(i)] = juce::Decibels::gainToDecibels(magnitude, negativeInfinity);
        }
        // ----------------------------------------------------

        // Отправляем готовый массив значений в dB в FIFO
        fftDataFifo.push(fftData);
    }
    
    void changeOrder(FFTOrder newOrder)
    {
        //when you change order, recreate the window, forwardFFT, fifo, fftData
        //also reset the fifoIndex
        //things that need recreating should be created on the heap via std::make_unique<>
        
        order = newOrder;
        auto fftSize = getFFTSize();
        
        forwardFFT = std::make_unique<juce::dsp::FFT>(order);
        window = std::make_unique<juce::dsp::WindowingFunction<float>>(fftSize, juce::dsp::WindowingFunction<float>::blackmanHarris);
        
        fftData.clear();
        fftData.resize(static_cast<size_t>(fftSize * 2), 0);

        fftDataFifo.prepare(fftData.size());
    }
    //==============================================================================
    int getFFTSize() const { return 1 << order; }
    int getNumAvailableFFTDataBlocks() const { return fftDataFifo.getNumAvailableForReading(); }
    //==============================================================================
    bool getFFTData(BlockType& data) { return fftDataFifo.pull(data); }
private:
    FFTOrder order;
    BlockType fftData;
    std::unique_ptr<juce::dsp::FFT> forwardFFT;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;
    
    Fifo<BlockType> fftDataFifo;
};

} //end namespace MBRP_GUI
