#include <string.h>
#include <stdio.h>
#include <sys/resource.h>
#include <math.h>
#include <signal.h>

#include <curl/curl.h>
#include <cJSON.h>

#include <orca/discord.h>
#include <orca/discord-internal.h>
#include <orca/websockets.h>
#include <orca/json-actor.h>

#include <sqlite3.h>

bool send_voice_server_payload = false;
u64_snowflake_t voice_server_guild_id = 0;
char session_id[64];
char all_event[1512] = "NULL";

bool send_play_payload = false;
char g_track[512] = "NULL";

char lavalinkNodeUrl[64] = "example.com"; // If it doesn't use SSL, replace the code below with http instead of https & ws instead of wss.
char lavalinkNodePassword[64] = "youshallnotpass";
char totalShards[64] = "1"; // Default.
char botId[18] = "BOT_ID_HERE";
struct discord *client;

void on_text(void *data, struct websockets *ws, struct ws_info *info, const char *text, size_t len) {
  (void)data; (void)info; (void)ws;
  cJSON *payload = cJSON_ParseWithLength(text, len); 
  cJSON *payloadOp = cJSON_GetObjectItemCaseSensitive(payload, "op");
  if(0 == strcmp(payloadOp->valuestring, "TrackEndEvent")) {
    g_track[0] = '\0';
  }
  cJSON_Delete(payload);
}

void default_config(struct ua_conn *conn, void *data) {
  (void) data;
  ua_conn_add_header(conn, "Authorization", lavalinkNodePassword);
  ua_conn_add_header(conn, "Client-Name", "MusicBotWithOrca");
}

void on_connect(
  void *data, 
  struct websockets *ws,
  struct ws_info *info, 
  const char *protocols) {
  (void)data; (void)ws; (void)info; (void)protocols;
  log_info("[LAVALINK] Lavalink connected with success.");
}

void on_close(
  void *data, 
  struct websockets *ws, 
  struct ws_info *info, 
  enum ws_close_reason wscode, 
  const char *reason, 
  size_t len) {
    (void)data; (void) ws; (void) info; (void)len;
    log_info("[LAVALINK] Error in the lavalink connection\n%d: '%s'", wscode, reason);
  }

void on_ready(
  struct discord *client) {
  (void) client;
  const struct discord_user *bot = discord_get_self(client);
  log_info("[DISCORD_GATEWAY] Logged in as %s#%s!", bot->username, bot->discriminator);
}

