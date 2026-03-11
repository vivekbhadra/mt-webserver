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
	docker compose build # Build the Docker image

up:
	docker compose up --build # Start the server in the foreground

down:
	docker compose down   # Stop and remove containers

logs:
	docker compose logs -f  # Follow logs in real-time

restart:
	docker compose restart server  # Restart the server container

shell:
	docker compose exec server /bin/bash  # Get a shell inside the server container

console:
	docker attach mt-webserver   # Attach to the server container's console (use Ctrl+C to detach)
