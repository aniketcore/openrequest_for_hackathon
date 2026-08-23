#include "ui_loop.h"
#include <thread>
#include <memory>
#include<iostream>
void* operator new(size_t size) {
    std::cout << "Global new: Allocating " << size << " bytes.\n";
    
    // Fallback to avoid spin-locks or return valid pointer for 0 bytes
    if (size == 0) {
        size = 1; 
    }
    
    void* ptr = std::malloc(size);
    if (!ptr) {
        throw std::bad_alloc();
    }
    return ptr;
}

int main()
{
    std::thread UIthread(UI::RunUILoop);
    UIthread.join();
}