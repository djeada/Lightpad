#include "projectsearch.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QRegularExpression>
#include <algorithm>

namespace {

QRegularExpression globExpression(const QString &glob,
                                  Qt::CaseSensitivity sensitivity) {
  QString expression = QStringLiteral("^");
  for (int i = 0; i < glob.size(); ++i) {
    const QChar ch = glob.at(i);
    if (ch == '*') {
      if (i + 1 < glob.size() && glob.at(i + 1) == '*') {
        ++i;
        if (i + 1 < glob.size() && glob.at(i + 1) == '/') {
          ++i;
          expression += QStringLiteral("(?:.*/)?");
        } else {
          expression += QStringLiteral(".*");
        }
      } else {
        expression += QStringLiteral("[^/]*");
      }
    } else if (ch == '?') {
      expression += QStringLiteral("[^/]");
    } else {
      expression += QRegularExpression::escape(QString(ch));
    }
  }
  expression += '$';
  return QRegularExpression(expression,
                            sensitivity == Qt::CaseInsensitive
                                ? QRegularExpression::CaseInsensitiveOption
                                : QRegularExpression::NoPatternOption);
}

struct Glob {
  QRegularExpression expression;
  bool pathPattern;
};

QVector<Glob> parseGlobs(const QString &text) {
  QVector<Glob> result;
  for (QString pattern : text.split(',', Qt::SkipEmptyParts)) {
    pattern = QDir::fromNativeSeparators(pattern.trimmed());
    if (pattern.startsWith("./")) {
      pattern.remove(0, 2);
    }
    if (!pattern.isEmpty()) {
      result.append({globExpression(pattern, Qt::CaseInsensitive),
                     pattern.contains('/')});
    }
  }
  return result;
}

bool matchesGlobs(const QVector<Glob> &globs, const QString &relativePath,
                  const QString &name, bool directory) {
  return std::any_of(globs.cbegin(), globs.cend(), [&](const Glob &glob) {
    const QString &subject = glob.pathPattern ? relativePath : name;
    return glob.expression.match(subject).hasMatch() ||
           (directory && glob.expression.match(subject + '/').hasMatch());
  });
}

bool betterMatch(const ProjectSearch::Match &a, const ProjectSearch::Match &b) {
  if (a.score != b.score) {
    return a.score > b.score;
  }
  const int order =
      QString::compare(a.relativePath, b.relativePath, Qt::CaseInsensitive);
  if (order != 0) {
    return order < 0;
  }
  if (a.relativePath != b.relativePath) {
    return a.relativePath < b.relativePath;
  }
  return a.line == b.line ? a.column < b.column : a.line < b.line;
}

bool isBinary(const QByteArray &data) {
  if (data.contains('\0')) {
    return true;
  }
  const int count = qMin<qsizetype>(data.size(), 8192);
  int controls = 0;
  for (int i = 0; i < count; ++i) {
    const unsigned char ch = static_cast<unsigned char>(data.at(i));
    if (ch < 32 && ch != '\n' && ch != '\r' && ch != '\t' && ch != '\f') {
      ++controls;
    }
  }
  return controls > count / 20;
}

bool withinRoot(const QString &root, const QString &path) {
  const QString relative = QDir(root).relativeFilePath(path);
  return !QDir::isAbsolutePath(relative) && relative != ".." &&
         !relative.startsWith("../");
}

} // namespace

