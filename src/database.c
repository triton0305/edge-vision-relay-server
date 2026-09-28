#include "database.h"

#include <stdio.h>

#define DB_PATH "data/edge_vision.db"

static int init_db(sqlite3* db)
{
  const char* sql =
    "CREATE TABLE IF NOT EXISTS detections ("
    "id INTEGER PRIMARY KEY, message_id TEXT NOT NULL UNIQUE,"
    "device_id TEXT NOT NULL, frame_id INTEGER NOT NULL,"
    "timestamp_ms INTEGER NOT NULL, class_id INTEGER NOT NULL,"
    "class_name TEXT NOT NULL, confidence REAL NOT NULL,"
    "bbox_x INTEGER NOT NULL, bbox_y INTEGER NOT NULL,"
    "bbox_width INTEGER NOT NULL, bbox_height INTEGER NOT NULL);"
    "CREATE TABLE IF NOT EXISTS traffic_counts ("
    "id INTEGER PRIMARY KEY, message_id TEXT NOT NULL UNIQUE,"
    "device_id TEXT NOT NULL, period_start_ms INTEGER NOT NULL,"
    "period_end_ms INTEGER NOT NULL, car_count INTEGER NOT NULL,"
    "motorcycle_count INTEGER NOT NULL, bus_count INTEGER NOT NULL,"
    "truck_count INTEGER NOT NULL);";
  char* error = NULL;
  int result = sqlite3_exec(db, sql, NULL, NULL, &error);

  if (result != SQLITE_OK)
  {
    fprintf(stderr, "SQLite init: %s\n", error ? error : sqlite3_errmsg(db));
  }

  sqlite3_free(error);
  return result == SQLITE_OK ? 0 : -1;
}

sqlite3* database_open(void)
{
  sqlite3* db = NULL;

  if (sqlite3_open(DB_PATH, &db) != SQLITE_OK)
  {
    fprintf(stderr, "SQLite open: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return NULL;
  }

  if (init_db(db) < 0)
  {
    sqlite3_close(db);
    return NULL;
  }

  return db;
}

static const char* save_vision(sqlite3* db, const ProtocolMessage* message)
{
  const VisionData* vision = &message->vision;
  const char* sql = "INSERT OR IGNORE INTO detections "
    "(message_id,device_id,frame_id,timestamp_ms,class_id,class_name,"
    "confidence,bbox_x,bbox_y,bbox_width,bbox_height) VALUES (?,?,?,?,?,?,?,?,?,?,?)";
  sqlite3_stmt* stmt = NULL;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
  {
    return "database_error";
  }

  sqlite3_bind_text(stmt, 1, message->message_id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, message->device_id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 3, vision->frame_id);
  sqlite3_bind_int64(stmt, 4, vision->timestamp_ms);
  sqlite3_bind_int64(stmt, 5, vision->class_id);
  sqlite3_bind_text(stmt, 6, vision->class_name, -1, SQLITE_TRANSIENT);
  sqlite3_bind_double(stmt, 7, vision->confidence);
  sqlite3_bind_int64(stmt, 8, vision->bbox_x);
  sqlite3_bind_int64(stmt, 9, vision->bbox_y);
  sqlite3_bind_int64(stmt, 10, vision->bbox_width);
  sqlite3_bind_int64(stmt, 11, vision->bbox_height);
  int result = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return result == SQLITE_DONE ? NULL : "database_error";
}

const char* database_save(sqlite3* db, const ProtocolMessage* message)
{
  if (message->type == MESSAGE_VISION)
  {
    return save_vision(db, message);
  }

  return "database_error";
}

void database_close(sqlite3* db)
{
  sqlite3_close(db);
}
