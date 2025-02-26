#ifndef SPECTRUM_DISPLAY_H
#define SPECTRUM_DISPLAY_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <functional>
#include <cmath>
#include <cstring>

// 定义 DrawPoint 回调函数类型
typedef std::function<void(int x, int y, uint8_t dot)> DrawPointCallback;

// 频谱风格枚举
enum SpectrumStyle
{
    STYLE_BAR,
    STYLE_LINE,
    STYLE_DOT,
    STYLE_POLYGON,
    STYLE_CENTERED_BAR, // 柱状居垂直方向中间的风格
    STYLE_GRADIENT_BAR  // 渐变柱状图风格
};

class SpectrumDisplay
{
private:
    int screenWidth;
    int screenHeight;
    SpectrumStyle currentStyle;
    DrawPointCallback drawPointCallback;
    float *currentFFTData = nullptr;
    float *interpolatedData = nullptr;
    float *targetFFTData = nullptr;
    int fftSize = 0;
    int animationStep = 0;
    const int totalAnimationSteps = 10;
    TaskHandle_t spectrumTaskHandle = nullptr;

    // 非线性动画函数，先快后慢
    float easeOutQuart(float t)
    {
        return 1 - std::pow(1 - t, 4);
    }

    // 计算当前步的插值数据
    void calculateInterpolatedData(float *interpolatedData)
    {
        float progress = static_cast<float>(animationStep) / totalAnimationSteps;
        float easedProgress = easeOutQuart(progress);
        for (int i = 0; i < fftSize; ++i)
        {
            interpolatedData[i] = currentFFTData[i] + (targetFFTData[i] - currentFFTData[i]) * easedProgress;
        }
    }

    // 找到 FFT 数据中的最大值
    float findMaxValue(const float *data)
    {
        float maxVal = 0.0f;
        for (int i = 0; i < fftSize; ++i)
        {
            if (data[i] > maxVal)
            {
                maxVal = data[i];
            }
        }
        return maxVal;
    }

    // 绘制柱状图
    void drawBarSpectrum(const float *data)
    {
        float maxVal = findMaxValue(data);
        for (int i = 0; i < fftSize; ++i)
        {
            int barX = i * (screenWidth / fftSize);
            float scaledHeight = (data[i] / maxVal) * screenHeight;
            int barHeight = static_cast<int>(scaledHeight);
            for (int x = barX; x < barX + 4; ++x)
            {
                for (int y = 0; y < barHeight; ++y)
                {
                    drawPointCallback(x, screenHeight - y - 1, 1);
                }
            }
        }
    }

    // 绘制折线图
    void drawLineSpectrum(const float *data)
    {
        float maxVal = findMaxValue(data);
        for (int i = 0; i < fftSize - 1; ++i)
        {
            int x1 = i * (screenWidth / fftSize);
            int x2 = (i + 1) * (screenWidth / fftSize);
            float scaledY1 = (data[i] / maxVal) * screenHeight;
            float scaledY2 = (data[i + 1] / maxVal) * screenHeight;
            int y1 = static_cast<int>(scaledY1);
            int y2 = static_cast<int>(scaledY2);
            // 简单的直线绘制，可使用更复杂的算法
            for (int x = x1; x <= x2; ++x)
            {
                int y = y1 + (y2 - y1) * (x - x1) / (x2 - x1);
                drawPointCallback(x, screenHeight - y - 1, 1);
            }
        }
    }

    // 绘制点状图
    void drawDotSpectrum(const float *data)
    {
        float maxVal = findMaxValue(data);
        for (int i = 0; i < fftSize; ++i)
        {
            int x = i * (screenWidth / fftSize);
            float scaledY = (data[i] / maxVal) * screenHeight;
            int y = static_cast<int>(scaledY);
            drawPointCallback(x, screenHeight - y - 1, 1);
        }
    }

    // 绘制多边形图
    void drawPolygonSpectrum(const float *data)
    {
        float maxVal = findMaxValue(data);
        for (int i = 0; i < fftSize - 1; ++i)
        {
            int x1 = i * (screenWidth / fftSize);
            int x2 = (i + 1) * (screenWidth / fftSize);
            float scaledY1 = (data[i] / maxVal) * screenHeight;
            float scaledY2 = (data[i + 1] / maxVal) * screenHeight;
            int y1 = static_cast<int>(scaledY1);
            int y2 = static_cast<int>(scaledY2);
            for (int x = x1; x <= x2; ++x)
            {
                int y = y1 + (y2 - y1) * (x - x1) / (x2 - x1);
                drawPointCallback(x, screenHeight - y - 1, 1);
                // 填充多边形内部
                for (int j = y; j < screenHeight; ++j)
                {
                    drawPointCallback(x, screenHeight - j - 1, 1);
                }
            }
        }
    }

