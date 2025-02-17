#include "../include/server.hpp"

Server *globalServer = nullptr;

std::atomic<uint32_t> audioMessageCounter(100);

uint32_t generateUniqueID() {
  // Атомарно увеличиваем и получаем следующий идентификатор
  return audioMessageCounter.fetch_add(1, std::memory_order_relaxed);
}

std::string Server::hashPassword(const std::string &password,
                                 const std::string &salt) {
  std::string saltedPassword = password + salt;
  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int hashLength = 0;

  EVP_MD_CTX *context = EVP_MD_CTX_new();
  if (context == nullptr) {
    throw std::runtime_error("Failed to create EVP_MD_CTX");
  }

  const EVP_MD *md = EVP_sha256();

  if (!EVP_DigestInit_ex(context, md, nullptr)) {
    EVP_MD_CTX_free(context);
    throw std::runtime_error("EVP_DigestInit_ex failed");
  }

  if (!EVP_DigestUpdate(context, saltedPassword.c_str(),
                        saltedPassword.size())) {
    EVP_MD_CTX_free(context);
    throw std::runtime_error("EVP_DigestUpdate failed");
  }

  if (!EVP_DigestFinal_ex(context, hash, &hashLength)) {
    EVP_MD_CTX_free(context);
    throw std::runtime_error("EVP_DigestFinal_ex failed");
  }

  EVP_MD_CTX_free(context);

  // Конвертация байтового массива хеша в строку hex
  std::stringstream ss;
  for (unsigned int i = 0; i < hashLength; ++i) {
    ss << std::hex << std::setw(2) << std::setfill('0')
       << static_cast<int>(hash[i]);
  }

  return ss.str();
}

bool Server::loginInSet(std::string login,
                        std::unordered_set<std::string> set) {
  return set.find(login) != set.end();
}

void Server::registrationOnServer(MySocket &client, std::string nickname,
                                  std::string login, std::string password) try {
  {
    std::lock_guard<std::mutex> lock(dbMutex);
    db.addUser(nickname, client.getSocket(), login, password);
    logMessage("Add user: " + nickname, SERVER_LOG_FILE);
  }

  logMessage("Name registered: " + nickname, SERVER_LOG_FILE);
} catch (const std::exception &e) {
  std::cerr << "Error during registration: " << e.what() << std::endl;
}

/**
 * Processes a command received from a client.
 *
 * @param client The socket object representing the client connection.
 * @param db The database object used to store and retrieve data.
 * @param id The ID of the client.
 * @param message The message received from the client.
 *
 * @throws None.
 */
void Server::commandProcessing(MySocket &client, User &user, Message &message) {
  std::string command = messageToString(message);
  // std::cout << "Received: " << command << std::endl;
  message = flagOff(message);
  std::string answer;
  std::istringstream iss(command);
  std::vector<std::string> words{std::istream_iterator<std::string>{iss},
                                 std::istream_iterator<std::string>{}};
  if (words[0] == "/read") {
    ReadCommand read;
    answer = read.handleCommand(words, db, user);
    message = stringToMessage(answer, message);
    client.sendMessage(message);
    logMessage("Read command: " + command + " from " + user.id,
               SERVER_LOG_FILE);
  } else if (words[0] == "/send") {
    SendCommand send;
    answer = send.handleCommand(words, db, user);
    message = stringToMessage(answer, message);
    client.sendMessage(message);
    logMessage("Send command: " + command + " from " + user.id,
               SERVER_LOG_FILE);
  } else if (words[0] == "/join") {
    JoinCommand join;
    answer = join.handleCommand(words, db, user);
    if (answer == "Channel does not exist") {
      message = flagOn(message, Flags::NO_CHANNEL);
    }
    message = stringToMessage(answer, message);
    client.sendMessage(message);
    logMessage("Join command: " + command + " from " + user.id,
               SERVER_LOG_FILE);
  } else if (words[0] == "/exit") {
    ExitCommand exit;
    answer = exit.handleCommand(words, db, user);
    message = stringToMessage(answer, message);
    client.sendMessage(message);
    logMessage("Exit command: " + command + " from " + user.id,
               SERVER_LOG_FILE);
  } else if (words[0] == "/nick") {
    NickCommand nick;
    answer = nick.handleCommand(words, db, user);
    message = flagOn(message, Flags::CHANGE_NICK);
    logMessage(answer, SERVER_LOG_FILE);
    message = stringToMessage(answer, message);

    client.sendMessage(message);
    logMessage("Change command: " + command + " from " + user.id,
               SERVER_LOG_FILE);
  } else if (words[0] == "/connect") {
    client.closeSocket();
    logMessage("Client disconnected: " + user.id, SERVER_LOG_FILE);
  } else if (words[0] == "/channels") {
    answer = db.listOfChannelsOnServer();
    message = stringToMessage(answer, message);
    client.sendMessage(message);
  } else if (words[0] == "/time_on") {
    user.timeFlag = true;
    answer = "Time on";
    message = flagOn(message, Flags::TIME_ON);
    message = stringToMessage(answer, message);
    client.sendMessage(message);
    logMessage("Time on command: " + command + " from " + user.id,
               SERVER_LOG_FILE);
  } else if (words[0] == "/time_off") {
    user.timeFlag = false;

    answer = "Time off";
    message = stringToMessage(answer, message);
    client.sendMessage(message);
    logMessage("Time off command: " + command + " from " + user.id,
               SERVER_LOG_FILE);
  } else if (words[0] == "/voicemail_on") {
    sendAudiofiletoClient(words[1], client);
  } else {
    message = stringToMessage("Wrong command", message);
    client.sendMessage(message);
    logMessage("Wrong command: " + command + " from " + user.id,
               SERVER_LOG_FILE);
  }
}

