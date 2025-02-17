#pragma once

#include <netinet/in.h>
#include <openssl/evp.h>
#include <opus/opus.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <mutex>
#include <queue>
#include <sstream>
#include <thread>
#include <unordered_set>
#include <vector>

#include "../command_handler/command_handler.hpp"

#define SAMPLE_RATE 48000
#define FRAMES_PER_BUFFER 480
#define NUM_CHANNELS 2
#define SAMPLE_TYPE int16_t
#define OPUS_MAX_PACKET_SIZE 4000

struct AudioMessage {
  std::string timestamp;       // Время отправки
  std::string senderNickname;  // Никнейм отправителя
  std::string senderIP;        // IP-адрес отправителя
  std::string messageID;       // ID аудиосообщения
  double duration;  // Продолжительность аудиосообщения в секундах
  std::string filePath;  // Путь к аудиофайлу
};

struct FileMessage {
  std::string timestamp;       // Время отправки
  std::string senderNickname;  // Никнейм отправителя
  std::string senderIP;        // IP-адрес отправителя
  std::string messageID;       // ID аудиосообщения
  std::string filename;        // Путь к аудиофайлу
  uint32_t fileSize;
};

class Server {
 public:
  bool serverRunning = true;
  MySocket serverSocket;
  DataBase db;

  std::mutex channelDataMutex;
  std::mutex clients_mutex;
  std::mutex client_buffers_mutex;
  std::mutex dbMutex;

  std::vector<int> client_sockets;
  std::unordered_map<std::string, std::vector<AudioMessage>>
      channelAudioMessages;
  std::map<int, std::queue<AudioPacket>> client_buffers;

  // Обработка и отправка голосовых сообщений в канале другим клиентам
  void broadcast_audio(const std::vector<SAMPLE_TYPE> &audioData,
                       int sender_socket, std::string &channel);
  // Функция для получения списка сокетов подключенных клиентов
  std::vector<int> getClientSocketsOptimized(
      const std::unordered_set<std::string> &channel_members,
      const std::unordered_map<std::string, int> &user_socket_map);
  // Функция для отправки аудиофайла другому клиенту
  void sendAudiofiletoClient(const std::string &audioId, MySocket &client);
  void helpToUse(const char *programName);  // вывод справки
  bool addChannelOnServer(std::string &channel);
  void processAudioMessage(const Message &message, const std::string &senderIP,
                           const std::string &senderNickname);
  void proccessFileMessage(const Message &message,
                           const std::string &senderNickname);
  bool removeMembersFromDeleteChannel(std::string &channel);
  void messageProcessing(
      MySocket &client, User &user,
      std::string &channel);  // обработка сообщений от клиента
  void registrationOnServer(MySocket &client, std::string nickname,
                            std::string login,
                            std::string password);  // регистрация пользователя
  bool checkLogin(const std::string &login, User &user, int regOrlog);
  bool loginInSet(std::string login, std::unordered_set<std::string> set);
  bool checkPasswordServer(const std::string &password, User &user);
  bool checkPasswordAthorization(const std::string &login,
                                 const std::string &password);
  std::string hashPassword(const std::string &password,
                           const std::string &salt);
  bool checkNickname(const std::string &nickname, User &user);

 private:
  void commandProcessing(MySocket &client, User &user,
                         Message &message);  // обработка команд
};

void serverCommand(int port, Server &server);
void handleClient(int clientSocket,
                  Server &server);  // обработка клиента
void signalHandlerServer(
    int signal);  // обработка сигналов для правильного отключения клиента
bool writeAudioFile(const std::string &filename,
                    const std::vector<uint8_t> &data);