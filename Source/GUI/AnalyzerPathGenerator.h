#pragma once
#include <JuceHeader.h>
#include "Utilities.h" // Для mapX, mapY, констант
#include "../DSP/Fifo.h"      // Для определения Fifo

namespace MBRP_GUI
{
    // Класс, отвечающий за преобразование вектора данных FFT (в dB) в объект juce::Path
    template<typename PathType> // PathType здесь juce::Path
    struct AnalyzerPathGenerator
    {
        /*
         converts 'renderData[]' (массив dB) into a juce::Path
         */
        void generatePath(const std::vector<float>& renderData, // Данные в dB
            juce::Rectangle<float> fftBounds,      // Область рисования
            int fftSize,                           // Размер FFT
            float binWidth,                        // Ширина частотной корзины
            float negativeInfinity)                // Нижний предел dB
        {
            auto top = fftBounds.getY();        // Верхняя Y координата
            auto bottom = fftBounds.getBottom(); // Нижняя Y координата
            auto width = fftBounds.getWidth();  // Ширина области

            int numBins = fftSize / 2; // Количество полезных бинов

            PathType p; // Создаем новый путь
            p.preallocateSpace(3 * (int)fftBounds.getWidth()); // Оптимизация: выделяем память

            // --- ОТЛАДКА ВХОДНЫХ ДАННЫХ ---
            float maxRenderedDb = negativeInfinity;
            if (!renderData.empty()) {
                maxRenderedDb = *std::max_element(renderData.begin(), renderData.end());
            }
            DBG("PATH GENERATOR: Max dB value received = " << maxRenderedDb);
            // -------------------------------

            // Отображаем первую точку (бин 0 - DC offset, обычно не рисуем или рисуем на краю)
            auto y = mapY(renderData[0], bottom, top); // Используем mapY
            if (std::isnan(y) || std::isinf(y)) y = bottom; // Защита от некорректных значений
            p.startNewSubPath(fftBounds.getX(), y); // Начинаем путь слева

            const int pathResolution = 2; // Рисуем линию каждые N пикселей по X (для оптимизации)
            float minY = bottom, maxY = top; // Для отладки диапазона Y

            // Итерируем по бинам FFT
            for (int binNum = 1; binNum < numBins; ++binNum)
            {
                y = mapY(renderData[static_cast<size_t>(binNum)], bottom, top); // Используем mapY

                if (!std::isnan(y) && !std::isinf(y)) // Пропускаем некорректные значения
                {
                    // Рассчитываем X координату для текущего бина
                    auto binFreq = (float)binNum * binWidth; // Частота бина
                    // Используем mapX для логарифмического отображения частоты
                    float binX = mapX(binFreq, fftBounds);

                    // Добавляем точку в путь, если прошло достаточно пикселей по X
                    if (std::abs(binX - p.getCurrentPosition().getX()) >= pathResolution)
                    {
                        p.lineTo(binX, y);
                        minY = std::min(minY, y);
                        maxY = std::max(maxY, y);
                    }
                }
            }
            DBG("PATH GENERATOR: Generated Path Y range = [" << minY << ", " << maxY << "]");

            // Добавляем сгенерированный путь в FIFO для извлечения в paint()
            pathFifo.push(p);
        }

        // Сколько готовых путей доступно для чтения
        int getNumPathsAvailable() const { return pathFifo.getNumAvailableForReading(); }

        // Извлечь готовый путь из FIFO
        bool getPath(PathType& path) { return pathFifo.pull(path); }

    private:
        // FIFO для хранения сгенерированных объектов juce::Path
        Fifo<PathType> pathFifo;
    };

} // Конец namespace MBRP_GUI