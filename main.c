#include <stdio.h>
#include <string.h>
#include <signal.h> // sigint

#include <sys/resource.h> // Required for .botinfo

#include <concord/jsmn.h> // Jsmnf functions
#include <concord/jsmn-find.h> // Jsmnf functions

#include <concord/discord.h>
#include <concord/discord-internal.h> // All ws functions related

#include <sqlite3.h> // Sqlite3 (db)

struct discord *client;
static struct websockets *g_ws;

char lavalinkNodeURL[128] = "example.com"; // Remember, if the Lavalink node does not have SSL, please change in line 1007 and 277 wss:// to ws:// and https:// to http://
char lavalinkNodePasswd[128] = "SomeWeirdAndNotSecurePasswd";
char botID[32] = "1231231232132311321";

struct memory {
  char *body;
  size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;
  struct memory *mem = (struct memory *)userp;

  char *ptr = realloc(mem->body, mem->size + realsize + 1);
  if (!ptr) {
    log_fatal("[SYSTEM] Not enough memory to realloc.\n");
    return 0;
  }

  mem->body = ptr;
  memcpy(&(mem->body[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->body[mem->size] = 0;

  return realsize;
}

void on_ready(struct discord *client, const struct discord_ready *event) {
  (void) event;

  const struct discord_user *bot = discord_get_self(client);
  log_trace("[DISCORD_GATEWAY] Logged in as %s#%s!", bot->username, bot->discriminator);
}

void on_message(struct discord *client, const struct discord_message *message) {
  if (message->author->bot) return;
  if (0 == strncmp(message->content, ".volume ", 8)) {
    char *volume = message->content + strlen(".volume ");

    if (!volume) {
      struct discord_embed embed[] = {
        {
          .description = "Sorry, you must put a volume number after the command.",
          .image =
            &(struct discord_embed_image){
              .url = "https://raw.githubusercontent.com/Cogmasters/concord/master/docs/static/social-preview.png",
            },
          .footer =
            &(struct discord_embed_footer){
              .text = "Powered by Concord",
              .icon_url = "https://raw.githubusercontent.com/Cogmasters/concord/master/docs/static/concord-small.png",
            },
          .timestamp = discord_timestamp(client),
          .color = 16711680
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

      discord_create_message(client, message->channel_id, &params, NULL);
      return;
    }

    char pJ[128];

    snprintf(pJ, sizeof(pJ), "{\"op\":\"volume\",\"guildId\":\"%"PRIu64"\",\"volume\": %s}", message->guild_id, volume);

    if (ws_send_text(g_ws, NULL, pJ, strlen(pJ)) == false) {
      log_fatal("[LIBCURL] Something went wrong while sending a payload with op volume to Lavalink.");
      return;
    } else {
      log_debug("[LIBCURL] Sucessfully sent the payload with op volume to Lavalink.");
    }

    struct discord_embed embed[] = {
      {
        .description = "Okay, the volume will be changed in some seconds.",
        .image =
          &(struct discord_embed_image){
            .url = "https://raw.githubusercontent.com/Cogmasters/concord/master/docs/static/social-preview.png",
          },
        .footer =
          &(struct discord_embed_footer){
            .text = "Powered by Concord",
            .icon_url = "https://raw.githubusercontent.com/Cogmasters/concord/master/docs/static/concord-small.png",
          },
        .timestamp = discord_timestamp(client),
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

    discord_create_message(client, message->channel_id, &params, NULL);
  }
  if (0 == strcmp(message->content, ".botinfo")) {
    struct rusage usage;

    getrusage(RUSAGE_SELF, &usage);

    char architecture[16] = "32"; char description[256];

    if (sizeof(void *) == 8) snprintf(architecture, sizeof(architecture), "64");

    snprintf(description, sizeof(description), "Howdy, I'm a bot made with the `C` language, using `Concord` Discord library.\n\nProgramming language: `C`\nCompiler: `Unknown`\nRAM usage: %limb\nArchitecture: %sbits", usage.ru_maxrss / 1000, architecture);

    struct discord_embed embed[] = {
      {
        .description = description,
        .image =
          &(struct discord_embed_image){
            .url = "https://raw.githubusercontent.com/Cogmasters/concord/master/docs/static/social-preview.png",
          },
        .footer =
          &(struct discord_embed_footer){
            .text = "Powered by Concord",
            .icon_url = "https://raw.githubusercontent.com/Cogmasters/concord/master/docs/static/concord-small.png",
          },
        .timestamp = discord_timestamp(client),
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

    discord_create_message(client, message->channel_id, &params, NULL);
  }
  if (0 == strncmp(".play ", message->content, 6)) {
    char *music = message->content + strlen(".play ");

    if (!music) {
      struct discord_embed embed[] = {
        {
          .description = "Sorry, you must put a music name after the command.",
          .image =
            &(struct discord_embed_image){
              .url = "https://raw.githubusercontent.com/Cogmasters/concord/master/docs/static/social-preview.png",
            },
          .footer =
            &(struct discord_embed_footer){
              .text = "Powered by Concord",
              .icon_url = "https://raw.githubusercontent.com/Cogmasters/concord/master/docs/static/concord-small.png",
            },
          .timestamp = discord_timestamp(client),
          .color = 16711680
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

      discord_create_message(client, message->channel_id, &params, NULL);
    }

    sqlite3 *db;
    int rc = sqlite3_open("db.sqlite", &db);

    if (rc != SQLITE_OK) {
      log_fatal("[SQLITE] Error when opening the db file. [%s]", sqlite3_errmsg(db));
      
      if (sqlite3_close(db) != SQLITE_OK) {
        log_fatal("[SQLITE] Failed to close sqlite db. [%s]", sqlite3_errmsg(db));
      }
      return;
    }

    sqlite3_stmt *stmt = NULL;

    char *query = sqlite3_mprintf("SELECT voice_channel_id FROM user_voice WHERE user_id = %"PRIu64";", message->author->id);
    rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);

    if ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
      sqlite3_free(query);

      char joinVCPayload[128];
      snprintf(joinVCPayload, sizeof(joinVCPayload), "{\"op\":4,\"d\":{\"guild_id\":%"PRIu64",\"channel_id\":\"%lld\",\"self_mute\":false,\"self_deaf\":true}}", message->guild_id, sqlite3_column_int64(stmt, 0));

      if (sqlite3_finalize(stmt) != SQLITE_OK) {
        log_fatal("[SQLITE] Error while executing function sqlite3_finalize.");
        return;
      }

      if (ws_send_text(client->gw.ws, NULL, joinVCPayload, strlen(joinVCPayload)) == false) {
        log_fatal("[LIBCURL] Something went wrong while sending a payload with op 4 to Discord.");
        return;
      } else {
        log_debug("[LIBCURL] Sucessfully sent the payload with op 4 to Discord.");
      }

      curl_global_init(CURL_GLOBAL_ALL);

      CURL *curl = curl_easy_init();
      char *musicSearchEncoded = curl_easy_escape(curl, music, strlen(music));

      if (!musicSearchEncoded) {
        log_fatal("[LIBCURL] Failed to escape the music search URL strings.");

        if (sqlite3_close(db) != SQLITE_OK) {
          log_fatal("[SQLITE] Failed to close sqlite db. [%s]", sqlite3_errmsg(db));
        }

        struct discord_embed embed[] = {
          {
            .description = "Something went wrong while escaping music search URL strings. Please try again.",
            .image =
              &(struct discord_embed_image){
                .url = "https://raw.githubusercontent.com/Cogmasters/concord/master/docs/static/social-preview.png",
              },
            .footer =
              &(struct discord_embed_footer){
                .text = "Powered by Concord",
                .icon_url = "https://raw.githubusercontent.com/Cogmasters/concord/master/docs/static/concord-small.png",
              },
            .timestamp = discord_timestamp(client),
            .color = 16711680
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

        discord_create_message(client, message->channel_id, &params, NULL);

        return;
      }

      char lavaURL[1024];
  
      snprintf(lavaURL, sizeof(lavaURL), "https://%s/loadtracks?identifier=", lavalinkNodeURL);

      if (0 == strncmp(music, "https://", strlen("https://"))) {
        strncat(lavaURL, musicSearchEncoded, sizeof(lavaURL) - 1);
      } else {
        strncat(lavaURL, "ytsearch:", sizeof(lavaURL) - 1);
        strncat(lavaURL, musicSearchEncoded, sizeof(lavaURL) - 1);
      }

      curl_free(musicSearchEncoded);

      log_debug("[LAVALINK] Going to load tracks for search \"%s\".", music);

      if (!curl) {
        log_fatal("[LIBCURL] Error while initializing libcurl.");
        return;
      } else {
        CURLcode res;
        struct memory req = { 0 };

        req.body = malloc(1);
        req.size = 0;

        res = curl_easy_setopt(curl, CURLOPT_URL, lavaURL);

        if (res != CURLE_OK) {
          log_fatal("[LIBCURL] curl_easy_setopt [1] failed: %s\n", curl_easy_strerror(res));
          return;
        }
        
        char AuthorizationH[256];
        snprintf(AuthorizationH, sizeof(AuthorizationH), "Authorization: %s", lavalinkNodePasswd);

        struct curl_slist *chunk = NULL;
        chunk = curl_slist_append(chunk, AuthorizationH);
        chunk = curl_slist_append(chunk, "Client-Name: MusicBotWithConcord");
        chunk = curl_slist_append(chunk, "User-Agent: libcurl");
        chunk = curl_slist_append(chunk, "Cleanup-Threshold: 600");

        res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
        if (res != CURLE_OK) {
          log_fatal("[LIBCURL] curl_easy_setopt [2] failed: %s\n", curl_easy_strerror(res));
          return;
        }

        res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        if (res != CURLE_OK) {
          log_fatal("[LIBCURL] curl_easy_setopt [3] failed: %s\n", curl_easy_strerror(res));
          return;
        }

        res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&req);
        if (res != CURLE_OK) {
          log_fatal("[LIBCURL] curl_easy_setopt [4] failed: %s\n", curl_easy_strerror(res));
          return;
        }

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
          log_fatal("[LIBCURL] curl_easy_perform failed: %s\n", curl_easy_strerror(res));
          return;
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(chunk);
        curl_global_cleanup();

        jsmn_parser parser;
        jsmntok_t tokens[1024];

        jsmn_init(&parser);
        int r = jsmn_parse(&parser, req.body, req.size, tokens, sizeof(tokens));

        if (r < 0) {
          log_error("[JSMNF] Failed to parse JSON.");
          return;
        }

        jsmnf_loader loader;
        jsmnf_pair pairs[1024];

        jsmnf_init(&loader);
        r = jsmnf_load(&loader, req.body, tokens, parser.toknext, pairs, 1024);

        if (r < 0) {
          log_error("[JSMNF] Failed to load JSMNF.");
          return;
        }

        jsmnf_pair *lT= jsmnf_find(pairs, req.body, "loadType", strlen("loadType"));

        char loadType[16];
        snprintf(loadType, sizeof(loadType), "%.*s", (int)lT->v.len, req.body + lT->v.pos);

        if (0 != strcmp(loadType, "SEARCH_RESULT")) {
          // Error and playing handling
        }

        char *path[] = { "tracks", "0", "track", NULL };
        jsmnf_pair *t = jsmnf_find_path(pairs, req.body, path, 3);

        path[2] = "info";
        path[3] = "title";
        jsmnf_pair *ti = jsmnf_find_path(pairs, req.body, path, 4);

        path[3] = "uri";
        jsmnf_pair *u = jsmnf_find_path(pairs, req.body, path, 4);

        path[3] = "author";
        jsmnf_pair *a = jsmnf_find_path(pairs, req.body, path, 4);

        path[3] = "length";
        jsmnf_pair *l = jsmnf_find_path(pairs, req.body, path, 4);

        if (!t || !ti || !u || !a || !l) {
          log_fatal("[JSMN_FIND] Error while trying to find %s field.", !t ? "track" : !ti ? "title": !u ? "uri" : !a ? "author" : !l ? "length" : "404");
          return;
        }

        char title[128], track[512], url[64], author[32], length[24];

        snprintf(track, sizeof(track), "%.*s", (int)t->v.len, req.body + t->v.pos);
        snprintf(title, sizeof(title), "%.*s", (int)ti->v.len, req.body + ti->v.pos);

        r = jsmnf_unescape(title, sizeof(title), title, ti->v.len);

        if (r < 0) {
          log_error("[JSMNF] Failed to parse JSON.");
          return;
        }

        snprintf(url, sizeof(url), "%.*s", (int)u->v.len, req.body + u->v.pos);
        snprintf(author, sizeof(author), "%.*s", (int)a->v.len, req.body + a->v.pos);
        snprintf(length, sizeof(length), "%.*s", (int)l->v.len, req.body + l->v.pos);

        free(req.body);

        char descriptionEmbed[512];

        float lengthFloat = atof(length);
        int rounded = (int)(lengthFloat < 0 ? (lengthFloat - 0.5) : (lengthFloat + 0.5));

        if ((rounded / 1000) % 60 > 10) {
          snprintf(descriptionEmbed, sizeof(descriptionEmbed), "Ok, playing the song NOW!\n:person_tipping_hand: | Author: `%s`\n:musical_note: | Name: `%s`\n:stopwatch:  | Time: `%d:%d`", author, title, ((int)(lengthFloat < 0 ? (lengthFloat - 0.5) : (lengthFloat + 0.5)) / 1000 / 60) << 0, (rounded / 1000) % 60);
        } else {
          snprintf(descriptionEmbed, sizeof(descriptionEmbed), "Ok, playing the song NOW!!\n:person_tipping_hand:  | Author: `%s`\n:musical_note: | Name: `%s`\n:stopwatch:  | Time: `%d:0%d`", author, title, ((int)(lengthFloat < 0 ? (lengthFloat - 0.5) : (lengthFloat + 0.5)) / 1000 / 60) << 0, (rounded / 1000) % 60);
        }

        struct discord_embed embed[] = {
           {
            .title = title,
            .url = url,
            .description = descriptionEmbed,
            .footer =
              &(struct discord_embed_footer){
                .text = "Powered by Concord",
                .icon_url = "https://raw.githubusercontent.com/Cogmasters/concord/master/docs/static/concord-small.png",
              },
            .timestamp = discord_timestamp(client),
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

        discord_create_message(client, message->channel_id, &params, NULL);

        char pJ[1024];

        snprintf(pJ, sizeof(pJ), "{\"op\":\"play\",\"guildId\":\"%"PRIu64"\",\"track\":\"%s\",\"noReplace\":false,\"pause\":false}", message->guild_id, track);

        if (ws_send_text(g_ws, NULL, pJ, strlen(pJ)) == false) {
          log_fatal("[LIBCURL] Something went wrong while sending a payload with op play to Lavalink.");
          return;
        } else {
          log_debug("[LIBCURL] Sucessfully sent a payload with op play to Lavalink.");
        }

        char *msgErr = NULL;

        char *query = sqlite3_mprintf("CREATE TABLE IF NOT EXISTS guild_channels(guild_id INT, channel_id);");
        rc = sqlite3_exec(db, query, NULL, NULL, &msgErr);

        sqlite3_free(query);

        if (rc != SQLITE_OK) {
          log_fatal("[SYSTEM] Something went wrong while creating the guild_channels table. [%s]", msgErr);
          if (sqlite3_close(db) != SQLITE_OK) {
            log_fatal("[SQLITE] Failed to close sqlite db. [%s]", sqlite3_errmsg(db));
          }
          return;
        }

        query = sqlite3_mprintf("INSERT INTO guild_channels(guild_id, channel_id) values(%"PRIu64", %"PRIu64");", message->guild_id, message->channel_id);
        rc = sqlite3_exec(db, query, NULL, NULL, &msgErr);

        sqlite3_free(query);

        if (rc != SQLITE_OK) {
          log_fatal("[SQLITE] Something went wrong while inserting values into guild_channels table. [%s]", msgErr);
          if (sqlite3_close(db) != SQLITE_OK) {
            log_fatal("[SQLITE] Failed to close sqlite db. [%s]", sqlite3_errmsg(db));
          }
          return;
        } else {
          log_debug("[SQLITE] Inserted into guild_channels table the values: guild_id: %"PRIu64", channel_id: %"PRIu64".", message->guild_id, message->channel_id);
        }

        if (sqlite3_close(db) != SQLITE_OK) {
          log_fatal("[SQLITE] Failed to close sqlite db. [%s]", sqlite3_errmsg(db));
        }
      }
    } else {
      if (sqlite3_finalize(stmt) != SQLITE_OK) {
        log_fatal("[SQLITE] Error while executing function sqlite3_finalize.");
        return;
      }
      sqlite3_free(query);
      if (sqlite3_close(db) != SQLITE_OK) {
        log_fatal("[SQLITE] Failed to close sqlite db. [%s]", sqlite3_errmsg(db));
      }

      struct discord_embed embed[] = {
        {
          .description = "You are not in a voice channel.",
          .image =
            &(struct discord_embed_image){
              .url = "https://raw.githubusercontent.com/Cogmasters/concord/master/docs/static/social-preview.png",
            },
          .footer =
            &(struct discord_embed_footer){
              .text = "Powered by Concord",
              .icon_url = "https://raw.githubusercontent.com/Cogmasters/concord/master/docs/static/concord-small.png",
            },
          .timestamp = discord_timestamp(client),
          .color = 16711680
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

      discord_create_message(client, message->channel_id, &params, NULL);

      return;
    }
  }
}

// WEBSOCKET CALLBACKS

void on_connect(void *data, struct websockets *ws, struct ws_info *info, const char *protocols) {
  (void)data; (void)ws; (void)info; (void)protocols;
  log_trace("[LAVALINK] Lavalink connected with success.");
}

void on_close(void *data, struct websockets *ws, struct ws_info *info, enum ws_close_reason wscode, const char *reason, size_t len) {
  (void)data; (void) ws; (void) info; (void) len;
  log_fatal("[LAVALINK] Error in the Lavalink connection [%i] because of \"%s\".", wscode, reason);
}

void on_cycle(struct discord *client) {
  (void) *client;
  uint64_t tstamp;
  ws_easy_run(g_ws, 5, &tstamp);
}

void on_text(void *data, struct websockets *ws, struct ws_info *info, const char *text, size_t len) {
  (void) ws; (void) info;
  printf("%s\n", text);

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
            .timestamp = discord_timestamp(client),
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

void sigint_handler(int signum) {
  (void)signum;
  log_fatal("SIGINT received, shutting down ...");
  discord_shutdown(client);
}

int main () {
  client = discord_config_init("config.json");

  // WEBSOCKET

  struct ws_callbacks cbs = {
    .on_text = &on_text,
    .on_connect = &on_connect,
    .on_close = &on_close,
    .data = client
  };

  CURLM *mhandle = curl_multi_init();
  g_ws = ws_init(&cbs, mhandle, NULL);
  
  char lNAWss[256];
  
  snprintf(lNAWss, sizeof(lNAWss), "wss://%s", lavalinkNodeURL);
  ws_set_url(g_ws, lNAWss, NULL);

  ws_start(g_ws);

  ws_add_header(g_ws, "Authorization", lavalinkNodePasswd);
  ws_add_header(g_ws, "Num-Shards", "1"); // You may want to change this.
  ws_add_header(g_ws, "User-Id", botID);
  ws_add_header(g_ws, "Client-Name", "MusicBotWithConcord");

  // WEBSOCKET

  discord_set_on_ready(client, &on_ready);
  discord_set_on_message_create(client, &on_message);
  discord_set_on_cycle(client, &on_cycle);
  discord_set_event_scheduler(client, &scheduler);

  discord_add_intents(client, DISCORD_GATEWAY_GUILD_VOICE_STATES);
  discord_add_intents(client, DISCORD_GATEWAY_MESSAGE_CONTENT);

  signal(SIGINT, &sigint_handler);

  discord_run(client);

  ws_end(g_ws);
  ws_cleanup(g_ws);
  curl_multi_cleanup(mhandle);
}
