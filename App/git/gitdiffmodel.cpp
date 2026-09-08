#include "gitdiffmodel.h"
#include <QRegularExpression>

namespace {

const QRegularExpression &hunkHeaderPattern() {
  static const QRegularExpression pattern(
      QStringLiteral("^@@ -(\\d+)(?:,(\\d+))? \\+(\\d+)(?:,(\\d+))? @@(.*)$"));
  return pattern;
}

QString renderHunkHeader(int oldStart, int oldCount, int newStart, int newCount,
                         const QString &section) {
  const auto range = [](int start, int count) {
    if (count == 1) {
      return QString::number(start);
    }
    return QStringLiteral("%1,%2").arg(start).arg(count);
  };
  QString header =
      QStringLiteral("@@ -%1 +%2 @@")
          .arg(range(oldStart, oldCount), range(newStart, newCount));
  if (!section.isEmpty()) {
    header += section;
  }
  return header;
}

QString lineMarker(GitDiffLineType type) {
  switch (type) {
  case GitDiffLineType::Added:
    return QStringLiteral("+");
  case GitDiffLineType::Removed:
    return QStringLiteral("-");
  case GitDiffLineType::NoNewline:
    return QStringLiteral("\\");
  case GitDiffLineType::Context:
    break;
  }
  return QStringLiteral(" ");
}

QString renderPatch(const GitDiffFile &file, const QStringList &hunkBlocks) {
  if (hunkBlocks.isEmpty()) {
    return QString();
  }
  QStringList out = file.headerLines;
  out += hunkBlocks;
  return out.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

} // namespace

int GitHunk::changedLineCount() const {
  int count = 0;
  for (const GitDiffLine &line : lines) {
    if (line.isChange()) {
      ++count;
    }
  }
  return count;
}

int GitHunk::additions() const {
  int count = 0;
  for (const GitDiffLine &line : lines) {
    if (line.type == GitDiffLineType::Added) {
      ++count;
    }
  }
  return count;
}

int GitHunk::deletions() const {
  int count = 0;
  for (const GitDiffLine &line : lines) {
    if (line.type == GitDiffLineType::Removed) {
      ++count;
    }
  }
  return count;
}

int GitDiffFile::additions() const {
  int count = 0;
  for (const GitHunk &hunk : hunks) {
    count += hunk.additions();
  }
  return count;
}

int GitDiffFile::deletions() const {
  int count = 0;
  for (const GitHunk &hunk : hunks) {
    count += hunk.deletions();
  }
  return count;
}

QList<GitDiffFile> parseUnifiedDiffFiles(const QString &diffText) {
  QList<GitDiffFile> files;
  if (diffText.isEmpty()) {
    return files;
  }

  const QStringList lines = diffText.split(QLatin1Char('\n'));
  GitDiffFile current;
  GitHunk hunk;
  bool inHunk = false;
  int oldLine = 0;
  int newLine = 0;

  const auto flushHunk = [&]() {
    if (inHunk) {
      current.hunks.append(hunk);
      inHunk = false;
    }
    hunk = GitHunk();
  };
  const auto flushFile = [&]() {
    flushHunk();
    if (current.valid) {
      files.append(current);
    }
    current = GitDiffFile();
  };

  for (const QString &line : lines) {
    if (line.startsWith(QLatin1String("diff --git "))) {
      flushFile();
      current.valid = true;
      current.headerLines << line;

      const QString paths = line.mid(11);
      const int split = paths.lastIndexOf(QStringLiteral(" b/"));
      if (split > 0) {
        current.oldPath = paths.left(split);
        if (current.oldPath.startsWith(QLatin1String("a/"))) {
          current.oldPath = current.oldPath.mid(2);
        }
        current.path = paths.mid(split + 3);
      }
      continue;
    }

    if (!current.valid) {
      continue;
    }

    if (line.startsWith(QLatin1Char('@'))) {
      const QRegularExpressionMatch match = hunkHeaderPattern().match(line);
      if (match.hasMatch()) {
        flushHunk();
        inHunk = true;
        hunk.header = line;
        hunk.oldStart = match.captured(1).toInt();
        hunk.oldCount =
            match.captured(2).isEmpty() ? 1 : match.captured(2).toInt();
        hunk.newStart = match.captured(3).toInt();
        hunk.newCount =
            match.captured(4).isEmpty() ? 1 : match.captured(4).toInt();
        hunk.section = match.captured(5);
        oldLine = hunk.oldStart;
        newLine = hunk.newStart;
        continue;
      }
    }

    if (!inHunk) {
      if (line.startsWith(QLatin1String("Binary files")) ||
          line.startsWith(QLatin1String("GIT binary patch"))) {
        current.isBinary = true;
      } else if (line.startsWith(QLatin1String("new file mode"))) {
        current.isNew = true;
      } else if (line.startsWith(QLatin1String("deleted file mode"))) {
        current.isDeleted = true;
      } else if (line.startsWith(QLatin1String("rename from")) ||
                 line.startsWith(QLatin1String("rename to")) ||
                 line.startsWith(QLatin1String("copy from")) ||
                 line.startsWith(QLatin1String("copy to"))) {
        current.isRenamed = true;
      }
      if (!line.isEmpty()) {
        current.headerLines << line;
      }
      continue;
    }

    GitDiffLine parsed;
    if (line.startsWith(QLatin1Char('+'))) {
      parsed.type = GitDiffLineType::Added;
      parsed.text = line.mid(1);
      parsed.newLine = newLine++;
    } else if (line.startsWith(QLatin1Char('-'))) {
      parsed.type = GitDiffLineType::Removed;
      parsed.text = line.mid(1);
      parsed.oldLine = oldLine++;
    } else if (line.startsWith(QLatin1Char('\\'))) {
      parsed.type = GitDiffLineType::NoNewline;
      parsed.text = line.mid(1).trimmed();
    } else if (line.startsWith(QLatin1Char(' '))) {
      parsed.type = GitDiffLineType::Context;
      parsed.text = line.mid(1);
      parsed.oldLine = oldLine++;
      parsed.newLine = newLine++;
    } else {

      flushHunk();
      continue;
    }
    hunk.lines.append(parsed);
  }

  flushFile();
  return files;
}

GitDiffFile parseUnifiedDiff(const QString &diffText) {
  const QList<GitDiffFile> files = parseUnifiedDiffFiles(diffText);
  return files.isEmpty() ? GitDiffFile() : files.first();
}

QString buildHunkPatch(const GitDiffFile &file, const QList<int> &hunkIndices) {
  QStringList blocks;

  int delta = 0;

  for (int index = 0; index < file.hunks.size(); ++index) {
    const GitHunk &hunk = file.hunks.at(index);
    if (!hunkIndices.contains(index)) {
      continue;
    }

    QStringList block;
    block << renderHunkHeader(hunk.oldStart, hunk.oldCount,
                              hunk.oldStart + delta, hunk.newCount,
                              hunk.section);
    for (const GitDiffLine &line : hunk.lines) {
      block << lineMarker(line.type) + line.text;
    }
    blocks << block.join(QLatin1Char('\n'));
    delta += hunk.newCount - hunk.oldCount;
  }

  return renderPatch(file, blocks);
}

QString buildLinePatchMulti(const GitDiffFile &file,
                            const QList<QPair<int, QSet<int>>> &selection,
                            bool reverse) {
  QStringList blocks;
  int delta = 0;

  for (int index = 0; index < file.hunks.size(); ++index) {
    const GitHunk &hunk = file.hunks.at(index);

    const QSet<int> *selected = nullptr;
    for (const auto &entry : selection) {
      if (entry.first == index) {
        selected = &entry.second;
        break;
      }
    }

    if (!selected || selected->isEmpty()) {
      continue;
    }

    QStringList body;
    int oldCount = 0;
    int newCount = 0;
    bool keptAnyChange = false;

    for (int lineIndex = 0; lineIndex < hunk.lines.size(); ++lineIndex) {
      const GitDiffLine &line = hunk.lines.at(lineIndex);

      if (line.type == GitDiffLineType::NoNewline) {
        body << QStringLiteral("\\ ") + line.text;
        continue;
      }
      if (line.type == GitDiffLineType::Context) {
        body << QStringLiteral(" ") + line.text;
        ++oldCount;
        ++newCount;
        continue;
      }

      const bool isSelected = selected->contains(lineIndex);
      if (isSelected) {
        keptAnyChange = true;
        body << lineMarker(line.type) + line.text;
        if (line.type == GitDiffLineType::Added) {
          ++newCount;
        } else {
          ++oldCount;
        }
        continue;
      }

      const bool keepAsContext = reverse
                                     ? line.type == GitDiffLineType::Added
                                     : line.type == GitDiffLineType::Removed;
      if (keepAsContext) {
        body << QStringLiteral(" ") + line.text;
        ++oldCount;
        ++newCount;
      }
    }

    if (!keptAnyChange) {
      continue;
    }

    QStringList block;
    block << renderHunkHeader(hunk.oldStart, oldCount, hunk.oldStart + delta,
                              newCount, hunk.section);
    block += body;
    blocks << block.join(QLatin1Char('\n'));

    delta += newCount - oldCount;
  }

  return renderPatch(file, blocks);
}

QString buildLinePatch(const GitDiffFile &file, int hunkIndex,
                       const QSet<int> &selectedLineIndices, bool reverse) {
  if (hunkIndex < 0 || hunkIndex >= file.hunks.size()) {
    return QString();
  }
  QList<QPair<int, QSet<int>>> selection;
  selection.append({hunkIndex, selectedLineIndices});
  return buildLinePatchMulti(file, selection, reverse);
}
