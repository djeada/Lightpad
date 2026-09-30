#include "connectionstore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

ConnectionStore::ConnectionStore(const QString &filePath)
    : m_path(filePath.isEmpty() ? defaultFilePath() : filePath) {}

QString ConnectionStore::defaultFilePath() {
  const QString base =
      QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  return QDir(base).filePath(QStringLiteral("database/connections.json"));
}

QString ConnectionStore::newId() {
  return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool ConnectionStore::load() {
  m_profiles.clear();
  QFile file(m_path);
  if (!file.exists()) {
    return true;
  }
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }
  QJsonParseError err;
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
  if (err.error != QJsonParseError::NoError || !doc.isObject()) {
    return false;
  }
  m_activeId = doc.object().value("activeId").toString();
  const QJsonArray arr = doc.object().value("connections").toArray();
  for (const QJsonValue &v : arr) {
    DbConnectionProfile p = DbConnectionProfile::fromJson(v.toObject());
    if (p.id.isEmpty()) {
      p.id = newId();
    }
    if (!p.name.isEmpty()) {
      m_profiles.append(p);
    }
  }
  return true;
}

bool ConnectionStore::save() const {
  QDir().mkpath(QFileInfo(m_path).absolutePath());
  QSaveFile file(m_path);
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }
  QJsonArray arr;
  for (const DbConnectionProfile &p : m_profiles) {
    arr.append(p.toJson());
  }
  QJsonObject root;
  root["version"] = 1;
  root["activeId"] = m_activeId;
  root["connections"] = arr;
  file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
  return file.commit();
}

const DbConnectionProfile *ConnectionStore::find(const QString &id) const {
  for (const DbConnectionProfile &p : m_profiles) {
    if (p.id == id) {
      return &p;
    }
  }
  return nullptr;
}

const DbConnectionProfile *
ConnectionStore::findByName(const QString &name) const {
  for (const DbConnectionProfile &p : m_profiles) {
    if (p.name.compare(name, Qt::CaseInsensitive) == 0) {
      return &p;
    }
  }
  return nullptr;
}

QString ConnectionStore::upsert(DbConnectionProfile profile) {
  if (profile.id.isEmpty()) {
    profile.id = newId();
  }
  for (DbConnectionProfile &p : m_profiles) {
    if (p.id == profile.id) {
      p = profile;
      return profile.id;
    }
  }
  m_profiles.append(profile);
  return profile.id;
}

bool ConnectionStore::remove(const QString &id) {
  for (int i = 0; i < m_profiles.size(); ++i) {
    if (m_profiles[i].id == id) {
      m_profiles.removeAt(i);
      return true;
    }
  }
  return false;
}

QString ConnectionStore::uniqueName(const QString &wanted,
                                    const QString &ignoreId) const {
  auto taken = [&](const QString &name) {
    for (const DbConnectionProfile &p : m_profiles) {
      if (p.id != ignoreId && p.name.compare(name, Qt::CaseInsensitive) == 0) {
        return true;
      }
    }
    return false;
  };
  if (!taken(wanted)) {
    return wanted;
  }
  for (int i = 2;; ++i) {
    const QString candidate = QStringLiteral("%1 %2").arg(wanted).arg(i);
    if (!taken(candidate)) {
      return candidate;
    }
  }
}
