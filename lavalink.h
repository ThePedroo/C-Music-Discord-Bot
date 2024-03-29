#ifndef LAVALINK_H
#define LAVALINK_H

static struct websockets *g_ws;

void on_connect(void *data, struct websockets *ws, struct ws_info *info, const char *protocols);

void on_close(void *data, struct websockets *ws, struct ws_info *info, enum ws_close_reason wscode, const char *reason, size_t len);

void on_cycle(struct discord *client);

void on_text(void *data, struct websockets *ws, struct ws_info *info, const char *text, size_t len);

enum discord_event_scheduler scheduler(struct discord *client, const char data[], size_t size, enum discord_gateway_events event);

void sendPayload(char payload[], char *payloadOP);

PGconn *connectDB(struct discord *client);

int _PQresultStatus(PGconn *conn, PGresult *res, char *action, char *msgDone);

#endif