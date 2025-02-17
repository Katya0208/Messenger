#include "../include/client.hpp"

Client *globalClient = nullptr;

/**
 * Handles the SIGINT signal by shutting down the client, closing the socket,
 * and exiting the program.
 *
 * @param signal The signal number that triggered the handler.
 *
 * @return void
 *
 * @throws None
 */
void signalHandler(int signal) {
  if (signal == SIGINT) {
    std::cout << "Shutting down client..." << std::endl;
    globalClient->clientRunning = false;
    globalClient->clientSocket.closeSocket();
    exit(EXIT_SUCCESS);
  }
}

std::string helloWindow() {
  std::ostringstream oss;
  oss << "============================================" << std::endl;
  oss << "             Welcome to chat!               " << std::endl;
  oss << "============================================" << std::endl;
  oss << std::endl;
  oss << "Please choose an option:" << std::endl;
  oss << std::endl;
  oss << "1. Sign up" << std::endl;
  oss << "2. Log in" << std::endl;
  oss << std::endl;
  oss << "Enter your choice(1/2): ";
  return oss.str();
}

/**
 * Receives messages from the server and prints them to the console. If an ID
 * message is received, it is processed and sent back to the server. The
 * function continues to receive messages until the client is shut down.
 *
 * @param id a reference to a string to store the ID message received from the
 * server
 *
 * @throws None
 */
void ReceiveMessage(std::string &id, std::string &currentChannel,
                    Client &client) {
  Message message;
  while (client.clientRunning) {
    if (!client.clientSocket.receiveMessage(message)) {
      std::cerr << "Server disconnected or error occurred." << std::endl;
      client.clientRunning = false;
      break;
    }
    if (message.header.type == DataType::AUDIO) {
      client.processAudioMessage(message);

    } else if (message.header.type == DataType::VOICE) {
    } else {
      if (message.header.flag == Flags::ID) {
        id = messageToString(message);
        client.id = id;
        message.clearMessage(message);
        message = stringToMessage(id, message);
        message = flagOn(message, Flags::ID);
        if (!client.clientSocket.sendMessage(message)) {
          std::cerr << "Failed to send ID." << std::endl;
        }
      } else if (message.header.flag == Flags::ID_CORRECT) {
        // std::cout << "ID CORRECT, nickname: " << messageToString(message)
        //          << std::endl;
        std::string nick = messageToString(message);
        client.setNickname(nick);
        {
          std::lock_guard<std::mutex> lock(client.mtx);  // Захват мьютекса
          ready = true;
        }
        client.cv.notify_all();
      } else if (message.header.flag == Flags::DEL_CHANNEL or
                 message.header.flag == Flags::NO_CHANNEL) {
        if (message.header.flag == Flags::NO_CHANNEL) {
          if (!client.lastChannel.empty()) {
            currentChannel = client.lastChannel;
            client.lastChannel = "";
          } else {
            currentChannel = "";
          }
        } else {
          currentChannel = "";
        }

        {
          std::lock_guard<std::mutex> lock(client.mtx);  // Захват мьютекса
          client.messageQueue.push(messageToString(message));
          ready = true;
        }
        client.cv.notify_all();
      } else if (message.header.flag == Flags::CHECK_LOGIN) {
        client.loginCorrect = true;
        std::cout << messageToString(message) << std::endl;
      } else if (message.header.flag == Flags::CHECK_PASSWORD) {
        client.passwordCorrect = true;

        std::cout << messageToString(message) << std::endl;
      } else if (message.header.flag == Flags::CHECK_NICKNAME) {
        client.nicknameCorrect = true;

        std::cout << messageToString(message) << std::endl;
      } else if (message.header.flag == Flags::REGISTERED) {
        client.passwordCorrect = true;

        client.cv.notify_all();
      } else if (message.header.flag == Flags::AUTHORIZED) {
        std::string nickname = messageToString(message);
        client.setNickname(nickname);

        client.passwordCorrect = true;
      } else if (message.header.flag == Flags::CHANGE_NICK) {
        std::string newNickname = messageToString(message);
        client.setNickname(newNickname);
        std::cout << "Nickname changed to: " << messageToString(message)
                  << std::endl;
        {
          std::lock_guard<std::mutex> lock(client.mtx);  // Захват мьютекса

          // client.messageQueue.push(messageToString(message));
          ready = true;
        }
        client.cv.notify_all();
      } else if (message.header.flag == Flags::TIME_ON) {
        client.timeFlag = true;
        {
          std::lock_guard<std::mutex> lock(client.mtx);  // Захват мьютекса

          client.messageQueue.push(messageToString(message));
          ready = true;
        }
        client.cv.notify_all();
      } else if (message.header.flag == Flags::TIME_OFF) {
        client.timeFlag = false;

        {
          std::lock_guard<std::mutex> lock(client.mtx);  // Захват мьютекса

          client.messageQueue.push(messageToString(message));
          ready = true;
        }
        client.cv.notify_all();
      } else if (message.header.flag == Flags::COMMAND) {
        {
          std::lock_guard<std::mutex> lock(client.mtx);  // Захват мьютекса
          client.messageQueue.push(messageToString(message));
          ready = true;
        }
        client.cv.notify_all();
      } else if (message.header.flag == Flags::AUDIOFILE_ERROR) {
        {
          std::lock_guard<std::mutex> lock(client.mtx);  // Захват мьютекса
          client.messageQueue.push(messageToString(message));
          ready = true;
        }
      } else if (message.header.flag == Flags::FILE_ERROR) {
      } else {
        std::cout << "Unknown flag: " << message.header.flag << std::endl;
        {
          std::lock_guard<std::mutex> lock(client.mtx);  // Захват мьютекса

          ready = true;
        }
        client.cv.notify_all();
      }
    }
  }
  client.clientSocket.closeSocket();
}

