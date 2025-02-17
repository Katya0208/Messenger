#include "command_handler.hpp"

std::string ReadCommand::handleCommand(std::vector<std::string>& command,
                                       DataBase& db, User& user) {
  if (!validateCommand(command)) {
    return "Error command, use: read <channel>";
  }

  std::string channel;
  parseReadCommand(command, channel);
  {
    std::lock_guard<std::mutex> lock(dbMutex);
    db.database_channels = db.channelsFile();
    db.database_channels_members = db.channelsMembersFile(channel);
  }
  // logMessage("1");
  if (db.ChannelExists(channel)) {
    std::cout << "Channel found: " << channel << std::endl;

    if (db.MemberInChannel(user.id)) {
      std::lock_guard<std::mutex> lock(dbMutex);
      logMessage("Start read file: " + channel, SERVER_LOG_FILE);
      logMessage("Start read", SERVER_LOG_FILE);
      db.database_channels_history = db.channelsHistoryFile(channel);
      logMessage("End read", SERVER_LOG_FILE);
      logMessage("Read file: " + channel, SERVER_LOG_FILE);
      db.database_names = db.nicknamesFile();

      if (db.database_channels_history.empty()) {
        return "Channel is empty";
      }

      return readCommandHistory(db, user);
    }

    return "You don't have access to this channel, use join <channel> for "
           "adding";
  }

  return "Channel not found, use /channels to see available channels";
}

bool ReadCommand::validateCommand(std::vector<std::string>& command) {
  return command.size() == 2;
}

void ReadCommand::parseReadCommand(std::vector<std::string>& command,
                                   std::string& channel) {
  channel = command[1];
}

std::string ReadCommand::readCommandHistory(DataBase& db, User& user) {
  std::ostringstream oss;
  bool first = true;

  // Загружаем пользователей, чтобы `db.database_names` был актуальным
  db.database_names = db.nicknamesFile();

  for (const History& line : db.database_channels_history) {
    logMessage("Read: " + line.time, SERVER_LOG_FILE);

    if (!line.id.empty()) {
      // Ищем пользователя по id
      auto userIt =
          std::find_if(db.database_names.begin(), db.database_names.end(),
                       [&](const User& u) { return u.id == line.id; });

      std::string nickname =
          line.id;  // По умолчанию используем id, если nickname не найден
      if (userIt != db.database_names.end()) {
        nickname = userIt->nickname;
      } else {
        logMessage("User not found for id: " + line.id, SERVER_LOG_FILE);
      }

      logMessage(
          "Find: " + line.time + " " + nickname + ": " +
              (line.isAudio ? "[Audio Message]"
                            : (line.isFile ? "[File Message]" : line.message)),
          SERVER_LOG_FILE);

      if (!first) {
        oss << '\n';
      }

      if (user.timeFlag) {
        oss << "[" << line.time << "] ";
      }

      oss << nickname << ": ";

      if (line.isAudio) {
        oss << "[Voicemail_ID: " << line.voicemailID
            << ", Duration: " << line.duration << "]";
      } else if (line.isFile) {
        oss << "[File_ID: " << line.fileID << ", Name: " << line.filename;
        if (!line.extension.empty()) {
          oss << ", Extension: " << line.extension;
        }
        oss << ", Size: " << line.fileSize << " bytes]";
      } else {
        oss << line.message;
      }

      first = false;

      logMessage(
          "End read: " + line.time + " " + nickname + ": " +
              (line.isAudio ? "[Audio Message]"
                            : (line.isFile ? "[File Message]" : line.message)),
          SERVER_LOG_FILE);
    } else {
      logMessage("ID is empty for line: " + line.time, SERVER_LOG_FILE);
    }
  }

  return oss.str();
}
