#ifndef _TIMER_HPP
#define _TIMER_HPP

#include <chrono>
#include <iostream>

class Timer {
private:
    std::chrono::high_resolution_clock::time_point start_time;
    std::chrono::high_resolution_clock::time_point end_time;
    bool is_running;

public:
    Timer() : is_running(false) {}
    
    void start() {
        start_time = std::chrono::high_resolution_clock::now();
        is_running = true;
    }
    
    double stop() {
        if (!is_running) {
            std::cerr << "Timer was not running!" << std::endl;
            return 0.0;
        }
        end_time = std::chrono::high_resolution_clock::now();
        is_running = false;
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        return duration.count() / 1000.0; // Convert to milliseconds
    }
};

#endif 