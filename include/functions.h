#ifndef ADD_H
#define ADD_H

#include <concord/discord.h>
#include <concord/websockets.h>
#include <concord/user-agent.h>

struct context {
  char session_id[64];
};

void on_connect(void *data, struct websockets *ws, struct ws_info *info, const char *protocols);

void on_close(void *data, struct websockets *ws, struct ws_info *info, enum ws_close_reason wscode, const char *reason, size_t len);

void on_ready(struct discord *client);

void default_config(struct ua_conn *conn, void *data);

void on_text(void *data, struct websockets *ws, struct ws_info *info, const char *text, size_t len);

CCORDcode discord_custom_run(struct discord *client);

void playMusic(const char *track, long unsigned int guildId);

void voiceUpdate(const char *allEvent, long unsigned int guildId, const char *sessionId);

void setMusicVolume(const char *volume, long unsigned int guildId);

#endif