void on_message(
  struct discord *client, 
  const struct discord_message *msg) {
    if (msg->author->bot) return;
    if (0 == strcmp(msg->content, "?ping")) {

      struct discord_embed embed = { .color = 15615 };

      discord_embed_set_title(&embed, "Ping %dms", discord_get_ping(client));
      discord_embed_set_footer(&embed, "Powered by Orca", "https://raw.githubusercontent.com/cee-studio/orca-docs/master/docs/source/images/icon.svg", NULL);
      discord_embed_set_image(&embed, "https://github.com/cee-studio/orca-docs/blob/master/docs/source/images/social-preview.png?raw=true", NULL, 0, 0);

      struct discord_create_message_params params = { .embed = &embed };
      
      discord_async_next(client, NULL);
      discord_create_message(client, msg -> channel_id, &params, NULL);

      discord_embed_cleanup(&embed);
  }

  if (0 == strncmp("?play ", msg->content, 6)) {
    char *content = msg->content + 6; 

    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_open("database.db", &db);
    char *query = NULL;

    query = sqlite3_mprintf("SELECT guild_id, user_id, voice_channel_id FROM guild_voice WHERE user_id = %"PRIu64";", msg->author->id);
    rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);

    if ((rc = sqlite3_step(stmt)) != SQLITE_DONE) {
      discord_voice_join(client, msg->guild_id, sqlite3_column_int64(stmt, 2), false, true);
         
      struct discord_embed embed = { .color = 15615 };

      struct ua_info info = { 0 };
      char endpoint[1024] = "";
        
      if (0 == strncmp(content, "https://", strlen("https://"))) {
        snprintf(endpoint, sizeof(endpoint), "https://%s/loadtracks?identifier=%s", lavalinkNodeUrl, content);
      } else {
        snprintf(endpoint, sizeof(endpoint), "https://%s/loadtracks?identifier=ytsearch:%s", lavalinkNodeUrl, content);
      }

      for (unsigned long i = 0; i < strlen(endpoint); i++){  
        if (endpoint[i] == ' ') endpoint[i] = '+';  
      }
        
      struct user_agent *ua = ua_init(NULL);
      struct ua_conn_attr conn_attr = { .method = HTTP_GET };
        
      ua_set_url(ua, endpoint);
      ua_set_opt(ua, NULL, &default_config);
      ua_easy_run(ua, &info, NULL, &conn_attr);

      struct sized_buffer body = ua_info_get_body(&info);

      cJSON *payload = cJSON_ParseWithLength(body.start, body.size); 
      cJSON *tracks = cJSON_GetObjectItemCaseSensitive(payload, "tracks");
      cJSON *trackFromTracks = cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(tracks, 0), "track");

      cJSON *track = cJSON_GetArrayItem(tracks, 0);
      cJSON *Info = cJSON_GetObjectItemCaseSensitive(track, "info");
      cJSON *title = cJSON_GetObjectItemCaseSensitive(Info, "title");
      cJSON *url = cJSON_GetObjectItemCaseSensitive(Info, "uri");
      cJSON *author = cJSON_GetObjectItemCaseSensitive(Info, "author");
      cJSON *length = cJSON_GetObjectItemCaseSensitive(Info, "length");
      cJSON *artwork = cJSON_GetObjectItemCaseSensitive(Info, "artwork");
        
      if (!body.size) {
        printf("[PLAY] Failed to fetch music information.\n");
        return; 
      }
        
      char descriptionEmbed[1024];

      if (((int) round(length->valuedouble) / 1000 % 60) > 10) {
        snprintf(descriptionEmbed, sizeof(descriptionEmbed), "<a:yes:757568594841305149> | Ok, playing music rn!\n<:Info:772480355293986826> | Author: `%s`\n:musical_note: | Name: `%s`\n<:Cooldown:735255003161165915> | Time: `%d:%d`", author->valuestring, title->valuestring, ((int) round(length->valuedouble / 1000) / 60) << 0, (int) round(length->valuedouble) / 1000 % 60);
      } else {
        snprintf(descriptionEmbed, sizeof(descriptionEmbed), "<a:yes:757568594841305149> | Ok, playing music rn!\n<:Info:772480355293986826> | Author: `%s`\n:musical_note: | Name: `%s`\n<:Cooldown:735255003161165915> | Time: `%d:0%d`", author->valuestring, title->valuestring, ((int) round(length->valuedouble / 1000) / 60) << 0, (int) round(length->valuedouble) / 1000 % 60);  
      }

      discord_embed_set_title(&embed, title->valuestring);
      discord_embed_set_url(&embed, url->valuestring);
      discord_embed_set_description(&embed, descriptionEmbed);
      discord_embed_set_image(&embed, artwork->valuestring, NULL, 0, 0);
      discord_embed_set_footer(&embed, "Powered by Orca", "https://raw.githubusercontent.com/cee-studio/orca-docs/master/docs/source/images/icon.svg", NULL);

      struct discord_create_message_params params = { .embed = &embed };
        
      discord_async_next(client, NULL);
      discord_create_message(client, msg -> channel_id, &params, NULL);

      discord_embed_cleanup(&embed);

      if (0 != strcmp(g_track, "NULL")) send_play_payload = true;
      snprintf(g_track, sizeof(g_track), "%s", trackFromTracks->valuestring);
      voice_server_guild_id = msg->guild_id;

      ua_cleanup(ua);
      cJSON_Delete(payload);
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    sqlite3_free(query);
  }
}

discord_event_scheduler_t scheduler(
  struct discord *client,
  struct sized_buffer *data,
  enum discord_gateway_events event) {
    switch (event) {
      case DISCORD_GATEWAY_EVENTS_VOICE_STATE_UPDATE: {
        struct discord_voice_state vs;
        discord_voice_state_from_json(data->start, data->size, &vs);

        sqlite3 *db = NULL;
        char *query = NULL, *errMsg = NULL;
        sqlite3_open("database.db", &db);
    
        const struct discord_user *bot = discord_get_self(client);

        if(vs.member->user->id == bot->id) snprintf(session_id, sizeof(session_id), "%s", vs.session_id);

        query = sqlite3_mprintf("CREATE TABLE IF NOT EXISTS guild_voice(guild_id INT, USER_ID INT, voice_channel_id INT);");
        sqlite3_exec(db, query, NULL, NULL, &errMsg);

        sqlite3_free(query);

        query = sqlite3_mprintf("DELETE FROM guild_voice WHERE user_id = %lu;", vs.user_id);
        sqlite3_exec(db, query, NULL, NULL, &errMsg);

        sqlite3_free(query);

        if (vs.channel_id) {
          query = sqlite3_mprintf("INSERT INTO guild_voice(guild_id, user_id, voice_channel_id) values(%lu, %lu, %lu);", vs.guild_id, vs.user_id, vs.channel_id);
          sqlite3_exec(db, query, NULL, NULL, &errMsg);

          sqlite3_free(query);
        }
        
        sqlite3_close(db);
        discord_voice_state_cleanup(&vs);
      } return DISCORD_EVENT_IGNORE;
    case DISCORD_GATEWAY_EVENTS_VOICE_SERVER_UPDATE: {
      u64_snowflake_t guild_id = 0;
      
      json_extract(data->start, data->size, "(guild_id):s_as_u64", &guild_id);

      voice_server_guild_id = guild_id;
      snprintf(all_event, sizeof(all_event), "%.*s", (int)data->size, data->start);
      send_voice_server_payload = true;
    
      if (0 != strcmp(g_track, "NULL")) send_play_payload = true;
   } return DISCORD_EVENT_IGNORE;
    default: 
      return DISCORD_EVENT_MAIN_THREAD;
   }
}

