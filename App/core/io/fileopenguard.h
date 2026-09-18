#ifndef FILEOPENGUARD_H
#define FILEOPENGUARD_H

#include <QByteArray>
#include <QString>

namespace FileOpenGuard {

constexpr qint64 LargeFileThreshold = 8LL * 1024 * 1024;

constexpr qint64 PreviewBytes = 2LL * 1024 * 1024;

constexpr int BinarySampleBytes = 8000;

enum class Verdict {

  Open,

  Large,

  Binary,

  Unreadable,
};

struct Assessment {
  Verdict verdict = Verdict::Open;

  qint64 sizeBytes = 0;
  bool looksBinary = false;
  bool exists = false;

  QString summary;

  QString consequence;

  bool needsConfirmation() const {
    return verdict == Verdict::Large || verdict == Verdict::Binary;
  }
};

bool looksBinary(const QByteArray &sample);

QString formatSize(qint64 bytes);

Assessment assess(const QString &filePath,
                  qint64 largeThreshold = LargeFileThreshold);

QString readTextCapped(const QString &filePath, qint64 maxBytes,
                       bool *truncated = nullptr, bool *ok = nullptr);

} // namespace FileOpenGuard

#endif
