#include "fileopenguard.h"

#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QStringDecoder>

namespace FileOpenGuard {

bool looksBinary(const QByteArray &sample) { return sample.contains('\0'); }

QString formatSize(qint64 bytes) {
  constexpr double kb = 1024.0;
  constexpr double mb = kb * 1024.0;
  constexpr double gb = mb * 1024.0;

  if (bytes >= static_cast<qint64>(gb)) {
    return QObject::tr("%1 GB").arg(bytes / gb, 0, 'f', 1);
  }
  if (bytes >= static_cast<qint64>(mb)) {
    return QObject::tr("%1 MB").arg(bytes / mb, 0, 'f', 1);
  }
  if (bytes >= static_cast<qint64>(kb)) {
    return QObject::tr("%1 KB").arg(bytes / kb, 0, 'f', 1);
  }
  return QObject::tr("%n byte(s)", "", static_cast<int>(bytes));
}

Assessment assess(const QString &filePath, qint64 largeThreshold) {
  Assessment result;

  const QFileInfo info(filePath);
  if (!info.exists() || !info.isFile()) {
    result.verdict = Verdict::Unreadable;
    result.summary = QObject::tr("There is no file at %1.").arg(filePath);
    return result;
  }

  result.exists = true;
  result.sizeBytes = info.size();

  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    result.verdict = Verdict::Unreadable;
    result.summary = QObject::tr("%1 cannot be read.").arg(info.fileName());
    return result;
  }

  const QByteArray sample = file.read(BinarySampleBytes);
  file.close();

  result.looksBinary = looksBinary(sample);

  if (result.looksBinary) {
    result.verdict = Verdict::Binary;
    result.summary = QObject::tr("%1 is not a text file (%2).")
                         .arg(info.fileName(), formatSize(result.sizeBytes));
    result.consequence = QObject::tr(
        "Opening it in the editor shows unreadable characters, and saving "
        "afterwards would corrupt it. It can be opened read-only if you want "
        "to look at it.");
    return result;
  }

  if (result.sizeBytes >= largeThreshold) {
    result.verdict = Verdict::Large;
    result.summary = QObject::tr("%1 is %2, which is large for the editor.")
                         .arg(info.fileName(), formatSize(result.sizeBytes));
    result.consequence =
        QObject::tr("Loading it whole needs several times its size in memory "
                    "and can stop Lightpad responding. Opening the first %1 "
                    "read-only is usually enough to see what is in it.")
            .arg(formatSize(PreviewBytes));
    return result;
  }

  result.verdict = Verdict::Open;
  return result;
}

QString readTextCapped(const QString &filePath, qint64 maxBytes,
                       bool *truncated, bool *ok) {
  if (truncated) {
    *truncated = false;
  }
  if (ok) {
    *ok = false;
  }

  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    return QString();
  }

  const qint64 total = file.size();
  const qint64 limit = maxBytes <= 0 ? total : qMin(maxBytes, total);

  QByteArray bytes = file.read(limit);
  file.close();

  if (truncated) {
    *truncated = limit < total;
  }
  if (ok) {
    *ok = true;
  }

  QStringDecoder decoder(QStringDecoder::Utf8);
  QString text = decoder.decode(bytes);
  return text;
}

} // namespace FileOpenGuard
