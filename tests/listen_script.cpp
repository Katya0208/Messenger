#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

/**
 * Читает содержимое файла и возвращает его как вектор строк,
 * где каждая строка представляет собой строку в файле.
 *
 * @param filePath путь к файлу для чтения
 * @return вектор строк, каждая из которых представляет строку в файле
 */
std::vector<std::string> readFile(const std::string& filePath) {
  std::vector<std::string> lines;
  std::ifstream file(filePath);
  if (file.is_open()) {
    std::string line;
    while (std::getline(file, line)) {
      lines.push_back(line);
    }
    file.close();
  }
  return lines;
}

std::string readResponse(int fd) {
  std::string response;
  char buffer[1024];
  ssize_t count;

  while (true) {
    fd_set set;
    struct timeval timeout;

    FD_ZERO(&set);
    FD_SET(fd, &set);

    // Устанавливаем таймаут
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    // Проверяем, есть ли данные для чтения
    int rv = select(fd + 1, &set, NULL, NULL, &timeout);
    if (rv == -1) {
      perror("select");
      break;
    } else if (rv == 0) {
      // Таймаут истек, данные больше не поступают
      break;
    } else {
      if (FD_ISSET(fd, &set)) {
        count = read(fd, buffer, sizeof(buffer) - 1);
        if (count > 0) {
          buffer[count] = '\0';
          response += buffer;
        } else if (count == 0) {
          // EOF
          break;
        } else {
          if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Нет больше данных
            break;
          } else {
            perror("read");
            break;
          }
        }
      }
    }
  }

  if (!response.empty()) {
    std::cout << "\033[93mServer response: " << std::endl
              << response << "\033[0m" << std::endl;
  }

  return response;
}
/**
 * Отправляет сообщение в дескриптор файла.
 *
 * @param fd дескриптор файла для отправки сообщения
 * @param message сообщение для отправки
 */
void sendMessage(int fd, const std::string& message) {
  std::string msg = message + "\n";
  write(fd, msg.c_str(), msg.size());
  std::cout << "Sending message: " << message << std::endl;
}

bool verifyResponse(const std::string& response, const std::string& message) {
  bool checkPassed = false;
  std::istringstream iss(response);
  std::string line;
  while (std::getline(iss, line)) {
    // Ищем позицию ": " в строке
    size_t pos = line.find(": ");
    if (pos != std::string::npos && pos + 2 < line.length()) {
      std::string receivedMessage =
          line.substr(pos + 2);  // Получаем часть после ": "
      // Удаляем возможные пробелы в конце строки
      receivedMessage.erase(receivedMessage.find_last_not_of(" \n\r\t") + 1);
      if (receivedMessage == message) {
        std::cout << "Message from server '" << receivedMessage
                  << "' matches with message from file '" << message << "'"
                  << std::endl;
        checkPassed = true;
        break;  // Если сообщение найдено, можно прервать цикл
      }
    }
  }
  if (!checkPassed) {
    std::cout << "Message verification failed or message not found."
              << std::endl;
  }
  return checkPassed;
}

/**
 * Проверяет, является ли строка командой.
 *
 * @param input входная строка
 * @return true, если строка начинается с '/', иначе false
 */
bool isCommand(const std::string& input) {
  return !input.empty() && input[0] == '/';
}

/**
 * Обрабатывает сообщения: отправляет их и проверяет ответы.
 *
 * @param writeFd дескриптор файла для записи сообщений
 * @param readFd дескриптор файла для чтения ответов
 * @param messages вектор сообщений для отправки
 * @param nick текущий ник
 * @param currentChannel текущий канал
 * @param delay задержка в секундах между отправкой сообщений
 * @return true, если все проверки прошли успешно, иначе false
 */
bool handleMessages(int writeFd, int readFd,
                    const std::vector<std::string>& messages, std::string& nick,
                    std::string& currentChannel, int delay) {
  bool allChecksPassed = true;
  int countMessages = 0;
  std::string response;

  for (const auto& message : messages) {
    if (!isCommand(message) && !currentChannel.empty()) {
      countMessages += 1;
      std::string readCommand = "/read " + currentChannel;
      sendMessage(writeFd, readCommand);
      std::this_thread::sleep_for(std::chrono::seconds(delay));
      response = readResponse(readFd);
      allChecksPassed &= verifyResponse(response, message);
      continue;
    }

    // Отправка команды или действия
    sendMessage(writeFd, message);
    std::this_thread::sleep_for(std::chrono::seconds(delay));
    response = readResponse(readFd);

    // Обработка специальных команд
    if (message.rfind("/join ", 0) == 0) {
      currentChannel = message.substr(6);
      continue;
    }
    if (message.rfind("/change nickname ", 0) == 0) {
      nick = message.substr(17);
      continue;
    }
    if (message.rfind("/exit ", 0) == 0) {
      currentChannel = "";
      continue;
    }

    if (!isCommand(message)) {
      countMessages += 1;
    }
  }

  if (!currentChannel.empty()) {
    std::string readCommand = "/read " + currentChannel;
    sendMessage(writeFd, readCommand);
    response = readResponse(readFd);
    int count = std::count(response.begin(), response.end(), '\n');
    if (count <
        countMessages) {  // Используем '<' для проверки недостатка сообщений
      std::cout << "Error: Number of messages from server does not match with "
                   "number of"
                << " messages from file." << std::endl;
      allChecksPassed = false;
    }
  }

  return allChecksPassed;
}

