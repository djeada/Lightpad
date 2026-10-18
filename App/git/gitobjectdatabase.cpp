#include "gitobjectdatabase.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSet>
#include <QtEndian>
#include <algorithm>
#include <limits>

namespace {
constexpr qint64 PreviewLimit = 256 * 1024;
constexpr int LinkLimit = 500;

bool stopped(const GitObjectDatabase::Cancelled &cancelled) {
  return cancelled && cancelled();
}

struct CommandResult {
  QByteArray out;
  QString error;
  bool ok = false;
  bool truncated = false;
};

CommandResult run(const QString &repository, const QStringList &args,
                  const QByteArray &input = {},
                  qint64 maxOutput = 4 * 1024 * 1024,
                  const GitObjectDatabase::Cancelled &cancelled = {},
                  int timeout = 30000) {
  CommandResult result;
  if (stopped(cancelled)) {
    result.error = QStringLiteral("Cancelled.");
    return result;
  }
  QProcess process;
  process.setWorkingDirectory(repository);
  auto env = QProcessEnvironment::systemEnvironment();
  env.insert("GIT_OPTIONAL_LOCKS", "0");
  env.insert("GIT_NO_LAZY_FETCH", "1");
  env.insert("GIT_NO_REPLACE_OBJECTS", "1");
  env.insert("GIT_TERMINAL_PROMPT", "0");
  env.insert("LC_ALL", "C");
  process.setProcessEnvironment(env);
  process.start("git", args);
  if (!process.waitForStarted(5000)) {
    result.error = process.errorString();
    return result;
  }
  process.write(input);
  process.closeWriteChannel();
  QElapsedTimer timer;
  timer.start();
  QByteArray errors;
  while (true) {
    process.waitForFinished(50);
    result.out += process.readAllStandardOutput();
    errors += process.readAllStandardError();
    if (result.out.size() > maxOutput) {
      result.out.truncate(maxOutput);
      result.truncated = true;
      process.kill();
      process.waitForFinished(1000);
      result.ok = true;
      return result;
    }
    if (stopped(cancelled) || timer.elapsed() > timeout) {
      process.kill();
      process.waitForFinished(1000);
      result.error = stopped(cancelled)
                         ? QStringLiteral("Cancelled.")
                         : QStringLiteral("Git command timed out.");
      return result;
    }
    if (process.state() == QProcess::NotRunning)
      break;
  }
  result.ok =
      process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
  if (!result.ok)
    result.error = QString::fromUtf8(errors).trimmed();
  if (!result.ok && result.error.isEmpty())
    result.error = process.errorString();
  return result;
}

bool isHash(const QString &text, int minLength, int maxLength) {
  static const QRegularExpression hex(QStringLiteral("^[0-9a-fA-F]+$"));
  return text.size() >= minLength && text.size() <= maxLength &&
         hex.match(text).hasMatch();
}

class PackIndex {
public:
  QFile file;
  const uchar *data = nullptr;
  quint32 count = 0;
  int version = 0;
  int hashBytes = 20;
  quint64 namesStart = 0;
  quint64 offsetsStart = 0;
  quint64 largeStart = 0;
  QString error;

  ~PackIndex() {
    if (data)
      file.unmap(const_cast<uchar *>(data));
  }

