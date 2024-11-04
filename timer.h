#include <chrono>

class Timer
{
    typedef std::chrono::high_resolution_clock HiResClock;

public:
    void run()
    {
        prev = HiResClock::now();
        running = true;
    }

    float millisecondsElapsed()
    {
        const auto now = HiResClock::now();
        const std::chrono::microseconds ms =
            std::chrono::duration_cast<std::chrono::microseconds>(now - prev);
        prev = now;
        return static_cast<float>(ms.count()) * 0.001f;
    }

    float secondsElapsed()
    {
        return millisecondsElapsed() * 0.001f;
    }

private:
    HiResClock::time_point prev;
    bool running = false;
};
