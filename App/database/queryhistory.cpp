#include "queryhistory.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

QueryHistory::QueryHistory(const QString &filePath, int maxEntries)
    : m_path(filePath.isEmpty() ? defaultFilePath() : filePath),
      m_max(maxEntries) {}

QString QueryHistory::defaultFilePath() {
  const QString base =
      QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  return QDir(base).filePath(QStringLiteral("database/history.json"));
}

bool QueryHistory::load() {
  m_entries.clear();
  QFile file(m_path);
  if (!file.exists()) {
    return true;
  }
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  const QJsonArray arr = doc.object().value("entries").toArray();
  for (const QJsonValue &v : arr) {
    const QJsonObject o = v.toObject();
    QueryHistoryEntry e;
    e.when = QDateTime::fromString(o.value("when").toString(), Qt::ISODate);
    e.connection = o.value("connection").toString();
    e.engine = o.value("engine").toString();
    e.sql = o.value("sql").toString();
    e.ok = o.value("ok").toBool(true);
    e.elapsedMs = static_cast<qint64>(o.value("elapsedMs").toDouble());
    e.rows = static_cast<qint64>(o.value("rows").toDouble(-1));
    e.error = o.value("error").toString();
    if (!e.sql.isEmpty()) {
      m_entries.append(e);
    }
  }
  return true;
}

bool QueryHistory::save() const {
  QDir().mkpath(QFileInfo(m_path).absolutePath());
  QSaveFile file(m_path);
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }
  QJsonArray arr;
  for (const QueryHistoryEntry &e : m_entries) {
    QJsonObject o;
    o["when"] = e.when.toString(Qt::ISODate);
    o["connection"] = e.connection;
    o["engine"] = e.engine;
    o["sql"] = e.sql;
    o["ok"] = e.ok;
    o["elapsedMs"] = static_cast<double>(e.elapsedMs);
    o["rows"] = static_cast<double>(e.rows);
    o["error"] = e.error;
    arr.append(o);
  }
  QJsonObject root;
  root["version"] = 1;
  root["entries"] = arr;
  file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  return file.commit();
}

void QueryHistory::add(const QueryHistoryEntry &entry) {
  QueryHistoryEntry e = entry;
  if (!e.when.isValid()) {
    e.when = QDateTime::currentDateTime();
  }

  if (!m_entries.isEmpty() && m_entries.first().sql == e.sql &&
      m_entries.first().connection == e.connection) {
    m_entries.removeFirst();
  }
  m_entries.prepend(e);
  while (m_entries.size() > m_max) {
    m_entries.removeLast();
  }
}

void QueryHistory::clear() { m_entries.clear(); }

QVector<QueryHistoryEntry>
QueryHistory::search(const QString &needle, const QString &connection) const {
  QVector<QueryHistoryEntry> out;
  for (const QueryHistoryEntry &e : m_entries) {
    if (!connection.isEmpty() && e.connection != connection) {
      continue;
    }
    if (!needle.isEmpty() && !e.sql.contains(needle, Qt::CaseInsensitive) &&
        !e.error.contains(needle, Qt::CaseInsensitive)) {
      continue;
    }
    out.append(e);
  }
  return out;
}
