# What's the "Music Discord bot with C"?

A light-weight music Discord bot using Concord.

# How to download and use it?

There are currently two ways of using it. Compiling the source code and using it, or using the pre-compiled version

## Compiling by yourself

* 1 - Use `git clone https://github.com/ThePedroo/C-Music-Discord-Bot.git` to get the bot files.
* 2 - Install clang / GCC.
* 3 - Install and build [Concord](https://github.com/Cogmasters/concord).
* Info. Don't have a [Lavalink](https://github.com/freyacodes/Lavalink) node? Setup it on [replit.com](https://replit.com/).
* 4 - Install sqlite3: `sudo apt install sqlite3`

If you did everything correct, use: 
```
make && ./ConcordBot
```

Done, now you will have a compiled version of the music bot for your architecture. :)

## Pre-compiled 

* 1 - Go to Github releases and download the pre-compiled file that matches your CPU architecute, after it runs it.

Done! Yeah, it's that simple to use it. ^^

## Using the bot

You can start using it by using `.play [music]`, and to see more commands you can use `.help`.
  
# Information
 
 * Project owner Discord tag: `Pedro.js#9446`
 * Concord owner: `müller#1001`
 * Concord support server: [Discord](https://discord.gg/YcaK3puy49)
 * lib used for make requests: [libcURL](https://curl.se/libcurl/c/)
 * library for websockets: [Concord](https://github.com/Cogmasters/concord)
 * JSON parse lib: [jsmn-find](https://github.com/lcsmuller/jsmn-find)
