#pragma once

#include "types.h"
#include <time.h>
#include <unistd.h>

class Timer {
    private:

        static constexpr uint64 nsSleepGranularityCycles = 5000000ULL; //5ms Just a guess - don't really know how to poll this on linux
        static constexpr bool kEnableLagDebugging = false;
        
        static inline
        uint64 TimespecToTimeRef(const timespec& ts) { return 1000000000ULL*ts.tv_sec + ts.tv_nsec; }

        static inline
        uint64 QueryTime() {
            timespec ts;
            RUNTIME_ASSERT(!clock_gettime(CLOCK_MONOTONIC_RAW, &ts), "Failed to get CLOCK_MONOTONIC_RAW time");
            return TimespecToTimeRef(ts);
        }

        uint64 timeRef;

    public:

        inline Timer(bool startTimer = false) { if(startTimer) Start(); };
        constexpr Timer(uint64 timeRef): timeRef(timeRef) {}
        constexpr Timer(const Timer& t): timeRef(t.timeRef) {};
        
        inline Timer& operator= (const Timer& t)  { timeRef = t.timeRef; return *this; }
        
        inline operator uint64() const { return timeRef; }
    
        inline bool operator < (const Timer& t) const { return timeRef < t.timeRef; }
        inline bool operator > (const Timer& t) const { return timeRef > t.timeRef; }
        
        inline Timer& operator+=(const Timer& t) { timeRef+= t.timeRef; return *this; }
        inline Timer& operator-=(const Timer& t) { timeRef-= t.timeRef; return *this; }

        inline Timer operator+ (const Timer& t) const { return Timer(timeRef)+=t; }
        inline Timer operator- (const Timer& t) const { return Timer(timeRef)-=t; }
        
        inline void Start() {
            timeRef = QueryTime();
        }
        
        inline uint64 ElapsedNs()   const { return QueryTime() - timeRef; }
        inline float ElapsedUs()    const { return 1E-3f*(QueryTime() - timeRef); }
        inline float ElapsedMs()    const { return 1E-6f*(QueryTime() - timeRef); }
        inline float ElapsedSec()   const { return 1E-9f*(QueryTime() - timeRef); }

        inline uint64 LapNs() {
            uint64  time = QueryTime(),
                    delta = time-timeRef;
            
            timeRef = time;
            return delta;
        }
        inline float LapUs()    { return 1E-3*LapNs(); }
        inline float LapMs()    { return 1E-6*LapNs(); }
        inline float LapSec()   { return 1E-9*LapNs(); }
        
        inline void SleepLapNs(uint64 ns) {
            uint64  endTime = timeRef + ns,
                    endSleepTime = endTime - nsSleepGranularityCycles;
            
            timeRef = QueryTime();
    
            if constexpr(kEnableLagDebugging) {
                if(timeRef >= endTime) {
                    uint64 endTimeDelta=timeRef-endTime;
                    Warn("Skipping Sleep { sleepTime: %f ms | endTimeDelta: %f ms }", (.1E-6f*ns),
                         (.1E-6f*endTimeDelta));
                    return;
                }
            }
    
            // sleep up to granularity intervals
            if(timeRef <= endSleepTime) {
                useconds_t usSleepTime = useconds_t((endSleepTime - timeRef)/1000);
                usleep(usSleepTime);
            }
            
            // burn loop until ready
            while(timeRef < endTime) timeRef = QueryTime();
            
            if constexpr(kEnableLagDebugging) {
                uint64 endTimeDelta=timeRef-endTime;
                if(endTimeDelta > nsSleepGranularityCycles) {
                    Error("BurnLoop overshot sleep granularity { endTimeDelta: %f ms }",
                          (.1E-6f*endTimeDelta));
                }
            }
        }
        
        inline void SleepLapUs(float us) { SleepLapNs(1E3f*us); }
        inline void SleepLapMs(float ms) { SleepLapNs(1E6f*ms); }
        inline void SleepLapSec(float sec) { SleepLapNs(1E9f*sec); }
};