/**
 * Sends a registration message with the given nickname to the server.
 *
 * @param nickname The nickname to register.
 *
 * @throws None
 */
void Client::registration(const std::string &nickname) {
  std::cout << "Registering nickname: " << nickname << std::endl;
  Message message;
  message = stringToMessage(nickname, message);
  message = flagOn(message, Flags::NICK);
  if (!clientSocket.sendMessage(message)) {
    std::cerr << "Failed to send nickname." << std::endl;
  }
}

void Client::helpFlagAnswer(const char *program_name) {
  std::cout << "Usage: " << std::endl
            << program_name << " <ip-address:port>" << std::endl;
  std::cout << program_name << std::endl;
  std::cout << "Options:\n"
            << "  -h, --help  Show this help message\n";
  std::cout << std::endl;
  std::cout << "Connect command:" << std::endl;
  std::cout << "Usage: " << std::endl << "/connect <ip:port>" << std::endl;
}

void Client::ParseIpPort(std::string ipAndPort, std::string &ip, int &port) {
  ip = ipAndPort.substr(0, ipAndPort.find(':'));
  std::string StrPort = ipAndPort.substr(ipAndPort.find(':') + 1);
  port = atoi(StrPort.c_str());
}

/**
 * Parses a connect command from the given input string and extracts the IP
 * address and port, nickname, and channel (if provided).
 *
 * @param input The input string containing the connect command.
 * @param ip_port The extracted IP address and port.
 * @param nick The extracted nickname.
 * @param channel The extracted channel (if provided).
 *
 * @return True if the connect command is valid and the IP address and port,
 * nickname, and channel are successfully extracted. False otherwise.
 *
 * @throws None
 */
bool Client::parseConnectCommand(const std::string &input, std::string &ip_port,
                                 std::string &nick, std::string &channel) {
  std::istringstream iss(input);
  std::vector<std::string> tokens;
  std::string token;

  while (iss >> token) {
    tokens.push_back(token);
  }
  // connect ip:port nick channel
  if (tokens.size() == 4 && tokens[0] == "/connect") {
    ip_port = tokens[1];
    if (!isValidIpPort(ip_port)) {
      std::cerr << "Invalid ip:port format. Please try again." << std::endl;
      ip_port = "";
      return false;
    }
    nick = tokens[2];
    channel = tokens[3];
    return true;
  }
  // connect ip:port nick
  else if (tokens.size() == 3 && tokens[0] == "/connect") {
    ip_port = tokens[1];
    if (!isValidIpPort(ip_port)) {
      std::cerr << "Invalid ip:port format. Please try again." << std::endl;
      ip_port = "";
      return false;
    }
    nick = tokens[2];
    return true;
  } else if (tokens.size() == 2 && tokens[0] == "/connect") {
    ip_port = tokens[1];
    if (!isValidIpPort(ip_port)) {
      std::cerr << "Invalid ip:port format. Please try again." << std::endl;
      ip_port = "";
      return false;
    }
    return true;
  }
  return false;
}

void Client::promptConnect(std::string &ip_port, std::string &nick,
                           std::string &channel) {
  std::string input;

  while (true) {
    std::getline(std::cin, input);

    if (parseConnectCommand(input, ip_port, nick, channel)) {
      break;
    } else {
      std::cout << "Invalid command. Please try again." << std::endl;
    }
  }
}

bool Client::isValidIp(const std::string &ip) {
  std::regex ipRegex(R"(^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$)");
  std::smatch match;

  if (std::regex_match(ip, match, ipRegex)) {
    for (size_t i = 1; i <= 4; ++i) {
      int value = std::stoi(match[i].str());
      if (value < 0 || value > 255) {
        return false;
      }
    }
    return true;
  }
  return false;
}

bool Client::isValidPort(const std::string &portStr) {
  std::regex portRegex(R"(^(\d{1,5})$)");
  std::smatch match;

  if (std::regex_match(portStr, match, portRegex)) {
    int port = std::stoi(portStr);
    if (port >= 1 && port <= 65535) {
      return true;
    }
  }
  return false;
}

/**
 * Validates if a given string is in the format of an IP address followed by a
 * port number.
 *
 * @param ip_port The string to be validated in the format of
 * "IP_ADDRESS:PORT_NUMBER".
 *
 * @return True if the string is in the correct format and the IP address and
 * port number are valid, False otherwise.
 *
 * @throws None
 */
bool Client::isValidIpPort(const std::string &ip_port) {
  std::regex ipPortRegex(R"(^(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}):(\d{1,5})$)");
  std::smatch match;

  if (std::regex_match(ip_port, match, ipPortRegex)) {
    std::string ip = match[1];
    std::string port = match[2];

    return isValidIp(ip) && isValidPort(port);
  }

  return false;
}