bool Server::checkLogin(const std::string &login, User &user, int regOrLog) {
  logMessage("Check login: " + login, SERVER_LOG_FILE);
  std::unordered_set<std::string> loginSet;
  {
    std::lock_guard<std::mutex> lock(dbMutex);
    db.database_names = db.nicknamesFile();
    for (User &user : db.database_names) {
      loginSet.insert(user.login);
    }
  }

  if (loginInSet(login, loginSet)) {
    if (regOrLog == 0) {
      return false;
    }
  } else if (regOrLog == 1) {
    return false;
  }

  user.login = login;
  return true;
}

bool Server::checkPasswordServer(const std::string &password, User &user) {
  logMessage("Check password: " + password, SERVER_LOG_FILE);
  std::string salt = "fixed_salt_value";
  try {
    std::string hashedPassword = hashPassword(password, salt);

    // std::cout << "Password: " << password << std::endl;
    // std::cout << "Salt: " << salt << std::endl;
    // std::cout << "Hashed Password: " << hashedPassword << std::endl;
    user.password = hashedPassword;
  } catch (const std::exception &ex) {
    std::cerr << "Error: " << ex.what() << std::endl;
    return false;
  }
  return true;
}

bool Server::checkPasswordAthorization(const std::string &login,
                                       const std::string &password) {
  logMessage("Проверка авторизации для логина: " + login, SERVER_LOG_FILE);
  std::string salt = "fixed_salt_value";
  std::string hashedPassword = hashPassword(password, salt);

  {
    std::lock_guard<std::mutex> lock(dbMutex);
    db.database_names = db.nicknamesFile();  // Загружаем пользователей из файла
    for (User &user : db.database_names) {
      if (user.login == login) {
        logMessage("Найден пользователь с логином: " + user.login,
                   SERVER_LOG_FILE);
        logMessage(
            "Сравнение паролей: " + user.password + " и " + hashedPassword,
            SERVER_LOG_FILE);
        if (user.password == hashedPassword) {
          // Логин и пароль совпадают
          user.password = hashedPassword;
          // user.id = user.id;
          // userInfo.nickname = user.nickname;
          return true;
        } else {
          // Логин найден, но пароль неверный
          logMessage("Пароль не совпадает для пользователя: " + user.login,
                     SERVER_LOG_FILE);
          return false;
        }
      }
    }
  }
  // Пользователь с таким логином не найден
  logMessage("Пользователь с логином " + login + " не найден", SERVER_LOG_FILE);
  return false;
}

bool Server::checkNickname(const std::string &nickname, User &user) {
  logMessage("Check nickname: " + nickname, SERVER_LOG_FILE);
  std::unordered_set<std::string> nicknameSet;
  {
    std::lock_guard<std::mutex> lock(dbMutex);
    db.database_names = db.nicknamesFile();
    for (User &user : db.database_names) {
      nicknameSet.insert(user.nickname);
    }
  }

  if (db.nickameInSet(nickname, nicknameSet)) {
    logMessage("Nickname already exists: " + nickname, SERVER_LOG_FILE);
    return false;
  }

  user.nickname = nickname;
  return true;
}

bool extractAudioMessageData(const Message &message, std::string &fileName,
                             std::vector<uint8_t> &audioData) {
  const uint8_t *dataPtr = message.body.data();
  size_t dataSize = message.body.size();

  // Извлекаем длину имени файла
  if (dataSize < sizeof(uint32_t)) {
    logMessage("Invalid AUDIO message: insufficient data for filename length",
               SERVER_LOG_FILE);
    return false;
  }
  uint32_t netFileNameLength;
  std::memcpy(&netFileNameLength, dataPtr, sizeof(uint32_t));
  uint32_t fileNameLength = ntohl(netFileNameLength);
  dataPtr += sizeof(uint32_t);
  dataSize -= sizeof(uint32_t);

  // Проверяем корректность длины имени файла
  if (dataSize < fileNameLength) {
    logMessage("Invalid AUDIO message: filename length mismatch",
               SERVER_LOG_FILE);
    return false;
  }

  // Извлекаем имя файла
  fileName.assign(reinterpret_cast<const char *>(dataPtr), fileNameLength);
  dataPtr += fileNameLength;
  dataSize -= fileNameLength;

  // Извлекаем аудиоданные
  audioData.assign(dataPtr, dataPtr + dataSize);

  return true;
}

bool appendToChannelLog(const std::string &channel, std::string &id_user,
                        const std::string &fileName) {
  // Получаем текущее время
  std::time_t currentTime = std::time(nullptr);
  std::tm *localTime = std::localtime(&currentTime);
  char timeBuffer[20];
  std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", localTime);

  // Формируем строку для записи в лог-файл канала
  std::string channelLogEntry =
      std::string(timeBuffer) + ":" + id_user + ":" + fileName + "\n";

  // Путь к файлу лога канала
  std::string channelLogFile = "channels/" + channel + "_audio.txt";

  // Добавляем запись в файл лога канала
  std::ofstream channelLog(channelLogFile, std::ios::app);
  if (!channelLog.is_open()) {
    logMessage("Failed to open channel log file: " + channelLogFile,
               SERVER_LOG_FILE);
    return false;
  }
  channelLog << channelLogEntry;
  channelLog.close();

  logMessage("Audio log entry added to " + channelLogFile, SERVER_LOG_FILE);
  return true;
}

