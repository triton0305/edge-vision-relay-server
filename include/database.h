#ifndef DATABASE_H
#define DATABASE_H

#include "protocol.h"

#include <sqlite3.h>

sqlite3* database_open(void);
const char* database_save(sqlite3* db, const ProtocolMessage* message);
void database_close(sqlite3* db);

#endif
