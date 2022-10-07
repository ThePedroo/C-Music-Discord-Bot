# What's the "Music Discord bot with C"?

A light-weight music Discord bot using Concord.

# How to download and use it?

* 1 - Use git clone https://github.com/ThePedroo/C-Music-Discord-Bot.git
* 2 - Install clang / GCC.
* 3 - Install and build [Concord](https://github.com/Cogmasters/concord).
* 4 - Edit config.json with your TOKEN.
* 5 - Fill Lavalink Node informations in main.c
* Info. Dont't have a [Lavalink](https://github.com/freyacodes/Lavalink) node? Setup it on [replit.com](https://replit.com/).
* 6 - Install sqlite3: `sudo apt install sqlite3`

If you did everything correct, use: 
```
make && ./ConcordBot
```

The bot is probably online, use .play <music> and feel free to listen to your favorite songs.
  
# Information
 
 * Project owner Discord tag: `Pedro.js#9446`
 * Concord owner: `müller#1001`
 * lib used for make requests: [libcURL](https://curl.se/libcurl/c/)
 * library for websockets: [Concord](https://github.com/Cogmasters/concord)
 * JSON parse lib: [jsmn-find](https://github.com/lcsmuller/jsmn-find)
