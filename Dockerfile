# ─────────────────────────────────────────────
# Stage 1: Builder
# ─────────────────────────────────────────────
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential \
    g++ \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY src/ .

RUN g++ -std=c++17 -O2 -pthread -Wall \
    main.cpp ConnectionManager.cpp RequestHandler.cpp \
    ConsoleThread.cpp WebServer.cpp \
    -I . -o server

# ─────────────────────────────────────────────
# Stage 2: Runtime (minimal image)
# ─────────────────────────────────────────────
FROM ubuntu:22.04 AS runtime

RUN groupadd -r appgroup && useradd -r -g appgroup appuser

WORKDIR /app
COPY --from=builder /build/server .
RUN chown appuser:appgroup /app/server

USER appuser

EXPOSE 8080

HEALTHCHECK --interval=10s --timeout=3s --start-period=5s --retries=3 \
    CMD bash -c 'echo > /dev/tcp/localhost/8080' || exit 1

CMD ["./server"]