  bool open(const QString &path, int hashLength) {
    hashBytes = hashLength / 2;
    file.setFileName(path);
    if (!file.open(QIODevice::ReadOnly)) {
      error = file.errorString();
      return false;
    }
    const quint64 size = file.size();
    if (size < 1024 + quint64(2 * hashBytes)) {
      error = QStringLiteral("Truncated pack index.");
      return false;
    }
    data = file.map(0, file.size());
    if (!data) {
      error = file.errorString();
      return false;
    }
    version = qFromBigEndian<quint32>(data) == 0xff744f63 ? 2 : 1;
    const int header = version == 2 ? 8 : 0;
    if (version == 2 && qFromBigEndian<quint32>(data + 4) != 2) {
      error = QStringLiteral("Unsupported pack index version.");
      return false;
    }
    quint32 previous = 0;
    for (int i = 0; i < 256; ++i) {
      const quint32 value = qFromBigEndian<quint32>(data + header + i * 4);
      if (value < previous) {
        error = QStringLiteral("Invalid pack index fanout table.");
        return false;
      }
      previous = value;
    }
    count = previous;
    namesStart = header + 1024;
    offsetsStart = namesStart + quint64(count) * (hashBytes + 4);
    largeStart = offsetsStart + quint64(count) * 4;
    const quint64 minimum =
        version == 2
            ? largeStart + 2 * hashBytes
            : namesStart + quint64(count) * (hashBytes + 4) + 2 * hashBytes;
    if (minimum > size || (version == 1 && minimum != size) ||
        (version == 2 && (size - minimum) % 8 != 0)) {
      error = QStringLiteral("Pack index tables are truncated or malformed.");
      return false;
    }
    return true;
  }

  QString hash(quint32 row) const {
    const quint64 pos = version == 2
                            ? namesStart + quint64(row) * hashBytes
                            : namesStart + quint64(row) * (hashBytes + 4) + 4;
    return QString::fromLatin1(
        QByteArray(reinterpret_cast<const char *>(data + pos), hashBytes)
            .toHex());
  }

  quint32 lowerBound(const QString &value) const {
    quint32 low = 0, high = count;
    while (low < high) {
      const quint32 mid = low + (high - low) / 2;
      if (hash(mid) < value)
        low = mid + 1;
      else
        high = mid;
    }
    return low;
  }

  bool location(quint32 row, GitObjectLocation *object) const {
    object->hash = hash(row);
    quint32 offset;
    if (version == 1) {
      offset = qFromBigEndian<quint32>(data + namesStart +
                                       quint64(row) * (hashBytes + 4));
    } else {
      const quint32 crc = qFromBigEndian<quint32>(
          data + namesStart + quint64(count) * hashBytes + quint64(row) * 4);
      object->crc = QStringLiteral("%1").arg(crc, 8, 16, QLatin1Char('0'));
      offset = qFromBigEndian<quint32>(data + offsetsStart + quint64(row) * 4);
      if (offset & 0x80000000U) {
        const quint64 pos = largeStart + quint64(offset & 0x7fffffffU) * 8;
        if (pos + 8 > quint64(file.size()) - 2 * hashBytes)
          return false;
        object->offset = qFromBigEndian<quint64>(data + pos);
        return true;
      }
    }
    object->offset = offset;
    return true;
  }
};

void readRepresentation(QFile &pack, int hashLength,
                        GitObjectLocation *object) {
  if (object->offset < 12 || object->offset >= quint64(pack.size()) ||
      !pack.seek(qint64(object->offset)))
    return;
  const QByteArray header = pack.read(128);
  if (header.isEmpty())
    return;
  const int type = (uchar(header[0]) >> 4) & 7;
  const QStringList names = {"invalid", "commit",   "tree",      "blob",
                             "tag",     "reserved", "ofs-delta", "ref-delta"};
  object->representation = names.value(type);
  int pos = 1;
  uchar byte = uchar(header[0]);
  while (byte & 128) {
    if (pos >= header.size())
      return;
    byte = uchar(header[pos++]);
  }
  if (type == 7 && header.size() - pos >= hashLength / 2) {
    object->deltaBase =
        QString::fromLatin1(header.mid(pos, hashLength / 2).toHex());
  } else if (type == 6 && pos < header.size()) {
    byte = uchar(header[pos++]);
    quint64 distance = byte & 127;
    while (byte & 128) {
      if (pos >= header.size() ||
          distance >= (std::numeric_limits<quint64>::max() >> 7) - 1)
        return;
      byte = uchar(header[pos++]);
      distance = ((distance + 1) << 7) + (byte & 127);
    }
    if (distance <= object->offset)
      object->deltaBase =
          QStringLiteral("offset %1").arg(object->offset - distance);
  }
}

void addMetadata(const QString &repository, QList<GitObjectLocation> *objects,
                 const GitObjectDatabase::Cancelled &cancelled,
                 QStringList *errors) {
  if (objects->isEmpty())
    return;
  QByteArray input;
  for (const auto &object : *objects)
    input += object.hash.toLatin1() + '\n';
  const auto result = run(
      repository,
      {"cat-file", "--batch-check=%(objectname) %(objecttype) %(objectsize)"},
      input, 1024 * 1024, cancelled);
  if (!result.ok) {
    *errors << result.error;
    return;
  }
  const auto lines = result.out.split('\n');
  for (int i = 0; i < objects->size() && i < lines.size(); ++i) {
    const auto fields = lines.at(i).split(' ');
    if (fields.size() == 3 && fields[0] == objects->at(i).hash.toLatin1()) {
      (*objects)[i].type = QString::fromLatin1(fields[1]);
      (*objects)[i].size = fields[2].toLongLong();
    }
  }
}
} // namespace

