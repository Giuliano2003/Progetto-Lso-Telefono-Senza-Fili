FROM debian:stable-slim

RUN apt-get update && \
    apt-get install -y build-essential libcurl4-openssl-dev curl libglib2.0-dev uuid-dev pkg-config sqlite3 libsqlite3-dev && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY translator.c .
COPY translator.h .
COPY server.c .
COPY Makefile .
COPY wait-for-libretranslate.sh .

RUN make

RUN chmod +x wait-for-libretranslate.sh

EXPOSE 8080

CMD ["./wait-for-libretranslate.sh", "./server.out"]
