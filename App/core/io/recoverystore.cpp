#include "recoverystore.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QObject>
#include <QSaveFile>
#include <QUuid>
#include <algorithm>

namespace {
bool validId(const QString &id) {
  const QUuid uuid(id);
  return !uuid.isNull() && uuid.toString(QUuid::WithoutBraces) == id;
}
bool failure(QString *error, const QString &message) {
  if (error)
    *error = message;
  return false;
}
bool privateDirectory(const QString &path, QString *error) {
  if (QFileInfo(path).isSymLink())
    return failure(
        error,
        QObject::tr("Recovery directory is a symbolic link: %1").arg(path));
  if (!QDir().mkpath(path) ||
      !QFile::setPermissions(path, QFileDevice::ReadOwner |
                                       QFileDevice::WriteOwner |
                                       QFileDevice::ExeOwner))
    return failure(error,
                   QObject::tr("Cannot create a private recovery directory: %1")
                       .arg(path));
  return true;
}
QString recordPath(const QString &root, const QString &session,
                   const QString &id) {
  return QDir(root).filePath(session + '/' + id + ".json");
}
QByteArray hash(const QByteArray &bytes) {
  return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
}
} // namespace

QString Recovery::newId() {
  return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
Recovery::Store::Store(QString rootPath, qint64 maxTextBytes)
    : m_root(QDir::cleanPath(QFileInfo(rootPath).absoluteFilePath())),
      m_session(newId()), m_maxTextBytes(qMax<qint64>(1, maxTextBytes)) {}
Recovery::Store::~Store() = default;
QString Recovery::Store::sessionId() const { return m_session; }
QString Recovery::Store::sessionPath() const {
  return QDir(m_root).filePath(m_session);
}

bool Recovery::Store::start(QString *error) {
  if (m_started)
    return true;
  if (!privateDirectory(m_root, error) ||
      !privateDirectory(sessionPath(), error))
    return false;
  auto lock =
      std::make_unique<QLockFile>(QDir(sessionPath()).filePath("session.lock"));
  lock->setStaleLockTime(0);
  if (!lock->tryLock(0))
    return failure(error, QObject::tr("Cannot lock the recovery session."));
  m_lock = std::move(lock);
  m_started = true;
  return true;
}

bool Recovery::Store::save(const Draft &draft, QString *error) {
  if (!m_started || !validId(draft.id))
    return failure(error, QObject::tr("Invalid recovery session or document."));
  const QByteArray text = draft.text.toUtf8();
  if (text.size() > m_maxTextBytes)
    return failure(error, QObject::tr("%1 exceeds the %2 recovery size limit. "
                                      "The previous recovery copy is retained.")
                              .arg(draft.title,
                                   FileOpenGuard::formatSize(m_maxTextBytes)));
  if (draft.title.size() > 4096 || draft.originalPath.size() > 32768)
    return failure(error,
                   QObject::tr("Recovery document metadata is too large."));
  QJsonObject object{
      {"version", 1},
      {"session", m_session},
      {"id", draft.id},
      {"path", draft.originalPath},
      {"title", draft.title},
      {"text", draft.text},
      {"sha256", QString::fromLatin1(hash(text))},
      {"updated", (draft.updated.isValid() ? draft.updated
                                           : QDateTime::currentDateTimeUtc())
                      .toUTC()
                      .toString(Qt::ISODateWithMs)},
      {"cursor", qBound(0, draft.cursor, int(draft.text.size()))},
      {"encoding", int(draft.format.encoding)},
      {"bom", draft.format.hasBom},
      {"lineEnding", draft.format.lineEnding}};
  const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
  const auto path = recordPath(m_root, m_session, draft.id);
  if (QFileInfo(path).isSymLink())
    return failure(error,
                   QObject::tr("A recovery snapshot is a symbolic link."));
  QSaveFile file(path);
  file.setDirectWriteFallback(false);
  if (!file.open(QIODevice::WriteOnly))
    return failure(error, file.errorString());
  if (!file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) ||
      file.write(bytes) != bytes.size()) {
    const QString message = file.errorString();
    file.cancelWriting();
    return failure(error, message);
  }
  if (!file.commit())
    return failure(error, file.errorString());
  return true;
}

bool Recovery::Store::read(const QString &session, const QString &id,
                           Draft *draft, QString *error) const {
  if (!validId(session) || !validId(id))
    return failure(error, QObject::tr("Invalid recovery identifier."));
  const auto path = recordPath(m_root, session, id);
  const QFileInfo info(path);
  if (!info.isFile() || info.isSymLink() ||
      info.size() > m_maxTextBytes * 6 + 65536)
    return failure(error,
                   QObject::tr("Cannot read recovery copy %1; it was retained.")
                       .arg(path));
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return failure(error, file.errorString());
  const auto bytes = file.readAll();
  if (file.error() != QFileDevice::NoError)
    return failure(error, file.errorString());
  QJsonParseError parse;
  const auto document = QJsonDocument::fromJson(bytes, &parse);
  const auto object = document.object();
  const QString text = object.value("text").toString();
  const QByteArray encoded = text.toUtf8();
  const int encoding = object.value("encoding").toInt(-1);
  const auto updated = QDateTime::fromString(object.value("updated").toString(),
                                             Qt::ISODateWithMs);
  const QString ending = object.value("lineEnding").toString();
  if (parse.error != QJsonParseError::NoError || !document.isObject() ||
      object.value("version").toInt() != 1 ||
      object.value("session").toString() != session ||
      object.value("id").toString() != id || !object.value("text").isString() ||
      encoded.size() > m_maxTextBytes ||
      object.value("sha256").toString().toLatin1() != hash(encoded) ||
      !updated.isValid() || (encoding != 0 && encoding != 1) ||
      (ending != "\n" && ending != "\r\n" && ending != "\r"))
    return failure(
        error,
        QObject::tr(
            "Recovery copy %1 is invalid or unsupported; it was retained.")
            .arg(path));
  *draft = {session,
            id,
            object.value("path").toString(),
            object.value("title").toString(),
            text,
            updated,
            qBound(0, object.value("cursor").toInt(), int(text.size())),
            {FileOpenGuard::TextEncoding(encoding),
             object.value("bom").toBool(), ending},
            hash(bytes)};
  return true;
}

