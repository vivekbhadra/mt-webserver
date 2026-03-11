CXX      = g++
CXXFLAGS = -std=c++17 -O2 -pthread -Wall

.PHONY: all clean run up down logs build restart shell console

# ── Local build (no Docker) ──────────────────
all:
	$(CXX) $(CXXFLAGS) -o server src/server.cpp

clean:
	rm -f server

run: all
	./server

# ── Docker Compose targets ───────────────────
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
