#include <stdio.h>
#include <string.h>

#include "functions.h"

#include <jsmn.h>
#include <jsmn-find.h>

#include <concord/discord.h>
#include <concord/discord-internal.h>
#include <concord/websockets.h>

#include <sqlite3.h>

char lavalinkSecure = false; // If its using SSL or not.
char lavalinkNodeUrl[64] = "example.com"; // If it doesn't use SSL, replace the code below with http instead of https & ws instead of wss.
char lavalinkNodePassword[64] = "youshallnotpass";
char totalShards[64] = "1"; // Default.
char botId[18] = "BOT_ID_HERE";

static struct websockets *g_ws;

void on_connect(void *data, struct websockets *ws, struct ws_info *info, const char *protocols) {
  (void)data; (void)ws; (void)info; (void)protocols;
  log_trace("[LAVALINK] Lavalink connected with success.");
}

void on_close(void *data, struct websockets *ws, struct ws_info *info, enum ws_close_reason wscode, const char *reason, size_t len) {
  (void)data; (void) ws; (void) info;
  log_fatal("[LAVALINK] Error in the LavaLink connection [%i] because of \"%s\". ( %i bytes )", wscode, reason, len);
}

void default_config(struct ua_conn *conn, void *data) {
  (void) data;
  ua_conn_add_header(conn, "Authorization", lavalinkNodePassword);
  ua_conn_add_header(conn, "Client-Name", "MusicBotWithConcord");
  ua_conn_add_header(conn, "User-Agent", "libcurl");
  ua_conn_add_header(conn, "Cleanup-Threshold", "600");
}

void done_channel_find(struct discord *client, void *data, const struct discord_channel *channel) {
  (void) data;
  struct discord_embed embed[] = {
    {
      .description = "<a:yes:757568594841305149> | The music has ended.",
      .color = 15615
    }
  };

  struct discord_create_message params = {
    .flags = 0,
    .embeds =
      &(struct discord_embeds){
        .size = 1,
        .array = embed,
      },
  };

  discord_create_message(client, channel->id, &params, NULL);
}

void on_text(void *data, struct websockets *ws, struct ws_info *info, const char *text, size_t len) {
  (void)info; (void)ws;
  struct discord *client = data;

  jsmnf *root = jsmnf_init();
  jsmnf_start(root, text, len);

  jsmnf *op = jsmnf_find(root, "op", strlen("op"));

  if (!op) {
    log_error("[JSMN_FIND] Error while trying to find op field.");

    jsmnf_cleanup(root);

    return;
  }

  char payloadOp[24], payloadType[24], guildId[24];

  snprintf(payloadOp, sizeof(payloadOp), "%.*s", op->val->end - op->val->start, text + op->val->start);

  if (0 == strcmp(payloadOp, "event")) {
    jsmnf *type = jsmnf_find(root, "type", strlen("type"));

    if (!type) {
      log_error("[JSMN_FIND] Error while trying to find type field.");
      
      jsmnf_cleanup(root);

      return;
    }

    snprintf(payloadType, sizeof(payloadType), "%.*s", type->val->end - type->val->start, text + type->val->start);
    if (0 == strcmp(payloadType, "TrackEndEvent")) {
      jsmnf *guild = jsmnf_find(root, "guildId", strlen("guildId"));

      if (!guild) {
        log_error("[JSMN_FIND] Error while trying to find guild field.");

        jsmnf_cleanup(root);

        return;
      }

      snprintf(guildId, sizeof(guildId), "%.*s", guild->val->end - guild->val->start, text + guild->val->start);

      sqlite3 *db = NULL;
      sqlite3_stmt *stmt = NULL;
      char *query = NULL;
      int rc = sqlite3_open("database.db", &db);

      query = sqlite3_mprintf("SELECT guild_id, channel_id FROM guild_channel WHERE guild_id = %s;", guildId);
      rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);

      if ((rc = sqlite3_step(stmt)) != SQLITE_DONE) {
        sqlite3_free(query);

        discord_get_channel(client, sqlite3_column_int64(stmt, 1), &(struct discord_ret_channel){
                                                                     .done = &done_channel_find,
                                                                     .fail = NULL,
                                                                     .data = NULL
                                                                    });

        query = sqlite3_mprintf("DELETE FROM guild_channel WHERE guild_id = %s;", guildId);
        sqlite3_exec(db, query, NULL, NULL, NULL);
      }

      sqlite3_finalize(stmt);
      sqlite3_close(db);
      sqlite3_free(query);
    }
  }
  jsmnf_cleanup(root);
}

