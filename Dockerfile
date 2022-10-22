FROM ubuntu:20.04

HEALTHCHECK NONE

RUN useradd -d /home/discordbot -m -s /bin/bash discordbot && apt update && apt -y install curl libsqlite3-dev build-essential wget libcurl4-openssl-dev libssl-dev git clang && cd /home/discordbot && git clone https://github.com/Cogmasters/concord.git -b dev && cd concord && make -j8 && make install

USER discordbot

COPY . .

RUN clang main.c -o /home/discordbot/ConcordBot -Wall -Wextra -Werror -O2 -ldiscord -lcurl -lsqlite3 -pthread

RUN chmod +x /home/discordbot/ConcordBot

CMD [ "/home/discordbot/ConcordBot" ]
