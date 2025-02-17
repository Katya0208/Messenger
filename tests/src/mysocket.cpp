#include "../include/mysocket.hpp"

bool MySocket::createSocket() {
  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    std::cerr << "Socket creation error" << std::endl;
    return false;
  }
  return true;
}

bool MySocket::connectSocket(const std::string& ip, int port) {
  address.sin_family = AF_INET;
  address.sin_port = htons(port);

  if (inet_pton(AF_INET, ip.c_str(), &address.sin_addr) <= 0) {
    std::cerr << "Invalid address / Address not supported" << std::endl;
    return false;
  }

  if (connect(sock, (struct sockaddr*)&address, sizeof(address)) < 0) {
    std::cerr << "Connection failed" << std::endl;
    return false;
  }

  return true;
}

bool MySocket::bindSocket(int port) {
  int opt = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                 sizeof(opt))) {
    std::cerr << "setsockopt" << std::endl;
    return false;
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  if (bind(sock, (struct sockaddr*)&address, sizeof(address)) < 0) {
    std::cerr << "bind failed" << std::endl;
    return false;
  }

  return true;
}

bool MySocket::listenSocket(int backlog) {
  if (listen(sock, backlog) < 0) {
    // std::cerr << "listen" << std::endl;
    return false;
  }
  return true;
}

int MySocket::acceptSocket() {
  int addrlen = sizeof(address);
  int new_socket =
      accept(sock, (struct sockaddr*)&address, (socklen_t*)&addrlen);
  if (new_socket < 0) {
    // std::cerr << "accept" << std::endl;
    return -1;
  }
  return new_socket;
}

void Message::serialize(std::vector<uint8_t>& buffer) const {
  // Размер заголовка: type (1 байт) + size (4 байта) + flag (4 байта)
  size_t headerSize = sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint32_t);

  buffer.resize(headerSize + body.size());
  uint8_t* ptr = buffer.data();

  // Сериализуем поле type
  *ptr = static_cast<uint8_t>(header.type);
  ptr += sizeof(uint8_t);

  // Сериализуем поле size (в сетевом порядке байтов)
  uint32_t netSize = htonl(header.size);
  std::memcpy(ptr, &netSize, sizeof(uint32_t));
  ptr += sizeof(uint32_t);

  // Сериализуем поле flag (в сетевом порядке байтов)
  uint32_t netFlag = htonl(header.flag);
  std::memcpy(ptr, &netFlag, sizeof(uint32_t));
  ptr += sizeof(uint32_t);

  // Сериализуем тело сообщения
  if (!body.empty()) {
    std::memcpy(ptr, body.data(), body.size());
  }
}

void Message::deserialize(const std::vector<uint8_t>& buffer) {
  const uint8_t* ptr = buffer.data();

  // Десериализуем поле type
  header.type = static_cast<DataType>(*ptr);
  ptr += sizeof(uint8_t);

  // Десериализуем поле size
  uint32_t netSize;
  std::memcpy(&netSize, ptr, sizeof(uint32_t));
  header.size = ntohl(netSize);
  ptr += sizeof(uint32_t);

  // Десериализуем поле flag
  uint32_t netFlag;
  std::memcpy(&netFlag, ptr, sizeof(uint32_t));
  header.flag = ntohl(netFlag);
  ptr += sizeof(uint32_t);

  // Десериализуем тело сообщения
  size_t bodySize = buffer.size() - (ptr - buffer.data());
  if (bodySize > 0) {
    body.assign(ptr, ptr + bodySize);
  } else {
    body.clear();
  }
}

bool MySocket::sendMessage(const Message& message) {
  std::lock_guard<std::mutex> lock(send_mutex);
  std::vector<uint8_t> buffer;
  message.serialize(buffer);

  size_t totalSent = 0;
  while (totalSent < buffer.size()) {
    ssize_t sent =
        send(sock, buffer.data() + totalSent, buffer.size() - totalSent, 0);
    if (sent <= 0) {
      std::cerr << "Error sending data to socket " << sock << ": "
                << strerror(errno) << std::endl;
      return false;
    }
    totalSent += sent;
  }
  // std::cout << "Successfully sent " << totalSent << " bytes to socket " <<
  // sock
  //           << std::endl;
  return true;
}