bool Client::parseArgc(int argc, char *argv[], std::string &ipPort,
                       std::string &nick, std::string &channel, std::string &ip,
                       int &port) {
  if (argc > 0 and argc < 3) {
    if (argc != 1) {  // все кроме ./client
      ipPort = argv[1];
      if (!isValidIpPort(ipPort)) {  // проверка формата ip:port
        std::cerr << "Invalid ip:port format. Please try again." << std::endl;
        return false;
      }
    }
    ParseIpPort(ipPort, ip, port);
  } else {
    std::cerr << "Usage: " << argv[0] << " <ip-address server:port>"
              << std::endl;
    return false;
  }
  return true;
}

bool handleConnectCommand(const std::string &input, std::string &ipPort,
                          std::string &ip, int &port, std::string &nick,
                          std::string &channel, std::string &id,
                          std::string &currentChannel, Client &client) {
  int size = input.size();
  if (size < 2) {
    return false;
  }

  if (client.parseConnectCommand(input, ipPort, nick, channel)) {
    client.ParseIpPort(ipPort, ip, port);

    // Отключение от текущего сервера
    client.clientRunning = false;
    client.clientSocket.closeSocket();
    std::this_thread::sleep_for(std::chrono::seconds(
        1));  // Даем время для корректного завершения предыдущего подключения

    // Подключение к новому серверу
    if (!client.clientSocket.createSocket()) {
      std::cerr << "Failed to create socket." << std::endl;
      return false;
    }

    if (!client.clientSocket.connectSocket(ip, port)) {
      std::cerr << "Failed to connect to server." << std::endl;
      return false;
    }

    client.clientRunning = true;
    std::thread receiveThread(ReceiveMessage, std::ref(id),
                              std::ref(currentChannel), std::ref(client));
    receiveThread.detach();

    // Присоединение к каналу, если указан
    if (!channel.empty()) {
      Message message;
      message = stringToMessage(channel, message);
      message = flagOn(message, Flags::CHANNEL);
      client.clientSocket.sendMessage(message);
    }
    if (!nick.empty()) {
      client.registration(nick);
    }

    std::cout << "Connected to " << ip << ":" << port << " as " << nick
              << std::endl;
  } else {
    return false;
  }
  return true;
}

void helpToUse() {
  std::cout
      << "To send a command, write '/' before the first word of the command"
      << std::endl;
  std::cout << "List of commands:" << std::endl;
  std::cout << std::endl;
  std::cout
      << "To join a channel to send messages, use command: /join <channel>"
      << std::endl;
  std::cout << "If you join channel all messages that are not commands are "
               "sent to the channel."
            << std::endl;
  std::cout << "Also you can use command: /send <channel> <message>."
            << std::endl;
  std::cout << "To leave a channel, use command: /exit <channel>" << std::endl;
  std::cout << "To see all available channels, use command: /channels"
            << std::endl;
  std::cout << "To turn on time, use command: /time_on" << std::endl;
  std::cout << "To turn off time, use command: /time_off" << std::endl;
  std::cout << "To connect to another server, use command: /connect <ip:port> "
               "<nickname> <channel>"
            << std::endl;
}

bool isCommand(std::string &input) {
  char character = '/';
  if (input.find(character) == std::string::npos) {
    return false;
  }
  return true;
}

#include <array>
#include <cstdint>
#include <vector>

void Client::processAudioMessage(const Message &message) {
  const uint8_t *dataPtr = message.body.data();
  size_t dataSize = message.body.size();

  // Извлекаем длину имени файла
  if (dataSize < sizeof(uint32_t)) {
    std::cerr << "Invalid AUDIO message: insufficient data for filename "
                 "length"
              << std::endl;
    return;
  }
  uint32_t netFileNameLength;
  std::memcpy(&netFileNameLength, dataPtr, sizeof(uint32_t));
  uint32_t fileNameLength = ntohl(netFileNameLength);
  dataPtr += sizeof(uint32_t);
  dataSize -= sizeof(uint32_t);

  // Проверяем корректность длины имени файла
  if (dataSize < fileNameLength) {
    std::cerr << "Invalid AUDIO message: filename length mismatch" << std::endl;
    return;
  }

  // Извлекаем имя файла
  std::string fileName(reinterpret_cast<const char *>(dataPtr), fileNameLength);
  dataPtr += fileNameLength;
  dataSize -= fileNameLength;

  // Оставшиеся данные — это аудиоданные
  std::vector<uint8_t> audioData(dataPtr, dataPtr + dataSize);
  std::cout << "Enter path for saving:";
  std::string path;
  std::cin >> path;
  if (!saveFile(path, fileName, audioData)) {
    std::cerr << "Error saving audio file" << std::endl;
    return;
  }
  std::cout << path + fileName << std::endl;
  double duration = getAudioDuration(path + "/" + fileName);
  std::cout << "Audio file saved successfully. Duration: " << duration
            << " seconds" << std::endl;

  play_audio(audioData);
  {
    std::lock_guard<std::mutex> lock(mtx);  // Захват мьютекса

    ready = true;
  }
  cv.notify_all();
}

