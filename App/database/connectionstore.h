#ifndef CONNECTIONSTORE_H
#define CONNECTIONSTORE_H

#include "dbtypes.h"

#include <QString>
#include <QVector>

class ConnectionStore {
public:
  explicit ConnectionStore(const QString &filePath = QString());

  static QString defaultFilePath();
  static QString newId();

  bool load();
  bool save() const;

  const QVector<DbConnectionProfile> &profiles() const { return m_profiles; }
  const DbConnectionProfile *find(const QString &id) const;
  const DbConnectionProfile *findByName(const QString &name) const;

  QString upsert(DbConnectionProfile profile);
  bool remove(const QString &id);

  QString uniqueName(const QString &wanted, const QString &ignoreId = {}) const;

  QString filePath() const { return m_path; }

  QString activeId() const { return m_activeId; }
  void setActiveId(const QString &id) { m_activeId = id; }

private:
  QString m_path;
  QVector<DbConnectionProfile> m_profiles;
  QString m_activeId;
};

#endif
