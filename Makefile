CXX      = g++
CXXFLAGS = -std=c++17 -O2 -pthread -Wall

TARGET = server
SRC    = src/server.cpp

.PHONY: all clean run docker-build docker-run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

run: all
	./$(TARGET)

# Docker targets
docker-build:
	docker build -t mt-webserver .

docker-run:
	docker run -it --rm -p 8080:8080 mt-webserver