void Server::proccessFileMessage(const Message &message,
                                 const std::string &senderNickname) {
  const uint8_t *dataPtr = message.body.data();
  size_t dataSize = message.body.size();

  // Извлекаем длину id
  if (dataSize < sizeof(uint32_t)) {
    logMessage("Invalid AUDIO message: insufficient data for id length",
               SERVER_LOG_FILE);
    return;
  }
  uint32_t netIdLength;
  std::memcpy(&netIdLength, dataPtr, sizeof(uint32_t));
  uint32_t idLength = ntohl(netIdLength);
  dataPtr += sizeof(uint32_t);
  dataSize -= sizeof(uint32_t);

  // Проверяем корректность длины id
  if (dataSize < idLength) {
    logMessage("Invalid AUDIO message: id length mismatch", SERVER_LOG_FILE);
    return;
  }

  // Извлекаем id
  std::string id(reinterpret_cast<const char *>(dataPtr), idLength);
  dataPtr += idLength;
  dataSize -= idLength;

  // Извлекаем длину имени канала
  if (dataSize < sizeof(uint32_t)) {
    logMessage("Invalid AUDIO message: insufficient data for channel length",
               SERVER_LOG_FILE);
    return;
  }
  uint32_t netChannelLength;
  std::memcpy(&netChannelLength, dataPtr, sizeof(uint32_t));
  uint32_t channelLength = ntohl(netChannelLength);
  dataPtr += sizeof(uint32_t);
  dataSize -= sizeof(uint32_t);

  // Проверяем корректность длины имени канала
  if (dataSize < channelLength) {
    logMessage("Invalid AUDIO message: channel length mismatch",
               SERVER_LOG_FILE);
    return;
  }

  // Извлекаем имя канала
  std::string channel(reinterpret_cast<const char *>(dataPtr), channelLength);
  dataPtr += channelLength;
  dataSize -= channelLength;

  // Извлекаем длину имени файла
  if (dataSize < sizeof(uint32_t)) {
    logMessage("Invalid AUDIO message: insufficient data for filename length",
               SERVER_LOG_FILE);
    return;
  }
  uint32_t netFileNameLength;
  std::memcpy(&netFileNameLength, dataPtr, sizeof(uint32_t));
  uint32_t fileNameLength = ntohl(netFileNameLength);
  dataPtr += sizeof(uint32_t);
  dataSize -= sizeof(uint32_t);

  // Проверяем корректность длины имени файла
  if (dataSize < fileNameLength) {
    logMessage("Invalid AUDIO message: filename length mismatch",
               SERVER_LOG_FILE);
    return;
  }

  // Извлекаем имя файла
  std::string fileName(reinterpret_cast<const char *>(dataPtr), fileNameLength);
  dataPtr += fileNameLength;
  dataSize -= fileNameLength;
  std::vector<uint8_t> fileData(dataPtr, dataPtr + dataSize);

  // Извлекаем размер файла
  if (dataSize < sizeof(uint32_t)) {
    logMessage("Invalid AUDIO message: insufficient data for file size",
               SERVER_LOG_FILE);
    return;
  }
  uint32_t netFileSize;
  std::memcpy(&netFileSize, dataPtr, sizeof(uint32_t));
  uint32_t fileSize = ntohl(netFileSize);
  dataPtr += sizeof(uint32_t);
  dataSize -= sizeof(uint32_t);

  uint32_t fileMessageID = generateUniqueID();

  // Преобразуем идентификатор в строку
  std::string fileMessageIDStr = std::to_string(fileMessageID);
  std::filesystem::path pathObj(fileName);
  // Сохраняем аудиофайл с новым именем (например, используем audioMessageID)
  std::string fileDirectory = "channels/files";
  std::string fileFilePath =
      fileDirectory + "/" + fileMessageIDStr + pathObj.extension().string();
  ;
  if (!saveFile(fileDirectory, fileMessageIDStr + pathObj.extension().string(),
                fileData)) {
    return;
  }

  // Получаем текущее время в формате YYYY-MM-DD HH:MM:SS
  std::time_t currentTime = std::time(nullptr);
  char timeBuffer[20];
  std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S",
                std::localtime(&currentTime));

  FileMessage fileMessage;
  fileMessage.messageID = fileMessageID;
  fileMessage.senderNickname = senderNickname;
  fileMessage.timestamp = timeBuffer;
  fileMessage.filename = fileName;
  fileMessage.fileSize = fileSize;

  {
    std::lock_guard<std::mutex> lock(dbMutex);
    db.database_channels_history = db.channelsHistoryFile(channel);
    db.addFileMessageToChannelHistory(senderNickname, channel, fileMessageIDStr,
                                      fileName, fileSize);
  }
  logMessage("File message saved for channel " + channel, SERVER_LOG_FILE);
}

