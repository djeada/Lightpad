#ifndef RECOVERYSTORE_H
#define RECOVERYSTORE_H

#include "fileopenguard.h"
#include <QByteArray>
#include <QDateTime>
#include <QStringList>
#include <QVector>
#include <memory>

class QLockFile;
namespace Recovery {
QString newId();

struct Draft {
  QString sessionId;
  QString id;
  QString originalPath;
  QString title;
  QString text;
  QDateTime updated;
  int cursor = 0;
  FileOpenGuard::TextFormat format;
  QByteArray fingerprint;
};
struct Scan {
  QVector<Draft> drafts;
  QStringList warnings;
};

class Store {
public:
  explicit Store(QString rootPath, qint64 maxTextBytes = 64LL * 1024 * 1024);
  ~Store();
  bool start(QString *error = nullptr);
  QString sessionId() const;
  QString sessionPath() const;
  bool save(const Draft &draft, QString *error = nullptr);
  bool remove(const QString &id, QString *error = nullptr);
  Scan scan() const;
  bool transfer(const Draft &draft, const QString &newId, Draft *adopted,
                QString *error = nullptr);
  bool discard(const Draft &draft, QString *error = nullptr);
  bool finish(QString *error = nullptr);

private:
  QString m_root, m_session;
  qint64 m_maxTextBytes;
  std::unique_ptr<QLockFile> m_lock;
  bool m_started = false;
  bool read(const QString &session, const QString &id, Draft *draft,
            QString *error) const;
  bool checkSnapshot(const Draft &draft, QString *error) const;
};
} // namespace Recovery
#endif
