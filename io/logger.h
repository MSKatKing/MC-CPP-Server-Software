//
// Created by wait4 on 4/7/2024.
//

#ifndef LOGGER_H
#define LOGGER_H
#include <string>


class Logger {
private:
    std::string threadName;
    bool open;

    static void printTimestamp();
    void printThreadName() const;
    static void moveCursorToStartOfLine();

public:
    explicit Logger(std::string  threadName);

    void info(const std::string& message) const;
    void warn(const std::string& message) const;
    void error(const std::string& message) const;
    void close();
};



#endif //LOGGER_H