void Server::processAudioMessage(const Message &message,
                                 const std::string &senderIP,
                                 const std::string &senderNickname) {
  const uint8_t *dataPtr = message.body.data();
  size_t dataSize = message.body.size();

  // Извлекаем длину id
  if (dataSize < sizeof(uint32_t)) {
    logMessage("Invalid AUDIO message: insufficient data for id length",
               SERVER_LOG_FILE);
    return;
  }
  uint32_t netIdLength;
  std::memcpy(&netIdLength, dataPtr, sizeof(uint32_t));
  uint32_t idLength = ntohl(netIdLength);
  dataPtr += sizeof(uint32_t);
  dataSize -= sizeof(uint32_t);

  // Проверяем корректность длины id
  if (dataSize < idLength) {
    logMessage("Invalid AUDIO message: id length mismatch", SERVER_LOG_FILE);
    return;
  }

  // Извлекаем id
  std::string id(reinterpret_cast<const char *>(dataPtr), idLength);
  dataPtr += idLength;
  dataSize -= idLength;

  // Извлекаем длину имени канала
  if (dataSize < sizeof(uint32_t)) {
    logMessage("Invalid AUDIO message: insufficient data for channel length",
               SERVER_LOG_FILE);
    return;
  }
  uint32_t netChannelLength;
  std::memcpy(&netChannelLength, dataPtr, sizeof(uint32_t));
  uint32_t channelLength = ntohl(netChannelLength);
  dataPtr += sizeof(uint32_t);
  dataSize -= sizeof(uint32_t);

  // Проверяем корректность длины имени канала
  if (dataSize < channelLength) {
    logMessage("Invalid AUDIO message: channel length mismatch",
               SERVER_LOG_FILE);
    return;
  }

  // Извлекаем имя канала
  std::string channel(reinterpret_cast<const char *>(dataPtr), channelLength);
  dataPtr += channelLength;
  dataSize -= channelLength;

  // Извлекаем длину имени файла
  if (dataSize < sizeof(uint32_t)) {
    logMessage("Invalid AUDIO message: insufficient data for filename length",
               SERVER_LOG_FILE);
    return;
  }
  uint32_t netFileNameLength;
  std::memcpy(&netFileNameLength, dataPtr, sizeof(uint32_t));
  uint32_t fileNameLength = ntohl(netFileNameLength);
  dataPtr += sizeof(uint32_t);
  dataSize -= sizeof(uint32_t);

  // Проверяем корректность длины имени файла
  if (dataSize < fileNameLength) {
    logMessage("Invalid AUDIO message: filename length mismatch",
               SERVER_LOG_FILE);
    return;
  }

  // Извлекаем имя файла
  std::string fileName(reinterpret_cast<const char *>(dataPtr), fileNameLength);
  dataPtr += fileNameLength;
  dataSize -= fileNameLength;

  // Оставшиеся данные — это аудиоданные
  std::vector<uint8_t> audioData(dataPtr, dataPtr + dataSize);

  // Генерируем уникальный идентификатор аудиосообщения (например, UUID или на
  // основе времени)

  uint32_t audioMessageID = generateUniqueID();

  // Преобразуем идентификатор в строку
  std::string audioMessageIDStr = std::to_string(audioMessageID);

  // Сохраняем аудиофайл с новым именем (например, используем audioMessageID)
  std::string audioDirectory = "channels/audio";
  std::string audioFilePath = audioDirectory + "/" + audioMessageIDStr + ".wav";
  if (!saveFile(audioDirectory, audioMessageIDStr + ".wav", audioData)) {
    return;
  }

  // Получаем текущее время в формате YYYY-MM-DD HH:MM:SS
  std::time_t currentTime = std::time(nullptr);
  char timeBuffer[20];
  std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S",
                std::localtime(&currentTime));

  // Вычисляем продолжительность аудиосообщения
  double duration = getAudioDuration(audioFilePath);

  // Создаем объект AudioMessage
  AudioMessage audioMessage;
  audioMessage.timestamp = timeBuffer;
  audioMessage.senderNickname = senderNickname;
  audioMessage.senderIP = senderIP;
  audioMessage.messageID = audioMessageIDStr;
  audioMessage.duration = duration;
  audioMessage.filePath = audioFilePath;

  // Добавляем аудиосообщение в структуру данных канала
  {
    std::lock_guard<std::mutex> lock(channelDataMutex);
    channelAudioMessages[channel].push_back(audioMessage);
  }
  {
    std::lock_guard<std::mutex> lock(dbMutex);
    db.database_channels_history = db.channelsHistoryFile(channel);
    db.addAudioMessageToChannelHistory(senderNickname, channel,
                                       audioMessageIDStr, duration);
  }
  logMessage("Audio message saved for channel " + channel, SERVER_LOG_FILE);
}

void Server::sendAudiofiletoClient(const std::string &audioId,
                                   MySocket &client) {
  Message msg;
  logMessage("Received audio request for ID: " + audioId, SERVER_LOG_FILE);

  // Построение пути к аудиофайлу
  std::string audioDirectory = "channels/audio/";
  std::string audioFilePath = audioDirectory + audioId + ".wav";

  // Проверка существования файла
  if (!std::filesystem::exists(audioFilePath)) {
    logMessage("Audio file not found: " + audioFilePath, SERVER_LOG_FILE);

    // Отправляем сообщение об ошибке клиенту
    msg = flagOn(msg, Flags::AUDIOFILE_ERROR);
    std::string errorText =
        "Error: Audio message with ID " + audioId + " not found.";
    client.sendMessage(msg);
    return;
  }
  msg.setAudioMessage(audioFilePath);
  if (client.sendMessage(msg)) {
    logMessage("Sent audio message ID " + audioId + " to client.",
               SERVER_LOG_FILE);
  } else {
    logMessage("Failed to send audio message ID " + audioId + " to client.",
               SERVER_LOG_FILE);
  }
}

