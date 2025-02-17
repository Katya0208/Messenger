#pragma once

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "mysocket.hpp"
#include "other.hpp"

namespace fs = std::filesystem;

struct User {
  std::string id;
  std::string nickname;
  std::string socketNumber;
  std::string login;
  std::string password;
  bool timeFlag = false;
};

struct History {
  std::string time;
  std::string id;       // ID пользователя
  std::string message;  // Текстовое сообщение
  bool isAudio = false;
  bool isFile = false;
  std::string voicemailID;  // Для аудиосообщений
  std::string duration;     // Для аудиосообщений
  std::string fileID;       // Для файловых сообщений
  std::string filename;     // Для файловых сообщений
  std::string extension;  // Для файловых сообщений (если добавлено)
  uint32_t fileSize;  // Для файловых сообщений
};

class DataBase {
 private:
  std::unordered_set<std::string> names;
  const std::string users_file = "users.txt";
  const std::string channels_file = "channels.txt";

 public:
  std::vector<User> database_names;
  std::unordered_set<std::string> database_channels;
  std::unordered_set<std::string> database_channels_members;
  std::vector<History> database_channels_history;
  DataBase() {
    std::ifstream Users(users_file);
    std::ifstream Channels(channels_file);
  }
  std::unordered_set<std::string> addFile(const std::string &element);
  std::vector<User> nicknamesFile();
  bool parseHistoryLine(const std::string &lineStr, History &historyEntry);
  std::unordered_set<std::string> channelsFile();
  std::unordered_set<std::string> channelsMembersFile(
      const std::string &channel);
  std::vector<History> channelsHistoryFile(const std::string &channel);
  int channelsMembersCount(const std::string &channel);
  void addUser(const std::string &username, int socketNumber,
               const std::string &login, const std::string &password);
  // void addUser(const std::string &username, const std::string &id);

  void addChannel(const std::string &channel);
  void addChannelMember(const std::string &username,
                        const std::string &channel);
  void addMessageInChannel(const std::string &username,
                           const std::string &channel,
                           const std::string &message);
  void addAudioMessageToChannelHistory(const std::string &senderNickname,
                                       const std::string &channel,
                                       const std::string &audioMessageID,
                                       double duration);
  void addFileMessageToChannelHistory(const std::string &senderNickname,
                                      const std::string &channel,
                                      const std::string &fileMessageID,
                                      std::string &filename,
                                      uint32_t &fileSize);

  void deleteChannelMember(const std::string &id, const std::string &channel);
  void deleteChannel(const std::string &channel);
  std::string pathToChannelsMembers(const std::string &channel);
  std::string pathToChannelsHistory(const std::string &channel);
  std::string pathToChannels();

  void changeNickname(const std::string &id, const std::string &newUsername);
  std::vector<User> addFileNicknames(const std::string &path);
  bool removeChannelFiles(const std::string &pathMembers,
                          const std::string &patHistory);
  bool ChannelExists(std::string &channel);  // проверка существования канала
  bool MemberInChannel(const std::string &id);
  bool nickameInSet(std::string nickname, std::unordered_set<std::string> set);
  std::string userId(std::string &login);  // поиск id клиента в базе данных
  std::string userNickbyId(std::string &id);
  void channelMessage(
      Message &message, std::string &channel,
      MySocket &client);  // проверка существования канала в базе
                          // данных и его создание если нету
  void nicknameMessage(Message &message, std::string &nickname, std::string &id,
                       std::string &channel, MySocket &client);
  bool idMessage(bool &idReceived, Message &message, std::string &id);
  std::string listOfChannelsOnServer();
};