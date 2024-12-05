#pragma once

#include <chrono>
#include <source_location>
#include <print>

class TimerToken
{
public:
    TimerToken(const std::source_location &location = std::source_location::current()) :
        mSource(location) {}

    ~TimerToken()
    {
        std::println("[Timer] {} ({} {}:{}) ",
                     this->mDuration.count(),
                     this->mSource.function_name(),
                     this->mSource.line(),
                     this->mSource.column());
    }

    void addDuration(const std::chrono::duration<double> &d)
    {
        this->mDuration += d;
    }

    void lock()
    {
        ++this->mLock;
    }

    bool unlock()
    {
        --this->mLock;
        return this->mLock == 0;
    }

private:
    int                           mLock;
    std::source_location          mSource;
    std::chrono::duration<double> mDuration;
};

class Timer
{
public:
    Timer(TimerToken &token) :
        start_time(std::chrono::steady_clock::now()), mToken(token)
    {
        token.lock();
    }

    ~Timer()
    {
        if (this->mToken.unlock())
            this->mToken.addDuration(std::chrono::steady_clock::now() - start_time);
    }

private:
    TimerToken                           &mToken;
    std::chrono::steady_clock::time_point start_time;
};