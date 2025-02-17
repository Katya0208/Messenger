#include "../include/other.hpp"

std::mutex dbMutex;
std::mutex logMutex;

void logMessage(const std::string &message, const std::string &filename) {
  try {
    std::lock_guard<std::mutex> lock(logMutex);
    std::ofstream logFile(filename, std::ios_base::app);
    logFile << message << std::endl;
    logFile.close();
  } catch (const std::ios_base::failure &e) {
    std::cerr << "Failed to log message: " << e.what() << std::endl;
  }
}