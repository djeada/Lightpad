#ifndef QUERYHISTORY_H
#define QUERYHISTORY_H

#include <QDateTime>
#include <QString>
#include <QVector>

struct QueryHistoryEntry {
  QDateTime when;
  QString connection;
  QString engine;
  QString sql;
  bool ok = true;
  qint64 elapsedMs = 0;
  qint64 rows = -1;
  QString error;
};

class QueryHistory {
public:
  explicit QueryHistory(const QString &filePath = QString(),
                        int maxEntries = 1000);

  static QString defaultFilePath();

  bool load();
  bool save() const;

  void add(const QueryHistoryEntry &entry);
  void clear();

  const QVector<QueryHistoryEntry> &entries() const { return m_entries; }
  QVector<QueryHistoryEntry> search(const QString &needle,
                                    const QString &connection = {}) const;

private:
  QString m_path;
  int m_max;
  QVector<QueryHistoryEntry> m_entries;
};

#endif