Recovery::Scan Recovery::Store::scan() const {
  Scan scan;
  qint64 retained = 0;
  const QDir root(m_root);
  for (const auto &session : root.entryList(
           QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::Name)) {
    if (session == m_session || !validId(session))
      continue;
    const QDir dir(root.filePath(session));
    QLockFile lock(dir.filePath("session.lock"));
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0))
      continue;
    for (const auto &file :
         dir.entryList({"*.json"}, QDir::Files, QDir::Name)) {
      Draft draft;
      QString error;
      if (!read(session, file.chopped(5), &draft, &error)) {
        scan.warnings.append(error);
        continue;
      }
      retained += draft.text.size() * qint64(sizeof(QChar));
      if (scan.drafts.size() >= 1000 || retained > 256LL * 1024 * 1024) {
        scan.warnings.append(
            QObject::tr("More recovery copies remain. Restore or discard this "
                        "batch, then refresh."));
        return scan;
      }
      scan.drafts.append(std::move(draft));
    }
  }
  std::stable_sort(
      scan.drafts.begin(), scan.drafts.end(),
      [](const Draft &a, const Draft &b) { return a.updated > b.updated; });
  return scan;
}

bool Recovery::Store::remove(const QString &id, QString *error) {
  if (!m_started || !validId(id))
    return failure(error, QObject::tr("Invalid recovery document."));
  const auto path = recordPath(m_root, m_session, id);
  if (!QFileInfo::exists(path))
    return true;
  if (!QFile::remove(path))
    return failure(error,
                   QObject::tr("Cannot remove recovery copy %1.").arg(path));
  return true;
}

bool Recovery::Store::checkSnapshot(const Draft &draft, QString *error) const {
  Draft current;
  if (!read(draft.sessionId, draft.id, &current, error))
    return false;
  if (current.fingerprint != draft.fingerprint)
    return failure(
        error,
        QObject::tr(
            "The recovery copy changed. Refresh the list before continuing."));
  return true;
}

bool Recovery::Store::transfer(const Draft &draft, const QString &id,
                               Draft *adopted, QString *error) {
  if (!m_started || !validId(draft.sessionId) || !validId(draft.id) ||
      !validId(id) || draft.sessionId == m_session ||
      QFileInfo(QDir(m_root).filePath(draft.sessionId)).isSymLink())
    return failure(error, QObject::tr("Invalid recovery transfer."));
  QLockFile lock(
      QDir(QDir(m_root).filePath(draft.sessionId)).filePath("session.lock"));
  lock.setStaleLockTime(0);
  if (!lock.tryLock(0))
    return failure(
        error, QObject::tr("That recovery session is open in another window."));
  if (!checkSnapshot(draft, error))
    return false;
  Draft copy = draft;
  copy.sessionId = m_session;
  copy.id = id;
  if (!save(copy, error))
    return false; // Durable destination precedes removing the source.
  if (!QFile::remove(recordPath(m_root, draft.sessionId, draft.id)))
    return failure(error, QObject::tr("The original recovery copy could not be "
                                      "removed; it was retained."));
  if (adopted)
    *adopted = std::move(copy);
  return true;
}

bool Recovery::Store::discard(const Draft &draft, QString *error) {
  if (!validId(draft.sessionId) || !validId(draft.id) ||
      draft.sessionId == m_session ||
      QFileInfo(QDir(m_root).filePath(draft.sessionId)).isSymLink())
    return failure(error, QObject::tr("Invalid recovery document."));
  QLockFile lock(
      QDir(QDir(m_root).filePath(draft.sessionId)).filePath("session.lock"));
  lock.setStaleLockTime(0);
  if (!lock.tryLock(0))
    return failure(
        error, QObject::tr("That recovery session is open in another window."));
  if (!checkSnapshot(draft, error))
    return false;
  if (!QFile::remove(recordPath(m_root, draft.sessionId, draft.id)))
    return failure(error, QObject::tr("Cannot discard the recovery copy."));
  return true;
}

bool Recovery::Store::finish(QString *error) {
  if (!m_started)
    return true;
  bool success = true;
  const QDir dir(sessionPath());
  for (const auto &file : dir.entryList({"*.json"}, QDir::Files, QDir::Name)) {
    if (!validId(file.chopped(5)))
      continue;
    if (!remove(file.chopped(5), error))
      success = false;
  }
  m_lock.reset();
  m_started = false;
  if (success)
    QDir().rmdir(sessionPath());
  return success;
}
