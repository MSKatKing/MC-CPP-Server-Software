//
// Created by wait4 on 4/7/2024.
//

#ifndef LOGGER_H
#define LOGGER_H
#include <string>

#define BLACK "\033[30m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"
#define WHITE "\033[37m"

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
