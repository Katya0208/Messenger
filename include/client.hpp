#pragma once
#include <arpa/inet.h>
#include <netinet/in.h>
#include <opus/opus.h>
#include <portaudio.h>
#include <sndfile.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstring>
#include <ctime>
#include <iostream>
#include <mutex>
#include <queue>
#include <regex>
#include <sstream>
#include <thread>

#define SAMPLE_RATE 48000
#define FRAMES_PER_BUFFER 480
#define NUM_CHANNELS 2
#define SAMPLE_TYPE paInt16
#define FILE_FORMAT (SF_FORMAT_WAV | SF_FORMAT_PCM_16)

#include "../include/mysocket.hpp"

bool ready = false;  // Флаг готовности для вывода приглашения

class Client {
  std::string nickname = "";

  void helpToUse();
  bool isValidIpPort(const std::string &ip_port);
  bool isValidPort(const std::string &portStr);
  bool isValidIp(const std::string &ip);
  int countWords(const std::string &text);

  bool isCommand(std::string &input);

  void signUp(std::string &answer);
  void LogIn(std::string &answer);
  bool checkPassword(const std::string &password);

 public:
  bool clientRunning = true;
  bool listeningStatus = true;
  std::string id = "";
  MySocket clientSocket;
  std::mutex mtx;
  std::condition_variable cv;
  std::string lastChannel = "";
  std::queue<std::string> messageQueue;  // Очередь сообщений от сервера
  bool loginCorrect = false;
  bool passwordCorrect = false;
  bool nicknameCorrect = false;
  bool timeFlag = false;
  bool recording_start = false;
  std::vector<short> audio_buffer;

  void record_audio(int recOrVoice, std::string &channel);
  void processAudioMessage(const Message &message);
  void processVoiceMessage(const Message &message);
  void save_audio_to_file(const std::string &filename);
  std::string generateFilename(const std::string &userId);

  bool parseConnectCommand(const std::string &input, std::string &ip_port,
                           std::string &nick, std::string &channel);
  void ParseIpPort(std::string ipAndPort, std::string &ip, int &port);
  void registration(const std::string &nickname);
  void helpFlagAnswer(const char *program_name);
  bool parseArgc(int argc, char *argv[], std::string &ipPort, std::string &nick,
                 std::string &channel, std::string &ip, int &port);
  void promptConnect(std::string &ip_port, std::string &nick,
                     std::string &channel);
  void enterOnServer();
  void setNickname(std::string &nick);
  std::string getNickname();
};

void signalHandler(int signal);
void ReceiveMessage(std::string &id, std::string &currentChannel,
                    Client &client);
bool handleConnectCommand(const std::string &input, std::string &ipPort,
                          std::string &ip, int &port, std::string &nick,
                          std::string &channel, std::string &id,
                          std::string &currentChannel, Client &client);
bool commandHandler(std::string &command, Message &message, std::string &ipPort,
                    std::string &ip, int &port, const std::string &nick,
                    std::string &channel, std::string &currentChannel,
                    std::string &id, Client &client);
void play_audio(const std::vector<uint8_t> &audioData);
void play_audio(const std::vector<int16_t> &audioData);