// Функция для создания map из user_id в socket
std::unordered_map<std::string, int> createUserSocketMap(
    const std::vector<User> &users) {
  std::unordered_map<std::string, int> user_socket_map;
  user_socket_map.reserve(users.size());

  for (const auto &user : users) {
    try {
      int socket = std::stoi(user.socketNumber);
      user_socket_map.emplace(user.id, socket);
    } catch (const std::invalid_argument &e) {
      std::cerr << "Неверный формат socket для user_id " << user.id << ": "
                << user.socketNumber << std::endl;
    } catch (const std::out_of_range &e) {
      std::cerr << "Socket вне диапазона для user_id " << user.id << ": "
                << user.socketNumber << std::endl;
    }
  }

  return user_socket_map;
}

// Функция для получения списка сокетов подключенных клиентов с использованием
// map
std::vector<int> Server::getClientSocketsOptimized(
    const std::unordered_set<std::string> &channel_members,
    const std::unordered_map<std::string, int> &user_socket_map) {
  client_sockets.clear();
  client_sockets.reserve(channel_members.size());

  for (const auto &user_id : channel_members) {
    auto it = user_socket_map.find(user_id);
    if (it != user_socket_map.end()) {
      client_sockets.push_back(it->second);
    } else {
      std::cerr << "Пользователь с user_id " << user_id << " не найден в map."
                << std::endl;
    }
  }

  return client_sockets;
}

void Server::broadcast_audio(const std::vector<SAMPLE_TYPE> &audioData,
                             int sender_socket, std::string &channel) {
  logMessage("broadcast_audio", SERVER_LOG_FILE);
  std::lock_guard<std::mutex> lock(clients_mutex);

  MySocket client;
  for (int client_socket : client_sockets) {
    if (client_socket != sender_socket) {
      logMessage(
          "Sending audio data to client " + std::to_string(client_socket),
          SERVER_LOG_FILE);
      // std::cout << "Sending audio data to client "
      //           << std::to_string(client_socket) << "..." << std::endl;
      Message voiceMessage;
      voiceMessage.setVoiceMessage(audioData, channel);
      client.setSocket(client_socket);
      client.sendMessage(voiceMessage);
      // send(client_socket, audioData.data(),
      //      audioData.size() * sizeof(SAMPLE_TYPE), 0);
    }
  }
}

