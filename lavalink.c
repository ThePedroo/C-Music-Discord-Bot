#include "lavalink.h"

#include <stdio.h>
#include <string.h>

#include <concord/discord.h>
#include <concord/discord-internal.h> // All ws functions related

#include <concord/jsmn.h> // Jsmnf functions
#include <concord/jsmn-find.h> // Jsmnf functions

#include <sqlite3.h> // Sqlite3 (db)

static struct websockets *g_ws;

// WEBSOCKET CALLBACKS

void on_connect(void *data, struct websockets *ws, struct ws_info *info, const char *protocols) {
  (void)data; (void)ws; (void)info; (void)protocols;
  log_trace("[LAVALINK] Lavalink connected with success.");
}

void on_close(void *data, struct websockets *ws, struct ws_info *info, enum ws_close_reason wscode, const char *reason, size_t len) {
  (void)data; (void) ws; (void) info; (void) len;
  log_fatal("[LAVALINK] Failed to connect to Lavalink node. [%i] [%s]", wscode, reason);
}

void on_cycle(struct discord *client) {
  (void) *client;
  uint64_t tstamp;
  ws_easy_run(g_ws, 5, &tstamp);
}

void on_text(void *data, struct websockets *ws, struct ws_info *info, const char *text, size_t len) {
  (void) ws; (void) info;

  jsmn_parser parser;
  jsmntok_t tokens[1024];

  jsmn_init(&parser);
  int r = jsmn_parse(&parser, text, len, tokens, sizeof(tokens));

  if (r < 0) {
    log_error("[JSMNF] Failed to parse JSON.");
    return;
  }

  jsmnf_loader loader;
  jsmnf_pair pairs[1024];

  jsmnf_init(&loader);
  r = jsmnf_load(&loader, text, tokens, parser.toknext, pairs, 1024);

  if (r < 0) {
    log_error("[JSMNF] Failed to load JSMNF.");
    return;
  }

  jsmnf_pair *op = jsmnf_find(pairs, text, "op", strlen("op"));

  if (!op) {
    log_error("[JSMNF] Failed to find op.");
    return;
  }

  char Op[16];
  snprintf(Op, sizeof(Op), "%.*s", (int)op->v.len, text + op->v.pos);

  if (0 == strcmp(Op, "event")) {
    jsmnf_pair *type = jsmnf_find(pairs, text, "type", strlen("type"));

    if (!type) {
      log_error("[JSMNF] Failed to find type.");
      return;
    }

    char Type[16];
    snprintf(Type, sizeof(Type), "%.*s", (int)type->v.len, text + type->v.pos);

    if (0 == strcmp(Type, "TrackEndEvent")) {
      jsmnf_pair *guildId = jsmnf_find(pairs, text, "guildId", strlen("guildId"));

      sqlite3 *db;
      int rc = sqlite3_open("db.sqlite", &db);

      if (rc != SQLITE_OK) {
        log_fatal("[SQLITE] Error when opening the db file. [%s]", sqlite3_errmsg(db));
        if (sqlite3_close(db) != SQLITE_OK) {
          log_fatal("[SQLITE] Failed to close sqlite db. [%s]", sqlite3_errmsg(db));
        }
        return;
      }

      char *query = NULL;
      sqlite3_stmt *stmt = NULL;

      query = sqlite3_mprintf("SELECT channel_id FROM guild_channels WHERE guild_id = %.*s;", (int)guildId->v.len, text + guildId->v.pos);
      rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);

      sqlite3_free(query);

      if ((rc = sqlite3_step(stmt)) != SQLITE_ROW) {
        log_error("[SQLITE] Error while trying to access guild_channels table. [%s]", sqlite3_errmsg(db));
        if (sqlite3_close(db) != SQLITE_OK) {
          log_fatal("[SQLITE] Failed to close sqlite db. [%s]", sqlite3_errmsg(db));
        }
        return;
      } else {
        struct discord_embed embed[] = {
          {
            .description = "The music has ended.",
            .image =
              &(struct discord_embed_image){
                .url = "https://raw.githubusercontent.com/Cogmasters/concord/master/docs/static/social-preview.png",
              },
            .footer =
              &(struct discord_embed_footer){
                .text = "Powered by Concord",
                .icon_url = "https://raw.githubusercontent.com/Cogmasters/concord/master/docs/static/concord-small.png",
              },
            .timestamp = discord_timestamp(data),
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

        discord_create_message(data, sqlite3_column_int64(stmt, 0), &params, NULL);
      }
      
      if (sqlite3_finalize(stmt) != SQLITE_OK) {
        log_fatal("[SQLITE] Error while executing function sqlite3_finalize.");
        return;
      }
      if (sqlite3_close(db) != SQLITE_OK) {
        log_fatal("[SQLITE] Failed to close sqlite db. [%s]", sqlite3_errmsg(db));
      }
    }
  }
}

// WEBSOCKET CALLBACKS

enum discord_event_scheduler scheduler(struct discord *client, const char data[], size_t size, enum discord_gateway_events event) {
  switch (event) {
    case DISCORD_EV_VOICE_STATE_UPDATE: {
      sqlite3 *db;
      int rc = sqlite3_open("db.sqlite", &db);

      if (rc != SQLITE_OK) {
        log_fatal("[SQLITE] Error when opening the db file. [%s]", sqlite3_errmsg(db));
        if (sqlite3_close(db) != SQLITE_OK) {
          log_fatal("[SQLITE] Failed to close sqlite db. [%s]", sqlite3_errmsg(db));
        }
        return DISCORD_EVENT_IGNORE;
      }

      char *msgErr = NULL;

      char *query = sqlite3_mprintf("CREATE TABLE IF NOT EXISTS user_voice(guild_id INT, user_id INT, voice_channel_id INT);");
      rc = sqlite3_exec(db, query, NULL, NULL, &msgErr);

      sqlite3_free(query);

      if (rc != SQLITE_OK) {
        log_fatal("[SYSTEM] Something went wrong while creating user_voice table. [%s]", msgErr);
        if (sqlite3_close(db) != SQLITE_OK) {
          log_fatal("[SQLITE] Failed to close sqlite db. [%s]", sqlite3_errmsg(db));
        }
        return DISCORD_EVENT_IGNORE;
      }

      jsmn_parser parser;
      jsmntok_t tokens[256];

      jsmn_init(&parser);
      int r = jsmn_parse(&parser, data, size, tokens, sizeof(tokens));

      if (r < 0) {
        log_error("[JSMNF] Failed to parse JSON.");
        if (sqlite3_close(db) != SQLITE_OK) {
          log_fatal("[SQLITE] Failed to close sqlite db. [%s]", sqlite3_errmsg(db));
        }
        return DISCORD_EVENT_IGNORE;
      }

      jsmnf_loader loader;
      jsmnf_pair pairs[128];

      jsmnf_init(&loader);
      r = jsmnf_load(&loader, data, tokens, parser.toknext, pairs, 128);

      if (r < 0) {
        log_error("[JSMNF] Failed to load JSMNF.");
        if (sqlite3_close(db) != SQLITE_OK) {
          log_fatal("[SQLITE] Failed to close sqlite db. [%s]", sqlite3_errmsg(db));
        }
        return DISCORD_EVENT_IGNORE;
      }

      jsmnf_pair *VGI = jsmnf_find(pairs, data, "guild_id", strlen("guild_id"));

      if (!VGI) {
        log_error("[JSMNF] Failed to find guild_id.");
        if (sqlite3_close(db) != SQLITE_OK) {
          log_fatal("[SQLITE] Failed to close sqlite db. [%s]", sqlite3_errmsg(db));
        }
        return DISCORD_EVENT_IGNORE;
      }

      jsmnf_pair *VUI = jsmnf_find(pairs, data, "user_id", strlen("user_id"));

      if (!VUI) {
        log_error("[JSMNF] Failed to find user_id.");
        if (sqlite3_close(db) != SQLITE_OK) {
          log_fatal("[SQLITE] Failed to close sqlite db. [%s]", sqlite3_errmsg(db));
        }
        return DISCORD_EVENT_IGNORE;
      }

      jsmnf_pair *VCI = jsmnf_find(pairs, data, "channel_id", strlen("channel_id"));

      char channel_id[32];

      if (!VCI) {
        log_error("[JSMNF] Failed to find channel_id.");
        if (sqlite3_close(db) != SQLITE_OK) {
          log_fatal("[SQLITE] Failed to close sqlite db. [%s]", sqlite3_errmsg(db));
        }
        return DISCORD_EVENT_IGNORE;
      } else {
        snprintf(channel_id, sizeof(channel_id), "%.*s", (int)VCI->v.len, data + VCI->v.pos);
        if (0 == strcmp(channel_id, "null")) {
          log_debug("[SYSTEM] Someone left a voice channel, deleting user_voice table from the user if exists.");
          query = sqlite3_mprintf("DELETE FROM user_voice WHERE user_id = %.*s;", (int)VUI->v.len, data + VUI->v.pos);
          rc = sqlite3_exec(db, query, NULL, NULL, &msgErr);

          if (rc != SQLITE_OK) {
            log_fatal("[SYSTEM] Something went wrong while deleting user_voice table from user_id. [%s]", msgErr);
            if (sqlite3_close(db) != SQLITE_OK) {
              log_fatal("[SQLITE] Failed to close sqlite db. [%s]", sqlite3_errmsg(db));
            }
            return DISCORD_EVENT_IGNORE;
          } else {
            log_debug("[SYSTEM] Deleted user_voice where user_id = %.*s.", (int)VUI->v.len, data + VUI->v.pos);
          }

          sqlite3_free(query);

          return DISCORD_EVENT_IGNORE;
        }
      }

      jsmnf_pair *SSI = jsmnf_find(pairs, data, "session_id", strlen("session_id"));

      if (!SSI) {
        log_error("[JSMNF] Failed to find session_id.");
        if (sqlite3_close(db) != SQLITE_OK) {
          log_fatal("[SQLITE] Failed to close sqlite db. [%s]", sqlite3_errmsg(db));
        }
        return DISCORD_EVENT_IGNORE;
      }

      const struct discord_user *bot = discord_get_self(client);

      // WILL NEED UPDATE
      char userID[32]; char botID[32];
      snprintf(userID, sizeof(userID), "%"PRIu64"", bot->id);
      snprintf(botID, sizeof(botID), "%.*s", (int)VUI->v.len, data + VUI->v.pos);
      // WILL NEED UPDATE

      if (0 == strcmp(userID, botID)) {
        log_debug("[SYSTEM] The bot joined the voice channel. Going to save its session_id.");

        char *query = sqlite3_mprintf("CREATE TABLE IF NOT EXISTS guild_voice(guild_id INT, voice_channel_id INT, session_id TEXT);");
        rc = sqlite3_exec(db, query, NULL, NULL, &msgErr);

        sqlite3_free(query);

        if (rc != SQLITE_OK) {
          log_fatal("[SYSTEM] Something went wrong while creating table guild_voice. [%s]", msgErr);
          if (sqlite3_close(db) != SQLITE_OK) {
            log_fatal("[SQLITE] Failed to close sqlite db. [%s]", sqlite3_errmsg(db));
          }
          return DISCORD_EVENT_IGNORE;
        }

        query = sqlite3_mprintf("DELETE FROM guild_voice WHERE guild_id = %.*s;", (int)VGI->v.len, data + VGI->v.pos);
        rc = sqlite3_exec(db, query, NULL, NULL, &msgErr);

        if (rc != SQLITE_OK) {
          log_fatal("[SYSTEM] Something went wrong while deleting guild_voice table from guild_id. [%s]", msgErr);
          if (sqlite3_close(db) != SQLITE_OK) {
            log_fatal("[SQLITE] Failed to close sqlite db. [%s]", sqlite3_errmsg(db));
          }
          return DISCORD_EVENT_IGNORE;
        } else {
          log_debug("[SYSTEM] Deleted guild_voice where guild_id = %.*s.", (int)VGI->v.len, data + VGI->v.pos);
        }

        sqlite3_free(query);

        query = sqlite3_mprintf("INSERT INTO guild_voice(guild_id, voice_channel_id, session_id) values(%.*s, %.*s, \"%.*s\");", (int)VGI->v.len, data + VGI->v.pos, (int)VCI->v.len, data + VCI->v.pos, (int)SSI->v.len, data + SSI->v.pos);
        rc = sqlite3_exec(db, query, NULL, NULL, &msgErr);

        sqlite3_free(query);

        if (rc != SQLITE_OK) {
          log_fatal("[SQLITE] Something went wrong while inserting values into guild_voice table. [%s]", msgErr);
          if (sqlite3_close(db) != SQLITE_OK) {
            log_fatal("[SQLITE] Failed to close sqlite db. [%s]", sqlite3_errmsg(db));
          }
          return DISCORD_EVENT_IGNORE;
        } else {
          log_debug("[SQLITE] Inserted into guild_voice table the values: guild_id: %.*s, voice_channel_id: %.*s, session_id: \"%.*s\".", (int)VGI->v.len, data + VGI->v.pos, (int)VCI->v.len, data + VCI->v.pos, (int)SSI->v.len, data + SSI->v.pos);
        }
        if (sqlite3_close(db) != SQLITE_OK) {
          log_fatal("[SQLITE] Failed to close sqlite db. [%s]", sqlite3_errmsg(db));
        }
        return DISCORD_EVENT_IGNORE;
      }

      if (VCI) {
        log_debug("[SYSTEM] Voice channel found, adding all informations into database. [guild_id: %.*s, user_id: %.*s, voice_channel_id: %.*s]", (int)VGI->v.len, data + VGI->v.pos, (int)VUI->v.len, data + VUI->v.pos, (int)VCI->v.len, data + VCI->v.pos);

        query = sqlite3_mprintf("DELETE FROM user_voice WHERE user_id = %.*s;", (int)VUI->v.len, data + VUI->v.pos);
        rc = sqlite3_exec(db, query, NULL, NULL, &msgErr);

        if (rc != SQLITE_OK) {
          log_fatal("[SYSTEM] Something went wrong while deleting user_voice table from user_id. [%s]", msgErr);
          if (sqlite3_close(db) != SQLITE_OK) {
            log_fatal("[SQLITE] Failed to close sqlite db. [%s]", sqlite3_errmsg(db));
          }
          return DISCORD_EVENT_IGNORE;
        } else {
          log_debug("[SYSTEM] Deleted user_voice where user_id = %.*s.", (int)VUI->v.len, data + VUI->v.pos);
        }

        sqlite3_free(query);

        query = sqlite3_mprintf("INSERT INTO user_voice(guild_id, user_id, voice_channel_id) values(%.*s, %.*s, %.*s);", (int)VGI->v.len, data + VGI->v.pos, (int)VUI->v.len, data + VUI->v.pos, (int)VCI->v.len, data + VCI->v.pos);
        rc = sqlite3_exec(db, query, NULL, NULL, &msgErr);

        sqlite3_free(query);

        if (rc != SQLITE_OK) {
          log_fatal("[SQLITE] Something went wrong while inserting values into user_voice table. [%s]", msgErr);
          if (sqlite3_close(db) != SQLITE_OK) {
            log_fatal("[SQLITE] Failed to close sqlite db. [%s]", sqlite3_errmsg(db));
          }
          return DISCORD_EVENT_IGNORE;
        } else {
          log_debug("[SQLITE] Inserted into user_voice the values: guild_id: %.*s, user_id: %.*s, voice_channel_id: %.*s.", (int)VGI->v.len, data + VGI->v.pos, (int)VUI->v.len, data + VUI->v.pos, (int)VCI->v.len, data + VCI->v.pos);
        }

        if (sqlite3_close(db) != SQLITE_OK) {
          log_fatal("[SQLITE] Failed to close sqlite db. [%s]", sqlite3_errmsg(db));
        }
      } else {
        log_fatal("[JSMNF] Unable to find a VCI. [%s]", data);
        if (sqlite3_close(db) != SQLITE_OK) {
          log_fatal("[SQLITE] Failed to close sqlite db. [%s]", sqlite3_errmsg(db));
        }
        return DISCORD_EVENT_IGNORE;
      }
    } return DISCORD_EVENT_IGNORE;
    case DISCORD_EV_VOICE_SERVER_UPDATE: {
      sqlite3 *db;
      int rc = sqlite3_open("db.sqlite", &db);

      if (rc != SQLITE_OK) {
        log_fatal("[SQLITE] Error when opening the db file. [%s]", sqlite3_errmsg(db));
        if (sqlite3_close(db) != SQLITE_OK) {
          log_fatal("[SQLITE] Failed to close sqlite db. [%s]", sqlite3_errmsg(db));
        }
        return DISCORD_EVENT_IGNORE;
      }

      jsmn_parser parser;
      jsmntok_t tokens[256];

      jsmn_init(&parser);
      int r = jsmn_parse(&parser, data, size, tokens, sizeof(tokens));

      if (r < 0) {
        log_error("[JSMNF] Failed to parse JSON.");
        if (sqlite3_close(db) != SQLITE_OK) {
          log_fatal("[SQLITE] Failed to close sqlite db. [%s]", sqlite3_errmsg(db));
        }
        return DISCORD_EVENT_IGNORE;
      }

      jsmnf_loader loader;
      jsmnf_pair pairs[128];

      jsmnf_init(&loader);
      r = jsmnf_load(&loader, data, tokens, parser.toknext, pairs, 128);

      if (r < 0) {
        log_error("[JSMNF] Failed to load JSMNF.");
        if (sqlite3_close(db) != SQLITE_OK) {
          log_fatal("[SQLITE] Failed to close sqlite db. [%s]", sqlite3_errmsg(db));
        }
        return DISCORD_EVENT_IGNORE;
      }

      jsmnf_pair *VGI = jsmnf_find(pairs, data, "guild_id", strlen("guild_id"));

      if (!VGI) {
        log_error("[JSMNF] Failed to find guild_id.");
        if (sqlite3_close(db) != SQLITE_OK) {
          log_fatal("[SQLITE] Failed to close sqlite db. [%s]", sqlite3_errmsg(db));
        }
        return DISCORD_EVENT_IGNORE;
      }

      char *query = NULL;
      sqlite3_stmt *stmt = NULL;

      query = sqlite3_mprintf("SELECT session_id FROM guild_voice WHERE guild_id = %.*s;", (int)VGI->v.len, data + VGI->v.pos);
      rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);

      sqlite3_free(query);

      if ((rc = sqlite3_step(stmt)) != SQLITE_ROW) {
        log_fatal("[SQLITE] Error while trying to access guild_voice table. [%s]", sqlite3_errmsg(db));
        if (sqlite3_close(db) != SQLITE_OK) {
          log_fatal("[SQLITE] Failed to close sqlite db. [%s]", sqlite3_errmsg(db));
        }
        return DISCORD_EVENT_IGNORE;
      } else {
        char VUP[1024];

        snprintf(VUP, sizeof(VUP), "{\"op\":\"voiceUpdate\",\"guildId\":\"%.*s\",\"sessionId\":\"%s\",\"event\":%.*s}", (int)VGI->v.len, data + VGI->v.pos, sqlite3_column_text(stmt, 0), (int)size, data);

        if (sqlite3_finalize(stmt) != SQLITE_OK) {
          log_fatal("[SQLITE] Error while executing function sqlite3_finalize.");
          return DISCORD_EVENT_IGNORE;
        }

        if (ws_send_text(g_ws, NULL, VUP, strlen(VUP)) == false) {
          log_fatal("[LIBCURL] Failed to send a voiceUpdate payload to Lavalink.");
          return DISCORD_EVENT_IGNORE;
        } else {
          log_debug("[LIBCURL] Sent with success a voiceUpdate payload to Lavalink.");
        }
      }
      if (sqlite3_close(db) != SQLITE_OK) {
        log_fatal("[SQLITE] Failed to close sqlite db. [%s]", sqlite3_errmsg(db));
      }
    } return DISCORD_EVENT_IGNORE;
    default:
      return DISCORD_EVENT_MAIN_THREAD;
  }
}

void sendPayload(char payload[], char *payloadOP) {
  if (ws_send_text(g_ws, NULL, payload, strlen(payload)) == false) {
    log_fatal("[LIBCURL] Something went wrong while sending a payload with op %s to Lavalink.", payloadOP);
    return;
  } else {
    log_debug("[LIBCURL] Sucessfully sent a payload with op %s to Lavalink.", payloadOP);
  }
}
