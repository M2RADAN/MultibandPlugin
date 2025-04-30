#pragma once
#include <JuceHeader.h>
#include "Utilities.h" // Для констант NEG_INFINITY и т.д.
#include "../DSP/Fifo.h"      // Для определения Fifo

namespace MBRP_GUI
{
    // Перечисление для порядков FFT
    enum FFTOrder
    {
        order2048 = 11,
        order4096 = 12,
        order8192 = 13
    };

    // Класс, отвечающий за выполнение FFT и подготовку данных для рендеринга
    template<typename BlockType> // BlockType здесь std::vector<float>
    struct FFTDataGenerator
    {
        FFTDataGenerator() { // Конструктор по умолчанию
            changeOrder(FFTOrder::order2048); // Устанавливаем порядок по умолчанию
        }

        // Метод для изменения порядка FFT (создает новые объекты)
        void changeOrder(FFTOrder newOrder)
        {
            order = newOrder;
            auto fftSize = getFFTSize();

            forwardFFT = std::make_unique<juce::dsp::FFT>(order);
            // Используем окно Блекмана-Харриса, как в SimpleMBComp
            window = std::make_unique<juce::dsp::WindowingFunction<float>>(fftSize, juce::dsp::WindowingFunction<float>::blackmanHarris);

            fftData.clear();
            // Вектор fftData хранит float, и его размер должен быть достаточным
            // для БПФ (in-place). Размер * 2 не обязателен для frequencyOnlyForwardTransform,
            // но безопаснее оставить, т.к. результат перезаписывается.
            fftData.resize(static_cast<size_t>(fftSize * 2), 0);

            // Подготавливаем FIFO для хранения std::vector<float>
            // Убедимся, что Fifo::prepare(size_t) вызывается
             fftDataFifo.prepare(fftData.size());
            DBG("FFTDataGenerator: Order changed to " << fftSize);
        }

