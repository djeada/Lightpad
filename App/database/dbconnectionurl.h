#ifndef DBCONNECTIONURL_H
#define DBCONNECTIONURL_H

#include "dbtypes.h"

#include <QString>

// Reads and writes connection strings in the forms that tools and cloud
// consoles hand out:
//  - URLs: postgresql://user:pw@host:5432/db?sslmode=require,
//    mysql://user@host/db?ssl-mode=REQUIRED, sqlserver://sa@host?database=d
//  - SQLAlchemy and Prisma: postgresql+psycopg2://..., mysql+pymysql://...,
//    sqlite:///relative.db, sqlite:////abs/path.db, file:./dev.db
//  - JDBC: jdbc:postgresql://..., jdbc:sqlserver://h:1433;databaseName=d
//  - ADO.NET / ODBC: Server=...;Database=...;User Id=...;Password=...;
//  - libpq: host=... port=... dbname=... user=... sslmode=require
//  - a bare path to a .db/.sqlite file
namespace DbConnectionUrl {

struct Parsed {
  DbConnectionProfile profile;
  QString password;
  bool hasPassword = false;
};

// True when the text looks like any of the supported connection strings.
bool looksLikeConnectionString(const QString &text);

// Relative SQLite paths resolve against baseDir (or stay relative when it is
// empty). Returns false with a message when the text is not understood.
bool parse(const QString &text, Parsed *out, QString *error = nullptr,
           const QString &baseDir = QString());

// The profile as a URL. The password is only included when given.
QString format(const DbConnectionProfile &profile,
               const QString &password = QString());

} // namespace DbConnectionUrl

#endif