void mix_audio_buffers(std::vector<int16_t *> &buffers, int16_t *output,
                       int frame_count) {
  memset(output, 0, frame_count * NUM_CHANNELS * sizeof(int16_t));

  for (int i = 0; i < frame_count * NUM_CHANNELS; ++i) {
    int32_t mixed_sample = 0;
    for (auto &buffer : buffers) {
      mixed_sample += buffer[i];
    }

    // Ограничение значений, чтобы избежать переполнения
    if (mixed_sample > INT16_MAX) mixed_sample = INT16_MAX;
    if (mixed_sample < INT16_MIN) mixed_sample = INT16_MIN;

    output[i] = static_cast<int16_t>(mixed_sample);
  }
}
void Server::messageProcessing(MySocket &client, User &user,
                               std::string &channel) {
  Message message;
  bool idReceived = false;

  {
    std::lock_guard<std::mutex> lock(dbMutex);
    db.database_names = db.nicknamesFile();
    // db.database_channels = db.channelsFile();
  }

  while (serverRunning) {
    message.clearMessage(message);
    message = flagOff(message);

    if (!client.receiveMessage(message)) {
      std::cerr << "Client disconnect" << std::endl;
      logMessage("Client disconnect", SERVER_LOG_FILE);
      client.closeSocket();
      break;
    }

    // Логирование полученного сообщения
    std::string messageContent = (message.header.type == DataType::AUDIO)
                                     ? "AUDIO"
                                     : messageToString(message);
    // logMessage("Message received: " + messageContent + " hgwith flag " +
    //                std::to_string(message.header.flag),
    //            SERVER_LOG_FILE);

    // Проверяем тип сообщения
    if (message.header.type == DataType::AUDIO) {
      logMessage("Received AUDIO message", SERVER_LOG_FILE);
      processAudioMessage(message, client.getIP(), user.id);

    } else if (message.header.type == DataType::VOICE) {
      logMessage("Received VOICE message", SERVER_LOG_FILE);
      AudioPacket packet = message.audioPacket;
      const uint8_t *dataPtr = message.body.data();
      size_t dataSize = message.body.size();

      // Извлекаем длину имени канала (4 байта)
      if (dataSize < sizeof(uint32_t)) {
        logMessage(
            "Invalid VOICE message: insufficient data for channel length",
            SERVER_LOG_FILE);
        return;
      }
      uint32_t netChannelLength;
      std::memcpy(&netChannelLength, dataPtr, sizeof(uint32_t));
      uint32_t channelLength = ntohl(netChannelLength);
      dataPtr += sizeof(uint32_t);
      dataSize -= sizeof(uint32_t);

      // Проверяем корректность длины имени канала
      if (dataSize < channelLength) {
        logMessage("Invalid VOICE message: channel length mismatch",
                   SERVER_LOG_FILE);
        return;
      }

      // Извлекаем имя канала
      std::string channel(reinterpret_cast<const char *>(dataPtr),
                          channelLength);
      dataPtr += channelLength;
      dataSize -= channelLength;

      // Извлекаем длину Opus-пакета (4 байта)
      if (dataSize < sizeof(uint32_t)) {
        logMessage("Invalid VOICE message: insufficient data for Opus length",
                   SERVER_LOG_FILE);
        return;
      }
      uint32_t netOpusLength;
      std::memcpy(&netOpusLength, dataPtr, sizeof(uint32_t));
      uint32_t opusLength = ntohl(netOpusLength);
      dataPtr += sizeof(uint32_t);
      dataSize -= sizeof(uint32_t);

      // Проверяем корректность длины Opus-пакета
      if (dataSize < opusLength) {
        logMessage("Invalid VOICE message: Opus data length mismatch",
                   SERVER_LOG_FILE);
        return;
      }

      // Извлекаем Opus-данные
      std::vector<uint8_t> opusData(dataPtr, dataPtr + opusLength);
      dataPtr += opusLength;
      dataSize -= opusLength;

      // Обновляем AudioPacket
      packet.opus_length = opusLength;
      memcpy(packet.opus_data, opusData.data(), opusLength);

      // Добавляем в буфер клиента
      {
        std::lock_guard<std::mutex> lock(client_buffers_mutex);
        client_buffers[client.getSocket()].push(packet);
      }

      // Декодирование и микширование аудиоданных
      std::vector<int16_t *> decoded_buffers;
      uint64_t min_timestamp = packet.timestamp;

      {
        std::lock_guard<std::mutex> lock(client_buffers_mutex);
        for (auto &[sock, buffer] : client_buffers) {
          if (!buffer.empty() && buffer.front().timestamp < min_timestamp) {
            min_timestamp = buffer.front().timestamp;
          }
        }

        int error;
        OpusDecoder *decoder =
            opus_decoder_create(SAMPLE_RATE, NUM_CHANNELS, &error);
        if (error != OPUS_OK) {
          std::cerr << "Failed to create Opus decoder: " << opus_strerror(error)
                    << std::endl;
          return;
        }

        for (auto &[sock, buffer] : client_buffers) {
          if (!buffer.empty() && buffer.front().timestamp <= min_timestamp) {
            AudioPacket &pkt = buffer.front();
            SAMPLE_TYPE decoded_data[FRAMES_PER_BUFFER * NUM_CHANNELS];
            int frame_count =
                opus_decode(decoder, pkt.opus_data, pkt.opus_length,
                            decoded_data, FRAMES_PER_BUFFER, 0);
            if (frame_count > 0) {
              decoded_buffers.push_back(decoded_data);
            }
            buffer.pop();
          }
        }

        opus_decoder_destroy(decoder);
      }

      // Обновление базы данных
      {
        std::lock_guard<std::mutex> lock(dbMutex);
        db.database_names = db.nicknamesFile();
        db.database_channels_members = db.channelsMembersFile(channel);
      }

      // Создание карты пользователей и получение списка сокетов клиентов
      std::unordered_map<std::string, int> user_socket_map;
      {
        std::lock_guard<std::mutex> lock(dbMutex);
        user_socket_map = createUserSocketMap(db.database_names);
      }

      std::vector<int> client_sockets;
      {
        std::lock_guard<std::mutex> lock(clients_mutex);
        client_sockets = getClientSocketsOptimized(db.database_channels_members,
                                                   user_socket_map);
      }

      // Удаляем отправителя из списка получателей
      {
        std::lock_guard<std::mutex> lock(clients_mutex);
        client_sockets.erase(
            std::remove(client_sockets.begin(), client_sockets.end(),
                        client.getSocket()),
            client_sockets.end());
      }

      if (!decoded_buffers.empty()) {
        std::vector<SAMPLE_TYPE> mixed_audio(FRAMES_PER_BUFFER * NUM_CHANNELS);
        mix_audio_buffers(decoded_buffers, mixed_audio.data(),
                          FRAMES_PER_BUFFER);
        // вывод client_sockets
        for (int sock : client_sockets) {
          std::cout << "Client socket: " << sock;
        }
        std::cout << std::endl;
        broadcast_audio(mixed_audio, client.getSocket(), channel);
      }

      // // Отправляем аудиосообщение другим клиентам
      // for (auto &sock : client_sockets) {
      //   MySocket recipient(sock);
      //   if (!recipient.sendMessage(audioMessage)) {
      //     std::cerr << "Failed to send audio message to socket: " << sock
      //               << std::endl;
      //     // Опционально: можно удалить клиента из списка при ошибке отправки
      //     // removeClient(sock);
      //   }
      // }
    } else if (message.header.type == DataType::FILE_TYPE) {
      logMessage("Received FILE_TYPE message", SERVER_LOG_FILE);
      proccessFileMessage(message, user.id);
    } else {
      // Обработка текстовых сообщений по флагам
      switch (message.header.flag) {
        case Flags::LOGIN_SIGN_UP:
          logMessage("LOGIN_SIGN_UP with login: " + messageToString(message),
                     SERVER_LOG_FILE);
          if (checkLogin(messageToString(message), user, 0)) {
            message = flagOn(message, Flags::CHECK_LOGIN);
            message = stringToMessage("Login correct", message);
            client.sendMessage(message);
            logMessage("Sending login correct", SERVER_LOG_FILE);
          } else {
            logMessage("Login incorrect", SERVER_LOG_FILE);
          }
          break;
        case Flags::LOGIN_LOG_IN:
          logMessage("LOGIN_LOG_IN with login: " + messageToString(message),
                     SERVER_LOG_FILE);
          if (checkLogin(messageToString(message), user, 1)) {
            message = flagOn(message, Flags::CHECK_LOGIN);
            message = stringToMessage("Login correct", message);
            client.sendMessage(message);
            logMessage("Sending login correct", SERVER_LOG_FILE);
          }
          break;

        case Flags::PASSWORD_SIGN_UP:
          logMessage(
              "PASSWORD_SIGN_UP with password: " + messageToString(message),
              SERVER_LOG_FILE);
          if (checkPasswordServer(messageToString(message), user)) {
            message = flagOn(message, Flags::CHECK_PASSWORD);
            message = stringToMessage("Password correct", message);
            client.sendMessage(message);
            logMessage("Sending password correct", SERVER_LOG_FILE);
          }
          break;

        case Flags::PASSWORD_LOG_IN:
          logMessage(
              "PASSWORD_LOG_IN with password: " + messageToString(message),
              SERVER_LOG_FILE);
          if (checkPasswordAthorization(user.login, messageToString(message))) {
            logMessage(
                "Authorization is successful, nickname: " + user.nickname,
                SERVER_LOG_FILE);
            message = flagOn(message, Flags::AUTHORIZED);
            message = stringToMessage(user.nickname, message);
            client.sendMessage(message);
            logMessage("Authorization is successful", SERVER_LOG_FILE);
          }
          break;

        case Flags::CHANNEL:
          db.channelMessage(message, channel, client);
          break;

        case Flags::NICK:
          logMessage("NICK flag", SERVER_LOG_FILE);
          if (checkNickname(messageToString(message), user)) {
            logMessage("Nickname correct", SERVER_LOG_FILE);
            message = flagOn(message, Flags::CHECK_NICKNAME);
            message = stringToMessage("Nickname correct", message);
            if (!user.nickname.empty() && !user.login.empty() &&
                !user.password.empty()) {
              logMessage("Registering user: " + user.nickname + " login: " +
                             user.login + " password: " + user.password,
                         SERVER_LOG_FILE);
              registrationOnServer(client, user.nickname, user.login,
                                   user.password);
              logMessage("Registered", SERVER_LOG_FILE);

              if (!channel.empty()) {
                std::lock_guard<std::mutex> lock(dbMutex);
                db.addChannelMember(user.id, channel);
              }
            }
            client.sendMessage(message);
            logMessage("Sending nickname correct", SERVER_LOG_FILE);

            message = flagOn(message, Flags::REGISTERED);
            client.sendMessage(
                stringToMessage("Registered successfully", message));
          } else {
            logMessage("Nickname incorrect", SERVER_LOG_FILE);
          }
          break;

        case Flags::CHECK_ID:
          logMessage("Checking id", SERVER_LOG_FILE);
          user.id = db.userId(user.login);
          logMessage("id: " + user.id, SERVER_LOG_FILE);
          message.clearMessage(message);

          message = stringToMessage(user.id, message);
          message = flagOn(message, Flags::ID);
          if (!client.sendMessage(message)) {
            std::cerr << "Failed to send id." << std::endl;
          }
          break;

        case Flags::ID:
          logMessage("ID flag", SERVER_LOG_FILE);
          if (db.idMessage(idReceived, message, user.id)) {
            message = flagOn(message, Flags::ID_CORRECT);
            user.nickname = db.userNickbyId(user.id);
            message = stringToMessage(user.nickname, message);
            if (!client.sendMessage(message)) {
              std::cerr << "Failed to send id." << std::endl;
            }
          }
          // userInfo = {"", "", "", ""};
          break;

        default:
          if (idReceived) {
            logMessage("START COMMAND PROCESSING", SERVER_LOG_FILE);
            commandProcessing(client, user, message);
          } else {
            logMessage(
                "Unknown message flag: " + std::to_string(message.header.flag),
                SERVER_LOG_FILE);
          }
          break;
      }
    }
  }
}

