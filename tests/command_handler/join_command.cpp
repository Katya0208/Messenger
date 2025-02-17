#include "command_handler.hpp"

std::string JoinCommand::handleCommand(std::vector<std::string>& command,
                                       DataBase& db, User& user) {
  if (!validateCommand(command)) {
    return "Error command, use: join <channel>";
  }
  std::string channel;
  parseJoinCommand(command, channel);
  {
    std::lock_guard<std::mutex> lock(dbMutex);
    db.database_channels = db.channelsFile();
    db.database_channels_members = db.channelsMembersFile(channel);
  }
  if (db.ChannelExists(channel)) {
    if (db.MemberInChannel(user.id)) {
      return "You already on this channel";
    }
    {
      std::lock_guard<std::mutex> lock(dbMutex);
      db.addChannelMember(user.id, channel);
    }
    return "You have joined the channel";
  }
  return "Channel does not exist";
}

void JoinCommand::parseJoinCommand(std::vector<std::string>& command,
                                   std::string& channel) {
  channel = command[1];
}

bool JoinCommand::validateCommand(std::vector<std::string>& command) {
  return command.size() == 2;
}