bool MySocket::sendMessage(const Message& message, int socket) {
  std::lock_guard<std::mutex> lock(send_mutex);
  std::vector<uint8_t> buffer;
  message.serialize(buffer);
  if (send(socket, buffer.data(), buffer.size(), 0) < 0) {
    return false;
  }
  return true;
}

bool MySocket::sendFile(const std::string& filePath, int socket) {
  // Проверяем существование файла
  std::ifstream file(filePath, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    return false;
  }

  // Получаем размер файла
  std::streamsize fileSize = file.tellg();
  if (fileSize > 10 * 1024 * 1024) {  // Максимальный размер 10 МБ
    return false;
  }
  file.seekg(0, std::ios::beg);

  // Проверяем разрешенный формат файла
  std::string allowedExtensions[] = {
      ".jpg", ".jpeg", ".png", ".gif",            // Изображения
      ".pdf", ".txt",  ".doc", ".docx", ".xlsx",  // Документы
      ".zip", ".rar",                             // Архивы
      ".mp3", ".wav",  ".aac", ".flac",           // Аудио
      ".mp4", ".avi",  ".mkv", ".mov"             // Видео
  };

  std::string extension = filePath.substr(filePath.find_last_of('.'));
  bool isAllowed = false;
  for (const auto& ext : allowedExtensions) {
    if (extension == ext) {
      isAllowed = true;
      break;
    }
  }
  if (!isAllowed) {
    return false;
  }

  // Читаем файл в буфер
  std::vector<uint8_t> buffer(fileSize);
  if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize)) {
    return false;
  }

  // Отправляем размер файла (в сетевом порядке байтов)
  uint32_t netFileSize = htonl(static_cast<uint32_t>(fileSize));
  if (send(socket, reinterpret_cast<char*>(&netFileSize), sizeof(netFileSize),
           0) < 0) {
    return false;
  }

  // Отправляем данные файла
  size_t totalSent = 0;
  while (totalSent < buffer.size()) {
    ssize_t sent =
        send(socket, buffer.data() + totalSent, buffer.size() - totalSent, 0);
    if (sent <= 0) {
      return false;
    }
    totalSent += sent;
  }

  return true;
}

bool MySocket::receiveMessage(Message& message) {
  message.clearMessage(message);

  // Размер заголовка
  size_t headerSize = sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint32_t);
  std::vector<uint8_t> headerBuffer(headerSize);

  size_t totalBytesRead = 0;
  while (totalBytesRead < headerSize) {
    ssize_t bytesRead = read(sock, headerBuffer.data() + totalBytesRead,
                             headerSize - totalBytesRead);
    if (bytesRead <= 0) {
      std::cerr << "Error reading header" << std::endl;
      return false;
    }

    totalBytesRead += bytesRead;
  }

  // Десериализуем заголовок
  message.deserialize(headerBuffer);

  // Проверка допустимого размера тела сообщения
  const uint32_t MAX_MESSAGE_SIZE = 10 * 1024 * 1024;  // 10 МБ
  if (message.header.size > MAX_MESSAGE_SIZE) {
    std::cerr << "Message size exceeds maximum allowed size" << std::endl;
    return false;
  }

  // Чтение тела сообщения
  message.body.resize(message.header.size);
  totalBytesRead = 0;
  while (totalBytesRead < message.header.size) {
    ssize_t bytesRead = read(sock, message.body.data() + totalBytesRead,
                             message.header.size - totalBytesRead);
    if (bytesRead <= 0) {
      std::cerr << "Error reading body" << std::endl;
      return false;
    }
    totalBytesRead += bytesRead;
  }
  // std::cout << "Successfully received " << totalBytesRead << " bytes to
  // socket "
  //           << sock << std::endl;
  return true;
}

DataType MySocket::determineType(const std::string& input) {
  if (input == "audio") {
    return AUDIO;
  }

  if (isNumber(input)) {
    return NUMBER;
  }

  return TEXT;
}

