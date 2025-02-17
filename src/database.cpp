#include "../include/database.hpp"

std::unordered_set<std::string> DataBase::addFile(const std::string &path) {
  std::unordered_set<std::string> set;
  std::ifstream dbFile(path);
  std::string elem;
  while (dbFile >> elem) {
    set.insert(elem);
  }
  return set;
}
std::vector<User> DataBase::addFileNicknames(const std::string &path) {
  User user;
  std::vector<User> users_map;
  std::ifstream dbFile(path);
  std::string line;
  while (std::getline(dbFile, line)) {
    std::istringstream iss(line);
    std::string id, nickname, socketNumber, login, password;

    if (std::getline(iss, id, ':') && std::getline(iss, nickname, ':') &&
        std::getline(iss, socketNumber, ':') && std::getline(iss, login, ':') &&
        std::getline(iss, password)) {
      user.id = id;
      user.nickname = nickname;
      user.socketNumber = socketNumber;
      user.login = login;
      user.password = password;
      users_map.push_back(user);
    }
  }
  return users_map;
}

void createDirectoryIfNeeded(const std::string &path) {
  fs::path dir(path);
  if (!fs::exists(dir)) {
    if (!fs::create_directories(dir)) {
      std::cerr << "Failed to create directory: " << path << std::endl;
    }
  }
}

std::string DataBase::pathToChannelsMembers(const std::string &channel) {
  std::string directory = "./channels/members/";
  createDirectoryIfNeeded(directory);
  return directory + channel + "_members.txt";
}

std::string DataBase::pathToChannelsHistory(const std::string &channel) {
  std::string directory = "./channels/history/";
  createDirectoryIfNeeded(directory);
  return directory + channel + "_history.txt";
}

std::string DataBase::pathToChannels() {
  std::string directory = "./channels/";
  createDirectoryIfNeeded(directory);
  return directory + "channels.txt";
}

std::vector<User> DataBase::nicknamesFile() {
  std::string directory = "./users/";
  createDirectoryIfNeeded(directory);
  return addFileNicknames(directory + users_file);
}

std::unordered_set<std::string> DataBase::channelsFile() {
  std::string directory = "./channels/";
  createDirectoryIfNeeded(directory);
  return addFile(directory + channels_file);
}

std::unordered_set<std::string> DataBase::channelsMembersFile(
    const std::string &channel) {
  std::string path = pathToChannelsMembers(channel);
  return addFile(path);
}

int DataBase::channelsMembersCount(const std::string &channel) {
  std::string path = pathToChannelsMembers(channel);
  std::ifstream dbFile(path);
  std::string line;
  int count = 0;
  while (std::getline(dbFile, line)) {
    count++;
  }
  return count;
}

#include <regex>

std::vector<History> DataBase::channelsHistoryFile(const std::string &channel) {
  std::string path = pathToChannelsHistory(channel);
  std::vector<History> container;
  std::ifstream dbFile(path);
  std::string line;

  // Регулярные выражения для текстовых, аудиосообщений и файловых сообщений
  std::regex textMessagePattern(R"(\[(.+?)\] (.+?): (.+))");
  std::regex audioMessagePattern(
      R"(\[(.+?)\] (.+?): \[Voicemail_ID: (.+), duration: (.+)\])");
  std::regex fileMessagePattern(
      R"(\[(.+?)\] (.+?): \[File_ID: (.+?), Name: (.+?),(?: Extension: (.+?),)? Size: (\d+) \])");

  while (std::getline(dbFile, line)) {
    std::smatch matches;
    History historyEntry;

    if (std::regex_match(line, matches, textMessagePattern)) {
      // Текстовое сообщение
      if (matches.size() == 4) {
        historyEntry.time = matches[1].str();
        historyEntry.id = matches[2].str();
        historyEntry.message = matches[3].str();
        historyEntry.isAudio = false;
        historyEntry.isFile = false;
        container.push_back(historyEntry);
      }
    } else if (std::regex_match(line, matches, audioMessagePattern)) {
      // Аудиосообщение
      if (matches.size() == 5) {
        historyEntry.time = matches[1].str();
        historyEntry.id = matches[2].str();
        historyEntry.voicemailID = matches[3].str();
        historyEntry.duration = matches[4].str();
        historyEntry.isAudio = true;
        historyEntry.isFile = false;
        container.push_back(historyEntry);
      }
    } else if (std::regex_match(line, matches, fileMessagePattern)) {
      // Файловое сообщение
      // matches:
      // 1 - time
      // 2 - id
      // 3 - fileID
      // 4 - filename
      // 5 - extension (опционально)
      // 6 - fileSize
      if (matches.size() >= 6) {
        historyEntry.time = matches[1].str();
        historyEntry.id = matches[2].str();
        historyEntry.fileID = matches[3].str();
        historyEntry.filename = matches[4].str();
        historyEntry.extension = matches[5].matched ? matches[5].str() : "";
        historyEntry.fileSize = std::stoul(matches[6].str());
        historyEntry.isAudio = false;
        historyEntry.isFile = true;
        container.push_back(historyEntry);
      }
    } else {
      std::cerr << "Failed to parse line: " << line << std::endl;
    }
  }
  return container;
}

