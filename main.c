#include <string.h>
#include <stdio.h>
#include <pthread.h>

#include <curl/curl.h>
#include <orca/cJSON.h>

#include <orca/discord.h>
#include <orca/discord-internal.h>
#include <orca/websockets.h>

#include <sqlite3.h>

bool send_voice_server_payload = false;
u64_snowflake_t voice_server_guild_id = 0;
char session_id[64];
char all_event[110] = "NULL";

bool send_play_payload = false;
char track[512] = "null";

char lavalinkNodeUrl[64] = "example.com"; //If it dosen't use SSL, replace https with http & wss with ws.
char lavalinkNodePassword[64] = "you should not pass";
char totalShards[64] = "1"; //Default.
char botId[16] = "BOT_ID_HERE";

pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;

int StartsWith(const char *a, const char *b) {
   if(strncmp(a, b, strlen(b)) == 0) return 1;
   return 0;
}


void curl_easy_setopt_cb(CURL *ehandle, void *data) {
  (void)data;
  curl_easy_setopt(ehandle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
}

void on_text(void *data, struct websockets *ws, struct ws_info *info, const char *text, size_t len) {
  (void)data; (void)info; (void)ws; (void)text; (void)len;
  cJSON *payload = cJSON_ParseWithLength(text, len); 
  cJSON *payloadOp = cJSON_GetObjectItemCaseSensitive(payload, "op");
  if(0 == strcmp(payloadOp->valuestring, "TrackEndEvent")) {
    track[0] = '\0';
  }
  printf("\n%s\n\n", text);
}

void on_connect(
  void *data, 
  struct websockets *ws,
  struct ws_info *info, 
  const char *protocols) {
  (void)data; (void)ws; (void)info; (void)protocols;
  log_info("[LAVALINK] Lavalink connected with sucess.");
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
  struct discord *client, 
  const struct discord_user *bot) {
  (void) client;
  log_info("[DISCORD_GATEWAY] Logged in as %s#%s!", bot->username, bot->discriminator);
}

void on_voice_state_update(
  struct discord *client, 
  const struct discord_user *bot, 
  const struct discord_voice_state *voice_state) {
    (void) client; (void) bot;
    sqlite3 *db = NULL;
    char *query = NULL, *errMsg = NULL;
    int rc = sqlite3_open("database.db", &db);
  
    pthread_mutex_lock(&global_lock);
      if(voice_state->member->user->id == bot->id) snprintf(session_id, sizeof(session_id), "%s", voice_state->session_id);
    pthread_mutex_unlock(&global_lock);
  
    query = sqlite3_mprintf("CREATE TABLE IF NOT EXISTS guild_voice(guild_id INT, USER_ID INT, voice_channel_id INT);");
    rc = sqlite3_exec(db, query, NULL, NULL, &errMsg);

    query = sqlite3_mprintf("DELETE FROM guild_voice WHERE user_id = %lu;", voice_state->user_id);
    rc = sqlite3_exec(db, query, NULL, NULL, &errMsg);

    if (voice_state->channel_id) {
      query = sqlite3_mprintf("INSERT INTO guild_voice(guild_id, user_id, voice_channel_id) values(%lu, %lu, %lu);", voice_state->guild_id, voice_state->user_id, voice_state->channel_id);
      rc = sqlite3_exec(db, query, NULL, NULL, &errMsg);
    }

    sqlite3_free(query);
  }

void on_voice_server_update(
  struct discord *client, 
  const struct discord_user *bot, 
  const char *token, 
  const u64_snowflake_t guild_id, 
  const char *endpoint) {
    (void) client; (void) bot;(void) token; (void) endpoint;
    pthread_mutex_lock(&global_lock);
      voice_server_guild_id = guild_id;
      snprintf(all_event, sizeof(all_event), "{\"token\":\"%s\",\"guild_id\":\"%"PRIu64"\",\"endpoint\": \"%s\"}", token, guild_id, endpoint);
      send_voice_server_payload = true;
    
      if (0 != strcmp(track, "null")) send_play_payload = true;
    pthread_mutex_unlock(&global_lock);
}

void on_message(
  struct discord *client, 
  const struct discord_user *bot, 
  const struct discord_message *msg) {
    (void) bot;
    
    if (0 == strcmp(msg->content, "!ping")) {
      if (msg->author->bot) return;

      struct discord_embed embed = { .color = 15615 };

      discord_embed_set_title(&embed, "Ping %dms", client -> gw.hbeat -> ping_ms);
      discord_embed_set_footer(&embed, "Powered by Orca", "https://raw.githubusercontent.com/cee-studio/orca-docs/master/docs/source/images/icon.svg", NULL);
      discord_embed_set_image(&embed, "https://github.com/cee-studio/orca-docs/blob/master/docs/source/images/social-preview.png?raw=true", NULL, 0, 0);

      struct discord_create_message_params params = { .embed = &embed };
      discord_create_message(client, msg -> channel_id, &params, NULL);
  }

    if (0 == strncmp("!play ", msg->content, 6)) {
      char *content = msg->content + 6; 

      sqlite3 *db = NULL;
      sqlite3_stmt *stmt = NULL;
      int rc = sqlite3_open("database.db", &db);
      char *query = NULL;

      query = sqlite3_mprintf("SELECT guild_id, user_id, voice_channel_id FROM guild_voice WHERE user_id = %"PRIu64";", msg->author->id);
      rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);

      struct discord_embed embed = { .color = 15615 };

      discord_embed_set_title(&embed, "Done, I'm your channel.");
      discord_embed_set_footer(&embed, "Powered by Orca", "https://raw.githubusercontent.com/cee-studio/orca-docs/master/docs/source/images/icon.svg", NULL);

      struct discord_create_message_params params = { .embed = &embed };
      discord_create_message(client, msg -> channel_id, &params, NULL);

      while ((rc = sqlite3_step(stmt)) != SQLITE_DONE) {
        discord_voice_join(client, msg->guild_id, sqlite3_column_int64(stmt, 2), false, true);

        FILE *fp = fopen("./config.json", "rb");
        struct logconf conf = {};
        logconf_setup(&conf, "USER_AGENT", fp);

        struct user_agent *ua = ua_init(&conf);
        
        char url[64];
        char pass[64];
        sprintf(url, "https://%s", lavalinkNodeUrl);
        sprintf(pass, "%s", lavalinkNodePassword);
        
        ua_set_url(ua, url);
        ua_reqheader_add(ua, "Authorization", pass);
        ua_reqheader_add(ua, "Client-Name", "MusicBotWithOrca");
        ua_curl_easy_setopt(ua, NULL, &curl_easy_setopt_cb);

        struct ua_info info = {};
        char endpoint[1024];
        if(StartsWith(content, "https://")) {
          snprintf(endpoint, sizeof(endpoint), "/loadtracks?identifier=%s", content);
        } else {
          snprintf(endpoint, sizeof(endpoint), "/loadtracks?identifier=ytsearch:%s", content);
        }

        for(unsigned long i = 0; i < strlen(endpoint); i++){  
          if(endpoint[i] == ' ') endpoint[i] = '+';  
        }
        
        ua_run(ua, &info, NULL, NULL, HTTP_GET, endpoint);

        struct sized_buffer body = ua_info_get_body(&info);

        cJSON *payload = cJSON_ParseWithLength(body.start, body.size); 
        cJSON *tracks = cJSON_GetObjectItemCaseSensitive(payload, "tracks");
        cJSON *trackFromTracks = cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(tracks, 0), "track");

        pthread_mutex_lock(&global_lock);
          if(0 != strcmp(track, "null")) send_play_payload = true;
          snprintf(track, sizeof(track), "%s", trackFromTracks->valuestring);
          voice_server_guild_id = msg->guild_id;
        pthread_mutex_unlock(&global_lock);
       
        ua_info_cleanup(&info);
        ua_cleanup(ua);
        cJSON_Delete(payload);
      }
      sqlite3_free(query);
    }
  }