GitObjectStore GitObjectDatabase::scan(const QString &repositoryPath,
                                       const Cancelled &cancelled) {
  GitObjectStore store;
  store.repositoryPath = repositoryPath;
  const auto directory =
      run(repositoryPath, {"rev-parse", "--git-path", "objects"}, {}, 8192,
          cancelled);
  if (!directory.ok) {
    store.errors << directory.error;
    return store;
  }
  const QString path =
      QDir(repositoryPath)
          .absoluteFilePath(QString::fromUtf8(directory.out).trimmed());
  store.objectDirectories << QDir::cleanPath(path);
  const auto format = run(repositoryPath, {"rev-parse", "--show-object-format"},
                          {}, 1024, cancelled);
  if (format.out.trimmed() == "sha256")
    store.hashLength = 64;
  const auto counts =
      run(repositoryPath, {"count-objects", "-v"}, {}, 1024 * 1024, cancelled);
  if (!counts.ok)
    store.errors << counts.error;
  for (const QByteArray &line : counts.out.split('\n')) {
    if (line.startsWith("count: "))
      store.looseObjects = line.mid(7).toLongLong();
    if (line.startsWith("alternate: ")) {
      const QString alternate =
          QDir::cleanPath(QString::fromUtf8(line.mid(11)));
      if (!store.objectDirectories.contains(alternate))
        store.objectDirectories << alternate;
    }
  }
  for (const QString &objects : store.objectDirectories) {
    if (stopped(cancelled))
      break;
    const QDir dir(objects + "/pack");
    for (const QFileInfo &file :
         dir.entryInfoList({"*.pack"}, QDir::Files, QDir::Name)) {
      GitPackInfo info;
      info.path = file.absoluteFilePath();
      info.name = file.fileName();
      info.indexPath = info.path.left(info.path.size() - 5) + ".idx";
      info.size = file.size();
      info.indexSize = QFileInfo(info.indexPath).size();
      info.alternate = objects != store.objectDirectories.first();
      for (const QString &suffix :
           {QStringLiteral("keep"), QStringLiteral("promisor"),
            QStringLiteral("mtimes")}) {
        if (QFileInfo::exists(info.path.left(info.path.size() - 4) + suffix))
          info.flags << (suffix == "mtimes" ? QStringLiteral("cruft") : suffix);
      }
      QFile pack(info.path);
      const bool opened = pack.open(QIODevice::ReadOnly);
      const QByteArray header = opened ? pack.read(12) : QByteArray();
      if (header.size() != 12 || !header.startsWith("PACK") ||
          info.size < 12 + store.hashLength / 2) {
        info.error = QStringLiteral("Invalid or truncated pack header.");
      } else {
        info.packVersion = qFromBigEndian<quint32>(header.constData() + 4);
        info.objectCount = qFromBigEndian<quint32>(header.constData() + 8);
        if (info.packVersion != 2 && info.packVersion != 3)
          info.error = QStringLiteral("Unsupported pack version.");
        PackIndex index;
        if (!index.open(info.indexPath, store.hashLength)) {
          info.error = index.error;
        } else {
          info.indexVersion = index.version;
          if (index.count != info.objectCount)
            info.error =
                QStringLiteral("Pack and index object counts disagree.");
          pack.seek(info.size - store.hashLength / 2);
          const QByteArray checksum = pack.read(store.hashLength / 2);
          const QByteArray indexedChecksum(
              reinterpret_cast<const char *>(index.data + index.file.size() -
                                             store.hashLength),
              store.hashLength / 2);
          if (checksum != indexedChecksum)
            info.error = QStringLiteral("Pack and index checksums disagree.");
        }
      }
      store.packs << info;
    }
  }
  return store;
}