bool MySocket::isNumber(const std::string& input) {
  if (input.empty()) return false;
  size_t start = 0;
  if (input[0] == '-') {
    if (input.size() == 1) return false;
    start = 1;
  }
  for (size_t i = start; i < input.size(); ++i) {
    if (!std::isdigit(input[i])) {
      return false;
    }
  }
  return true;
}

void MySocket::closeSocket() {
  if (sock != -1) {
    close(sock);
    sock = -1;
  }
}

std::string messageToString(Message& message) {
  return std::string(message.body.begin(), message.body.end());
}

Message stringToMessage(const std::string& text, Message& message) {
  message.header.type = DataType::TEXT;
  message.body.assign(text.begin(), text.end());
  message.header.size = message.body.size();
  return message;
}

Message flagOn(Message& message, int flag) {
  message.header.flag = flag;
  return message;
}

Message flagOff(Message& message) {
  message.header.flag = Flags::COMMAND;
  return message;
}

std::string MySocket::getIP() {
  sockaddr_in addr;
  socklen_t addr_len = sizeof(addr);
  getpeername(sock, (struct sockaddr*)&addr, &addr_len);
  char ipStr[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &(addr.sin_addr), ipStr, INET_ADDRSTRLEN);
  return std::string(ipStr);
}

/**
 * @brief Установить тип сообщения VOICE
 *
 * @details
 * - Записывает длину имени канала (uint32_t) в сетевом порядке байтов
 * - Записывает имя канала
 * - Записывает длину Opus-пакета (uint32_t) в сетевом порядке байтов
 * - Записывает Opus-данные
 * - Устанавливает размер тела сообщения
 *
 * @param packet - пакет аудиоданных
 * @param channel - имя канала
 *
 * @return true, если успешно
 */
bool Message::setVoiceMessage(AudioPacket& packet, const std::string& channel) {
  header.type = DataType::VOICE;
  audioPacket = packet;
  body.clear();

  // Записываем длину имени канала (uint32_t) в сетевом порядке байтов
  uint32_t channelLength = static_cast<uint32_t>(channel.size());
  uint32_t netChannelLength = htonl(channelLength);
  body.insert(body.end(), reinterpret_cast<const uint8_t*>(&netChannelLength),
              reinterpret_cast<const uint8_t*>(&netChannelLength) +
                  sizeof(netChannelLength));

  // Записываем имя канала
  body.insert(body.end(), channel.begin(), channel.end());

  // Записываем длину Opus-пакета (uint32_t) в сетевом порядке байтов
  uint32_t opusLength = static_cast<uint32_t>(packet.opus_length);
  uint32_t netOpusLength = htonl(opusLength);
  body.insert(
      body.end(), reinterpret_cast<const uint8_t*>(&netOpusLength),
      reinterpret_cast<const uint8_t*>(&netOpusLength) + sizeof(netOpusLength));

  // Записываем Opus-данные
  body.insert(body.end(), packet.opus_data, packet.opus_data + opusLength);

  // Устанавливаем размер тела сообщения
  header.size = static_cast<uint32_t>(body.size());

  return true;
}

bool Message::setVoiceMessage(const std::vector<int16_t>& audioData,
                              const std::string& channel) {
  header.type = DataType::VOICE;
  body.clear();

  // Записываем длину имени канала (uint32_t) в сетевом порядке байтов
  uint32_t channelLength = static_cast<uint32_t>(channel.size());
  uint32_t netChannelLength = htonl(channelLength);
  body.insert(
      body.end(), reinterpret_cast<uint8_t*>(&netChannelLength),
      reinterpret_cast<uint8_t*>(&netChannelLength) + sizeof(netChannelLength));

  // Записываем имя канала
  body.insert(body.end(), channel.begin(), channel.end());

  // Записываем аудиоданные
  const uint8_t* audioBytes =
      reinterpret_cast<const uint8_t*>(audioData.data());
  body.insert(body.end(), audioBytes,
              audioBytes + audioData.size() * sizeof(int16_t));

  // Устанавливаем размер тела сообщения
  header.size = static_cast<uint32_t>(body.size());

  return true;
}

