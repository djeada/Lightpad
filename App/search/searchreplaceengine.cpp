#include "searchreplaceengine.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QObject>
#include <QSaveFile>
#include <QSet>
#include <QStringDecoder>
#include <algorithm>
#include <limits>

namespace {
QString decodeEscapes(const QString &text) {
  QString result;
  for (int i = 0; i < text.size(); ++i) {
    if (text.at(i) == '\\' && i + 1 < text.size()) {
      const QChar next = text.at(i + 1);
      if (next == 'n' || next == 'r' || next == 't' || next == '\\') {
        result += next == 'n'   ? QChar('\n')
                  : next == 'r' ? QChar('\r')
                  : next == 't' ? QChar('\t')
                                : QChar('\\');
        ++i;
        continue;
      }
    }
    result += text.at(i);
  }
  return result;
}

bool decodeFile(const QByteArray &bytes, QString *content) {
  if (bytes.contains('\0'))
    return false;
  const QByteArray payload =
      bytes.startsWith("\xef\xbb\xbf") ? bytes.mid(3) : bytes;
  QStringDecoder decoder(QStringDecoder::Utf8,
                         QStringConverter::Flag::Stateless);
  *content = decoder(payload);
  if (decoder.hasError())
    return false;
  int controls = 0;
  const int count = qMin(8192, int(content->size()));
  for (int i = 0; i < count; ++i) {
    const QChar ch = content->at(i);
    if (ch.unicode() < 32 && ch != '\n' && ch != '\r' && ch != '\t' &&
        ch != '\f')
      ++controls;
  }
  return controls <= count / 20;
}

QByteArray encodedChange(const SearchReplace::Change &change) {
  QString original;
  if (!decodeFile(change.source.bytes, &original))
    return {};
  QVector<int> offsets;
  offsets.reserve(change.source.content.size() + 1);
  for (int i = 0; i < original.size(); ++i) {
    offsets.append(i);
    if (original.at(i) == '\r' && i + 1 < original.size() &&
        original.at(i + 1) == '\n')
      ++i;
  }
  offsets.append(original.size());
  const int crlf = original.count("\r\n");
  const int lf = original.count('\n') - crlf;
  const int cr = original.count('\r') - crlf;
  const QString newline = crlf > 0 && crlf >= lf && crlf >= cr ? "\r\n"
                          : cr > lf                            ? "\r"
                                                               : "\n";
  for (auto it = change.edits.crbegin(); it != change.edits.crend(); ++it) {
    QString replacement = it->replacement;
    replacement.replace("\n", newline);
    original.replace(offsets.at(it->start),
                     offsets.at(it->start + it->length) - offsets.at(it->start),
                     replacement);
  }
  QByteArray result = original.toUtf8();
  if (change.source.bytes.startsWith("\xef\xbb\xbf"))
    result.prepend("\xef\xbb\xbf");
  return result;
}

bool readExact(const QString &path, QByteArray *bytes, QString *error) {
  const QFileInfo info(path);
  if (!info.isFile() || info.isSymLink()) {
    *error = QObject::tr(
                 "The file is missing, linked, or no longer a regular file: %1")
                 .arg(path);
    return false;
  }
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    *error = QObject::tr("Cannot read %1: %2").arg(path, file.errorString());
    return false;
  }
  *bytes = file.readAll();
  if (file.error() != QFileDevice::NoError) {
    *error = QObject::tr("Cannot read %1: %2").arg(path, file.errorString());
    return false;
  }
  return true;
}

bool validateDisk(const SearchReplace::DiskChange &change, QString *error) {
  const QFileInfo info(change.path);
  if (!info.isWritable() ||
      !(info.permissions() & (QFileDevice::WriteUser | QFileDevice::WriteGroup |
                              QFileDevice::WriteOther))) {
    *error = QObject::tr("The file is read-only: %1").arg(change.path);
    return false;
  }
  QByteArray current;
  if (!readExact(change.path, &current, error))
    return false;
  if (current != change.before) {
    *error =
        QObject::tr("%1 changed since the preview. Refresh before replacing.")
            .arg(change.path);
    return false;
  }
  return true;
}

bool atomicWrite(const QString &path, const QByteArray &bytes, QString *error) {
  QSaveFile file(path);
  file.setDirectWriteFallback(false);
  if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() ||
      !file.commit()) {
    *error =
        QObject::tr("Could not write %1: %2").arg(path, file.errorString());
    return false;
  }
  return true;
}
} // namespace

QString SearchReplace::fileKey(const QString &path) {
  if (path.isEmpty())
    return {};
  const QFileInfo info(path);
  const QString canonical = info.canonicalFilePath();
  return canonical.isEmpty() ? QDir::cleanPath(info.absoluteFilePath())
                             : canonical;
}

QString SearchReplace::normalizedText(const QString &text) {
  QString result = text;
  result.replace("\r\n", "\n");
  result.replace('\r', '\n');
  return result;
}