void *lavalink(void *id) {
  (void)id;
  struct ws_callbacks cbs = {
    .on_text = &on_text,
    .on_connect = &on_connect,
    .on_close = &on_close
  };

  FILE *fp = fopen("./config.json", "rb");
  struct logconf conf = {};
  logconf_setup(&conf, "LAVALINK", fp);
  struct websockets *ws = ws_init(&cbs, &conf);
  
  char url[64];
  char pass[64];
  char botid[18];
  char shards[64];
  snprintf(url, sizeof(url), "wss://%s", lavalinkNodeUrl);
  snprintf(pass, sizeof(pass), "%s", lavalinkNodePassword);
  snprintf(botid, sizeof(botId), "%s", botId);
  snprintf(shards, sizeof(shards), "%s", totalShards);
  
  ws_set_url(ws, lavalinkNodeUrl, NULL);
  ws_start(ws);
  ws_reqheader_add(ws, "Authorization", lavalinkNodePassword);
  ws_reqheader_add(ws, "Num-Shards", totalShards);
  ws_reqheader_add(ws, "User-Id", botId);
  ws_reqheader_add(ws, "Client-Name", "MusicBotWithOrca");
  
  bool is_running = false;
  while (1) {
    ws_perform(ws, &is_running, 5);
    if(!is_running) break;

    pthread_mutex_lock(&global_lock);
    if (send_voice_server_payload == true && voice_server_guild_id && 0 != strcmp(session_id, "null") && 0 != strcmp(all_event, "NULL")) {
      char payloadJson[1024];
      snprintf(payloadJson, sizeof(payloadJson), "{\"op\":\"voiceUpdate\",\"guildId\":\"%"PRIu64"\",\"sessionId\":\"%s\",\"event\":%s}", voice_server_guild_id, session_id, all_event);
      ws_send_text(ws, NULL, payloadJson, strlen(payloadJson));
      printf("\n\n%s\n\n", payloadJson);
      session_id[0] = '\0';
      all_event[0] = '\0';
      send_voice_server_payload = false;
    }

    if (send_play_payload == true && voice_server_guild_id && 0 != strcmp(track, "null")) {
      char payloadJson[1024];
      snprintf(payloadJson, sizeof(payloadJson), "{\"op\":\"play\",\"guildId\":\"%"PRIu64"\",\"track\":\"%s\",\"noReplace\":\"false\",\"pause\":\"false\"}", voice_server_guild_id, track);
      ws_send_text(ws, NULL, payloadJson, strlen(payloadJson));
      printf("\n\n%s\n\n", payloadJson);
      track[0] = '\0';
      send_voice_server_payload = 0;
      send_play_payload = false;
    } 
    pthread_mutex_unlock(&global_lock);
  }
  return NULL;
}

int main() {
  pthread_t thread_id;

  pthread_create(&thread_id, NULL, lavalink, NULL);

  struct discord *client = discord_config_init("./config.json");
  discord_set_on_ready(client, &on_ready);
  discord_set_on_message_create(client, &on_message);
  discord_set_on_voice_state_update(client, &on_voice_state_update);
  discord_set_on_voice_server_update(client, &on_voice_server_update);
  
  discord_run(client);
  pthread_join(thread_id, NULL);
}