GitObjectSearchResult GitObjectDatabase::search(const GitObjectStore &store,
                                                const QString &hashPrefix,
                                                const QString &packPath,
                                                qint64 skip, int limit,
                                                const Cancelled &cancelled) {
  GitObjectSearchResult result;
  const QString prefix = hashPrefix.trimmed().toLower();
  if (!prefix.isEmpty() && !isHash(prefix, 4, store.hashLength)) {
    result.errors << QStringLiteral("Enter 4–%1 hexadecimal characters.")
                         .arg(store.hashLength);
    return result;
  }
  skip = qMax<qint64>(0, skip);
  limit = qBound(1, limit, 1000);
  auto include = [&](const GitObjectLocation &object) {
    if (result.totalMatches >= skip && result.objects.size() < limit)
      result.objects << object;
    ++result.totalMatches;
  };
  for (const auto &info : store.packs) {
    if (stopped(cancelled))
      break;
    if (!packPath.isEmpty() && info.path != packPath)
      continue;
    if (!info.error.isEmpty()) {
      result.errors << info.name + ": " + info.error;
      continue;
    }
    PackIndex index;
    if (!index.open(info.indexPath, store.hashLength)) {
      result.errors << info.name + ": " + index.error;
      continue;
    }
    const quint32 first = prefix.isEmpty() ? 0 : index.lowerBound(prefix);
    // 'g' sorts immediately after every possible hex suffix, including odd
    // prefixes. The range is found in O(log n) without reading every hash.
    const quint32 last =
        prefix.isEmpty() ? index.count : index.lowerBound(prefix + 'g');
    const qint64 count = qint64(last) - first;
    const qint64 begin = qMax<qint64>(0, skip - result.totalMatches);
    const qint64 take = qMin<qint64>(qMax<qint64>(0, count - begin),
                                     limit - result.objects.size());
    QFile pack(info.path);
    pack.open(QIODevice::ReadOnly);
    for (qint64 i = begin; i < begin + take; ++i) {
      GitObjectLocation object;
      if (!index.location(first + quint32(i), &object)) {
        result.errors << info.name + ": invalid large offset.";
        continue;
      }
      object.packPath = info.path;
      readRepresentation(pack, store.hashLength, &object);
      result.objects << object;
    }
    result.totalMatches += count;
  }
  if (packPath.isEmpty()) {
    for (const QString &directory : store.objectDirectories) {
      const QDir dir(directory);
      const QStringList buckets =
          prefix.isEmpty()
              ? dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)
              : QStringList{prefix.left(2)};
      for (const QString &bucket : buckets) {
        if (stopped(cancelled))
          break;
        if (!isHash(bucket, 2, 2))
          continue;
        const QDir loose(dir.filePath(bucket));
        for (const QString &file :
             loose.entryList({prefix.mid(2) + '*'}, QDir::Files, QDir::Name)) {
          const QString hash = bucket + file;
          if (!isHash(hash, store.hashLength, store.hashLength))
            continue;
          GitObjectLocation object;
          object.hash = hash;
          object.loosePath = loose.filePath(file);
          object.representation = QStringLiteral("loose");
          include(object);
        }
      }
    }
  }
  result.cancelled = stopped(cancelled);
  if (!result.cancelled)
    addMetadata(store.repositoryPath, &result.objects, cancelled,
                &result.errors);
  return result;
}