QRegularExpression SearchReplace::pattern(const Options &options) {
  QString expression =
      options.regex
          ? options.query
          : QRegularExpression::escape(options.escapeSequences
                                           ? decodeEscapes(options.query)
                                           : options.query);
  if (options.wholeWord) {
    expression =
        QStringLiteral(
            "(?<![\\p{L}\\p{N}\\p{M}_])(?:%1)(?![\\p{L}\\p{N}\\p{M}_])")
            .arg(expression);
  }
  auto flags = QRegularExpression::UseUnicodePropertiesOption |
               QRegularExpression::MultilineOption;
  if (!options.matchCase)
    flags |= QRegularExpression::CaseInsensitiveOption;
  if (options.dotMatchesNewline)
    flags |= QRegularExpression::DotMatchesEverythingOption;
  return QRegularExpression(expression, flags);
}

QString SearchReplace::preserveCase(const QString &replacement,
                                    const QString &matched) {
  bool upper = false, lower = false;
  for (const QChar ch : matched) {
    upper |= ch.isUpper();
    lower |= ch.isLower();
  }
  if (upper && !lower)
    return replacement.toUpper();
  if (lower && !upper)
    return replacement.toLower();
  if (!matched.isEmpty() && matched.at(0).isUpper() &&
      matched.mid(1) == matched.mid(1).toLower()) {
    QString title = replacement.toLower();
    if (!title.isEmpty())
      title[0] = title.at(0).toUpper();
    return title;
  }
  return replacement;
}

QString SearchReplace::replacementText(const Options &options,
                                       const QRegularExpressionMatch &match) {
  QString result =
      options.regex ? FindReplaceSearch::expandRegexReplacement(
                          options.replacement, match, options.escapeSequences)
      : options.escapeSequences ? decodeEscapes(options.replacement)
                                : options.replacement;
  result = normalizedText(result);
  return options.preserveCase ? preserveCase(result, match.captured()) : result;
}

SearchReplace::Results
SearchReplace::search(const Request &request,
                      const std::function<bool()> &cancelled,
                      const std::function<void(int)> &progress) {
  Results results;
  const auto stopped = [&]() {
    if (cancelled && cancelled()) {
      results.cancelled = true;
      return true;
    }
    return false;
  };
  if (stopped() || request.options.query.isEmpty())
    return results;
  const auto expression = pattern(request.options);
  if (!expression.isValid()) {
    results.error = QObject::tr("Invalid regular expression: %1 (position %2)")
                        .arg(expression.errorString())
                        .arg(expression.patternErrorOffset());
    return results;
  }
  QHash<QString, Snapshot> buffers;
  QSet<QString> conflictingBuffers;
  QStringList order;
  for (const auto &buffer : request.buffers) {
    if (buffers.contains(buffer.key)) {
      if (buffers.value(buffer.key).content != buffer.content) {
        conflictingBuffers.insert(buffer.key);
      }
      continue;
    }
    buffers.insert(buffer.key, buffer);
    order.append(buffer.key);
  }
  if (request.scope == Scope::Project) {
    auto files = request.files;
    files.query = "*";
    files.mode = ProjectSearch::Mode::FileNames;
    files.includeDirectories = false;
    files.maxResults = std::numeric_limits<int>::max() / 4;
    const auto listing = ProjectSearch::search(files, cancelled, progress);
    if (listing.cancelled) {
      results.cancelled = true;
      return results;
    }
    if (!listing.error.isEmpty()) {
      results.error = listing.error;
      return results;
    }
    order.clear();
    for (const auto &file : listing.matches)
      order.append(file.filePath);
  } else if (request.scope == Scope::CurrentFile ||
             request.scope == Scope::Selection) {
    order = buffers.contains(request.currentKey)
                ? QStringList{request.currentKey}
                : QStringList{};
  }
  qint64 retainedBytes = 0;
  QSet<QString> visited;
  for (const QString &candidate : order) {
    if (stopped())
      return results;
    const QString key =
        request.scope == Scope::Project ? fileKey(candidate) : candidate;
    if (visited.contains(key))
      continue;
    visited.insert(key);
    if (conflictingBuffers.contains(key)) {
      const auto buffer = buffers.value(key);
      results.error =
          QObject::tr("Open editors disagree about %1. Resolve their unsaved "
                      "changes first.")
              .arg(buffer.path.isEmpty() ? buffer.label : buffer.path);
      return results;
    }
    Snapshot source = buffers.value(key);
    if (source.key.isEmpty()) {
      source.key = key;
      source.path = candidate;
      source.label = QFileInfo(candidate).fileName();
      const QFileInfo info(candidate);
      if (!info.isFile() || info.isSymLink() ||
          info.size() > request.options.maxFileBytes) {
        ++results.skippedFiles;
        continue;
      }
      QFile file(candidate);
      if (!file.open(QIODevice::ReadOnly)) {
        ++results.skippedFiles;
        continue;
      }
      source.bytes = file.read(request.options.maxFileBytes + 1);
      if (file.error() != QFileDevice::NoError ||
          source.bytes.size() > request.options.maxFileBytes ||
          !decodeFile(source.bytes, &source.content)) {
        ++results.skippedFiles;
        continue;
      }
      source.content = normalizedText(source.content);
    }
    ++results.scannedFiles;
    if (progress && (results.scannedFiles % 64) == 0)
      progress(results.scannedFiles);
    const int start =
        request.scope == Scope::Selection
            ? qBound(0, request.selectionStart, int(source.content.size()))
            : 0;
    const int end =
        request.scope == Scope::Selection && request.selectionEnd >= 0
            ? qBound(start, request.selectionEnd, int(source.content.size()))
            : source.content.size();
    QVector<int> lineStarts{0};
    for (int i = 0; i < source.content.size(); ++i)
      if (source.content.at(i) == '\n')
        lineStarts.append(i + 1);
    FileResult found;
    auto matches = expression.globalMatch(source.content, start);
    while (matches.hasNext()) {
      if (stopped())
        return results;
      const auto match = matches.next();
      if (match.capturedStart() > end)
        break;
      if (match.capturedEnd() > end)
        continue;
      if (results.matchCount >= request.options.maxMatches) {
        results.truncated = true;
        break;
      }
      if (found.occurrences.isEmpty()) {
        const qint64 bytes =
            source.bytes.size() + source.content.size() * qint64(sizeof(QChar));
        if (retainedBytes + bytes > request.options.maxSnapshotBytes) {
          results.truncated = true;
          break;
        }
        retainedBytes += bytes;
        found.source = source;
      }
      const int offset = match.capturedStart();
      const int line =
          std::upper_bound(lineStarts.cbegin(), lineStarts.cend(), offset) -
          lineStarts.cbegin() - 1;
      const int newline = source.content.indexOf('\n', lineStarts.at(line));
      Occurrence occurrence;
      occurrence.location = {
          source.path,
          line + 1,
          offset - lineStarts.at(line) + 1,
          offset,
          int(match.capturedLength()),
          source.content.mid(lineStarts.at(line),
                             newline < 0 ? -1 : newline - lineStarts.at(line))};
      occurrence.replacement = replacementText(request.options, match);
      const qint64 replacementBytes =
          occurrence.replacement.size() * qint64(sizeof(QChar));
      if (retainedBytes + replacementBytes > request.options.maxSnapshotBytes) {
        results.truncated = true;
        break;
      }
      retainedBytes += replacementBytes;
      found.occurrences.append(occurrence);
      ++results.matchCount;
    }
    if (!found.occurrences.isEmpty())
      results.files.append(found);
    if (results.truncated)
      break;
  }
  return results;
}