        // Основной метод обработки аудиоданных (логика из SimpleMBComp)
        void produceFFTDataForRendering(const juce::AudioBuffer<float>& audioData, const float negativeInfinity)
        {
            const auto fftSize = getFFTSize();

            // Проверяем размер входного буфера
            auto numInputSamples = audioData.getNumSamples();
            if (numInputSamples < fftSize)
            {
                DBG("FFTDataGenerator Warning: Input buffer smaller than FFT size. Padding with zeros.");
                // Можно добавить логику дополнения нулями, если нужно, но пока пропустим
            }

            // Убедимся, что наш внутренний буфер fftData правильного размера
            if (fftData.size() < fftSize * 2) { // Безопаснее оставить fftSize * 2
                fftData.resize(fftSize * 2, 0);
                DBG("FFTDataGenerator Warning: Resized internal fftData buffer.");
            }
            fftData.assign(fftData.size(), 0); // Очищаем буфер FFT

            // Копируем необходимое количество сэмплов
            auto* readIndex = audioData.getReadPointer(0); // Читаем из канала 0
            auto numSamplesToCopy = std::min(fftSize, numInputSamples); // Копируем не больше, чем есть или чем нужно
            std::copy(readIndex, readIndex + numSamplesToCopy, fftData.begin());

            // 1. Применяем окно к первым fftSize элементам
            window->multiplyWithWindowingTable(fftData.data(), static_cast<size_t>(fftSize));

            // 2. Выполняем FFT (результат записывается в начало fftData)
            forwardFFT->performFrequencyOnlyForwardTransform(fftData.data());

            int numBins = fftSize / 2; // Количество полезных частотных бинов

            // --- Нормализация как в SimpleMBComp ---
            // Делим магнитуду каждого бина на общее количество бинов
            for (int i = 0; i < numBins; ++i)
            {
                auto v = fftData[static_cast<size_t>(i)];
                // Проверка на inf/nan перед делением
                if (!std::isinf(v) && !std::isnan(v))
                {
                    v /= static_cast<float>(numBins); // Нормализация
                }
                else
                {
                    v = 0.f; // Заменяем некорректные значения нулем
                }
                fftData[static_cast<size_t>(i)] = v; // Записываем нормализованную магнитуду
            }
            // ---------------------------------------

            // --- Преобразование в децибелы ---
            float maxDbValue = negativeInfinity; // Используем константу из Utilities.h
            for (int i = 0; i < numBins; ++i)
            {
                // Берем нормализованное значение из предыдущего цикла
                auto normalizedMagnitude = fftData[static_cast<size_t>(i)];
                // Преобразуем в dB, используя negativeInfinity как нижний предел
                float dbValue = juce::Decibels::gainToDecibels(normalizedMagnitude, negativeInfinity);
                // Дополнительное ограничение снизу (на всякий случай)
                if (dbValue < negativeInfinity) dbValue = negativeInfinity;
                // Записываем значение в dB обратно в ту же ячейку вектора
                fftData[static_cast<size_t>(i)] = dbValue;
              //  maxDbValue = std::max(maxDbValue, dbValue); // Отслеживаем максимум для отладки
            }
            auto normalizedMagnitudes = fftData; // Копируем вектор

            for (int i = 0; i < numBins; ++i)
            {
                // Берем значение из КОПИИ
                float normMag = normalizedMagnitudes[static_cast<size_t>(i)];
                // Преобразуем в dB
                float dbValue = juce::Decibels::gainToDecibels(normMag, negativeInfinity);
                // Ограничение снизу
                if (dbValue < negativeInfinity) dbValue = negativeInfinity;
                // Записываем результат dB обратно в ОСНОВНОЙ fftData
                fftData[static_cast<size_t>(i)] = dbValue;
                maxDbValue = std::max(maxDbValue, dbValue);
            }
            DBG("FFT GENERATOR (SimpleMBComp Logic): Max dB Value generated = " << maxDbValue);
            int testBinIndex = numBins / 4; // Пример - берем бин из первой четверти спектра
            if (testBinIndex >= 0 && testBinIndex < numBins) {
                float testNormMag = normalizedMagnitudes[static_cast<size_t>(testBinIndex)]; // Берем из копии
                float testDbValue = juce::Decibels::gainToDecibels(testNormMag, negativeInfinity);
                DBG("FFT GENERATOR SINGLE BIN TEST: Bin " << testBinIndex << " NormMag=" << testNormMag << " -> dB=" << testDbValue);
            }
            // ---------------------------------

            // --- Отладка перед PUSH (оставляем) ---
            if (fftData.size() >= numBins && numBins >= 5) // Убедимся, что есть хотя бы 5 бинов
            {
                DBG("FFT GENERATOR: Pushing vector. Size=" << fftData.size() << ", First 5 dB bins: "
                    << fftData[0] << " " << fftData[1] << " " << fftData[2] << " " << fftData[3] << " " << fftData[4]);
            }
            else {
                DBG("FFT GENERATOR: Pushing vector. Size=" << fftData.size());
            }
            // -------------------------------------

            // Отправляем результат (вектор со значениями в dB) в FIFO
            bool pushedOk = fftDataFifo.push(fftData);
            DBG("FFT GENERATOR: Pushed to FIFO ok = " << (pushedOk ? "Yes" : "No"));
        }
        // --- Конец метода produceFFTDataForRendering ---

        // --- Методы доступа ---
        int getFFTSize() const { return 1 << order; }
        int getNumAvailableFFTDataBlocks() const { return fftDataFifo.getNumAvailableForReading(); }
        // Метод для извлечения данных из FIFO
        bool getFFTData(BlockType& data) // BlockType здесь std::vector<float>
        {
            // Убедимся, что data имеет достаточный размер перед вызовом pull
            // (или pull должен сам корректно копировать/ресайзить)
            return fftDataFifo.pull(data);
        }

    private:
        FFTOrder order; // Порядок FFT (например, 11 для 2048)
        BlockType fftData; // Вектор для хранения данных FFT (сначала время, потом магнитуды, потом dB)
        std::unique_ptr<juce::dsp::FFT> forwardFFT; // Объект для выполнения FFT
        std::unique_ptr<juce::dsp::WindowingFunction<float>> window; // Оконная функция
        Fifo<BlockType> fftDataFifo; // FIFO для передачи готовых данных dB в PathProducer
    }; // Конец struct FFTDataGenerator

} // Конец namespace MBRP_GUI