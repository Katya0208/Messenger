#pragma once

#include <arpa/inet.h>
#include <sndfile.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <vector>
#define OPUS_MAX_PACKET_SIZE 4000

enum DataType { TEXT, NUMBER, AUDIO, FILE_TYPE, VOICE };

struct AudioPacket {
  uint64_t timestamp;  // метка времени в миллисекундах
  unsigned char opus_data[OPUS_MAX_PACKET_SIZE];  // аудиоданные
  int opus_length;                                // длина данных
};
struct FilePacket {
  uint64_t timestamp;
  std::string filename;
  uint32_t file_size;
  std::vector<uint8_t> file_data;  // Хранение данных файла
  int file_id;  // Уникальный идентификатор файла
};

enum Flags {
  COMMAND = 0,
  CHANNEL = 1,
  NICK = 2,
  ID = 3,
  DEL_CHANNEL = 4,
  NO_CHANNEL = 5,
  LOGIN_SIGN_UP = 6,
  PASSWORD_SIGN_UP = 7,
  NICK_SIGN_UP = 8,
  LOGIN_LOG_IN = 9,
  PASSWORD_LOG_IN = 10,
  NICK_LOG_IN = 11,
  CHECK_LOGIN = 12,
  CHECK_PASSWORD = 13,
  CHECK_NICKNAME = 14,
  REGISTERED = 15,
  CHECK_ID = 16,
  ID_CORRECT = 17,
  AUTHORIZED = 18,
  CHANGE_NICK = 19,
  AUDIOFILE_ERROR = 20,
  TIME_ON = 21,
  TIME_OFF = 22,
  FILE_ERROR = 23,
};

struct MessageHeader {
  DataType type;
  uint32_t size;
  uint32_t flag = 0;
};

struct Message {
  MessageHeader header;
  std::vector<uint8_t> body;
  AudioPacket audioPacket;

  void serialize(std::vector<uint8_t> &buffer) const;
  void deserialize(const std::vector<uint8_t> &buffer);

  void clearMessage(Message &message) {
    message.header = {};  // Resets to default values
    message.body.clear();
  }

  void setTextMessage(const std::string &text) {
    header.type = DataType::TEXT;
    body.assign(text.begin(), text.end());
    header.size = body.size();
  }

  bool setAudioMessage(const std::string &filePath, const std::string &id,
                       const std::string &channel);
  bool setAudioMessage(const std::string &filePath);
  bool setVoiceMessage(const std::vector<int16_t> &audioData,
                       const std::string &channel);
  bool setVoiceMessage(AudioPacket &packet, const std::string &channel);

  bool setFileMessage(const FilePacket &packet, const std::string &channel,
                      const std::string &id);
};

class MySocket {
 public:
  MySocket() {}
  ~MySocket() { closeSocket(); }

  bool createSocket();
  bool connectSocket(const std::string &ip, int port);
  bool bindSocket(int port);
  bool listenSocket(int backlog = 3);
  int acceptSocket();

  void setSocket(int socket) { sock = socket; }
  int getSocket() const { return sock; }
  std::string getIP();
  bool sendFileIfExists(const std::string &filePath, const std::string &channel,
                        const std::string &id);
  bool sendMessage(const Message &message);
  bool sendMessage(const Message &message, int socket);
  bool sendAudioMessage(AudioPacket &packet);
  bool receiveMessage(Message &message);
  bool processIncomingData();
  bool sendAudioMessage(const Message &message);
  bool receiveAudioMessage(Message &message);
  bool sendFile(const std::string &filePath, int socket);

  static DataType determineType(const std::string &input);
  static bool isNumber(const std::string &input);

  void closeSocket();

 private:
  int sock;
  std::mutex send_mutex;
  struct sockaddr_in address;

  bool extractMessage(Message &message);
};

std::string messageToString(Message &message);
Message stringToMessage(const std::string &text, Message &message);
bool saveFile(const std::string &directory, const std::string &fileName,
              const std::vector<uint8_t> &audioData);
double getAudioDuration(const std::string &filePath);
Message flagOn(Message &message, int flag);
Message flagOff(Message &message);
