#ifndef GITOBJECTDATABASE_H
#define GITOBJECTDATABASE_H

#include <QByteArray>
#include <QList>
#include <QStringList>
#include <functional>

struct GitPackInfo {
  QString path;
  QString indexPath;
  QString name;
  quint32 objectCount = 0;
  qint64 size = 0;
  qint64 indexSize = 0;
  int packVersion = 0;
  int indexVersion = 0;
  bool alternate = false;
  QStringList flags;
  QString error;
};

struct GitObjectStore {
  QString repositoryPath;
  QStringList objectDirectories;
  QList<GitPackInfo> packs;
  int hashLength = 40;
  qint64 looseObjects = 0;
  QStringList errors;
};

struct GitObjectLocation {
  QString hash;
  QString packPath;
  QString loosePath;
  quint64 offset = 0;
  QString crc;
  QString type;
  qint64 size = -1;
  QString representation;
  QString deltaBase;
};

struct GitObjectSearchResult {
  QList<GitObjectLocation> objects;
  qint64 totalMatches = 0;
  QStringList errors;
  bool cancelled = false;
};

struct GitObjectLink {
  QString hash;
  QString type;
  QString label;
};

struct GitObjectDetails {
  QString hash;
  QString type;
  qint64 size = 0;
  QByteArray preview;
  bool binary = false;
  bool truncated = false;
  QList<GitObjectLink> links;
  bool linksTruncated = false;
  QString error;
};

class GitObjectDatabase {
public:
  using Cancelled = std::function<bool()>;
  static GitObjectStore scan(const QString &repositoryPath,
                             const Cancelled &cancelled = {});
  static GitObjectSearchResult search(const GitObjectStore &store,
                                      const QString &hashPrefix,
                                      const QString &packPath = {},
                                      qint64 skip = 0, int limit = 250,
                                      const Cancelled &cancelled = {});
  static GitObjectDetails inspect(const QString &repositoryPath,
                                  const QString &hash,
                                  const Cancelled &cancelled = {});
  static QString verifyPack(const QString &repositoryPath,
                            const QString &indexPath, bool *ok,
                            const Cancelled &cancelled = {});
};

#endif