CCORDcode discord_custom_run(struct discord *client) {
  struct ws_callbacks cbs = {
    .on_text = &on_text,
    .on_connect = &on_connect,
    .on_close = &on_close,
    .data = client
  };

  CURLM *mhandle = curl_multi_init();
  g_ws = ws_init(&cbs, mhandle, NULL);
  
  char url[128];
  if (lavalinkSecure == true) snprintf(url, sizeof(url), "wss://%s", lavalinkNodeUrl);
  else snprintf(url, sizeof(url), "ws://%s", lavalinkNodeUrl);

  ws_set_url(g_ws, url, NULL);

  ws_start(g_ws);

  ws_add_header(g_ws, "Authorization", lavalinkNodePassword);
  ws_add_header(g_ws, "Num-Shards", totalShards);
  ws_add_header(g_ws, "User-Id", botId);
  ws_add_header(g_ws, "Client-Name", "MusicBotWithConcord");

  uint64_t tstamp;

  int64_t next_gateway_run, now;
  CCORDcode code;

  while (1) {
    if (CCORD_OK != (code = discord_gateway_start(&client->gw))) break;

    next_gateway_run = cog_timestamp_ms();
    while (1) {
      now = cog_timestamp_ms();
      int poll_time = 0;
      if (!client->on_idle) {
        poll_time = now < next_gateway_run ? next_gateway_run - now : 0;
        if (-1 != client->wakeup_timer.next)
          if (client->wakeup_timer.next <= now + poll_time)
            poll_time = client->wakeup_timer.next - now;
      }

      int poll_result = io_poller_poll(client->io_poller, poll_time);
      if (-1 == poll_result) {
        //TODO: handle poll error here
      } else if (0 == poll_result) {
        if (client->on_idle)
          client->on_idle(client);
      }

      if (client->on_cycle)
          client->on_cycle(client);

      if (CCORD_OK != (code = io_poller_perform(client->io_poller))) break;

      now = cog_timestamp_ms();
      if (client->wakeup_timer.next != -1) {
        if (now >= client->wakeup_timer.next) {
          client->wakeup_timer.next = -1;
          if (client->wakeup_timer.cb)
            client->wakeup_timer.cb(client);
        }
      }
      if (next_gateway_run <= now) {
        if (CCORD_OK != (code = discord_gateway_perform(&client->gw))) break;

        next_gateway_run = now + 1000;
      }
      ws_easy_run(g_ws, 5, &tstamp);
    }

    if (true == discord_gateway_end(&client->gw)) {
      discord_adapter_stop_all(&client->adapter);
      break;
    }
  }

  ws_end(g_ws);

  ws_cleanup(g_ws);
  curl_multi_cleanup(mhandle);

  return code;
}

void playMusic(const char *track, unsigned long int guildId) {
  char pJ[1024];

  snprintf(pJ, sizeof(pJ), "{\"op\":\"play\",\"guildId\":\"%"PRIu64"\",\"track\":\"%s\",\"noReplace\":\"false\",\"pause\":\"false\"}", guildId, track);
  ws_send_text(g_ws, NULL, pJ, strlen(pJ));
}

void voiceUpdate(const char *allEvent, unsigned long int guildId, const char *sessionId) {
  char pJ[1024];

  snprintf(pJ, sizeof(pJ), "{\"op\":\"voiceUpdate\",\"guildId\":\"%"PRIu64"\",\"sessionId\":\"%s\",\"event\":%s}", guildId, sessionId, allEvent);
  ws_send_text(g_ws, NULL, pJ, strlen(pJ));
}

void setMusicVolume(const char *volume, unsigned long int guildId) {
  char pJ[1024];

  snprintf(pJ, sizeof(pJ), "{\"op\":\"volume\",\"guildId\":\"%"PRIu64"\",\"volume\": %s}", guildId, volume);
  ws_send_text(g_ws, NULL, pJ, strlen(pJ));
}