QVector<SearchReplace::Change>
SearchReplace::selectedChanges(const Results &results) {
  QVector<Change> changes;
  if (results.truncated || results.cancelled || !results.error.isEmpty())
    return changes;
  for (const auto &file : results.files) {
    Change change;
    change.source = file.source;
    change.after = file.source.content;
    for (const auto &occurrence : file.occurrences) {
      if (occurrence.selected &&
          file.source.content.mid(occurrence.location.matchStart,
                                  occurrence.location.matchLength) !=
              occurrence.replacement) {
        change.edits.append({occurrence.location.matchStart,
                             occurrence.location.matchLength,
                             occurrence.replacement});
      }
    }
    for (auto it = change.edits.crbegin(); it != change.edits.crend(); ++it)
      change.after.replace(it->start, it->length, it->replacement);
    if (change.edits.isEmpty())
      continue;
    if (!change.source.openBuffer)
      change.afterBytes = encodedChange(change);
    changes.append(change);
  }
  return changes;
}

SearchReplace::DiskReport
SearchReplace::applyDiskChanges(const QVector<DiskChange> &changes,
                                const std::function<bool()> &cancelled,
                                const std::function<void(int, int)> &progress) {
  DiskReport report;
  for (const auto &change : changes) {
    if (cancelled && cancelled()) {
      report.error = QObject::tr("Replacement cancelled.");
      return report;
    }
    if (!validateDisk(change, &report.error))
      return report;
  }
  int written = 0;
  for (const auto &change : changes) {
    if (progress)
      progress(written, changes.size());
    if (cancelled && cancelled()) {
      report.error = QObject::tr("Replacement cancelled.");
      break;
    }
    if (!validateDisk(change, &report.error) ||
        !atomicWrite(change.path, change.after, &report.error))
      break;
    report.touchedPaths.append(change.path);
    ++written;
  }
  if (written == changes.size() && !(cancelled && cancelled())) {
    report.success = true;
    return report;
  }
  if (report.error.isEmpty())
    report.error = QObject::tr("Replacement cancelled.");
  for (int i = written - 1; i >= 0; --i) {
    const auto &change = changes.at(i);
    const DiskChange reverse{change.path, change.after, change.before};
    QString error;
    if (!validateDisk(reverse, &error) ||
        !atomicWrite(reverse.path, reverse.after, &error)) {
      report.retained.prepend(change);
      report.error +=
          QObject::tr("\nCould not roll back %1: %2").arg(change.path, error);
    }
  }
  return report;
}
