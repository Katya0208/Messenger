#include "command_handler.hpp"

std::string ExitCommand::handleCommand(std::vector<std::string> &command,
                                       DataBase &db, User &user) {
  if (!validateCommand(command)) {
    return "Error command, usage: exit <channel>";
  }
  std::string channel;
  parseExitCommand(command, channel);
  {
    std::lock_guard<std::mutex> lock(dbMutex);
    db.database_channels = db.channelsFile();
    db.database_channels_members = db.channelsMembersFile(channel);
  }

  if (db.ChannelExists(channel)) {
    if (db.MemberInChannel(user.id)) {
      {
        std::lock_guard<std::mutex> lock(dbMutex);
        db.deleteChannelMember(user.id, channel);
      }
      return "You have left the channel";
    }
    return "You are not in this channel";
  }
  return "Channel not found";
}

bool ExitCommand::validateCommand(std::vector<std::string> &command) {
  return command.size() == 2;
}

void ExitCommand::parseExitCommand(std::vector<std::string> &command,
                                   std::string &channel) {
  channel = command[1];
}
