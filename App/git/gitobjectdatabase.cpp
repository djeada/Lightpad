#include "gitobjectdatabase.h"

#include <QCryptographicHash>
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
#include <zlib.h>

namespace {
constexpr qint64 PreviewLimit = 256 * 1024;
constexpr int LinkLimit = 500;
constexpr qint64 DecodeLimit = 32 * 1024 * 1024;

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

struct InflatedData {
  QByteArray data;
  quint64 consumed = 0;
  bool complete = false;
  QString error;
};

InflatedData inflateEntry(QFile &file, quint64 end,
                          const GitObjectDatabase::Cancelled &cancelled) {
  InflatedData result;
  z_stream stream{};
  if (inflateInit(&stream) != Z_OK) {
    result.error = QStringLiteral("Could not initialize zlib.");
    return result;
  }
  QByteArray input;
  char output[32768];
  while (true) {
    if (stopped(cancelled)) {
      result.error = QStringLiteral("Cancelled.");
      break;
    }
    if (!stream.avail_in) {
      input = file.read(qMin<quint64>(65536, end - quint64(file.pos())));
      if (input.isEmpty()) {
        result.error = QStringLiteral("Truncated zlib stream.");
        break;
      }
      stream.next_in = reinterpret_cast<Bytef *>(input.data());
      stream.avail_in = input.size();
    }
    stream.next_out = reinterpret_cast<Bytef *>(output);
    stream.avail_out = sizeof(output);
    const int status = inflate(&stream, Z_NO_FLUSH);
    result.data.append(output, sizeof(output) - stream.avail_out);
    if (status == Z_STREAM_END) {
      result.complete = true;
      break;
    }
    if (status != Z_OK) {
      result.error = QStringLiteral("Invalid zlib stream: %1")
                         .arg(QString::fromLatin1(stream.msg ? stream.msg
                                                             : "decode error"));
      break;
    }
    if (result.data.size() > DecodeLimit) {
      result.data.truncate(DecodeLimit);
      break;
    }
  }
  result.consumed = stream.total_in;
  inflateEnd(&stream);
  return result;
}

bool deltaInteger(const QByteArray &data, qsizetype *pos, quint64 *value) {
  *value = 0;
  for (int shift = 0; shift < 64 && *pos < data.size(); shift += 7) {
    const uchar byte = uchar(data[(*pos)++]);
    if (shift == 63 && (byte & 0x7e))
      return false;
    *value |= quint64(byte & 127) << shift;
    if (!(byte & 128))
      return true;
  }
  return false;
}

QByteArray applyDelta(const QByteArray &base, const QByteArray &delta,
                      QStringList *operations, QString *error,
                      const GitObjectDatabase::Cancelled &cancelled) {
  qsizetype pos = 0;
  quint64 baseSize = 0, resultSize = 0;
  if (!deltaInteger(delta, &pos, &baseSize) ||
      !deltaInteger(delta, &pos, &resultSize) ||
      baseSize != quint64(base.size())) {
    *error = QStringLiteral("Invalid delta sizes or base object size.");
    return {};
  }
  if (resultSize > quint64(DecodeLimit)) {
    *error =
        QStringLiteral("Delta reconstruction exceeds the 32 MiB decode limit. "
                       "Use Content for Git's bounded preview.");
    return {};
  }
  QByteArray result;
  result.reserve(resultSize);
  while (pos < delta.size()) {
    if (stopped(cancelled)) {
      *error = QStringLiteral("Cancelled.");
      return {};
    }
    const uchar opcode = uchar(delta[pos++]);
    if (!opcode) {
      *error = QStringLiteral("Invalid zero delta opcode.");
      return {};
    }
    quint64 offset = 0, size = 0;
    if (opcode & 128) {
      for (int bit = 0; bit < 7; ++bit) {
        if (!(opcode & (1 << bit)))
          continue;
        if (pos >= delta.size()) {
          *error = QStringLiteral("Truncated delta copy instruction.");
          return {};
        }
        const quint64 byte = uchar(delta[pos++]);
        if (bit < 4)
          offset |= byte << (bit * 8);
        else
          size |= byte << ((bit - 4) * 8);
      }
      if (!size)
        size = 65536;
      if (offset > quint64(base.size()) ||
          size > quint64(base.size()) - offset ||
          size > resultSize - quint64(result.size())) {
        *error = QStringLiteral("Delta copy is outside the base or result.");
        return {};
      }
      if (operations->size() < 100)
        *operations
            << QStringLiteral(
                   "Copy %1 bytes from base offset %2 to result offset %3")
                   .arg(size)
                   .arg(offset)
                   .arg(result.size());
      result.append(base.constData() + offset, size);
    } else {
      size = opcode;
      if (size > quint64(delta.size() - pos) ||
          size > resultSize - quint64(result.size())) {
        *error = QStringLiteral("Invalid delta insert instruction.");
        return {};
      }
      if (operations->size() < 100)
        *operations << QStringLiteral("Insert %1 bytes at result offset %2")
                           .arg(size)
                           .arg(result.size());
      result.append(delta.constData() + pos, size);
      pos += size;
    }
  }
  if (quint64(result.size()) != resultSize)
    *error =
        QStringLiteral("Reconstructed delta size does not match its header.");
  return result;
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
                                                const Cancelled &cancelled,
                                                bool looseOnly) {
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
    if (looseOnly)
      break;
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

GitStoredObject
GitObjectDatabase::readStoredObject(const GitObjectStore &store,
                                    const GitObjectLocation &location,
                                    const Cancelled &cancelled) {
  GitStoredObject result;
  result.location = location;
  const auto fail = [&](const QString &error) {
    result.error = error;
    return result;
  };
  if (!isHash(location.hash, store.hashLength, store.hashLength))
    return fail(QStringLiteral("Invalid stored object hash."));
  const bool packed = !location.packPath.isEmpty();
  QFile file(packed ? location.packPath : location.loosePath);
  if (!file.open(QIODevice::ReadOnly))
    return fail(file.errorString());
  quint64 end = file.size();
  quint64 baseOffset = 0;
  quint64 declaredSize = 0;
  if (packed) {
    const auto info = std::find_if(store.packs.cbegin(), store.packs.cend(),
                                   [&](const GitPackInfo &pack) {
                                     return pack.path == location.packPath;
                                   });
    if (info == store.packs.cend())
      return fail(QStringLiteral(
          "The selected pack is no longer in this storage scan."));
    PackIndex index;
    if (!index.open(info->indexPath, store.hashLength))
      return fail(index.error);
    const quint32 row = index.lowerBound(location.hash);
    GitObjectLocation indexed;
    if (row >= index.count || !index.location(row, &indexed) ||
        indexed.hash != location.hash || indexed.offset != location.offset)
      return fail(
          QStringLiteral("The selected pack entry changed. Refresh storage."));
    end -= store.hashLength / 2;
    if (location.offset < 12 || location.offset >= end ||
        !file.seek(location.offset))
      return fail(QStringLiteral("Invalid pack object offset."));
    QByteArray header = file.read(128);
    if (header.isEmpty())
      return fail(QStringLiteral("Missing pack entry header."));
    const int type = (uchar(header[0]) >> 4) & 7;
    const QStringList types{"invalid", "commit",   "tree",      "blob",
                            "tag",     "reserved", "ofs-delta", "ref-delta"};
    if (type == 0 || type == 5)
      return fail(QStringLiteral("Invalid pack entry type."));
    result.location.representation = types[type];
    qsizetype pos = 1;
    uchar byte = uchar(header[0]);
    declaredSize = byte & 15;
    for (int shift = 4; byte & 128; shift += 7) {
      if (pos >= header.size() || shift > 60)
        return fail(QStringLiteral("Invalid pack entry size."));
      byte = uchar(header[pos++]);
      if (shift == 60 && (byte & 0x70))
        return fail(QStringLiteral("Pack entry size overflow."));
      declaredSize |= quint64(byte & 127) << shift;
    }
    if (type == 6) {
      if (pos >= header.size())
        return fail(QStringLiteral("Missing delta base offset."));
      byte = uchar(header[pos++]);
      quint64 distance = byte & 127;
      while (byte & 128) {
        if (pos >= header.size() ||
            distance >= (std::numeric_limits<quint64>::max() >> 7) - 1)
          return fail(QStringLiteral("Invalid delta base offset."));
        byte = uchar(header[pos++]);
        distance = ((distance + 1) << 7) + (byte & 127);
      }
      if (!distance || distance > location.offset - 12)
        return fail(QStringLiteral("Delta base is outside the pack."));
      baseOffset = location.offset - distance;
    } else if (type == 7) {
      const int hashBytes = store.hashLength / 2;
      if (pos + hashBytes > header.size())
        return fail(QStringLiteral("Missing delta base hash."));
      result.baseHash = QString::fromLatin1(header.mid(pos, hashBytes).toHex());
      pos += hashBytes;
    } else {
      result.type = types[type];
    }
    for (quint32 i = 0; i < index.count; ++i) {
      if (stopped(cancelled))
        return fail(QStringLiteral("Cancelled."));
      GitObjectLocation other;
      if (!index.location(i, &other))
        return fail(QStringLiteral("Invalid pack index offset."));
      if (other.offset > location.offset)
        end = qMin(end, other.offset);
      if (baseOffset && other.offset == baseOffset)
        result.baseHash = other.hash;
    }
    if (baseOffset && result.baseHash.isEmpty())
      return fail(
          QStringLiteral("Delta base offset is absent from the index."));
    result.header = header.left(pos);
    result.dataOffset = location.offset + pos;
    if (result.dataOffset >= end)
      return fail(QStringLiteral("Truncated pack entry."));
  } else {
    bool known = false;
    for (const auto &directory : store.objectDirectories) {
      const QString path = QDir(directory).filePath(location.hash.left(2) +
                                                    '/' + location.hash.mid(2));
      known |= QFileInfo(path).absoluteFilePath() ==
               QFileInfo(location.loosePath).absoluteFilePath();
    }
    if (!known)
      return fail(QStringLiteral(
          "The loose file does not match this object's storage path."));
    result.location.representation = QStringLiteral("loose");
  }
  result.storedSize = end - (packed ? location.offset : 0);
  file.seek(packed ? location.offset : 0);
  result.storedPreview = file.read(qMin<qint64>(4096, result.storedSize));
  file.seek(result.dataOffset);
  auto inflated = inflateEntry(file, end, cancelled);
  if (!inflated.error.isEmpty())
    return fail(inflated.error);
  if (inflated.complete && result.dataOffset + inflated.consumed != end)
    return fail(
        QStringLiteral("Unexpected bytes after the selected zlib stream."));
  QByteArray content;
  if (!packed) {
    const int zero = inflated.data.indexOf('\0');
    const int space = inflated.data.indexOf(' ');
    if (zero < 0 || zero > 127 || space <= 0 || space >= zero)
      return fail(QStringLiteral("Invalid loose object header."));
    result.header = inflated.data.left(zero + 1);
    result.type = QString::fromLatin1(inflated.data.left(space));
    if (result.type != "commit" && result.type != "tree" &&
        result.type != "blob" && result.type != "tag")
      return fail(QStringLiteral("Invalid loose object type."));
    bool ok = false;
    declaredSize =
        inflated.data.mid(space + 1, zero - space - 1).toULongLong(&ok);
    if (!ok)
      return fail(QStringLiteral("Invalid loose object size."));
    content = inflated.data.mid(zero + 1);
  } else if (!result.baseHash.isEmpty()) {
    if (!inflated.complete)
      return fail(
          QStringLiteral("Delta instructions exceed the 32 MiB decode limit."));
    if (quint64(inflated.data.size()) != declaredSize)
      return fail(QStringLiteral(
          "Inflated delta size does not match its pack header."));
    const auto base =
        run(store.repositoryPath, {"cat-file", "--batch"},
            result.baseHash.toLatin1() + '\n', DecodeLimit + 1024, cancelled);
    const int newline = base.out.indexOf('\n');
    const auto fields = base.out.left(newline).split(' ');
    if (!base.ok || base.truncated || newline < 0 || fields.size() != 3)
      return fail(
          QStringLiteral(
              "Could not read delta base within the 32 MiB decode limit. %1")
              .arg(base.error));
    bool ok = false;
    const qint64 size = fields[2].toLongLong(&ok);
    if (!ok || size < 0 || size > DecodeLimit ||
        base.out.size() != newline + 1 + size + 1)
      return fail(
          QStringLiteral("Delta base is missing, incomplete or too large."));
    result.type = QString::fromLatin1(fields[1]);
    content = applyDelta(base.out.mid(newline + 1, size), inflated.data,
                         &result.deltaOperations, &result.error, cancelled);
    if (!result.error.isEmpty())
      return result;
    declaredSize = content.size();
  } else {
    content = std::move(inflated.data);
  }
  if (declaredSize > quint64(std::numeric_limits<qint64>::max()))
    return fail(QStringLiteral("Invalid decoded object size."));
  result.size = declaredSize;
  if (inflated.complete) {
    if (quint64(content.size()) != declaredSize)
      return fail(
          QStringLiteral("Decoded object size does not match its header."));
    QCryptographicHash hash(store.hashLength == 64 ? QCryptographicHash::Sha256
                                                   : QCryptographicHash::Sha1);
    hash.addData(result.type.toLatin1() + ' ' +
                 QByteArray::number(result.size) + '\0');
    hash.addData(content);
    result.hashVerified =
        QString::fromLatin1(hash.result().toHex()) == location.hash;
    if (!result.hashVerified)
      return fail(QStringLiteral(
          "Decoded contents do not match the selected object's hash."));
  }
  result.truncated = !inflated.complete || content.size() > PreviewLimit;
  result.preview = content.left(PreviewLimit);
  return result;
}

GitObjectDetails GitObjectDatabase::inspect(const QString &repositoryPath,
                                            const QString &hash,
                                            const Cancelled &cancelled) {
  GitObjectDetails details;

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