void DataBase::addUser(const std::string &username, int socketNumber,
                       const std::string &login, const std::string &password) {
  std::string directory = "./users/";
  std::string colon = ":";
  boost::uuids::random_generator generator;
  boost::uuids::uuid uuid = generator();
  createDirectoryIfNeeded(directory);
  std::ofstream UsersFile(directory + users_file, std::ios_base::app);
  UsersFile << uuid << colon << username << colon << socketNumber << colon
            << login << colon << password << std::endl;
}

void DataBase::addChannel(const std::string &channel) {
  std::string directory = "./channels/";
  createDirectoryIfNeeded(directory);
  std::ofstream ChannelsFile(directory + channels_file, std::ios_base::app);
  ChannelsFile << channel << std::endl;
}

void DataBase::addChannelMember(const std::string &id,
                                const std::string &channel) {
  std::string channel_members_file = pathToChannelsMembers(channel);
  std::ofstream ChannelMembersFile(channel_members_file, std::ios_base::app);

  ChannelMembersFile << id << std::endl;
}

void DataBase::addMessageInChannel(const std::string &id,
                                   const std::string &channel,
                                   const std::string &message) {
  std::string channel_members_file = pathToChannelsHistory(channel);
  // database_names = nicknamesFile();
  std::string nickname;
  std::ofstream ChannelHistoryFile(channel_members_file, std::ios_base::app);
  std::time_t currentTime = std::time(nullptr);
  char timeBuffer[9];
  std::strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S",
                std::localtime(&currentTime));
  ChannelHistoryFile << "[" << timeBuffer << "] " << id << ": " << message
                     << std::endl;
}

bool DataBase::parseHistoryLine(const std::string &lineStr,
                                History &historyEntry) {
  // Regex patterns for matching text and audio messages
  std::regex textMessagePattern(R"(\[(.+?)\] (.+?): (.+))");
  std::regex audioMessagePattern(
      R"(\[(.+?)\] (.+?): \[Voicemail_ID: (.+), duration: (.+)\])");

  std::smatch matches;

  if (std::regex_match(lineStr, matches, textMessagePattern)) {
    // It's a text message
    if (matches.size() == 4) {
      historyEntry.time = matches[1].str();
      historyEntry.id = matches[2].str();
      historyEntry.message = matches[3].str();
      historyEntry.isAudio = false;
      return true;
    }
  } else if (std::regex_match(lineStr, matches, audioMessagePattern)) {
    // It's an audio message
    if (matches.size() == 5) {
      historyEntry.time = matches[1].str();
      historyEntry.id = matches[2].str();
      historyEntry.voicemailID = matches[3].str();
      historyEntry.duration = matches[4].str();
      historyEntry.isAudio = true;
      return true;
    }
  }

  return false;  // Failed to parse
}

std::string formatFileSize(uint32_t fileSize) {
  double sizeInMB = static_cast<double>(fileSize) / (1024 * 1024);
  char buffer[50];
  std::snprintf(buffer, sizeof(buffer), "%.2f MB", sizeInMB);
  return std::string(buffer);
}