    // 绘制垂直居中的柱状图
    void drawCenteredBarSpectrum(const float *data)
    {
        float maxVal = findMaxValue(data);
        for (int i = 0; i < fftSize; ++i)
        {
            int barX = i * (screenWidth / fftSize);
            float scaledHeight = (data[i] / maxVal) * screenHeight;
            int barHeight = static_cast<int>(scaledHeight);
            int centerY = screenHeight / 2;
            int startY = centerY - barHeight / 2;
            for (int x = barX; x < barX + 4; ++x)
            {
                for (int y = startY; y < startY + barHeight; ++y)
                {
                    drawPointCallback(x, y, 1);
                }
            }
        }
    }

    // 绘制渐变柱状图
    void drawGradientBarSpectrum(const float *data)
    {
        float maxVal = findMaxValue(data);
        for (int i = 0; i < fftSize; ++i)
        {
            int barX = i * (screenWidth / fftSize);
            float scaledHeight = (data[i] / maxVal) * screenHeight;
            int barHeight = static_cast<int>(scaledHeight);
            for (int x = barX; x < barX + 4; ++x)
            {
                for (int y = 0; y < barHeight; ++y)
                {
                    uint8_t dotValue = static_cast<uint8_t>(255 * (1.0f - static_cast<float>(y) / barHeight));
                    drawPointCallback(x, screenHeight - y - 1, dotValue);
                }
            }
        }
    }

public:
    SpectrumDisplay(int width, int height) : screenWidth(width), screenHeight(height), currentStyle(STYLE_BAR)
    {
        interpolatedData = new float[fftSize];
    }

    ~SpectrumDisplay()
    {
        if (spectrumTaskHandle != nullptr)
        {
            vTaskDelete(spectrumTaskHandle);
        }
        if (currentFFTData != nullptr)
        {
            delete[] currentFFTData;
        }
        if (targetFFTData != nullptr)
        {
            delete[] targetFFTData;
        }
        if (interpolatedData != nullptr)
        {
            delete[] interpolatedData;
        }
    }

    void setDrawPointCallback(DrawPointCallback callback)
    {
        drawPointCallback = callback;
    }

    void setScreenSize(int width, int height)
    {
        screenWidth = width;
        screenHeight = height;
    }

    void setSpectrumStyle(SpectrumStyle style)
    {
        currentStyle = style;
        animationStep = 0; 
    }

    void inputFFTData(const float *data, int size)
    {
        if (fftSize != size)
        {
            if (currentFFTData != nullptr)
            {
                delete[] currentFFTData;
            }
            if (targetFFTData != nullptr)
            {
                delete[] targetFFTData;
            }
            currentFFTData = new float[size];
            targetFFTData = new float[size];
            fftSize = size;
        }
        if (animationStep < totalAnimationSteps)
        {
            // 如果动画还在进行中，将当前插值数据作为新的起始点
            float *temp = new float[fftSize];
            calculateInterpolatedData(temp);
            std::memcpy(currentFFTData, temp, fftSize * sizeof(float));
            delete[] temp;
        }
        else
        {
            std::memcpy(currentFFTData, targetFFTData, fftSize * sizeof(float));
        }
        std::memcpy(targetFFTData, data, fftSize * sizeof(float));
        animationStep = 0;
    }

    void spectrumProcess(void)
    {
        if (animationStep < totalAnimationSteps)
        {
            calculateInterpolatedData(interpolatedData);
            switch (currentStyle)
            {
            case STYLE_BAR:
                drawBarSpectrum(interpolatedData);
                break;
            case STYLE_LINE:
                drawLineSpectrum(interpolatedData);
                break;
            case STYLE_DOT:
                drawDotSpectrum(interpolatedData);
                break;
            case STYLE_POLYGON:
                drawPolygonSpectrum(interpolatedData);
                break;
            case STYLE_CENTERED_BAR:
                drawCenteredBarSpectrum(interpolatedData);
                break;
            case STYLE_GRADIENT_BAR:
                drawGradientBarSpectrum(interpolatedData);
                break;
            default:
                break;
            }
            animationStep++;
        }
    }
};
#endif