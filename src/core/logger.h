#pragma once

#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <sstream>

// ANSI Escape Codes for Colors
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_BRIGHT_GREEN "\x1b[92m"
#define ANSI_COLOR_BRIGHT_RED   "\x1b[91m"
#define ANSI_COLOR_WHITE        "\x1b[37m"
#define ANSI_COLOR_RESET   "\x1b[0m"

// Timestamp helper helper function (defined inline)
inline std::string logger_get_time() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%H:%M:%S");
    return ss.str();
}

// Logger Macros
#define LOG_INTERNAL(level, color, msg) \
    std::cout << "[" << ANSI_COLOR_WHITE << logger_get_time() << ANSI_COLOR_RESET << "] [" << color << level << ANSI_COLOR_RESET << "] " << msg << std::endl

#define LOG_INFO(msg)    LOG_INTERNAL("INFO",    ANSI_COLOR_CYAN,    msg)
#define LOG_DEBUG(msg)   LOG_INTERNAL("DEBUG",   ANSI_COLOR_BLUE,    msg)
#define LOG_TRACE(msg)   LOG_INTERNAL("TRACE",   ANSI_COLOR_MAGENTA, msg)
#define LOG_SUCCESS(msg) LOG_INTERNAL("SUCCESS", ANSI_COLOR_BRIGHT_GREEN, msg)
#define LOG_NOTE(msg)    LOG_INTERNAL("NOTE",    ANSI_COLOR_YELLOW,  msg)
#define LOG_WARNING(msg) LOG_INTERNAL("WARNING", ANSI_COLOR_YELLOW,  msg)
#define LOG_ERROR(msg)   LOG_INTERNAL("ERROR",   ANSI_COLOR_RED,     msg)
#define LOG_FATAL(msg)   LOG_INTERNAL("FATAL",   ANSI_COLOR_BRIGHT_RED, msg)