bool Message::setAudioMessage(const std::string& filePath,
                              const std::string& id,
                              const std::string& channel) {
  // Проверяем, что пользователь находится в канале
  if (channel.empty()) {
    std::cerr << "Пользователь не находится в канале." << std::endl;
    return false;
  }

  // Проверяем существование файла
  std::ifstream file(filePath, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    std::cerr << "Не удалось открыть аудиофайл: " << filePath << std::endl;
    return false;
  }

  // Проверяем, что это файл .wav
  std::string extension = filePath.substr(filePath.find_last_of('.'));
  if (extension != ".wav") {
    std::cerr << "Неподдерживаемый формат аудиофайла: " << extension
              << std::endl;
    return false;
  }

  // Получаем размер файла
  std::streamsize fileSize = file.tellg();
  if (fileSize > 10 * 1024 * 1024) {  // Максимальный размер 10 МБ
    std::cerr << "Размер аудиофайла превышает 10 МБ." << std::endl;
    return false;
  }
  file.seekg(0, std::ios::beg);

  // Читаем файл в буфер
  std::vector<uint8_t> audioData(fileSize);
  if (!file.read(reinterpret_cast<char*>(audioData.data()), fileSize)) {
    std::cerr << "Ошибка чтения аудиофайла." << std::endl;
    return false;
  }

  // Получаем имя файла
  std::string fileName = filePath.substr(filePath.find_last_of("/\\") + 1);

  // Подготавливаем тело сообщения
  body.clear();

  // Записываем длину id (в сетевом порядке байтов)
  uint32_t idLength = static_cast<uint32_t>(id.size());
  uint32_t netIdLength = htonl(idLength);
  body.insert(body.end(), reinterpret_cast<uint8_t*>(&netIdLength),
              reinterpret_cast<uint8_t*>(&netIdLength) + sizeof(netIdLength));

  // Записываем id
  body.insert(body.end(), id.begin(), id.end());

  // Записываем длину имени канала (в сетевом порядке байтов)
  uint32_t channelLength = static_cast<uint32_t>(channel.size());
  uint32_t netChannelLength = htonl(channelLength);
  body.insert(
      body.end(), reinterpret_cast<uint8_t*>(&netChannelLength),
      reinterpret_cast<uint8_t*>(&netChannelLength) + sizeof(netChannelLength));

  // Записываем имя канала
  body.insert(body.end(), channel.begin(), channel.end());

  // Записываем длину имени файла (в сетевом порядке байтов)
  uint32_t fileNameLength = static_cast<uint32_t>(fileName.size());
  uint32_t netFileNameLength = htonl(fileNameLength);
  body.insert(body.end(), reinterpret_cast<uint8_t*>(&netFileNameLength),
              reinterpret_cast<uint8_t*>(&netFileNameLength) +
                  sizeof(netFileNameLength));

  // Записываем имя файла
  body.insert(body.end(), fileName.begin(), fileName.end());

  // Записываем данные аудио
  body.insert(body.end(), audioData.begin(), audioData.end());

  // Устанавливаем заголовок
  header.type = DataType::AUDIO;
  header.size = body.size();

  return true;
}

bool Message::setAudioMessage(const std::string& filePath) {
  // Проверяем существование файла
  std::ifstream file(filePath, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    std::cerr << "Не удалось открыть аудиофайл: " << filePath << std::endl;
    return false;
  }

  // Проверяем, что это файл .wav
  std::string extension = filePath.substr(filePath.find_last_of('.'));
  if (extension != ".wav") {
    std::cerr << "Неподдерживаемый формат аудиофайла: " << extension
              << std::endl;
    return false;
  }

  // Получаем размер файла
  std::streamsize fileSize = file.tellg();
  if (fileSize > 10 * 1024 * 1024) {  // Максимальный размер 10 МБ
    std::cerr << "Размер аудиофайла превышает 10 МБ." << std::endl;
    return false;
  }
  file.seekg(0, std::ios::beg);  // Сбрасываем позицию в файле

  // Читаем файл в буфер
  std::vector<uint8_t> audioData(fileSize);
  if (!file.read(reinterpret_cast<char*>(audioData.data()), fileSize)) {
    std::cerr << "Ошибка чтения аудиофайла." << std::endl;
    return false;
  }

  // Получаем имя файла
  std::string fileName = filePath.substr(filePath.find_last_of("/\\") + 1);

  // Подготавливаем тело сообщения
  body.clear();

  // Записываем длину имени файла (в сетевом порядке байтов)
  uint32_t fileNameLength = static_cast<uint32_t>(fileName.size());
  uint32_t netFileNameLength = htonl(fileNameLength);
  body.insert(body.end(), reinterpret_cast<uint8_t*>(&netFileNameLength),
              reinterpret_cast<uint8_t*>(&netFileNameLength) +
                  sizeof(netFileNameLength));

  // Записываем имя файла
  body.insert(body.end(), fileName.begin(), fileName.end());

  // Записываем данные аудио
  body.insert(body.end(), audioData.begin(), audioData.end());

  // Устанавливаем заголовок
  header.type = DataType::AUDIO;
  header.size = body.size();

  return true;
}