void Client::processVoiceMessage(const Message &message) {
  const uint8_t *dataPtr = message.body.data();
  size_t dataSize = message.body.size();
  std::vector<int16_t> audioData(dataPtr, dataPtr + dataSize);
  play_audio(audioData);
}

void Client::record_audio(int recOrVoice, std::string &channel) {
  // rec = 0, voice = 1
  Message message;
  PaStream *stream;
  PaError err;
  short buffer[FRAMES_PER_BUFFER * NUM_CHANNELS];

  err = Pa_Initialize();
  if (err != paNoError) {
    std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
    return;
  }

  PaStreamParameters inputParameters;
  inputParameters.device = Pa_GetDefaultInputDevice();
  inputParameters.channelCount = NUM_CHANNELS;
  inputParameters.sampleFormat = SAMPLE_TYPE;
  inputParameters.suggestedLatency =
      Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
  inputParameters.hostApiSpecificStreamInfo = NULL;

  err = Pa_OpenStream(&stream, &inputParameters, NULL, SAMPLE_RATE,
                      FRAMES_PER_BUFFER, paClipOff, NULL, NULL);
  if (err != paNoError) {
    std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
    Pa_Terminate();
    return;
  }

  err = Pa_StartStream(stream);
  if (err != paNoError) {
    std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
    Pa_CloseStream(stream);
    Pa_Terminate();
    return;
  }

  std::cout << "Recording started..." << std::endl;
  // audio_buffer.clear();  // Очищаем буфер перед записью

  while (recording_start) {
    err = Pa_ReadStream(stream, buffer, FRAMES_PER_BUFFER);
    if (err && err != paInputOverflowed) {
      std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
      break;
    }

    if (recOrVoice) {
      int error;
      unsigned char opus_data[OPUS_MAX_PACKET_SIZE];
      OpusEncoder *encoder = opus_encoder_create(SAMPLE_RATE, NUM_CHANNELS,
                                                 OPUS_APPLICATION_VOIP, &error);
      if (error != OPUS_OK) {
        std::cerr << "Failed to create Opus encoder: " << opus_strerror(error)
                  << std::endl;
        return;
      }
      int opus_length =
          opus_encode(encoder, (const opus_int16 *)buffer, FRAMES_PER_BUFFER,
                      opus_data, OPUS_MAX_PACKET_SIZE);
      if (opus_length < 0) {
        std::cerr << "Opus encoding error: " << opus_strerror(opus_length)
                  << std::endl;
        break;
      }

      AudioPacket packet;
      packet.timestamp =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::system_clock::now().time_since_epoch())
              .count();
      packet.opus_length = opus_length;
      memcpy(packet.opus_data, opus_data, opus_length);
      message.setVoiceMessage(packet, channel);
      clientSocket.sendMessage(message);
      // send(clientSocket.getSocket(), &packet, sizeof(packet), 0);
    } else {
      // Сохраняем данные в буфер
      audio_buffer.insert(audio_buffer.end(), buffer,
                          buffer + FRAMES_PER_BUFFER * NUM_CHANNELS);
    }
  }

  Pa_StopStream(stream);
  Pa_CloseStream(stream);
  Pa_Terminate();

  std::cout << "Recording stopped." << std::endl;
}
void play_audio(const std::vector<int16_t> &audioData) {
  std::cout << "Playing audio..." << std::endl;
  PaError err;
  PaStream *stream;

  err = Pa_Initialize();
  if (err != paNoError) {
    std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
    return;
  }

  PaStreamParameters outputParameters;
  outputParameters.device = Pa_GetDefaultOutputDevice();
  if (outputParameters.device == paNoDevice) {
    std::cerr << "Error: No default output device." << std::endl;
    Pa_Terminate();
    return;
  }
  outputParameters.channelCount = NUM_CHANNELS;
  outputParameters.sampleFormat = paInt16;
  outputParameters.suggestedLatency =
      Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
  outputParameters.hostApiSpecificStreamInfo = NULL;

  err = Pa_OpenStream(&stream, NULL, &outputParameters, SAMPLE_RATE,
                      FRAMES_PER_BUFFER, paClipOff, NULL, NULL);
  if (err != paNoError) {
    std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
    Pa_Terminate();
    return;
  }

  err = Pa_StartStream(stream);
  if (err != paNoError) {
    std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
    Pa_CloseStream(stream);
    Pa_Terminate();
    return;
  }

  err =
      Pa_WriteStream(stream, audioData.data(), audioData.size() / NUM_CHANNELS);
  if (err != paNoError) {
    std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
  }

  Pa_StopStream(stream);
  Pa_CloseStream(stream);
  Pa_Terminate();
}
void play_audio(const std::vector<uint8_t> &audioData) {
  PaError err;
  PaStream *stream;

  // Инициализация PortAudio
  err = Pa_Initialize();
  if (err != paNoError) {
    std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
    return;
  }

  // Настройка параметров вывода
  PaStreamParameters outputParameters;
  outputParameters.device = Pa_GetDefaultOutputDevice();
  if (outputParameters.device == paNoDevice) {
    std::cerr << "Error: No default output device." << std::endl;
    Pa_Terminate();
    return;
  }
  outputParameters.channelCount = NUM_CHANNELS;
  outputParameters.sampleFormat = paInt16;  // 16-битные сэмплы
  outputParameters.suggestedLatency =
      Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
  outputParameters.hostApiSpecificStreamInfo = NULL;

  // Открытие потока
  err = Pa_OpenStream(&stream, NULL, &outputParameters, SAMPLE_RATE,
                      FRAMES_PER_BUFFER, paClipOff, NULL, NULL);
  if (err != paNoError) {
    std::cerr << "PortAudio error during OpenStream: " << Pa_GetErrorText(err)
              << std::endl;
    Pa_Terminate();
    return;
  }

  // Запуск потока
  err = Pa_StartStream(stream);
  if (err != paNoError) {
    std::cerr << "PortAudio error during StartStream: " << Pa_GetErrorText(err)
              << std::endl;
    Pa_CloseStream(stream);
    Pa_Terminate();
    return;
  }

  // Проверка корректности размера данных
  size_t bytesPerSample = sizeof(int16_t);
  size_t bytesPerFrame = NUM_CHANNELS * bytesPerSample;
  if (audioData.size() % bytesPerFrame != 0) {
    std::cerr << "Warning: audioData size is not a multiple of bytes per "
                 "frame. Padding with zeros."
              << std::endl;
  }

  // Рассчёт количества фреймов
  size_t totalFrames = audioData.size() / bytesPerFrame;

  // Если необходимо, добавьте паддинг
  std::vector<uint8_t> paddedAudioData = audioData;
  size_t remainingBytes = audioData.size() % bytesPerFrame;
  if (remainingBytes != 0) {
    paddedAudioData.resize(audioData.size() + (bytesPerFrame - remainingBytes),
                           0);
    totalFrames += 1;  // Один дополнительный фрейм
  }

  // Преобразование данных из uint8_t в int16_t
  const int16_t *samples =
      reinterpret_cast<const int16_t *>(paddedAudioData.data());

  // Запись данных в поток
  err = Pa_WriteStream(stream, samples, totalFrames);
  if (err != paNoError) {
    std::cerr << "PortAudio error during WriteStream: " << Pa_GetErrorText(err)
              << std::endl;
  }

  // Остановка потока
  err = Pa_StopStream(stream);
  if (err != paNoError) {
    std::cerr << "PortAudio error during StopStream: " << Pa_GetErrorText(err)
              << std::endl;
  }

  // Закрытие потока
  err = Pa_CloseStream(stream);
  if (err != paNoError) {
    std::cerr << "PortAudio error during CloseStream: " << Pa_GetErrorText(err)
              << std::endl;
  }

  // Завершение работы PortAudio
  Pa_Terminate();
}