void DataBase::addFileMessageToChannelHistory(const std::string &senderNickname,
                                              const std::string &channel,
                                              const std::string &fileMessageID,
                                              std::string &filename,
                                              uint32_t &fileSize) {
  std::string channelHistoryFilePath = pathToChannelsHistory(channel);
  std::ofstream channelHistoryFile(channelHistoryFilePath, std::ios_base::app);
  if (!channelHistoryFile.is_open()) {
    std::cerr << "Не удалось открыть файл истории канала: "
              << channelHistoryFilePath << std::endl;
    return;
  }

  // Получаем текущее время в формате YYYY-MM-DD HH:MM:SS
  std::time_t currentTime = std::time(nullptr);
  char timeBuffer[20];
  std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S",
                std::localtime(&currentTime));

  // Записываем в файл истории
  channelHistoryFile << "[" << timeBuffer << "] " << senderNickname << ": "
                     << "[File_ID: " << fileMessageID << ", Name: " << filename
                     << ", Size: " << formatFileSize(fileSize) << "]"
                     << std::endl;
  channelHistoryFile.close();
}
void DataBase::addAudioMessageToChannelHistory(
    const std::string &senderNickname, const std::string &channel,
    const std::string &audioMessageID, double duration) {
  std::string channelHistoryFilePath = pathToChannelsHistory(channel);
  std::ofstream channelHistoryFile(channelHistoryFilePath, std::ios_base::app);
  if (!channelHistoryFile.is_open()) {
    std::cerr << "Не удалось открыть файл истории канала: "
              << channelHistoryFilePath << std::endl;
    return;
  }

  // Получаем текущее время в формате YYYY-MM-DD HH:MM:SS
  std::time_t currentTime = std::time(nullptr);
  char timeBuffer[20];
  std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S",
                std::localtime(&currentTime));

  // Форматируем продолжительность
  int durationMinutes = static_cast<int>(duration) / 60;
  int durationSeconds = static_cast<int>(duration) % 60;
  std::ostringstream durationStream;
  durationStream << std::setfill('0') << std::setw(2) << durationMinutes << ":"
                 << std::setfill('0') << std::setw(2) << durationSeconds;
  std::string durationStr = durationStream.str();

  // Записываем в файл истории
  channelHistoryFile << "[" << timeBuffer << "] " << senderNickname << ": "
                     << "[Voicemail_ID: " << audioMessageID
                     << ", duration: " << durationStr << "]" << std::endl;

  channelHistoryFile.close();
}

void DataBase::deleteChannelMember(const std::string &id,
                                   const std::string &channel) {
  std::string channel_members_file = pathToChannelsMembers(channel);
  std::ifstream ChannelMembersFile(channel_members_file);
  if (!ChannelMembersFile.is_open()) {
    std::cerr << "Error: Unable to open file " << std::endl;
    return;
  }
  std::ofstream temp_file;
  temp_file.open("temp.txt");
  std::string line;
  while (std::getline(ChannelMembersFile, line)) {
    if (line != id) {
      temp_file << line << std::endl;
    }
  }
  temp_file.close();
  ChannelMembersFile.close();
  if (remove(channel_members_file.c_str()) != 0) {
    std::cerr << "Error: Unable to remove original channel members file"
              << std::endl;
    return;
  }

  if (rename("temp.txt", channel_members_file.c_str()) != 0) {
    std::cerr << "Error: Unable to rename temporary file to " << channel
              << std::endl;
    return;
  }
}

void DataBase::deleteChannel(const std::string &channel) {
  std::string channels_file = pathToChannels();
  std::ifstream ChannelsFile(channels_file);
  if (!ChannelsFile.is_open()) {
    std::cerr << "Error: Unable to open file " << std::endl;
    return;
  }
  std::ofstream temp_file;
  temp_file.open("temp.txt");
  std::string line;
  while (std::getline(ChannelsFile, line)) {
    if (line != channel) {
      temp_file << line << std::endl;
    }
  }
  temp_file.close();
  ChannelsFile.close();

  if (remove(channels_file.c_str()) != 0) {
    std::cerr << "Error: Unable to remove original channel members file"
              << std::endl;
    return;
  }

  if (rename("temp.txt", channels_file.c_str()) != 0) {
    std::cerr << "Error: Unable to rename temporary file to " << channel
              << std::endl;
    return;
  }
}

