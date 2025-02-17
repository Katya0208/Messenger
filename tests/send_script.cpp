#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

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

/**
 * Читает ответ из заданного дескриптора файла и возвращает его как строку.
 *
 * @param fd дескриптор файла для чтения
 * @return ответ как строка
 */
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

/**
 * Отправляет сообщения в дескриптор файла и читает ответы.
 *
 * @param writeFd дескриптор файла для записи сообщений
 * @param readFd дескриптор файла для чтения ответов
 * @param messages вектор сообщений для отправки
 * @param delay задержка в секундах между отправкой сообщений
 * @param repeat флаг для циклической отправки сообщений
 */
void handleMessages(int writeFd, int readFd,
                    const std::vector<std::string>& messages, int delay,
                    bool repeat) {
  size_t messageCount = messages.size();
  size_t index = 0;
  while (true) {
    if (messageCount == 0) {
      break;  // Нет сообщений для отправки, выход из цикла
    }

    // Отправка текущего сообщения
    sendMessage(writeFd, messages[index]);
    std::this_thread::sleep_for(std::chrono::seconds(delay));
    std::string response = readResponse(readFd);

    // Переход к следующему сообщению
    if (repeat) {
      index = (index + 1) % messageCount;  // Круговая итерация
    } else {
      ++index;
      if (index >= messageCount) {
        break;  // Выход из цикла после отправки всех сообщений, если не
                // повторять
      }
    }
  }
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
  bool repeat = false;

  if (argc < 5 || argc > 6) {  // Разрешаем опцию -r
    std::cerr << "Usage: " << argv[0] << " -f <file> -t <delay> [-r]"
              << std::endl;
    return 1;
  }

  std::string filePath;
  int delay = 0;

  // Парсинг аргументов командной строки
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-f" && i + 1 < argc) {
      filePath = argv[++i];
    } else if (arg == "-t" && i + 1 < argc) {
      delay = std::stoi(argv[++i]);
    } else if (arg == "-r") {
      repeat = true;  // Включение циклической отправки сообщений
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

  std::string serverAddress, login, password, nickname;
  {
    std::istringstream iss(lines[0]);
    iss >> serverAddress >> login >> password >> nickname;
  }

  // Остальные строки файла — это сообщения для отправки
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
    sendMessage(writeFd, login);  // Ввод логина
    readResponse(readFd);
    sendMessage(writeFd, password);  // Ввод пароля
    readResponse(readFd);
    sendMessage(writeFd, nickname);  // Ввод ника
    readResponse(readFd);
    sendMessage(writeFd, "/join ch");  // Подключение к каналу "ch"

    // Читаем ответ после регистрации и подключения
    readResponse(readFd);

    // Отправляем остальные сообщения из файла
    handleMessages(writeFd, readFd, messages, delay, repeat);

    std::cout << "No more commands to send. Exiting." << std::endl;

    close(writeFd);
    waitpid(pid, nullptr, 0);
  }

  return 0;
}
