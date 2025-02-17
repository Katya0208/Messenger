#pragma once

#include <fstream>
#include <iostream>
#include <mutex>
#include <string>

#define SERVER_LOG_FILE "server.log"

extern std::mutex dbMutex;
extern std::mutex logMutex;

void logMessage(const std::string &message,
                const std::string &filename);  // Логирование
