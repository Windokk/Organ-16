#pragma once

#include <stdexcept>
#include <mutex>

void UpdateClockLabel(bool newValue);

class Clock{
    private:
        Clock() {}
        // Static pointer to the clock instance
        static Clock* instancePtr;
        // Mutex to ensure thread safety
        static std::mutex mtx;

        int value = false;        

        int frequency = 0; // [1Mhz - 100Mhz] 0 = Manual

    public:

        Clock(const Clock&) = delete;
        Clock& operator=(const Clock&) = delete;
        Clock(Clock&&) = delete;
        Clock& operator=(Clock&&) = delete;

        // Static method to get the clock instance
        static Clock* GetInstance() {
            if (instancePtr == nullptr) {
                std::lock_guard<std::mutex> lock(mtx);
                if (instancePtr == nullptr) {
                    instancePtr = new Clock();
                }
            }
            return instancePtr;
        }

        void Increment(){
            value = !value;
            UpdateClockLabel(value);
        }

        int GetClockSignal(bool halt){
            return value && !halt;
        }

        int GetFrequency(){
            return frequency;
        }

        void SetFrequency(int newFrequency){
            frequency = newFrequency;
        }

        void Reset(){
            value = false;
            UpdateClockLabel(value);
        }
};