bool writeAudioFile(const std::string &filename,
                    const std::vector<uint8_t> &data) {
  std::ofstream file(filename, std::ios::binary);
  if (!file) {
    std::cerr << "Не удалось открыть файл для записи аудиоданных: " << filename
              << std::endl;
    return false;
  }

  file.write(reinterpret_cast<const char *>(data.data()), data.size());
  if (!file) {
    std::cerr << "Не удалось записать аудиоданные в файл: " << filename
              << std::endl;
    return false;
  }

  return true;
}

/**
 * Handles a client connection by processing incoming messages and closing the
 * socket.
 *
 * @param clientSocket The socket file descriptor of the client connection.
 * @param db The reference to the database object.
 *
 * @throws None.
 */
void handleClient(int clientSocket, Server &server) {
  MySocket client;
  Message message;
  User user;
  std::string channel = "";
  client.setSocket(clientSocket);

  server.messageProcessing(client, user, channel);
  // server.messageProcessing(client, db, user.id, channel, user.nickname);

  client.closeSocket();
}

/**
 * Handles the signal Ctrl+C and shuts down the server.
 *
 * @param signal the signal received
 *
 * @throws None
 */
void signalHandlerServer(int signal) {
  if (signal == SIGINT) {
    logMessage("Interrupt signal (" + std::to_string(signal) +
                   ") received. Shutting down server...",
               SERVER_LOG_FILE);
    std::cout << "Shutting down server..." << std::endl;
    if (globalServer) {
      globalServer->serverRunning = false;
      globalServer->serverSocket.closeSocket();
    }
  }
}

