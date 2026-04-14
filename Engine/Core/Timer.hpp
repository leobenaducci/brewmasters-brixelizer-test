#pragma once
#include <windows.h>

class Timer {
public:
    Timer() {
        QueryPerformanceFrequency(&m_Frequency);
        QueryPerformanceCounter(&m_Start);
    }

    float GetDeltaTime() {
        LARGE_INTEGER current;
        QueryPerformanceCounter(&current);
        float delta = (float)(current.QuadPart - m_Last.QuadPart) / m_Frequency.QuadPart;
        m_Last = current;
        return delta;
    }

private:
    LARGE_INTEGER m_Frequency, m_Start, m_Last = {0};
};