#pragma once

#include <string>
#include <vector>

#include "../include/database.hpp"

class CommandHandler {
 public:
  virtual std::string handleCommand(std::vector<std::string> &command,
                                    DataBase &db, User &user) = 0;

 private:
  virtual bool validateCommand(std::vector<std::string> &command) = 0;
};

class ReadCommand : public CommandHandler {
 public:
  std::string handleCommand(std::vector<std::string> &command, DataBase &db,
                            User &user) override;

 private:
  bool validateCommand(std::vector<std::string> &command) override;
  void parseReadCommand(std::vector<std::string> &command,
                        std::string &channel);
  std::string readCommandHistory(DataBase &db, User &user);
};

class SendCommand : public CommandHandler {
 public:
  std::string handleCommand(std::vector<std::string> &command, DataBase &db,
                            User &user) override;

 private:
  bool validateCommand(std::vector<std::string> &command) override;
  void parseSendCommand(std::vector<std::string> &command, std::string &channel,
                        std::string &message);
};

class JoinCommand : public CommandHandler {
 public:
  std::string handleCommand(std::vector<std::string> &command, DataBase &db,
                            User &user) override;

 private:
  bool validateCommand(std::vector<std::string> &command) override;
  void parseJoinCommand(std::vector<std::string> &command,
                        std::string &channel);
};

class ExitCommand : public CommandHandler {
 public:
  std::string handleCommand(std::vector<std::string> &command, DataBase &db,
                            User &user) override;

 private:
  bool validateCommand(std::vector<std::string> &command) override;
  void parseExitCommand(std::vector<std::string> &command,
                        std::string &channel);
};

class NickCommand : public CommandHandler {
 public:
  std::string handleCommand(std::vector<std::string> &command, DataBase &db,
                            User &user) override;

 private:
  bool validateCommand(std::vector<std::string> &command) override;
  void parseNickCommand(std::vector<std::string> &command,
                        std::string &newNnickname);
};