bool saveFile(const std::string& directory, const std::string& fileName,
              const std::vector<uint8_t>& audioData) {
  // Создаем директорию, если она не существует
  std::error_code ec;
  if (!std::filesystem::exists(directory)) {
    if (!std::filesystem::create_directories(directory, ec)) {
      return false;
    }
  }

  // Формируем полный путь для сохранения файла
  std::string filePath = directory + "/" + fileName;

  // Сохраняем аудиофайл
  std::ofstream outFile(filePath, std::ios::binary);
  if (!outFile.is_open()) {
    return false;
  }
  outFile.write(reinterpret_cast<const char*>(audioData.data()),
                audioData.size());
  outFile.close();

  return true;
}

double getAudioDuration(const std::string& filePath) {
  SF_INFO sfInfo;
  SNDFILE* sndFile = sf_open(filePath.c_str(), SFM_READ, &sfInfo);
  if (!sndFile) {
    return 0.0;
  }

  double duration = static_cast<double>(sfInfo.frames) / sfInfo.samplerate;
  sf_close(sndFile);
  return duration;
}

bool Message::setFileMessage(const FilePacket& packet,
                             const std::string& channel,
                             const std::string& id) {
  header.type = DataType::FILE_TYPE;
  body.clear();

  // Записываем длину id (в сетевом порядке байтов)
  uint32_t idLength = static_cast<uint32_t>(id.size());
  uint32_t netIdLength = htonl(idLength);
  body.insert(body.end(), reinterpret_cast<uint8_t*>(&netIdLength),
              reinterpret_cast<uint8_t*>(&netIdLength) + sizeof(netIdLength));

  // Записываем id
  body.insert(body.end(), id.begin(), id.end());

  // Записываем длину имени канала (uint32_t) в сетевом порядке байтов
  uint32_t channelLength = static_cast<uint32_t>(channel.size());
  uint32_t netChannelLength = htonl(channelLength);
  body.insert(body.end(), reinterpret_cast<const uint8_t*>(&netChannelLength),
              reinterpret_cast<const uint8_t*>(&netChannelLength) +
                  sizeof(netChannelLength));

  // Записываем имя канала
  body.insert(body.end(), channel.begin(), channel.end());

  // Записываем длину имени файла (uint32_t) в сетевом порядке байтов
  uint32_t filenameLength = static_cast<uint32_t>(packet.filename.size());
  uint32_t netFilenameLength = htonl(filenameLength);
  body.insert(body.end(), reinterpret_cast<const uint8_t*>(&netFilenameLength),
              reinterpret_cast<const uint8_t*>(&netFilenameLength) +
                  sizeof(netFilenameLength));

  // Записываем имя файла
  body.insert(body.end(), packet.filename.begin(), packet.filename.end());

  // Записываем размер файла (uint32_t) в сетевом порядке байтов
  uint32_t netFileSize = htonl(packet.file_size);
  body.insert(
      body.end(), reinterpret_cast<const uint8_t*>(&netFileSize),
      reinterpret_cast<const uint8_t*>(&netFileSize) + sizeof(netFileSize));

  // Записываем данные файла
  body.insert(body.end(), packet.file_data.begin(), packet.file_data.end());

  // Устанавливаем размер тела сообщения
  header.size = static_cast<uint32_t>(body.size());

  return true;
}
