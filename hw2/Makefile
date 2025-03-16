CXX = g++
INCLUDE_OPENCV = `pkg-config --cflags --libs opencv4`
LINK_PTHREAD = -lpthread

CLIENT = client.cpp
SERVER = server.cpp
OPEN_CV = openCV.cpp
PTHREAD = pthread.c
CLI = client
SER = server

all: server client
  
server: $(SERVER)
	$(CXX) $(SERVER) -o $(SER) $(INCLUDE_OPENCV)
client: $(CLIENT)
	$(CXX) $(CLIENT) -o $(CLI) $(INCLUDE_OPENCV)

.PHONY: clean

clean:
	rm $(CLI) $(SER)
