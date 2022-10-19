FROM ubuntu:20.04

HEALTHCHECK NONE

RUN useradd -d /home/discordbot -m -s /bin/bash discordbot && apt-get update && apt-get install curl sqlite3 build-essential wget libcurl4-openssl-dev libssl-dev git --yes && git clone https://github.com/Cogmasters/concord.git -b dev && cd concord && make && make install

USER discordbot

COPY . .

CMD [ "clang" "main.c" "-o" "ConcordBot" "-Wall" "-Wextra" "-Werror" "-g" "-ldiscord" "-lcurl" "-lsqlite3" "-pthread" "-I/usr/include/postgresql" "lpq" "&&" "./ConcordBot" ]
