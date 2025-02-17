CXX = g++
CXXFLAGS =  -I./include  -g $(shell pkg-config --cflags portaudio-2.0)
LDFLAGS = $(shell pkg-config --libs portaudio-2.0) -lboost_system -lboost_filesystem -lssl -lcrypto -lsndfile -lopus

SRCFILES = $(filter-out ./src/client.cpp ./src/server.cpp, $(wildcard ./src/*.cpp))
CMDFILES = ./command_handler/*.cpp

all: clean server client test

# Целевой исполняемый файл server
server: ./src/server.cpp
	$(CXX) $(CXXFLAGS) -o ./program/server ./src/server.cpp $(SRCFILES) $(CMDFILES) $(LDFLAGS)

# Целевой исполняемый файл client
client: ./src/client.cpp
	$(CXX) $(CXXFLAGS) -o ./program/client ./src/client.cpp ./src/mysocket.cpp $(LDFLAGS)

# Целевые скрипты
test: ./tests/send_script.cpp ./tests/listen_script.cpp
	$(CXX) $(CXXFLAGS) -o ./tests/send_script ./tests/send_script.cpp
	$(CXX) $(CXXFLAGS) -o ./tests/listen_script ./tests/listen_script.cpp

# Очистка собранных файлов
clean:
	rm -f ./program/client ./program/server ./tests/send_script ./tests/listen_script subprocess sys time argparse
	rm -f ./program/channels/*.txt
	rm -f ./program/server.log
	rm -f ./program/channels/members/*.txt
	rm -f ./program/users/*.txt
	rm -f ./program/channels/history/*.txt
	rm -f *.o
	rm -f ./program/channels/audio/*.wav
	rm -f ./program/channels/files/*
