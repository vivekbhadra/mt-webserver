FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential \
    g++ \
    make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY src/server.cpp .

RUN g++ -std=c++17 -O2 -pthread -o server server.cpp

EXPOSE 8080

CMD ["./server"]
