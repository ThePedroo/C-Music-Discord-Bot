# What's the "Music Discord bot with C"?

A light-weight music Discord bot using Orca for it's bot.

It's easy to use and install.

# How to download and use it?

1 - Use git clone https://github.com/ThePedroo/C-Music-Discord-Bot.git

2 - Install and build [Orca](https://github.com/cee-studio/orca).

3 - Install clang / GCC.

4 - Edit config.json with you're TOKEN.

5 - Fill the URL of the Lavalinknode

5 - info - Doesn't have a [Lavalink](https://github.com/freyacodes/Lavalink) node? Setup it on [replit.com](https://replit.com/).
  
6 - If you did everything correct, use:
```bash
clang main.c -o main.out -Wall -Wextra -Werror -g -pthread -ldiscord -lcurl -lcrypto -lpthread -lm -lsqlite3 && ./main.out
```
or
```bash
gcc main.c -o main.out -Wall -Wextra -Werror -g -pthread -ldiscord -lcurl -lcrypto -lpthread -lm -lsqlite3 && ./main.out
```

7 - The bot is probably online, use !play <music> and feel free to listen to your favorite musics.
  
# Information
 
 Project owner Discord tag: `Pedro.js#9446`

 Orca owner: `müller#1001` / `Icon from asterranaut (Solancia)#4228` / `segfault#8223`

 Orca support server: [Click here](https://discord.gg/9cHUyCc7rs)
  
 lib used for make requests: [Orca](https://github.com/cee-studio/orca)/[libcURL](https://curl.se/libcurl/c/)

 library for websockets: [Orca](https://github.com/cee-studio/orca)

 JSON parse lib: [cJSON](https://github.com/DaveGamble/cJSON)