void Server::helpToUse(const char *programName) {
  std::cout << "Usage: " << programName << " <port>" << std::endl;
  std::cout << "Options:\n"
            << "  -h, --help  Show this help message\n";
}

bool Server::addChannelOnServer(std::string &channel) {
  {
    std::lock_guard<std::mutex> lock(dbMutex);
    db.database_channels = db.channelsFile();
  }
  if (db.ChannelExists(channel)) {
    std::cout << "Channel already exists." << std::endl;
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(dbMutex);
    db.addChannel(channel);
  }
  std::cout << "A new channel has been created: " << channel << std::endl;
  return true;
}

bool Server::removeMembersFromDeleteChannel(std::string &channel) {
  Message message;
  message = flagOn(message, Flags::DEL_CHANNEL);
  {
    std::lock_guard<std::mutex> lock(dbMutex);
    db.database_channels_members = db.channelsMembersFile(channel);
    db.database_names = db.nicknamesFile();
  }
  std::string pathMembers = "channels/members/" + channel + "_members.txt";
  std::string pathHistory = "channels/history/" + channel + "_history.txt";
  if (db.database_channels_members.empty()) {
    std::cout << "No members in this channel. Removing channel..." << std::endl;
    if (db.removeChannelFiles(pathMembers, pathHistory)) {
      std::cout << "Channel removed." << std::endl;
      return true;
    }
  }
  std::string notification =
      "Channel " + channel + " removed on server. You exit from channel.";

  for (std::string id : db.database_channels_members) {
    for (User &user : db.database_names) {
      if (id == user.id) {
        message = flagOn(message, Flags::DEL_CHANNEL);
        serverSocket.sendMessage(stringToMessage(notification, message),
                                 std::stoi(user.socketNumber));
      }
    }
  }
  if (db.removeChannelFiles(pathMembers, pathHistory)) {
    std::cout << "Notifications have been sent to users. Channel removed."
              << std::endl;
    return true;
  }

  //  оповещение пользователей об удалении канала
  return false;
}

void serverCommand(int port, Server &server) {
  std::cout << "Server listening on port " << port << "." << std::endl;
  logMessage("Server listening on port " + std::to_string(port) + ".",
             SERVER_LOG_FILE);

  std::string command;
  while (server.serverRunning) {
    std::getline(std::cin, command);
    std::istringstream iss(command);
    std::vector<std::string> words{std::istream_iterator<std::string>{iss},
                                   std::istream_iterator<std::string>{}};
    if (words[0] == "/channels") {
      if (words.size() != 1) {
        std::cout << "Wrong command, use /help" << std::endl;
        continue;
      }
      logMessage("Server command: /channels", SERVER_LOG_FILE);
      std::cout << server.db.listOfChannelsOnServer() << std::endl;
    } else if (words[0] == "/add_channel") {
      logMessage("Server command: /add_channel", SERVER_LOG_FILE);
      if (words.size() != 2) {
        std::cout << "Wrong command, use /help" << std::endl;
        continue;
      }
      if (!server.addChannelOnServer(words[1])) {
        continue;
      }
    } else if (words[0] == "/del_channel") {
      if (words.size() != 2) {
        std::cout << "Wrong command, use /help" << std::endl;
        continue;
      }
      logMessage("Server command: /del_channel", SERVER_LOG_FILE);
      server.db.deleteChannel(words[1]);
      server.removeMembersFromDeleteChannel(words[1]);
    } else {
      std::cout << "Wrong command, use /help" << std::endl;
    }
  }
}

int main(int argc, char *argv[]) {
  Server server;
  globalServer = &server;
  if (argc == 2 &&
      (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
    server.helpToUse(argv[0]);
    return 0;
  }

  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
    exit(1);
  }

  int port = std::atoi(argv[1]);

  signal(SIGINT, signalHandlerServer);

  if (!server.serverSocket.createSocket()) {
    std::cerr << "socket failed" << std::endl;
    logMessage("Socket creation failed", SERVER_LOG_FILE);
    exit(EXIT_FAILURE);
  }

  if (!server.serverSocket.bindSocket(port)) {
    std::cerr << "bind failed" << std::endl;
    logMessage("Bind failed", SERVER_LOG_FILE);
    server.serverSocket.closeSocket();
    exit(EXIT_FAILURE);
  }

  if (!server.serverSocket.listenSocket()) {
    std::cerr << "listen error" << std::endl;
    logMessage("Listen failed", SERVER_LOG_FILE);
    server.serverSocket.closeSocket();
    exit(EXIT_FAILURE);
  }

  std::thread serverThread(serverCommand, port, std::ref(server));
  serverThread.detach();

  std::vector<std::thread> clientThreads;

  while (server.serverRunning) {
    int clientSocket = server.serverSocket.acceptSocket();

    if (!server.serverRunning) {
      close(clientSocket);
      break;
    }

    if (clientSocket < 0) {
      std::cerr << "accept error" << std::endl;
      logMessage("Accept failed", SERVER_LOG_FILE);
      if (server.serverRunning) {
        server.serverSocket.closeSocket();
        exit(EXIT_FAILURE);
      }
    } else {
      logMessage("Connection established", SERVER_LOG_FILE);
      clientThreads.emplace_back(handleClient, clientSocket, std::ref(server));
    }
  }

  for (auto &thread : clientThreads) {
    if (thread.joinable()) {
      thread.join();
    }
  }

  server.serverSocket.closeSocket();
  std::cout << "Server shut down successfully." << std::endl;
  return 0;
}
