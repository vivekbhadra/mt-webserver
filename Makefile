CXX      = g++
CXXFLAGS = -std=c++17 -O2 -pthread -Wall
SRCS     = src/main.cpp src/ConnectionManager.cpp src/RequestHandler.cpp \
           src/ConsoleThread.cpp src/WebServer.cpp
INCS     = -I src

.PHONY: all clean run up down logs build restart shell console

all:
	$(CXX) $(CXXFLAGS) $(SRCS) $(INCS) -o server

clean:
	rm -f server

run: all
	./server

build:
	docker compose build

up:
	docker compose up --build

down:
	docker compose down

logs:
	docker compose logs -f

restart:
	docker compose restart server

shell:
	docker compose exec server /bin/bash

console:
	docker attach mt-webserver
