#include "command_handler.hpp"

std::string NickCommand::handleCommand(std::vector<std::string> &command,
                                       DataBase &db, User &user) {
  if (!validateCommand(command)) {
    return "Error command, usage: /nick <new_nickname>";
  }
  std::string newNickname = "";
  parseNickCommand(command, newNickname);

  {
    std::lock_guard<std::mutex> lock(dbMutex);
    db.changeNickname(user.id, newNickname);
  }

  return newNickname;
}

bool NickCommand::validateCommand(std::vector<std::string> &command) {
  return command.size() == 2;
}

void NickCommand::parseNickCommand(std::vector<std::string> &command,
                                   std::string &newNnickname) {
  newNnickname = command[1];
}