void DataBase::changeNickname(const std::string &id,
                              const std::string &newUsername) {
  std::string users_file = "./users/users.txt";
  std::ifstream UsersFile(users_file);

  if (!UsersFile.is_open()) {
    std::cerr << "Error: Unable to open file " << users_file << std::endl;
    return;
  }

  std::ofstream NewUsersFile("NewUsersFile.txt");
  if (!NewUsersFile.is_open()) {
    std::cerr << "Error: Unable to create temporary file" << std::endl;
    UsersFile.close();
    return;
  }

  std::string line;

  while (std::getline(UsersFile, line)) {
    std::istringstream iss(line);
    std::string userId, nickname, socketNumber, login, password;

    if (std::getline(iss, userId, ':') && std::getline(iss, nickname, ':') &&
        std::getline(iss, socketNumber, ':') && std::getline(iss, login, ':') &&
        std::getline(iss, password)) {
      if (id != userId) {
        // Write the original line if the ID doesn't match
        NewUsersFile << line << std::endl;
      } else {
        // Replace the nickname with newUsername
        NewUsersFile << userId << ":" << newUsername << ":" << socketNumber
                     << ":" << login << ":" << password << std::endl;
      }
    } else {
      std::cerr << "Error: Malformed line in users file: " << line << std::endl;
      // Optionally, you can decide how to handle malformed lines.
      // Here, we'll write them back as is.
      NewUsersFile << line << std::endl;
    }
  }

  UsersFile.close();
  NewUsersFile.close();

  // Replace the old users file with the new one
  if (remove(users_file.c_str()) != 0) {
    std::cerr << "Error: Unable to remove original users file" << std::endl;
    return;
  }

  if (rename("NewUsersFile.txt", users_file.c_str()) != 0) {
    std::cerr << "Error: Unable to rename temporary file to " << users_file
              << std::endl;
    return;
  }
}

bool DataBase::removeChannelFiles(const std::string &pathMembers,
                                  const std::string &patHistory) {
  try {
    if (std::remove(pathMembers.data()) == 0 &&
        std::remove(patHistory.data()) == 0) {
      return true;
    } else {
      // Обработка ошибки
      switch (errno) {
        case ENOENT:
          // Файл не существует
          return false;
        case EACCES:
          // Нет доступа к файлу
          return false;
        default:
          // Другая ошибка
          return false;
      }
    }
  } catch (const std::exception &e) {
    // Обработка исключения
    return false;
  }
}

bool DataBase::ChannelExists(std::string &channel) {
  std::lock_guard<std::mutex> lock(dbMutex);
  return database_channels.find(channel) != database_channels.end();
}

bool DataBase::nickameInSet(std::string nickname,
                            std::unordered_set<std::string> set) {
  return set.find(nickname) != set.end();
}

std::string DataBase::userId(std::string &login) {
  std::string id;
  {
    std::lock_guard<std::mutex> lock(dbMutex);
    database_names = nicknamesFile();
    for (User user : database_names) {
      if (user.login == login) {
        logMessage("Find id: " + user.id, SERVER_LOG_FILE);
        id = user.id;
        break;
      } else {
        logMessage("Not find id: " + user.id, SERVER_LOG_FILE);
      }
    }
  }
  return id;
}

std::string DataBase::userNickbyId(std::string &id) {
  std::string nickname;
  {
    std::lock_guard<std::mutex> lock(dbMutex);
    database_names = nicknamesFile();
    for (User user : database_names) {
      if (user.id == id) {
        nickname = user.nickname;
        break;
      }
    }
  }
  return nickname;
}

void DataBase::channelMessage(Message &message, std::string &channel,
                              MySocket &client) {
  logMessage("CHANNEL FLAG.............", SERVER_LOG_FILE);
  channel = messageToString(message);
  {
    std::lock_guard<std::mutex> lock(dbMutex);
    database_channels_members = channelsMembersFile(channel);
  }
  if (!ChannelExists(channel)) {
    std::lock_guard<std::mutex> lock(dbMutex);
    addChannel(channel);
  }

  if (ChannelExists(channel)) {
    logMessage("Channel already exists: " + channel, SERVER_LOG_FILE);
  } else {
    logMessage("Channel created: " + channel, SERVER_LOG_FILE);
  }
}

bool DataBase::idMessage(bool &idReceived, Message &message, std::string &id) {
  idReceived = true;
  std::string receivedId = messageToString(message);
  if (receivedId != id) {
    std::cerr << "Wrong id from client" << std::endl;
    return false;
  }

  logMessage("Received id from client: " + id, SERVER_LOG_FILE);
  return true;
}

std::string DataBase::listOfChannelsOnServer() {
  std::string output;
  int countMembers = 0;
  std::lock_guard<std::mutex> lock(dbMutex);
  database_channels = channelsFile();
  if (database_channels.empty()) {
    output += "No existing channels.\n";
    return output;
  }
  for (auto channel : database_channels) {
    countMembers = channelsMembersCount(channel);
    output += "Name: " + channel +
              ", Number of active users: " + std::to_string(countMembers) +
              "\n";
  }
  return output;
}

bool DataBase::MemberInChannel(const std::string &id) {
  std::lock_guard<std::mutex> lock(dbMutex);
  return database_channels_members.find(id) != database_channels_members.end();
}