void Client::save_audio_to_file(const std::string &filename) {
  SF_INFO sfinfo;
  sfinfo.channels = NUM_CHANNELS;
  sfinfo.samplerate = SAMPLE_RATE;
  sfinfo.format = FILE_FORMAT;

  SNDFILE *outfile = sf_open(filename.c_str(), SFM_WRITE, &sfinfo);
  if (!outfile) {
    std::cerr << "Error opening file: " << sf_strerror(NULL) << std::endl;
    return;
  }

  sf_write_short(outfile, audio_buffer.data(), audio_buffer.size());
  sf_close(outfile);

  std::cout << "Audio saved to " << filename << std::endl;
}

std::string generateFilename(const std::string &nick) {
  // Получаем текущее время
  std::time_t currentTime = std::time(nullptr);
  // Преобразуем в локальное время
  std::tm *localTime = std::localtime(&currentTime);

  // Форматируем дату в строку "DD:MM:YYYY"
  char dateBuffer[11];  // "DD:MM:YYYY" + завершающий нуль
  std::strftime(dateBuffer, sizeof(dateBuffer), "%d-%m-%Y", localTime);

  // Генерируем имя файла
  std::string filename = std::string(dateBuffer) + "_" + nick + ".wav";

  return filename;
}

bool MySocket::sendFileIfExists(const std::string &filePath,
                                const std::string &channel,
                                const std::string &id) {
  // Проверяем, существует ли файл
  if (!std::filesystem::exists(filePath)) {
    std::cerr << "Файл не найден: " << filePath << std::endl;
    return false;
  }

  // Открываем файл в бинарном режиме
  std::ifstream file(filePath, std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "Не удалось открыть файл: " << filePath << std::endl;
    return false;
  }

  // Получаем размер файла
  file.seekg(0, std::ios::end);
  std::streamsize fileSize = file.tellg();
  file.seekg(0, std::ios::beg);

  // Читаем содержимое файла в буфер
  std::vector<uint8_t> fileData(static_cast<size_t>(fileSize));
  if (!file.read(reinterpret_cast<char *>(fileData.data()), fileSize)) {
    std::cerr << "Ошибка при чтении файла: " << filePath << std::endl;
    return false;
  }

  // Создаем FilePacket
  FilePacket packet;
  // Извлекаем только имя файла из пути
  packet.filename = std::filesystem::path(filePath).filename().string();
  packet.file_size = static_cast<uint32_t>(fileSize);
  packet.file_data = std::move(fileData);

  // Создаем сообщение
  Message message;
  if (!message.setFileMessage(packet, channel, id)) {
    std::cerr << "Не удалось установить FileMessage." << std::endl;
    return false;
  }

  // Отправляем сообщение
  if (!sendMessage(message)) {
    std::cerr << "Не удалось отправить сообщение с файлом." << std::endl;
    return false;
  }

  std::cout << "Файл успешно отправлен: " << filePath << std::endl;
  return true;
}

