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
    STYLE_GRADIENT_BAR, // 渐变柱状图风格
    STYLE_MAX
};

#define FFT_FACTOR 0.5f
class SpectrumDisplay
{
private:
    int screenWidth;
    int screenHeight;
    SpectrumStyle currentStyle = STYLE_DOT;
    DrawPointCallback drawPointCallback;
    float currentFFTData[512 / 2];
    float interpolatedData[512 / 2];
    float targetFFTData[512 / 2];
    const int fftSize = 512 / 2;
    int animationStep = 0;
    const int totalAnimationSteps = 5;

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
    // 绘制柱状图
    void drawBarSpectrum(const float *data)
    {
        for (int i = 0; i < screenWidth; ++i)
        {
            int x = i * fftSize / screenWidth;
            float scaledHeight = (data[x] * FFT_FACTOR);
            int barHeight = static_cast<int>(scaledHeight);
            for (int xPos = i; xPos < i + 4; ++xPos)
            {
                for (int y = 0; y < barHeight; ++y)
                {
                    drawPointCallback(xPos, screenHeight - y - 1, 1);
                    if (y > screenHeight)
                        break;
                }
            }
        }
    }

    // 绘制折线图
    void drawLineSpectrum(const float *data)
    {
        for (int i = 0; i < screenWidth - 1; ++i)
        {
            int x1 = i * fftSize / screenWidth;
            int x2 = (i + 1) * fftSize / screenWidth;
            float scaledY1 = (data[x1] * FFT_FACTOR);
            float scaledY2 = (data[x2] * FFT_FACTOR);
            int y1 = static_cast<int>(scaledY1);
            int y2 = static_cast<int>(scaledY2);

            if (x2 - x1 == 0)
            {
                continue;
            }
            for (int x = i; x <= i + 1; ++x)
            {
                int y = y1 + (y2 - y1) * (x - i) / (1);
                drawPointCallback(x, screenHeight - y - 1, 1);
                if (y > screenHeight)
                    break;
            }
        }
    }

    // 绘制点状图
    // 原函数已经是这种形式，无需修改
    void drawDotSpectrum(const float *data)
    {
        for (int i = 0; i < screenWidth; ++i)
        {
            int x = i * (fftSize - 10) / screenWidth + 10;
            int y = static_cast<int>(data[x] * FFT_FACTOR);

            if (y > (screenHeight - 1))
                y = screenHeight - 1;
            if (y < 1)
                y = 0;
            int startY = (screenHeight - y) / 2;

            for (int j = startY; j < startY + y; j++)
            {
                drawPointCallback(i, j, 1);
                if (j > screenHeight)
                    break;
            }
        }
    }

    // 绘制多边形图
    void drawPolygonSpectrum(const float *data)
    {
        for (int i = 0; i < screenWidth - 1; ++i)
        {
            int x1 = i * fftSize / screenWidth;
            int x2 = (i + 1) * fftSize / screenWidth;
            float scaledY1 = (data[x1] * FFT_FACTOR);
            float scaledY2 = (data[x2] * FFT_FACTOR);
            int y1 = static_cast<int>(scaledY1);
            int y2 = static_cast<int>(scaledY2);

            if (x2 - x1 == 0)
            {
                continue;
            }

            for (int x = i; x <= i + 1; ++x)
            {
                int y = y1 + (y2 - y1) * (x - i) / (1);
                if (x >= 0 && x < screenWidth && screenHeight - y - 1 >= 0 && screenHeight - y - 1 < screenHeight)
                {
                    drawPointCallback(x, y, 1);
                    if (y > screenHeight)
                        break;
                }
                // 填充多边形内部
                for (int j = y; j < screenHeight; ++j)
                {
                    if (x >= 0 && x < screenWidth && screenHeight - j - 1 >= 0 && screenHeight - j - 1 < screenHeight)
                    {
                        drawPointCallback(x, j, 1);
                    }
                    if (y > screenHeight)
                        break;
                }
            }
        }
    }

    // 绘制垂直居中的柱状图
    void drawCenteredBarSpectrum(const float *data)
    {
        for (int i = 0; i < screenWidth; ++i)
        {
            int x = i * fftSize / screenWidth;
            float scaledHeight = (data[x] * FFT_FACTOR);
            int barHeight = static_cast<int>(scaledHeight);
            int centerY = screenHeight / 2;
            int startY = centerY - barHeight / 2;
            for (int xPos = i; xPos < i + 4; ++xPos)
            {
                for (int y = startY; y < startY + barHeight; ++y)
                {
                    drawPointCallback(xPos, y, 1);
                    if (y > screenHeight)
                        break;
                }
            }
        }
    }

    // 绘制渐变柱状图
    void drawGradientBarSpectrum(const float *data)
    {
        for (int i = 0; i < screenWidth; ++i)
        {
            int x = i * fftSize / screenWidth;
            float scaledHeight = (data[x] * FFT_FACTOR);
            int barHeight = static_cast<int>(scaledHeight);
            for (int xPos = i; xPos < i + 4; ++xPos)
            {
                for (int y = 0; y < barHeight; ++y)
                {
                    drawPointCallback(xPos, screenHeight - y - 1, barHeight);
                    if (y > screenHeight)
                        break;
                }
            }
        }
    }

public:
    SpectrumDisplay(int width, int height) : screenWidth(width), screenHeight(height), currentStyle(STYLE_BAR)
    {
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
        if (animationStep < totalAnimationSteps)
        {
            calculateInterpolatedData(currentFFTData);
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
        static int64_t start_time = esp_timer_get_time() / 1000;
        int64_t current_time = esp_timer_get_time() / 1000;

        int64_t elapsed_time = current_time - start_time;

        if (elapsed_time >= 5000)
        {
            start_time = current_time;
            currentStyle = (SpectrumStyle)((currentStyle + 1) % STYLE_MAX);
        }

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