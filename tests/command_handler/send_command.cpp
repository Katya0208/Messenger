#include "command_handler.hpp"

bool SendCommand::validateCommand(std::vector<std::string>& command) {
  return command.size() >= 3;
}

void SendCommand::parseSendCommand(std::vector<std::string>& command,
                                   std::string& channel, std::string& message) {
  channel = command[1];
  for (int i = 2; i < command.size(); i++) {
    message += command[i] + " ";
  }
}

std::string SendCommand::handleCommand(std::vector<std::string>& command,
                                       DataBase& db, User& user) {
  if (!validateCommand(command)) {
    return "Error command, use: send <channel> <message>";
  }
  std::string channel, message;
  parseSendCommand(command, channel, message);

  {
    std::lock_guard<std::mutex> lock(dbMutex);
    db.database_channels = db.channelsFile();
    db.database_channels_members = db.channelsMembersFile(channel);
  }
  if (db.ChannelExists(channel)) {
    if (db.MemberInChannel(user.id)) {
      {
        std::lock_guard<std::mutex> lock(dbMutex);
        db.database_channels_history = db.channelsHistoryFile(channel);
        db.addMessageInChannel(user.id, channel, message);
      }
      return "Message sent";
    }
    return "You don't have access to this channel, use join <channel> for "
           "adding";
  }
  return "Channel not found, use /channels to see available channels";
}
