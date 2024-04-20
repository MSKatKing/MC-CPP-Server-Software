//
// Created by wait4 on 4/7/2024.
//

#include "logger.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

Logger::Logger(std::string threadName) : threadName(std::move(threadName)), open(true) {}

void Logger::info(const std::string &message) const {
    if(!open) return;
    moveCursorToStartOfLine();
    printTimestamp();
    printThreadName();
    std::cout << "| INFO] " << message << std::endl;
}

void Logger::warn(const std::string &message) const {
    if(!open) return;
    moveCursorToStartOfLine();
    std::cout << "\033[93m";
    printTimestamp();
    printThreadName();
    std::cout << "| WARN] " << message << "\033[0m" << std::endl;
}

void Logger::error(const std::string &message) const {
    if(!open) return;
    moveCursorToStartOfLine();
    std::cout << "\033[91m";
    printTimestamp();
    printThreadName();
    std::cout << "| ERROR] " << message << "\033[0m" << std::endl;
}

void Logger::printTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto now_c = std::chrono::system_clock::to_time_t(now);
    std::cout << std::put_time(std::localtime(&now_c), "[%H:%M:%S] ");
}

void Logger::printThreadName() const {
    std::cout << "[" << threadName << " ";
}

void Logger::close() {
    open = false;
}


void Logger::moveCursorToStartOfLine() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    const COORD cursorPos = {0, csbi.dwCursorPosition.Y};
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), cursorPos);
#else
    std::cout << "\033[1G";
#endif
}