ORCAcode discord_custom_run(struct discord *client) {
  ORCAcode code;

  struct ws_callbacks cbs = {
    .on_text = &on_text,
    .on_connect = &on_connect,
    .on_close = &on_close
  };

  CURLM *mhandle = curl_multi_init();
  struct websockets *ws = ws_init(&cbs, mhandle, NULL);
  
  char url[128];
  snprintf(url, sizeof(url), "wss://%s", lavalinkNodeUrl);
  
  ws_set_url(ws, url, NULL);
  ws_start(ws);
  ws_add_header(ws, "Authorization", lavalinkNodePassword);
  ws_add_header(ws, "Num-Shards", totalShards);
  ws_add_header(ws, "User-Id", botId);
  ws_add_header(ws, "Client-Name", "MusicBotWithOrca");
  
  uint64_t tstamp;

  while (1) {
    code = discord_gateway_start(&client->gw);
    if (code != ORCA_OK) break;

    do {
      code = discord_gateway_perform(&client->gw);
      if (code != ORCA_OK) break;

      code = discord_adapter_perform(&client->adapter);
      if (code != ORCA_OK) break;
          
      ws_easy_run(ws, 5, &tstamp);

      if (send_voice_server_payload == true && voice_server_guild_id && 0 != strcmp(session_id, "NULL") && 0 != strcmp(all_event, "NULL")) {
        char payloadJson[1650];

        snprintf(payloadJson, sizeof(payloadJson), "{\"op\":\"voiceUpdate\",\"guildId\":\"%"PRIu64"\",\"sessionId\":\"%s\",\"event\":%s}", voice_server_guild_id, session_id, all_event);
        ws_send_text(ws, NULL, payloadJson, strlen(payloadJson));

        session_id[0] = '\0';
        all_event[0] = '\0';
        send_voice_server_payload = false;
      }
  
      if (send_play_payload == true && voice_server_guild_id && 0 != strcmp(g_track, "NULL")) {
        char payloadJson[1650];

        snprintf(payloadJson, sizeof(payloadJson), "{\"op\":\"play\",\"guildId\":\"%"PRIu64"\",\"track\":\"%s\",\"noReplace\":\"false\",\"pause\":\"false\"}", voice_server_guild_id, g_track);
        ws_send_text(ws, NULL, payloadJson, strlen(payloadJson));

        g_track[0] = '\0';
        send_voice_server_payload = 0;
        send_play_payload = false;
      } 
    } while (1);

    if (discord_gateway_end(&client->gw)) {
      discord_adapter_stop_all(&client->adapter);
      break;
    }
  }

  ws_end(ws);

  ws_cleanup(ws);
  curl_multi_cleanup(mhandle);

  return code;
}

void sigint_handler(int signum) {
  (void)signum;
  log_fatal("SIGINT received, shutting down ...");
  discord_shutdown(client);
}

int main() {
  client = discord_config_init("./config.json");
  
  struct logconf *conf = discord_get_logconf(client);
  logconf_set_quiet(conf, false);
  
  discord_set_on_ready(client, &on_ready);
  discord_set_on_message_create(client, &on_message);
  discord_set_event_scheduler(client, &scheduler);

  signal(SIGINT, &sigint_handler);
  
  discord_add_intents(client, DISCORD_GATEWAY_GUILD_VOICE_STATES);
  discord_custom_run(client);
  
  discord_cleanup(client);
  orca_global_cleanup();
}
