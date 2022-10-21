#include "lavalink.h"

#include <stdio.h>
#include <string.h>

#include <concord/discord.h>
#include <concord/discord-internal.h> // All ws functions related

#include <concord/jsmn.h> // Jsmnf functions
#include <concord/jsmn-find.h> // Jsmnf functions

#include <sqlite3.h> // Sqlite3 (db)
#include <libpq-fe.h>

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
  (void)ws; (void)info;

  jsmn_parser parser;
  jsmntok_t tokens[1024];

  jsmn_init(&parser);
  int r = jsmn_parse(&parser, text, len, tokens, sizeof(tokens));

  if (r < 0) {
    log_error("[jsmn-find] Failed to parse JSON.");
    return;
  }

  jsmnf_loader loader;
  jsmnf_pair pairs[1024];

  jsmnf_init(&loader);
  r = jsmnf_load(&loader, text, tokens, parser.toknext, pairs, 1024);

  if (r < 0) {
    log_error("[jsmn-find] Failed to load jsmn-find.");
    return;
  }

  jsmnf_pair *op = jsmnf_find(pairs, text, "op", 2);

  if (!op) {
    log_error("[jsmn-find] Failed to find op.");
    return;
  }

  char Op[16];
  snprintf(Op, sizeof(Op), "%.*s", (int)op->v.len, text + op->v.pos);

  if (0 == strcmp(Op, "event")) {
    jsmnf_pair *type = jsmnf_find(pairs, text, "type", 4);

    if (!type) {
      log_error("[jsmn-find] Failed to find type.");
      return;
    }

    char Type[16];
    snprintf(Type, sizeof(Type), "%.*s", (int)type->v.len, text + type->v.pos);

    if (0 == strcmp(Type, "TrackEndEvent")) {
      jsmnf_pair *guildId = jsmnf_find(pairs, text, "guildId", 7);

      PGconn *conn = connectDB(data);
      if (!conn) return;

      char command[2048];
      snprintf(command, sizeof(command), "SELECT channel_id FROM guild_channels WHERE guild_id = %.*s;", (int)guildId->v.len, text + guildId->v.pos);

      PGresult *res = PQexec(conn, command);

      int resultCode = _PQresultStatus(conn, res, "trying to retrieve the channel_id", NULL);
      if (!resultCode) return;

      unsigned long long channelID = strtoull(PQgetvalue(res, 0, 0), NULL, 10);

      PQclear(res);

      snprintf(command, sizeof(command), "SELECT \"array\" FROM guild_queue WHERE guild_id = %.*s;", (int)guildId->v.len, text + guildId->v.pos);

      res = PQexec(conn, command);

      resultCode = _PQresultStatus(conn, res, "trying to retrieve the array", NULL);
      if (!resultCode) return;

      char *arrQueue = PQgetvalue(res, 0, 0);

      jsmn_parser parser;
      jsmntok_t tokens[1024];

      jsmn_init(&parser);
      int r = jsmn_parse(&parser, arrQueue, strlen(arrQueue), tokens, sizeof(tokens));

      if (r < 0) {
        log_error("[jsmn-find] Failed to parse JSON.");
        return;
      }

      jsmnf_loader loader;
      jsmnf_pair pairs[1024];

      jsmnf_init(&loader);
      r = jsmnf_load(&loader, arrQueue, tokens, parser.toknext, pairs, 1024);

      if (r < 0) {
        log_error("[jsmn-find] Failed to load jsmn-find.");
        return;
      }

      jsonb b;
      char qbuf[1024];

      jsonb_init(&b);
      jsonb_array(&b, qbuf, sizeof(qbuf));

      if (pairs->size == 1) {
        PQclear(res);

        snprintf(command, sizeof(command), "DELETE FROM guild_queue WHERE guild_id = %.*s;", (int)guildId->v.len, text + guildId->v.pos);

        res = PQexec(conn, command);

        int resultCode = _PQresultStatus(conn, res, "deleting guild_queue table where guild_id matches the guild's Id", "Successfully deleted guild_queue table from guild_id = Guild ID.");
        if (!resultCode) return;

        PQclear(res);

        snprintf(command, sizeof(command), "DELETE FROM guild_channels WHERE guild_id = %.*s;", (int)guildId->v.len, text + guildId->v.pos);

        res = PQexec(conn, command);

        resultCode = _PQresultStatus(conn, res, "deleting guild_channels table where guild_id matches the guild's Id", "Successfully deleted guild_channels table from guild_id = Guild ID.");
        if (!resultCode) return;

        PQclear(res);

        snprintf(command, sizeof(command), "DELETE FROM guild_voice WHERE guild_id = %.*s;", (int)guildId->v.len, text + guildId->v.pos);

        res = PQexec(conn, command);

        resultCode = _PQresultStatus(conn, res, "deleting guild_voice table where guild_id matches the guild's Id", "Successfully deleted guild_voice table from guild_id = Guild ID.");
        if (!resultCode) return;

        PQclear(res);

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

        discord_create_message(data, channelID, &params, NULL);
        return;
      }
      jsmnf_pair *f = &pairs->fields[1];

      char pJ[512];
      snprintf(pJ, sizeof(pJ), "{\"op\":\"play\",\"guildId\":\"%.*s\",\"track\":\"%.*s\"}", (int)guildId->v.len, text + guildId->v.pos, (int)f->v.len, arrQueue + f->v.pos);

      PQclear(res);

      for (int i = 1; i < pairs->size; ++i) {
        f = &pairs->fields[i];
        char arrayTrack[256];
        snprintf(arrayTrack, sizeof(arrayTrack), "%.*s", (int)f->v.len, arrQueue + f->v.pos);
        jsonb_string(&b, qbuf, sizeof(qbuf), arrayTrack, strlen(arrayTrack));
      }

      jsonb_array_pop(&b, qbuf, sizeof(qbuf));

      snprintf(command, sizeof(command), "DELETE FROM guild_queue WHERE guild_id = %.*s;", (int)guildId->v.len, text + guildId->v.pos);

      res = PQexec(conn, command);

      resultCode = _PQresultStatus(conn, res, "deleting guild_queue table where guild_id matches the guild's Id", "Successfully deleted guild_queue table from guild_id = Guild ID.");
      if (!resultCode) return;

      PQclear(res);

      snprintf(command, sizeof(command), "INSERT INTO guild_queue(guild_id, \"array\") values(%.*s, '%s');", (int)guildId->v.len, text + guildId->v.pos, qbuf);

      res = PQexec(conn, command);

      resultCode = _PQresultStatus(conn, res, "inserting records into guild_queue table", "Successfully inserted records into the guild_queue table.");
      if (!resultCode) return;

      PQclear(res);

      struct discord_embed embed[] = {
        {
          .description = "Now, going to play next music from queue.",
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

      discord_create_message(data, channelID, &params, NULL);

      PQfinish(conn);
    }
  }
}

// WEBSOCKET CALLBACKS

enum discord_event_scheduler scheduler(struct discord *client, const char data[], size_t size, enum discord_gateway_events event) {
  switch (event) {
    case DISCORD_EV_VOICE_STATE_UPDATE: {
      PGconn *conn = connectDB(client);
      if (!conn) return DISCORD_EVENT_IGNORE;

      jsmn_parser parser;
      jsmntok_t tokens[256];

      jsmn_init(&parser);
      int r = jsmn_parse(&parser, data, size, tokens, sizeof(tokens));

      if (r < 0) {
        log_error("[jsmn-find] Failed to parse JSON.");
        PQfinish(conn);
        return DISCORD_EVENT_IGNORE;
      }

      jsmnf_loader loader;
      jsmnf_pair pairs[128];

      jsmnf_init(&loader);
      r = jsmnf_load(&loader, data, tokens, parser.toknext, pairs, 128);

      if (r < 0) {
        log_error("[jsmn-find] Failed to load jsmn-find.");
        PQfinish(conn);
        return DISCORD_EVENT_IGNORE;
      }

      jsmnf_pair *VGI = jsmnf_find(pairs, data, "guild_id", 8);

      if (!VGI) {
        log_error("[jsmn-find] Failed to find guild_id.");
        PQfinish(conn);
        return DISCORD_EVENT_IGNORE;
      }

      jsmnf_pair *VUI = jsmnf_find(pairs, data, "user_id", 7);

      if (!VUI) {
        log_error("[jsmn-find] Failed to find user_id.");
        PQfinish(conn);
        return DISCORD_EVENT_IGNORE;
      }

      // WILL NEED UPDATE
      const struct discord_user *bot = discord_get_self(client);

      char userID[32]; char botID[32];
      snprintf(userID, sizeof(userID), "%"PRIu64"", bot->id);
      snprintf(botID, sizeof(botID), "%.*s", (int)VUI->v.len, data + VUI->v.pos);
      // WILL NEED UPDATE

      jsmnf_pair *VCI = jsmnf_find(pairs, data, "channel_id", 10);

      char channel_id[32];

      if (!VCI) {
        log_error("[jsmn-find] Failed to find channel_id.");
        PQfinish(conn);
        return DISCORD_EVENT_IGNORE;
      } else {  
        snprintf(channel_id, sizeof(channel_id), "%.*s", (int)VCI->v.len, data + VCI->v.pos);

        if (0 == strcmp(userID, botID) && 0 == strcmp(channel_id, "null")) {
          log_debug("[SYSTEM] The bot left the voice channel. Going to delete its session_id.");

          char command[128];
          snprintf(command, sizeof(command), "DELETE FROM guild_voice WHERE guild_id = %.*s;", (int)VGI->v.len, data + VGI->v.pos);

          PGresult *res = PQexec(conn, command);

          int resultCode = _PQresultStatus(conn, res, "deleting guild_voice table where guild_id matches the guild's Id", "Successfully deleted guild_voice table from guild_id = Voice Channel Guild ID.");
          if (!resultCode) return DISCORD_EVENT_IGNORE;

          PQclear(res);
          PQfinish(conn);

          return DISCORD_EVENT_IGNORE;
        }

        if (0 == strcmp(channel_id, "null")) {
          PGresult *res = PQexec(conn, "CREATE TABLE IF NOT EXISTS user_voice(user_id BIGINT UNIQUE NOT NULL, voice_channel_id BIGINT NOT NULL);");

          int resultCode = _PQresultStatus(conn, res, "creating user_voice table", NULL);
          if (!resultCode) return DISCORD_EVENT_IGNORE;

          PQclear(res);

          log_debug("[SYSTEM] Someone left a voice channel, deleting user_voice table from the user if exists.");

          char command[128];
          snprintf(command, sizeof(command), "DELETE FROM user_voice WHERE user_id = %.*s;", (int)VUI->v.len, data + VUI->v.pos);

          res = PQexec(conn, command);

          resultCode = _PQresultStatus(conn, res, "deleting user_voice table where user_id matches the user's Id", "Successfully deleted user_voice table from user_id = Voice Channel User ID.");
          if (!resultCode) return DISCORD_EVENT_IGNORE;
  
          PQclear(res);
          PQfinish(conn);

          return DISCORD_EVENT_IGNORE;
        }
      }

      jsmnf_pair *SSI = jsmnf_find(pairs, data, "session_id", 10);

      if (!SSI) {
        log_error("[jsmn-find] Failed to find session_id.");
        PQfinish(conn);
        return DISCORD_EVENT_IGNORE;
      }

      if (0 == strcmp(userID, botID)) {
        log_debug("[SYSTEM] The bot joined the voice channel. Going to save its session_id.");

        PGresult *res = PQexec(conn, "CREATE TABLE IF NOT EXISTS guild_voice(guild_id BIGINT UNIQUE NOT NULL, voice_channel_id BIGINT UNIQUE NOT NULL, session_id TEXT UNIQUE NOT NULL);");

        int resultCode = _PQresultStatus(conn, res, "creating table guild_voice", NULL);
        if (!resultCode) return DISCORD_EVENT_IGNORE;

        PQclear(res);

        char command[256];
        snprintf(command, sizeof(command), "DELETE FROM guild_voice WHERE guild_id = %.*s;", (int)VGI->v.len, data + VGI->v.pos);

        res = PQexec(conn, command);

        resultCode = _PQresultStatus(conn, res, "deleting guild_voice table where guild_id matches the guild's Id", "Deleted guild_voice where guild_id = Voice Channel Guild ID.");
        if (!resultCode) return DISCORD_EVENT_IGNORE;

        PQclear(res);

        snprintf(command, sizeof(command), "INSERT INTO guild_voice(guild_id, voice_channel_id, session_id) values(%.*s, %.*s, \'%.*s\');", (int)VGI->v.len, data + VGI->v.pos, (int)VCI->v.len, data + VCI->v.pos, (int)SSI->v.len, data + SSI->v.pos);

        res = PQexec(conn, command);

        resultCode = _PQresultStatus(conn, res, "inserting records into guild_voice table", "Successfully inserted records into the guild_voice table.");
        if (!resultCode) return DISCORD_EVENT_IGNORE;

        PQclear(res);
        PQfinish(conn);

        return DISCORD_EVENT_IGNORE;
      }

      if (VCI) {
        log_debug("[SYSTEM] Voice channel found, adding all informations into database. [guild_id: %.*s, user_id: %.*s, voice_channel_id: %.*s]", (int)VGI->v.len, data + VGI->v.pos, (int)VUI->v.len, data + VUI->v.pos, (int)VCI->v.len, data + VCI->v.pos);

        PGresult *res = PQexec(conn, "CREATE TABLE IF NOT EXISTS user_voice(user_id BIGINT UNIQUE NOT NULL, voice_channel_id BIGINT NOT NULL);");

        int resultCode = _PQresultStatus(conn, res, "creating user_voice table", NULL);
        if (!resultCode) return DISCORD_EVENT_IGNORE;

        PQclear(res);

        char command[256];
        snprintf(command, sizeof(command), "DELETE FROM user_voice WHERE user_id = %.*s;", (int)VUI->v.len, data + VUI->v.pos);

        res = PQexec(conn, command);

        resultCode = _PQresultStatus(conn, res, "deleting user_voice table where user_id matches the users's Id", "Successfully deleted user_voice table from user_id = Voice Channel User ID.");
        if (!resultCode) return DISCORD_EVENT_IGNORE;

        PQclear(res);

        snprintf(command, sizeof(command), "INSERT INTO user_voice(user_id, voice_channel_id) values(%.*s, %.*s);",(int)VUI->v.len, data + VUI->v.pos, (int)VCI->v.len, data + VCI->v.pos);

        res = PQexec(conn, command);

        resultCode = _PQresultStatus(conn, res, "inserting records into user_voice table", "Successfully inserted records into the user_voice table.");
        if (!resultCode) return DISCORD_EVENT_IGNORE;

        PQclear(res);
        PQfinish(conn);

      } else {
        log_fatal("[jsmn-find] Unable to find a VCI. [%s]", data);
        PQfinish(conn);
        return DISCORD_EVENT_IGNORE;
      }
    } return DISCORD_EVENT_IGNORE;
    case DISCORD_EV_VOICE_SERVER_UPDATE: {
      PGconn *conn = connectDB(client);
      if (!conn) return DISCORD_EVENT_IGNORE;

      jsmn_parser parser;
      jsmntok_t tokens[256];

      jsmn_init(&parser);
      int r = jsmn_parse(&parser, data, size, tokens, sizeof(tokens));

      if (r < 0) {
        log_error("[jsmn-find] Failed to parse JSON.");
        PQfinish(conn);
        return DISCORD_EVENT_IGNORE;
      }

      jsmnf_loader loader;
      jsmnf_pair pairs[128];

      jsmnf_init(&loader);
      r = jsmnf_load(&loader, data, tokens, parser.toknext, pairs, 128);

      if (r < 0) {
        log_error("[jsmn-find] Failed to load jsmn-find.");
        PQfinish(conn);
        return DISCORD_EVENT_IGNORE;
      }

      jsmnf_pair *VGI = jsmnf_find(pairs, data, "guild_id", 8);

      if (!VGI) {
        log_error("[jsmn-find] Failed to find guild_id.");
        PQfinish(conn);
        return DISCORD_EVENT_IGNORE;
      }

      char command[128];
      snprintf(command, sizeof(command), "SELECT session_id FROM guild_voice WHERE guild_id = %.*s;", (int)VGI->v.len, data + VGI->v.pos);

      PGresult *res = PQexec(conn, command);

      int resultCode = _PQresultStatus(conn, res, "access guild_voice table", NULL);
      if (!resultCode) return DISCORD_EVENT_IGNORE;

      char *sessionId = PQgetvalue(res, 0, 0);

      char VUP[1024];
      snprintf(VUP, sizeof(VUP), "{\"op\":\"voiceUpdate\",\"guildId\":\"%.*s\",\"sessionId\":\"%s\",\"event\":%.*s}", (int)VGI->v.len, data + VGI->v.pos, sessionId, (int)size, data);

      sendPayload(VUP, "voiceUpdate");

      PQclear(res);
      PQfinish(conn);
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

PGconn *connectDB(struct discord *client) {
  char connInfo[512];
  struct ccord_szbuf_readonly value = discord_config_get_field(client, (char *[2]){ "postgresql", "connInfo" }, 2);
  snprintf(connInfo, sizeof(connInfo), "%.*s", (int)value.size, value.start);

  PGconn *conn = PQconnectdb(connInfo);

  switch (PQstatus(conn)) {
    case CONNECTION_OK:
    case CONNECTION_MADE:
      log_trace("[libpq] Successfully connected to the postgres database server.");
      break;
    case CONNECTION_STARTED:
      log_trace("[libpq] Waiting for connection to be made.");
      break;
    case CONNECTION_AWAITING_RESPONSE:
      log_trace("[libpq] Waiting for a response from the server.");
      break;
    case CONNECTION_AUTH_OK:
      log_trace("[libpq] Received authentication; waiting for backend start-up to finish.");
      break;
    case CONNECTION_SSL_STARTUP:
      log_trace("[libpq] Almost connecting; negotiating SSL encryption.");
      break;
    case CONNECTION_SETENV:
      log_trace("[libpq] Almost connecting; negotiating environment-driven parameter settings.");
      break;

    default:
      log_fatal("[libpq] Error when trying to connect to the postgres database server. [%s]\n", PQerrorMessage(conn));
      PQfinish(conn);
      return NULL;
  }

  return conn;
}

int _PQresultStatus(PGconn *conn, PGresult *res, char *action, char *msgDone) {
  int resStatus = 0;
  switch (PQresultStatus(res)) {
    case PGRES_EMPTY_QUERY:
      log_fatal("[libpq] Error while %s, the string sent to the postgresql database server was empty.", action);
      PQclear(res);
      PQfinish(conn);
      break;
    case PGRES_TUPLES_OK:
    case PGRES_COMMAND_OK:
      if (msgDone) log_trace("[libpq]: %s", msgDone);
      resStatus = 1;
      break;
    case PGRES_BAD_RESPONSE:
      log_fatal("[libpq] Error while %s, the postgresql database server's response was not understood.", action);
      PQclear(res);
      PQfinish(conn);
      break;
    case PGRES_NONFATAL_ERROR:
      log_warn("[libpq] Warning or a notice while %s");
      resStatus = 1;
      break;
    case PGRES_FATAL_ERROR:
      log_fatal("[libpq] Error while %s.\n%s", action, PQresultErrorMessage(res));
      PQclear(res);
      PQfinish(conn);
      break;

    default:
      log_warn("[SYSTEM] A resultStatus is unknown for the system; setting it as \"no errors\". [PQresultStatus(res) = %d]", PQresultStatus(res));
      break;
  }
  return resStatus;
}
