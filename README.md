# What's the "Music Discord bot with C"?

A light-weight music Discord bot using Orca for it's bot.

It's easy to use and install.

# How to download and use it?

1 - Use git clone https://github.com/ThePedroo/C-Music-Discord-Bot.git

2 - Install and build [Orca](https://github.com/cee-studio/orca).

3 - Install clang / GCC.

4 - Edit config.json with your TOKEN.

5 - Fill Lavalink Node informations in main.c

6 - info - Doesn't have a [Lavalink](https://github.com/freyacodes/Lavalink) node? Setup it on [replit.com](https://replit.com/).
  
## Before we continue...

The bot use sqlite3 for save the channelId that the user is in it, so you will need to install sqlite3 with:
```bash
apt install sqlite3
```

7 - If you did everything correct, use: 
```
make && ./main
```

8 - The bot is probably online, use ?play <music> and feel free to listen to your favorite musics.
  
# Information
 
 Project owner Discord tag: `Pedro.js#9446`

 Orca owners: `müller#1001` / `Icon from asterranaut (Solancia)#4228` / `segfault#8223`

 Orca support server: [Click here](https://discord.gg/9cHUyCc7rs)
  
 lib used for make requests: [Orca](https://github.com/cee-studio/orca)/[libcURL](https://curl.se/libcurl/c/)

 library for websockets: [Orca](https://github.com/cee-studio/orca)

 JSON parse lib: [cJSON](https://github.com/DaveGamble/cJSON)
