#include <stdio.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/resource.h>

#include "functions.h"

#include <jsmn.h>
#include <jsmn-find.h>

#include <concord/discord.h>
#include <concord/discord-internal.h>

#include <sqlite3.h>

char lavalinkSecure = false; // If its using SSL or not.
char lavalinkNodeUrl[64] = "example.com"; // Lavalink node URL.

struct discord *client;

void on_ready(struct discord *client) {
  const struct discord_user *bot = discord_get_self(client);
  log_trace("[DISCORD_GATEWAY] Logged in as %s#%s!", bot->username, bot->discriminator);
}

void on_message(struct discord *client, const struct discord_message *msg) {
  if (msg->author->bot) return;
  if (0 == strcmp(msg->content, "?ping")) {
    char description[64];

    snprintf(description, sizeof(description), "Ping %dms", discord_get_ping(client));

    struct discord_embed embed[] = {
      {
        .description = description,
        .footer =
          &(struct discord_embed_footer){
            .text = "Powered by Concord"
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

    discord_create_message(client, msg->channel_id, &params, NULL);
  }
  if (0 == strncmp("?volume ", msg->content, 8)) {
    char *content = msg->content + strlen("?volume ");

    struct discord_create_message params = { .content = "Done!" };

    discord_create_message(client, msg->channel_id, &params, NULL);

    setMusicVolume(content, msg->guild_id);
  }
  if (0 == strncmp("?play ", msg->content, 6)) {
    clock_t start, end;
    double cpu_time_used;

    start = clock();

    char *content = msg->content + strlen("?play ");
    char *ptr;

    ptr = strchr(content, '\n');
    if (ptr) *ptr = '\0';

    if (!content) {
      struct discord_embed embed[] = {
        {
          .description = "<a:Noo:757568484086382622> | You must say a music name or url.",
          .footer =
            &(struct discord_embed_footer){
              .text = "Powered by Concord"
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

      discord_create_message(client, msg->channel_id, &params, NULL);
    }

    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    char *query = NULL;
    int rc = sqlite3_open("database.db", &db);

    query = sqlite3_mprintf("SELECT guild_id, user_id, voice_channel_id FROM guild_voice WHERE user_id = %"PRIu64";", msg->author->id);
    rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);

    if ((rc = sqlite3_step(stmt)) != SQLITE_DONE) {
      sqlite3_free(query);
      discord_voice_join(client, msg->guild_id, sqlite3_column_int64(stmt, 2), false, true);

      CURL *curl = curl_easy_init();
      char *contentEncoded = curl_easy_escape(curl, content, strlen(content));

      struct ua_info info = { 0 };
      char endpoint[1024];
      
      if (lavalinkSecure == true) snprintf(endpoint, sizeof(endpoint), "https://%s/loadtracks?identifier=", lavalinkNodeUrl);
      else snprintf(endpoint, sizeof(endpoint), "http://%s/loadtracks?identifier=", lavalinkNodeUrl);

      if(0 == strncmp(content, "https://", strlen("https://"))) {
        strncat(endpoint, contentEncoded, sizeof(endpoint) - 1);
      } else {
        strncat(endpoint, "ytsearch:", sizeof(endpoint) - 1);
        strncat(endpoint, contentEncoded, sizeof(endpoint) - 1);
      }

      struct user_agent *ua = ua_init(NULL);
      ua_set_url(ua, endpoint);
      ua_set_opt(ua, NULL, &default_config);

      struct ua_conn_attr conn_attr = { .method = HTTP_GET };

      ua_easy_run(ua, &info, NULL, &conn_attr);

      struct sized_buffer body = ua_info_get_body(&info);

      if (!body.size) {
        log_fatal("[USER_AGENT] Failed to fetch music information.");

        ua_info_cleanup(&info);
        ua_cleanup(ua);

        sqlite3_finalize(stmt);
        sqlite3_close(db);

        return;
      }

      jsmnf *root = jsmnf_init();
      jsmnf_start(root, body.start, body.size);

      char *path[] = { "tracks", "0", "track", NULL };
      jsmnf *t = jsmnf_find_path(root, path, 3);

      path[2] = "info";
      path[3] = "title";
      jsmnf *ti = jsmnf_find_path(root, path, 4);

      path[3] = "uri";
      jsmnf *u = jsmnf_find_path(root, path, 4);

      path[3] = "author";
      jsmnf *a = jsmnf_find_path(root, path, 4);

      path[3] = "length";
      jsmnf *l = jsmnf_find_path(root, path, 4);

      path[3] = "artwork";
      jsmnf *ar = jsmnf_find_path(root, path, 4);

      if (!t || !ti || !u || !a || !l) {
        log_fatal("[JSMN_FIND] Error while trying to find %s field.", !t ? "track" : !ti ? "title": !u ? "uri" : !a ? "author" : !l ? "length" : "404");

        curl_free(contentEncoded);
        curl_easy_cleanup(curl);

        ua_info_cleanup(&info);
        ua_cleanup(ua);

        sqlite3_finalize(stmt);
        sqlite3_close(db);

        return;
      }

      char trackFromTracks[1024], Url[64], Author[32], Length[24], Artwork[64];
      char *Title = NULL;

      snprintf(trackFromTracks, sizeof(trackFromTracks), "%.*s", t->val->end - t->val->start, body.start + t->val->start);
      jsmnf_unescape(&Title, body.start + ti->val->start, ti->val->end - ti->val->start);
      snprintf(Url, sizeof(Url), "%.*s", u->val->end - u->val->start, body.start + u->val->start);
      snprintf(Author, sizeof(Author), "%.*s", a->val->end - a->val->start, body.start + a->val->start);
      snprintf(Length, sizeof(Length), "%.*s", l->val->end - l->val->start, body.start + l->val->start);
      snprintf(Artwork, sizeof(Artwork), "%.*s", ar->val->end - ar->val->start, body.start + ar->val->start);

      query = sqlite3_mprintf("CREATE TABLE IF NOT EXISTS guild_channel(guild_id INT, channel_id INT);");
      rc = sqlite3_exec(db, query, NULL, NULL, NULL);

      sqlite3_free(query);

      query = sqlite3_mprintf("INSERT INTO guild_channel(guild_id, channel_id) values(%lu, %lu);", msg->guild_id, msg->channel_id);
      rc = sqlite3_exec(db, query, NULL, NULL, NULL);

      char descriptionEmbed[256];

      float lengthFloat = atof(Length);
      int arounded = (int)(lengthFloat < 0 ? (lengthFloat - 0.5) : (lengthFloat + 0.5));

      if ((arounded / 1000) % 60 > 10) {
        snprintf(descriptionEmbed, sizeof(descriptionEmbed), "<a:yes:757568594841305149> | Ok, playing rn!\n<:Info:772480355293986826> | Author: `%s`\n:musical_note: | Name: `%s`\n<:Cooldown:735255003161165915> | Time length: `%d:%d`", Author, Title, (arounded / 1000 / 60) << 0, (arounded / 1000) % 60);
      } else {
        snprintf(descriptionEmbed, sizeof(descriptionEmbed), "<a:yes:757568594841305149> | Ok, playing rn!\n<:Info:772480355293986826> | Author: `%s`\n:musical_note: | Name: `%s`\n<:Cooldown:735255003161165915> | Time length: `%d:0%d`", Author, Title, (arounded / 1000 / 60) << 0, (arounded / 1000) % 60);
      }

      if (!*Artwork) {
         struct discord_embed embed[] = {
           {
            .title = Title,
            .url = Url,
            .description = descriptionEmbed,
            .footer =
              &(struct discord_embed_footer){
                .text = "Powered by Concord"
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

        discord_create_message(client, msg->channel_id, &params, NULL);
      } else {
        struct discord_embed embed[] = {
          {
            .title = Title,
            .url = Url,
            .description = descriptionEmbed,
            .image =
              &(struct discord_embed_image){
                .url = Artwork,
              },
            .footer =
              &(struct discord_embed_footer){
                .text = "Powered by Concord"
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

        discord_create_message(client, msg->channel_id, &params, NULL);
      }

      playMusic(trackFromTracks, msg->guild_id);

      curl_free(contentEncoded);
      curl_easy_cleanup(curl);

      ua_info_cleanup(&info);
      ua_cleanup(ua);

      free(Title);

      jsmnf_cleanup(root);

      end = clock();
      cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
      log_debug("[ TIME ] %f seconds to execute play command.", cpu_time_used);
    } else {
      struct discord_embed embed[] = {
        {
          .description = "<a:Noo:757568484086382622> | You are not in a voice channel.",
          .footer =
            &(struct discord_embed_footer){
              .text = "Powered by Concord"
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

      discord_create_message(client, msg->channel_id, &params, NULL);
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    sqlite3_free(query);
  }
}

discord_event_scheduler_t scheduler(struct discord *client, struct sized_buffer *data, enum discord_gateway_events event) {
  switch (event) {
    case DISCORD_GATEWAY_EVENTS_VOICE_STATE_UPDATE: {
      struct context *ctx = discord_get_data(client);
      struct discord_voice_state vs;
      discord_voice_state_from_json(data->start, data->size, &vs);

      sqlite3 *db = NULL;
      char *query = NULL;
      sqlite3_open("database.db", &db);

      const struct discord_user *bot = discord_get_self(client);

      if (vs.member->user->id == bot->id) snprintf(ctx->session_id, sizeof(ctx->session_id), "%s", vs.session_id);

      query = sqlite3_mprintf("CREATE TABLE IF NOT EXISTS guild_voice(guild_id INT, user_id INT, voice_channel_id INT);");
      sqlite3_exec(db, query, NULL, NULL, NULL);

      sqlite3_free(query);

      query = sqlite3_mprintf("DELETE FROM guild_voice WHERE user_id = %lu;", vs.user_id);
      if (vs.channel_id) sqlite3_exec(db, query, NULL, NULL, NULL);

      sqlite3_free(query);

      query = sqlite3_mprintf("INSERT INTO guild_voice(guild_id, user_id, voice_channel_id) values(%lu, %lu, %lu);", vs.guild_id, vs.user_id, vs.channel_id);
      if (vs.channel_id) sqlite3_exec(db, query, NULL, NULL, NULL);

      sqlite3_close(db);
      sqlite3_free(query);
      discord_voice_state_cleanup(&vs);
    } return DISCORD_EVENT_IGNORE;
    case DISCORD_GATEWAY_EVENTS_VOICE_SERVER_UPDATE: {
      struct context *ctx = discord_get_data(client);
      u64snowflake guild_id = 0;
      char all_event[1024];

      jsmnf *root = jsmnf_init();

      jsmnf_start(root, data->start, data->size);

      jsmnf *f = jsmnf_find(root, "guild_id", strlen("guild_id"));

      sscanf(data->start + f->val->start, "%" SCNu64, &guild_id);

      snprintf(all_event, sizeof(all_event), "%.*s", (int)data->size, data->start); 

      voiceUpdate(all_event, guild_id, ctx->session_id);

      jsmnf_cleanup(root);
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


int main() {
  client = discord_config_init("config.json");

  struct logconf *conf = discord_get_logconf(client);
  logconf_set_quiet(conf, false);

  struct discord_activity activities[] = {
    {
      .name = "Hello, world!",
      .type = DISCORD_ACTIVITY_GAME,
      .details = "Hi..?",
    },
  };

  struct discord_presence_update status = {
    .activities =
      &(struct discord_activities){
        .size = sizeof(activities) / sizeof *activities,
        .array = activities,
      },
    .status = "idle",
    .afk = false,
    .since = discord_timestamp(client),
  };

  discord_set_presence(client, &status);

  struct context ctx = { .session_id = "NULL" };
  discord_set_data(client, &ctx);

  discord_set_on_ready(client, &on_ready);
  discord_set_on_message_create(client, &on_message);
  discord_set_event_scheduler(client, &scheduler);

  signal(SIGINT, &sigint_handler);

  discord_add_intents(client, DISCORD_GATEWAY_GUILD_VOICE_STATES);
  discord_custom_run(client);

  discord_cleanup(client);
  ccord_global_cleanup();
}