/**
 * Проверяет, соответствует ли первая строка файла ожидаемому формату.
 * Ожидаемый формат: serverAddress:port login password nickname
 *
 * @param line строка для проверки
 * @return true, если формат корректен, иначе false
 */
bool isValidFormat(const std::string& line) {
  std::istringstream iss(line);
  std::string serverAddress, login, password, nickname;
  if (!(iss >> serverAddress >> login >> password >> nickname)) {
    return false;  // Не удалось извлечь все четыре части
  }
  return true;
}

int main(int argc, char* argv[]) {
  if (argc < 5 || argc > 6) {  // Разрешаем опцию -r, если потребуется
    std::cerr << "Usage: " << argv[0] << " -f <file> -t <delay> [-r]"
              << std::endl;
    return 1;
  }

  std::string filePath;
  int delay = 0;
  bool repeat = false;

  // Парсинг аргументов командной строки
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-f" && i + 1 < argc) {
      filePath = argv[++i];
    } else if (arg == "-t" && i + 1 < argc) {
      delay = std::stoi(argv[++i]);
    } else if (arg == "-r") {
      repeat =
          true;  // Включение циклической отправки сообщений (если потребуется)
    }
  }

  auto lines = readFile(filePath);

  if (lines.empty()) {
    std::cerr << "File is empty or cannot be read." << std::endl;
    return 1;
  }

  if (!isValidFormat(lines[0])) {
    std::cerr << "First line does not have the correct format." << std::endl;
    return 1;
  }

  std::string serverAddress, login, password, nick;
  {
    std::istringstream iss(lines[0]);
    iss >> serverAddress >> login >> password >> nick;
  }

  // Остальные строки файла — это сообщения для отправки и проверки
  std::vector<std::string> messages(lines.begin() + 1, lines.end());

  int to_child[2];
  int from_child[2];

  if (pipe(to_child) == -1 || pipe(from_child) == -1) {
    perror("pipe");
    return 1;
  }

  pid_t pid = fork();
  if (pid == -1) {
    perror("fork");
    return 1;
  }

  if (pid == 0) {
    // Дочерний процесс
    close(to_child[1]);
    close(from_child[0]);
    dup2(to_child[0], STDIN_FILENO);
    dup2(from_child[1], STDOUT_FILENO);
    close(to_child[0]);
    close(from_child[1]);
    execl("../program/client", "../program/client", serverAddress.c_str(),
          nullptr);
    perror("execl");
    exit(1);
  } else {
    // Родительский процесс
    close(to_child[0]);
    close(from_child[1]);

    int writeFd = to_child[1];
    int readFd = from_child[0];

    std::cout << "Waiting for initial server responses..." << std::endl;
    readResponse(readFd);

    // Симулируем ввод для регистрации
    sendMessage(writeFd, "1");  // Выбор опции "Sign up"
    readResponse(readFd);
    std::this_thread::sleep_for(std::chrono::seconds(delay + 1));
    sendMessage(writeFd, login);  // Ввод логина
    readResponse(readFd);
    std::this_thread::sleep_for(std::chrono::seconds(delay + 1));
    sendMessage(writeFd, password);  // Ввод пароля
    readResponse(readFd);
    std::this_thread::sleep_for(std::chrono::seconds(delay + 1));
    sendMessage(writeFd, nick);  // Ввод ника
    readResponse(readFd);
    std::this_thread::sleep_for(std::chrono::seconds(delay + 1));
    sendMessage(writeFd, "/join ch");  // Подключение к каналу "ch"

    // Читаем ответ после регистрации и подключения
    readResponse(readFd);
    std::this_thread::sleep_for(std::chrono::seconds(delay + 1));

    std::string currentChannel = "ch";

    // Отправляем и проверяем сообщения из файла
    bool allChecksPassed =
        handleMessages(writeFd, readFd, messages, nick, currentChannel, delay);

    if (allChecksPassed) {
      std::cout << "\033[38;5;82mAll messages verified "
                   "successfully.\033[0m"
                << std::endl;
    } else {
      std::cout << "\033[31mThere were verification errors.\033[0m"
                << std::endl;
    }

    std::cout << "No more commands to send. Exiting." << std::endl;

    close(writeFd);
    waitpid(pid, nullptr, 0);
  }

  return 0;
}