/**
 * Handles the command entered by the user and performs the corresponding
 * actions.
 *
 * @param client The socket object used for communication.
 * @param command The command entered by the user.
 * @param message The message object used for sending messages.
 * @param ipPort The IP address and port of the server.
 * @param ip The IP address of the server.
 * @param port The port number of the server.
 * @param nick The nickname of the user.
 * @param channel The channel the user is currently in.
 * @param currentChannel The channel the user wants to join or exit.
 * @param id The ID of the user.
 *
 * @return true if the command was handled successfully, false otherwise.
 *
 * @throws None.
 */

bool commandHandler(std::string &command, Message &message, std::string &ipPort,
                    std::string &ip, int &port, std::string &channel,
                    std::string &currentChannel, std::string &id,
                    Client &client) {
  std::istringstream iss(command);
  std::string word;
  iss >> word;
  if (word == "/join") {
    std::string newChannel;
    iss >> newChannel;
    if (!newChannel.empty()) {
      client.lastChannel = currentChannel;
      currentChannel = newChannel;
      command = "/join " + currentChannel;
      message = stringToMessage(command, message);

      client.clientSocket.sendMessage(message);
    } else {
      std::cout << "Invalid join command. Usage: join <channel>" << std::endl;
      return false;
    }
  } else if (word == "/read") {
    std::string channel;
    iss >> channel;
    if (!channel.empty()) {
      message = stringToMessage(command, message);
      client.clientSocket.sendMessage(message);
    } else {
      std::cout << "Invalid read command. Usage: read <channel>" << std::endl;
      return false;
    }
  } else if (word == "/exit") {
    std::string channel;

    iss >> channel;
    if (!channel.empty()) {
      if (currentChannel == channel) currentChannel = "";
      message = stringToMessage(command, message);
      client.clientSocket.sendMessage(message);
    } else {
      std::cout << "Invalid exit command. Usage: exit <channel>" << std::endl;
      return false;
    }
  } else if (word == "/nick") {
    std::string newNickname;
    iss >> newNickname;

    if (!newNickname.empty()) {
      message = stringToMessage(command, message);
      client.clientSocket.sendMessage(message);
    } else {
      std::cout << "Invalid /nick command. Usage: /nick <new "
                   "nickname >"
                << std::endl;
      return false;
    }
  } else if (word == "/send") {
    std::string flag;
    iss >> flag;
    if (flag == "-f") {
      std::string sendChannel;
      iss >> sendChannel;
      std::string filePath;
      iss >> filePath;
      if (!filePath.empty()) {
        FilePacket packet;
        if (!client.clientSocket.sendFileIfExists(filePath, sendChannel, id)) {
          std::cerr << "Failed to send file." << std::endl;
          return false;
        }
      }
    } else {
      std::string sendChannel = flag;
      std::string sendMessage;
      std::getline(iss, sendMessage);

      if (!sendChannel.empty() && !sendMessage.empty()) {
        message = stringToMessage(command, message);
        client.clientSocket.sendMessage(message);
      } else {
        std::cout << "Invalid send command. Usage: send <channel> <message>"
                  << std::endl;
        return false;
      }
    }
    ready = true;
  } else if (word == "/connect") {
    std::string nickname = client.getNickname();
    if (!handleConnectCommand(command, ipPort, ip, port, nickname, channel, id,
                              currentChannel, client)) {
      return false;
    };
    currentChannel = channel;
    message = stringToMessage(command, message);
    client.clientSocket.sendMessage(message);
  } else if (word == "/help") {
    helpToUse();
  } else if (word == "/channels") {
    message = stringToMessage(command, message);
    client.clientSocket.sendMessage(message);
  } else if (word == "/time_on") {
    message = stringToMessage(command, message);
    client.clientSocket.sendMessage(message);
    client.timeFlag = true;
  } else if (word == "/time_off") {
    message = stringToMessage(command, message);
    client.clientSocket.sendMessage(message);
    client.timeFlag = false;
  } else if (word == "/rec") {
    std::string filename = "";
    while (true) {
      std::string input;
      std::getline(std::cin, input);
      std::cout << "Press ENTER to start/stop recording..." << std::endl;
      if (input.empty()) {
        if (!client.recording_start) {
          // Начинаем запись
          client.recording_start = true;
          std::thread record_thread(&Client::record_audio, &client, 0,
                                    std::ref(currentChannel));
          record_thread.detach();
        } else {
          // Останавливаем запись
          client.recording_start = false;
          filename = generateFilename(client.getNickname());
          client.save_audio_to_file(filename);
          message.setAudioMessage(filename, id, currentChannel);
          client.clientSocket.sendMessage(message);
          if (std::remove(filename.c_str()) != 0) {
            std::cerr << "Ошибка при удалении файла: " << filename << std::endl;
          }
          break;
        }
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    ready = true;

  } else if (word == "/voicemail_on") {
    std::string audioId = "";
    iss >> audioId;
    if (!audioId.empty()) {
      message = stringToMessage(command, message);
      client.clientSocket.sendMessage(message);
    } else {
      std::cout
          << "Invalid voicemail_on command. Usage: /voicemail_on <audioId>"
          << std::endl;
      return false;
    }
  } else if (word == "/sound_on") {
    client.listeningStatus = true;
    while (true) {
      std::string input;
      std::getline(std::cin, input);
      std::cout << "Press ENTER to start/stop recording..." << std::endl;
      if (input.empty()) {
        if (!client.recording_start) {
          // Начинаем запись
          client.recording_start = true;
          std::thread record_thread(&Client::record_audio, &client, 1,
                                    std::ref(currentChannel));
          record_thread.detach();
        } else {
          client.recording_start = false;
        }
      } else if (input == "/sound_off") {
        break;
      } else {
        std::cout << "Press ENTER to start/stop recording..." << std::endl;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    ready = true;
  } else {
    return false;
  }
  return true;
}

std::string toUpper(const std::string &input) {
  std::string upper;
  upper.resize(input.size());
  std::transform(input.begin(), input.end(), upper.begin(), ::toupper);
  return upper;
}

std::string rulesForLoginNick() {
  std::string answer;
  answer = "Your login must consist of 3-24 characters.\n";
  answer += "Your login must start with a letter.\n";
  answer += "You can use only letters, digits, underscores and dots.";
  return answer;
}

std::string rulesForPassword() {
  std::string answer;
  answer = "Your password must consist of 8-64 characters.\n";
  answer +=
      "Characters: must include at least one character from each of the "
      "following categories:\n";
  answer += "Uppercase Latin letters (A-Z);\n";
  answer += "Lowercase Latin letters (a-z);\n";
  answer += "Digits (0-9);\n";
  answer += "Special characters (!@#$%^&*()-_=+[]{}|;:,.<>?/~`).\n";
  answer +=
      "Spaces and repetition of identical characters longer than 3 characters "
      "are prohibited.";

  return answer;
}

bool checkLoginOrNick(const std::string &login) {
  if (login.size() < 3 or login.size() > 24) {
    return false;
  }
  if (login[0] == '_' || login[0] == '.' || login[0] == '-') {
    return false;
  }
  for (char c : login) {
    if (!(isalpha(c) || isdigit(c) || c == '_' || c == '.' || c == '-')) {
      return false;
    }
    if (c == '_' && login.find("__") != std::string::npos) {
      return false;
    }
  }
  return true;
}

bool Client::checkPassword(const std::string &password) {
  if (password.size() < 8 || password.size() > 64) {
    return false;
  }

  bool hasUpper = false;
  bool hasLower = false;
  bool hasDigit = false;
  bool hasSpecial = false;

  for (char c : password) {
    if (isupper(c)) {
      hasUpper = true;
    } else if (islower(c)) {
      hasLower = true;
    } else if (isdigit(c)) {
      hasDigit = true;
    } else if (c == '!' || c == '@' || c == '#' || c == '$' || c == '%' ||
               c == '^' || c == '&' || c == '*' || c == '(' || c == ')' ||
               c == '-' || c == '_' || c == '=' || c == '+' || c == '[' ||
               c == ']' || c == '{' || c == '}' || c == '|' || c == ';' ||
               c == ':' || c == ',' || c == '.' || c == '<' || c == '>' ||
               c == '?' || c == '/' || c == '~') {
      hasSpecial = true;
    }

    if (c == ' ') {
      return false;
    }

    if (password.find(std::string(4, c)) != std::string::npos) {
      return false;
    }
  }

  if (!hasUpper || !hasLower || !hasDigit || !hasSpecial) {
    return false;
  }

  return true;
}
void Client::signUp(std::string &answer) {
  Message message;

  std::cout << "Enter login: ";
  std::string login = "";
  std::getline(std::cin, login);
  while (!checkLoginOrNick(login)) {
    std::cout << rulesForLoginNick() << std::endl;
    std::cout << "Enter login: ";
    std::getline(std::cin, login);
  }
  message = flagOn(message, Flags::LOGIN_SIGN_UP);
  message = stringToMessage(login, message);
  clientSocket.sendMessage(message);
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  if (loginCorrect) {
    std::cout << "Enter password: ";
    std::string password = "";
    std::getline(std::cin, password);
    while (!checkPassword(password)) {
      std::cout << rulesForPassword() << std::endl;
      std::cout << "Enter password: ";
      std::getline(std::cin, password);
    }
    message = flagOn(message, Flags::PASSWORD_SIGN_UP);
    message = stringToMessage(password, message);
    clientSocket.sendMessage(message);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  if (passwordCorrect and nickname.empty()) {
    std::cout << "Enter nickname: ";
    std::getline(std::cin, nickname);
    while (!checkLoginOrNick(nickname)) {
      std::cout << rulesForLoginNick() << std::endl;
      std::cout << "Enter nickname: ";
      std::getline(std::cin, nickname);
    }
    message = flagOn(message, Flags::NICK);
    message = stringToMessage(nickname, message);
    clientSocket.sendMessage(message);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  if (loginCorrect and passwordCorrect and !nickname.empty()) {
    std::cout << "Registration successful." << std::endl;
    setNickname(nickname);
    message = flagOn(message, Flags::CHECK_ID);
    message = stringToMessage("id", message);
    clientSocket.sendMessage(message);
  } else {
    std::cout << "Failed to sign up." << std::endl;
    exit(1);
  }
}

void Client::LogIn(std::string &answer) {
  ;
  Message message;
  std::cout << "Enter login: ";
  std::string login = "";
  std::getline(std::cin, login);
  message = flagOn(message, Flags::LOGIN_LOG_IN);
  message = stringToMessage(login, message);
  clientSocket.sendMessage(message);
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  if (loginCorrect) {
    std::cout << "Enter password: ";
    std::string password = "";
    std::getline(std::cin, password);
    message = flagOn(message, Flags::PASSWORD_LOG_IN);
    message = stringToMessage(password, message);
    clientSocket.sendMessage(message);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  if (passwordCorrect) {
    std::cout << "Authorization successful." << std::endl;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  if (loginCorrect and passwordCorrect) {
    message = flagOn(message, Flags::CHECK_ID);
    message = stringToMessage("id", message);
    clientSocket.sendMessage(message);
  } else {
    std::cout << "Failed to log in." << std::endl;
    exit(1);
  }
}

void Client::enterOnServer() {
  std::string answer;
  std::getline(std::cin, answer);
  if (answer == "1") {
    signUp(answer);
  } else if (answer == "2") {
    LogIn(answer);
  } else {
    std::cout << "Invalid answer. Choose 1 or 2: ";
    enterOnServer();
  }
}

void Client::setNickname(std::string &nick) {
  std::lock_guard<std::mutex> lock(mtx);
  nickname = nick;
}

std::string Client::getNickname() {
  std::lock_guard<std::mutex> lock(mtx);
  return nickname;
}

int main(int argc, char *argv[]) {
  Client client;
  globalClient = &client;
  // Обрабатываем сигнал Ctrl+C
  signal(SIGINT, signalHandler);

  std::string ipPort = "", nick = "", channel = "", ip = "", id = "";
  int port;
  // int joinFlag = 0;
  std::string currentChannel = "";  // Текущий канал

  // Обрабатываем флаг -h и --help
  if (argc == 2 &&
      (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
    client.helpFlagAnswer(argv[0]);
    return false;
  }

  // Парсим аргументы при ./client
  if (!client.parseArgc(argc, argv, ipPort, nick, channel, ip, port)) {
    exit(1);
  }

  if (ipPort.empty()) {
    std::cout << "Enter /connect <ip:port>" << std::endl;
    client.promptConnect(ipPort, nick, channel);
    client.ParseIpPort(ipPort, ip, port);
  }

  if (!client.clientSocket.createSocket()) {
    std::cerr << "Failed to create socket." << std::endl;
    return 1;
  }

  if (!client.clientSocket.connectSocket(ip, port)) {
    std::cerr << "Failed to connect to server." << std::endl;
    return 2;
  }

  std::thread receiveThread(ReceiveMessage, std::ref(id),
                            std::ref(currentChannel), std::ref(client));
  Message message;

  std::cout << helloWindow();

  if (client.clientRunning) {
    client.enterOnServer();
  }
  std::string upperNick = "";
  while (client.clientRunning) {  // основной поток ввода сообщений
    // Ник в верхнем регистре
    {
      std::unique_lock<std::mutex> lock(client.mtx);

      // Ожидание, пока есть новые сообщения
      client.cv.wait(lock, [] { return ready; });

      while (!client.messageQueue.empty()) {
        std::cout << client.messageQueue.front() << std::endl;
        client.messageQueue.pop();
      }
      ready = false;  // Сброс флага готовности
    }
    std::string currentNickname = client.getNickname();
    upperNick = toUpper(currentNickname);

    std::string command;
    message = flagOff(message);
    if (currentChannel.empty()) {
      if (client.timeFlag) {
        std::time_t currentTime = std::time(nullptr);
        char timeBuffer[9];
        std::strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S",
                      std::localtime(&currentTime));
        std::cout << "[" << timeBuffer << "] " << upperNick << "@> ";
      } else {
        std::cout << upperNick << "@> ";
      }
    } else {
      if (client.timeFlag) {
        std::time_t currentTime = std::time(nullptr);
        char timeBuffer[9];
        std::strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S",
                      std::localtime(&currentTime));
        std::cout << "[" << timeBuffer << "] " << upperNick << "@"
                  << toUpper(currentChannel) << "> ";
      } else {
        std::cout << upperNick << "@" << toUpper(currentChannel) << "> ";
      }
    }

    std::getline(std::cin, command);

    if (isCommand(command)) {
      if (!commandHandler(command, message, ipPort, ip, port, channel,
                          currentChannel, id, client)) {
        std::cout << "Wrong command. Usage: /help" << std::endl;
        ready = true;
        continue;
      }
    } else if (!currentChannel.empty() && !command.empty()) {
      command = "/send " + currentChannel + " " + command;
      message = stringToMessage(command, message);
      client.clientSocket.sendMessage(message);
    } else {
      std::cout << "Wrong command. Usage: /help" << std::endl;
      ready = true;
      continue;
    }
  }

  client.clientRunning = false;
  client.clientSocket.closeSocket();

  std::cout << "Client shut down successfully." << std::endl;
  return 0;
}