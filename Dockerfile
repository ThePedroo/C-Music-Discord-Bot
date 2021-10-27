FROM ubuntu:20.04

HEALTHCHECK NONE

RUN useradd -d /home/discordbot -m -s /bin/bash discordbot && apt-get update && apt-get install curl sqlite3 build-essential wget libcurl4-openssl-dev libssl-dev git --yes && git clone https://github.com/cee-studio/orca.git && cd orca && make && make install

USER discordbot

COPY . .

CMD [ "clang" "main.c" "-o" "main.out" "-Wall" "-Wextra" "-Werror" "-g" "-pthread" "-ldiscord" "-lcurl" "-lcrypto" "-lpthread" "-lm" "-lsqlite3" "&&" "./main.out" ]