int ProjectSearch::fileNameScore(const QString &query,
                                 const QString &relativePath,
                                 Qt::CaseSensitivity sensitivity) {
  const QString name = relativePath.section('/', -1);
  const QStringList tokens = query.trimmed().split(
      QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
  int score = 0;
  for (const QString &token : tokens) {
    if (token.contains('*') || token.contains('?')) {
      const auto expression = globExpression(token, sensitivity);
      if (!expression.match(token.contains('/') ? relativePath : name)
               .hasMatch()) {
        return -1;
      }
      score += 600;
    } else if (name.compare(token, sensitivity) == 0) {
      score += 1000;
    } else if (name.startsWith(token, sensitivity)) {
      score += 800;
    } else if (name.contains(token, sensitivity)) {
      score += 600;
    } else if (relativePath.contains(token, sensitivity)) {
      score += 400;
    } else {

      int position = 0;
      int gaps = 0;
      for (const QChar ch : token) {
        const int next = relativePath.indexOf(ch, position, sensitivity);
        if (next < 0) {
          return -1;
        }
        gaps += next - position;
        position = next + 1;
      }
      score += qMax(1, 200 - gaps);
    }
  }
  return score;
}

ProjectSearch::Results
ProjectSearch::search(const Options &options,
                      const std::function<bool()> &isCancelled,
                      const std::function<void(int)> &progress) {
  Results results;
  const auto cancelled = [&]() {
    if (isCancelled && isCancelled()) {
      results.cancelled = true;
      return true;
    }
    return false;
  };
  if (cancelled() || options.query.isEmpty() ||
      (options.mode == Mode::FileNames && options.query.trimmed().isEmpty())) {
    return results;
  }

  const QFileInfo rootInfo(options.rootPath);
  if (options.rootPath.isEmpty() || !rootInfo.isDir()) {
    results.error = QObject::tr("Open a project folder to search.");
    return results;
  }
  const QString root = rootInfo.canonicalFilePath();
  const QString scope = options.scopePath.isEmpty()
                            ? root
                            : QFileInfo(options.scopePath).canonicalFilePath();
  if (scope.isEmpty() || !QFileInfo(scope).isDir() ||
      !withinRoot(root, scope)) {
    results.error = QObject::tr("Choose a search folder inside this project.");
    return results;
  }

  QRegularExpression pattern;
  if (options.mode == Mode::Contents) {
    QString expression = options.regularExpression
                             ? options.query
                             : QRegularExpression::escape(options.query);
    if (options.wholeWord) {
      expression =
          QStringLiteral("(?<![\\p{L}\\p{N}_])(?:%1)(?![\\p{L}\\p{N}_])")
              .arg(expression);
    }
    pattern = QRegularExpression(
        expression, QRegularExpression::UseUnicodePropertiesOption |
                        (options.caseSensitive
                             ? QRegularExpression::NoPatternOption
                             : QRegularExpression::CaseInsensitiveOption));
    if (!pattern.isValid()) {
      results.error = QObject::tr("Invalid regular expression: %1")
                          .arg(pattern.errorString());
      return results;
    }
  }

  const QVector<Glob> includes = parseGlobs(options.includes);
  const QVector<Glob> excludes = parseGlobs(options.excludes);
  const QDir rootDir(root);
  QStringList pending{scope};
  const int limit = qMax(1, options.maxResults);
  while (!pending.isEmpty()) {
    if (cancelled()) {
      break;
    }
    const QDir directory(pending.takeLast());
    QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot;
    if (options.includeHidden) {
      filters |= QDir::Hidden | QDir::System;
    }
    const QFileInfoList entries = directory.entryInfoList(
        filters, QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &entry : entries) {
      if (cancelled()) {
        break;
      }
      const QString relative =
          rootDir.relativeFilePath(entry.absoluteFilePath());
      const QString name = entry.fileName();
      const bool isDirectory = entry.isDir();
      if (matchesGlobs(excludes, relative, name, isDirectory)) {
        continue;
      }
      if (isDirectory && !entry.isSymLink()) {
        pending.append(entry.absoluteFilePath());
      }
      if (!isDirectory) {
        ++results.scannedFiles;
        if (progress && (results.scannedFiles % 128) == 0) {
          progress(results.scannedFiles);
        }
      }
      if (!includes.isEmpty() &&
          !matchesGlobs(includes, relative, name, isDirectory)) {
        continue;
      }
      if (options.mode == Mode::FileNames) {
        if (isDirectory && !options.includeDirectories) {
          continue;
        }
        const int score = fileNameScore(
            options.query, relative,
            options.caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
        if (score >= 0) {
          Match match;
          match.filePath = entry.absoluteFilePath();
          match.relativePath = relative;
          match.isDirectory = isDirectory;
          match.score = score;
          results.matches.append(match);

          if (results.matches.size() > 2 * limit) {
            std::sort(results.matches.begin(), results.matches.end(),
                      betterMatch);
            results.matches.resize(limit);
            results.truncated = true;
          }
        }
        continue;
      }
      if (isDirectory) {
        continue;
      }
      if (!entry.isFile() || entry.isSymLink() ||
          entry.size() > options.maxFileBytes) {
        ++results.skippedFiles;
        continue;
      }
      QFile file(entry.absoluteFilePath());
      if (!file.open(QIODevice::ReadOnly)) {
        ++results.skippedFiles;
        continue;
      }
      const QByteArray data =
          file.read(qMax<qint64>(0, options.maxFileBytes) + 1);
      if (file.error() != QFileDevice::NoError ||
          data.size() > options.maxFileBytes || isBinary(data)) {
        ++results.skippedFiles;
        continue;
      }
      QString content = QString::fromUtf8(data);
      if (content.startsWith(QChar(0xfeff))) {
        content.remove(0, 1);
      }
      int lineNumber = 0;
      qsizetype lineStart = 0;
      while (lineStart <= content.size()) {
        if (cancelled()) {
          break;
        }
        const qsizetype newline = content.indexOf('\n', lineStart);
        QString line =
            content.mid(lineStart, newline < 0 ? -1 : newline - lineStart);
        if (line.endsWith('\r')) {
          line.chop(1);
        }
        ++lineNumber;
        auto matches = pattern.globalMatch(line);
        while (matches.hasNext()) {
          if (cancelled()) {
            break;
          }
          const auto found = matches.next();
          if (results.matches.size() >= limit) {
            results.truncated = true;
            return results;
          }
          Match match;
          match.filePath = entry.absoluteFilePath();
          match.relativePath = relative;
          match.line = lineNumber;
          match.column = found.capturedStart() + 1;
          match.length = found.capturedLength();
          const int previewOffset = qMax(0, match.column - 1 - 70);
          match.preview = line.mid(previewOffset, 240);
          match.previewStart = match.column - 1 - previewOffset;
          match.previewLength = qMin(match.length, int(match.preview.size()) -
                                                       match.previewStart);
          if (previewOffset > 0) {
            match.preview.prepend(QChar(0x2026));
            ++match.previewStart;
          }
          if (previewOffset + 240 < line.size()) {
            match.preview.append(QChar(0x2026));
          }
          results.matches.append(match);
        }
        if (newline < 0) {
          break;
        }
        lineStart = newline + 1;
      }
    }
  }
  std::sort(results.matches.begin(), results.matches.end(), betterMatch);
  if (results.matches.size() > limit) {
    results.matches.resize(limit);
    results.truncated = true;
  }
  return results;
}