GitObjectDetails GitObjectDatabase::inspect(const QString &repositoryPath,
                                            const QString &hash,
                                            const Cancelled &cancelled) {
  GitObjectDetails details;
  // Only full object IDs reach plumbing; expressions, options and ambiguous
  // prefixes are resolved by selecting a search result first.
  if (!isHash(hash, 40, 40) && !isHash(hash, 64, 64)) {
    details.error =
        QStringLiteral("Select a full object ID from the search results.");
    return details;
  }
  details.hash = hash.toLower();
  const auto metadata = run(
      repositoryPath, {"cat-file", "--batch-check=%(objecttype) %(objectsize)"},
      details.hash.toLatin1() + '\n', 1024, cancelled);
  const auto fields = metadata.out.trimmed().split(' ');
  if (!metadata.ok || fields.size() != 2 || fields[1] == "missing") {
    details.error = metadata.ok
                        ? QStringLiteral("Object is missing or unreadable.")
                        : metadata.error;
    return details;
  }
  details.type = QString::fromLatin1(fields[0]);
  details.size = fields[1].toLongLong();
  const auto content = run(repositoryPath, {"cat-file", "-p", details.hash}, {},
                           PreviewLimit, cancelled);
  if (!content.ok) {
    details.error = content.error;
    return details;
  }
  details.preview = content.out;
  details.truncated = content.truncated;
  details.binary = content.out.contains('\0');
  if (details.type == "tree") {
    const auto tree = run(repositoryPath, {"ls-tree", "-z", details.hash}, {},
                          PreviewLimit, cancelled);
    if (!tree.ok) {
      details.error = tree.error;
      return details;
    }
    details.linksTruncated = tree.truncated;
    const auto entries = tree.out.split('\0');
    for (int i = 0; i + 1 < entries.size(); ++i) {
      const auto &entry = entries[i];
      const int tab = entry.indexOf('\t');
      const auto header = entry.left(tab).split(' ');
      if (tab < 0 || header.size() != 3)
        continue;
      if (details.links.size() == LinkLimit) {
        details.linksTruncated = true;
        break;
      }
      details.links << GitObjectLink{QString::fromLatin1(header[2]),
                                     QString::fromLatin1(header[1]),
                                     QString::fromUtf8(entry.mid(tab + 1))};
    }
  } else if (details.type == "commit" || details.type == "tag") {
    for (const auto &line : details.preview.split('\n')) {
      if (line.isEmpty())
        break;
      const int space = line.indexOf(' ');
      const QByteArray key = line.left(space);
      const QString target = QString::fromLatin1(line.mid(space + 1));
      if ((key == "tree" || key == "parent" || key == "object") &&
          isHash(target, hash.size(), hash.size())) {
        const QString type = key == "tree"     ? QStringLiteral("tree")
                             : key == "parent" ? QStringLiteral("commit")
                                               : QStringLiteral("object");
        details.links << GitObjectLink{target, type, QString::fromLatin1(key)};
      }
    }
  }
  return details;
}

QString GitObjectDatabase::verifyPack(const QString &repositoryPath,
                                      const QString &indexPath, bool *ok,
                                      const Cancelled &cancelled) {
  const auto result =
      run(repositoryPath,
          {"verify-pack", "--", QFileInfo(indexPath).absoluteFilePath()}, {},
          1024 * 1024, cancelled, 120000);
  if (ok)
    *ok = result.ok;
  return result.ok ? QStringLiteral("Pack and index verified successfully.")
                   : result.error;
}
