CXX      = g++
CXXFLAGS = -std=c++17 -O2 -pthread -Wall

.PHONY: all clean run dev up down logs build restart shell

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

dev:
	docker compose up --build --attach server   # only stream server logs

down:
	docker compose down

logs:
	docker compose logs -f

restart:
	docker compose restart server

shell:
	docker compose exec server /bin/bash

# ── Console access (for console thread) ──────
console:
	docker attach mt-webserver
