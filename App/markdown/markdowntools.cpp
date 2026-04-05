#include "markdowntools.h"
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QStringList>
#include <QTextStream>

static const QRegularExpression s_headingRe(R"(^(#{1,6})\s+(.+)$)");
static const QRegularExpression s_fencedBlockRe(R"(^(`{3,}|~{3,}))");
static const QRegularExpression
    s_checkboxRe(R"(^(\s*[-*+]\s+)\[([ xX])\](.*)$)");
static const QRegularExpression s_orderedListRe(R"(^(\s*)\d+\.\s+(.*)$)");
static const QRegularExpression s_linkRe(R"(\[([^\]]*)\]\(([^)]*)\))");
static const QRegularExpression s_imageRe(R"(!\[([^\]]*)\]\(([^)]*)\))");
static const QRegularExpression s_imageNoAltRe(R"(!\[\]\(([^)]*)\))");
static const QRegularExpression s_trailingSpaceRe(R"([ \t]+$)");
static const QRegularExpression s_tocMarkerRe(R"(^\s*<!--\s*TOC\s*-->\s*$)");
static const QRegularExpression
    s_tocEndMarkerRe(R"(^\s*<!--\s*/TOC\s*-->\s*$)");
static const QRegularExpression s_tablePipeRe(R"(^\|.*\|$)");
static const QRegularExpression s_tableSepRe(R"(^\|[\s:]*-[-\s:|]*\|$)");

QList<MarkdownHeading> MarkdownTools::extractHeadings(const QString &text) {
  QList<MarkdownHeading> headings;
  const QStringList lines = text.split('\n');
  bool inFencedBlock = false;

  for (int i = 0; i < lines.size(); ++i) {
    const QString &line = lines[i];

    QRegularExpressionMatch fenceMatch = s_fencedBlockRe.match(line);
    if (fenceMatch.hasMatch()) {
      inFencedBlock = !inFencedBlock;
      continue;
    }

    if (inFencedBlock)
      continue;

    QRegularExpressionMatch match = s_headingRe.match(line);
    if (match.hasMatch()) {
      MarkdownHeading heading;
      heading.level = match.captured(1).length();
      heading.text = match.captured(2).trimmed();
      heading.lineNumber = i;
      heading.anchor = generateAnchor(heading.text);
      headings.append(heading);
    }
  }

  return headings;
}

QString MarkdownTools::generateAnchor(const QString &headingText) {
  QString anchor = headingText.toLower();
  anchor.replace(QRegularExpression(R"([^\w\s-])"), "");
  anchor.replace(QRegularExpression(R"(\s+)"), "-");
  anchor.replace(QRegularExpression(R"(-+)"), "-");
  anchor = anchor.trimmed();
  if (anchor.startsWith('-'))
    anchor = anchor.mid(1);
  if (anchor.endsWith('-'))
    anchor.chop(1);
  return anchor;
}

QString MarkdownTools::generateToc(const QString &text, int maxDepth) {
  QList<MarkdownHeading> headings = extractHeadings(text);
  QString toc;
  QTextStream stream(&toc);

  for (const MarkdownHeading &h : headings) {
    if (h.level > maxDepth)
      continue;
    QString indent;
    for (int i = 1; i < h.level; ++i)
      indent += "  ";
    stream << indent << "- [" << h.text << "](#" << h.anchor << ")\n";
  }

  return toc;
}

QString MarkdownTools::insertOrUpdateToc(const QString &text, int maxDepth) {
  QStringList lines = text.split('\n');
  int tocStart = -1;
  int tocEnd = -1;

  for (int i = 0; i < lines.size(); ++i) {
    if (s_tocMarkerRe.match(lines[i]).hasMatch()) {
      tocStart = i;
    } else if (s_tocEndMarkerRe.match(lines[i]).hasMatch()) {
      tocEnd = i;
    }
  }

  QString toc = generateToc(text, maxDepth);

  if (tocStart >= 0 && tocEnd > tocStart) {
    QStringList result;
    for (int i = 0; i <= tocStart; ++i)
      result.append(lines[i]);
    result.append("");
    result.append(toc.trimmed());
    result.append("");
    for (int i = tocEnd; i < lines.size(); ++i)
      result.append(lines[i]);
    return result.join('\n');
  }

  return "<!-- TOC -->\n\n" + toc.trimmed() + "\n\n<!-- /TOC -->\n\n" + text;
}

QString MarkdownTools::toggleCheckbox(const QString &lineText) {
  QRegularExpressionMatch match = s_checkboxRe.match(lineText);
  if (!match.hasMatch())
    return lineText;

  QString prefix = match.captured(1);
  QString state = match.captured(2);
  QString rest = match.captured(3);

  if (state == " ") {
    return prefix + "[x]" + rest;
  } else {
    return prefix + "[ ]" + rest;
  }
}

int MarkdownTools::countTasks(const QString &text, int *completed) {
  const QStringList lines = text.split('\n');
  int total = 0;
  int done = 0;

  for (const QString &line : lines) {
    QRegularExpressionMatch match = s_checkboxRe.match(line);
    if (match.hasMatch()) {
      ++total;
      QString state = match.captured(2);
      if (state == "x" || state == "X")
        ++done;
    }
  }

  if (completed)
    *completed = done;
  return total;
}

QString MarkdownTools::formatTable(const QString &tableText) {
  QStringList lines = tableText.split('\n');
  QList<QStringList> rows;
  int maxCols = 0;

  for (const QString &line : lines) {
    if (!s_tablePipeRe.match(line).hasMatch())
      continue;
    if (s_tableSepRe.match(line).hasMatch()) {
      rows.append(QStringList{"__SEP__"});
      continue;
    }

    QString trimmed = line.mid(1);
    if (trimmed.endsWith('|'))
      trimmed.chop(1);

    QStringList cells;
    const QStringList parts = trimmed.split('|');
    for (const QString &cell : parts)
      cells.append(cell.trimmed());

    maxCols = qMax(maxCols, cells.size());
    rows.append(cells);
  }

  if (rows.isEmpty() || maxCols == 0)
    return tableText;

  QList<int> widths(maxCols, 3);
  for (const QStringList &row : rows) {
    if (row.size() == 1 && row[0] == "__SEP__")
      continue;
    for (int c = 0; c < row.size() && c < maxCols; ++c) {
      widths[c] = qMax(widths[c], row[c].length());
    }
  }

  QString result;
  QTextStream stream(&result);

  for (const QStringList &row : rows) {
    if (row.size() == 1 && row[0] == "__SEP__") {
      stream << "|";
      for (int c = 0; c < maxCols; ++c) {
        stream << " " << QString(widths[c], '-') << " |";
      }
      stream << "\n";
      continue;
    }

    stream << "|";
    for (int c = 0; c < maxCols; ++c) {
      QString cell = (c < row.size()) ? row[c] : "";
      stream << " " << cell.leftJustified(widths[c]) << " |";
    }
    stream << "\n";
  }

  return result.trimmed();
}

QString MarkdownTools::normalizeListNumbering(const QString &text) {
  QStringList lines = text.split('\n');
  QStringList result;
  QMap<int, int> counters;

  for (const QString &line : lines) {
    QRegularExpressionMatch match = s_orderedListRe.match(line);
    if (match.hasMatch()) {
      int indent = match.captured(1).length();
      QString content = match.captured(2);

      if (!counters.contains(indent))
        counters[indent] = 0;
      counters[indent]++;

      QMap<int, int>::iterator it = counters.begin();
      while (it != counters.end()) {
        if (it.key() > indent) {
          it = counters.erase(it);
        } else {
          ++it;
        }
      }

      result.append(match.captured(1) + QString::number(counters[indent]) +
                    ". " + content);
    } else {
      result.append(line);
    }
  }

  return result.join('\n');
}

QList<LspDiagnostic> MarkdownTools::lint(const QString &text,
                                         const QString &filePath) {
  QList<LspDiagnostic> diagnostics;

  diagnostics.append(checkDuplicateHeadings(text));
  diagnostics.append(checkMissingAltText(text));
  diagnostics.append(checkTrailingSpaces(text));
  diagnostics.append(checkInconsistentHeadingLevels(text));
  diagnostics.append(checkBrokenLocalLinks(text, filePath));
  diagnostics.append(checkBrokenImagePaths(text, filePath));
  diagnostics.append(checkMalformedLists(text));
  diagnostics.append(checkOverlongLines(text));

  return diagnostics;
}

QList<LspDiagnostic>
MarkdownTools::checkDuplicateHeadings(const QString &text) {
  QList<LspDiagnostic> diagnostics;
  QList<MarkdownHeading> headings = extractHeadings(text);
  QMap<QString, int> seen;

  for (const MarkdownHeading &h : headings) {
    QString key = h.text.toLower();
    if (seen.contains(key)) {
      LspDiagnostic diag;
      diag.range.start = {h.lineNumber, 0};
      diag.range.end = {h.lineNumber, 0};
      diag.severity = LspDiagnosticSeverity::Warning;
      diag.source = "markdown-lint";
      diag.code = "MD024";
      diag.message = QString("Duplicate heading '%1' (first seen on line %2)")
                         .arg(h.text)
                         .arg(seen[key] + 1);
      diagnostics.append(diag);
    } else {
      seen[key] = h.lineNumber;
    }
  }

  return diagnostics;
}

QList<LspDiagnostic>
MarkdownTools::checkBrokenLocalLinks(const QString &text,
                                     const QString &filePath) {
  QList<LspDiagnostic> diagnostics;
  if (filePath.isEmpty())
    return diagnostics;

  QFileInfo fileInfo(filePath);
  QDir baseDir = fileInfo.absoluteDir();
  const QStringList lines = text.split('\n');
  bool inFencedBlock = false;

  for (int i = 0; i < lines.size(); ++i) {
    const QString &line = lines[i];

    if (s_fencedBlockRe.match(line).hasMatch()) {
      inFencedBlock = !inFencedBlock;
      continue;
    }
    if (inFencedBlock)
      continue;

    QRegularExpressionMatchIterator it = s_linkRe.globalMatch(line);
    while (it.hasNext()) {
      QRegularExpressionMatch match = it.next();
      QString target = match.captured(2);

      if (target.isEmpty() || target.startsWith("http://") ||
          target.startsWith("https://") || target.startsWith("mailto:") ||
          target.startsWith("#"))
        continue;

      QString localTarget = target.split('#').first();
      if (localTarget.isEmpty())
        continue;

      QString resolved = baseDir.absoluteFilePath(localTarget);
      if (!QFileInfo::exists(resolved)) {
        LspDiagnostic diag;
        diag.range.start = {i, static_cast<int>(match.capturedStart(2))};
        diag.range.end = {i, static_cast<int>(match.capturedEnd(2))};
        diag.severity = LspDiagnosticSeverity::Warning;
        diag.source = "markdown-lint";
        diag.code = "MD042";
        diag.message =
            QString("Broken local link: '%1' not found").arg(localTarget);
        diagnostics.append(diag);
      }
    }
  }

  return diagnostics;
}

QList<LspDiagnostic> MarkdownTools::checkMissingAltText(const QString &text) {
  QList<LspDiagnostic> diagnostics;
  const QStringList lines = text.split('\n');
  bool inFencedBlock = false;

  for (int i = 0; i < lines.size(); ++i) {
    const QString &line = lines[i];

    if (s_fencedBlockRe.match(line).hasMatch()) {
      inFencedBlock = !inFencedBlock;
      continue;
    }
    if (inFencedBlock)
      continue;

    QRegularExpressionMatchIterator it = s_imageNoAltRe.globalMatch(line);
    while (it.hasNext()) {
      QRegularExpressionMatch match = it.next();
      LspDiagnostic diag;
      diag.range.start = {i, static_cast<int>(match.capturedStart())};
      diag.range.end = {i, static_cast<int>(match.capturedEnd())};
      diag.severity = LspDiagnosticSeverity::Information;
      diag.source = "markdown-lint";
      diag.code = "MD045";
      diag.message = "Image has no alt text";
      diagnostics.append(diag);
    }
  }

  return diagnostics;
}

QList<LspDiagnostic> MarkdownTools::checkTrailingSpaces(const QString &text) {
  QList<LspDiagnostic> diagnostics;
  const QStringList lines = text.split('\n');

  for (int i = 0; i < lines.size(); ++i) {
    const QString &line = lines[i];
    QRegularExpressionMatch match = s_trailingSpaceRe.match(line);
    if (match.hasMatch()) {
      int trailingLen = match.capturedLength();
      if (trailingLen == 2 && line.endsWith("  "))
        continue;

      LspDiagnostic diag;
      diag.range.start = {i, static_cast<int>(match.capturedStart())};
      diag.range.end = {i, static_cast<int>(match.capturedEnd())};
      diag.severity = LspDiagnosticSeverity::Hint;
      diag.source = "markdown-lint";
      diag.code = "MD009";
      diag.message = "Trailing spaces";
      diagnostics.append(diag);
    }
  }

  return diagnostics;
}

QList<LspDiagnostic>
MarkdownTools::checkInconsistentHeadingLevels(const QString &text) {
  QList<LspDiagnostic> diagnostics;
  QList<MarkdownHeading> headings = extractHeadings(text);

  if (headings.isEmpty())
    return diagnostics;

  if (headings.first().level != 1) {
    LspDiagnostic diag;
    diag.range.start = {headings.first().lineNumber, 0};
    diag.range.end = {headings.first().lineNumber, 0};
    diag.severity = LspDiagnosticSeverity::Information;
    diag.source = "markdown-lint";
    diag.code = "MD002";
    diag.message = "First heading should be a top-level heading (h1)";
    diagnostics.append(diag);
  }

  for (int i = 1; i < headings.size(); ++i) {
    if (headings[i].level > headings[i - 1].level + 1) {
      LspDiagnostic diag;
      diag.range.start = {headings[i].lineNumber, 0};
      diag.range.end = {headings[i].lineNumber, 0};
      diag.severity = LspDiagnosticSeverity::Warning;
      diag.source = "markdown-lint";
      diag.code = "MD001";
      diag.message = QString("Heading level skipped: h%1 after h%2")
                         .arg(headings[i].level)
                         .arg(headings[i - 1].level);
      diagnostics.append(diag);
    }
  }

  return diagnostics;
}

QList<LspDiagnostic>
MarkdownTools::checkBrokenImagePaths(const QString &text,
                                     const QString &filePath) {
  QList<LspDiagnostic> diagnostics;
  if (filePath.isEmpty())
    return diagnostics;

  QFileInfo fileInfo(filePath);
  QDir baseDir = fileInfo.absoluteDir();
  const QStringList lines = text.split('\n');
  bool inFencedBlock = false;

  for (int i = 0; i < lines.size(); ++i) {
    const QString &line = lines[i];

    if (s_fencedBlockRe.match(line).hasMatch()) {
      inFencedBlock = !inFencedBlock;
      continue;
    }
    if (inFencedBlock)
      continue;

    QRegularExpressionMatchIterator it = s_imageRe.globalMatch(line);
    while (it.hasNext()) {
      QRegularExpressionMatch match = it.next();
      QString target = match.captured(2);

      if (target.isEmpty() || target.startsWith("http://") ||
          target.startsWith("https://") || target.startsWith("data:"))
        continue;

      QString resolved = baseDir.absoluteFilePath(target);
      if (!QFileInfo::exists(resolved)) {
        LspDiagnostic diag;
        diag.range.start = {i, static_cast<int>(match.capturedStart(2))};
        diag.range.end = {i, static_cast<int>(match.capturedEnd(2))};
        diag.severity = LspDiagnosticSeverity::Warning;
        diag.source = "markdown-lint";
        diag.code = "MD041";
        diag.message = QString("Broken image path: '%1' not found").arg(target);
        diagnostics.append(diag);
      }
    }
  }

  return diagnostics;
}

QList<LspDiagnostic> MarkdownTools::checkMalformedLists(const QString &text) {
  QList<LspDiagnostic> diagnostics;
  const QStringList lines = text.split('\n');
  bool inFencedBlock = false;
  static const QRegularExpression listItemRe(R"(^(\s*)([-*+]|\d+\.)\s)");
  bool prevWasList = false;
  bool prevWasBlank = true;

  for (int i = 0; i < lines.size(); ++i) {
    const QString &line = lines[i];

    if (s_fencedBlockRe.match(line).hasMatch()) {
      inFencedBlock = !inFencedBlock;
      prevWasList = false;
      prevWasBlank = false;
      continue;
    }
    if (inFencedBlock) {
      prevWasList = false;
      prevWasBlank = false;
      continue;
    }

    bool isList = listItemRe.match(line).hasMatch();
    bool isBlank = line.trimmed().isEmpty();

    if (isList && !prevWasList && !prevWasBlank && i > 0) {
      LspDiagnostic diag;
      diag.range.start = {i, 0};
      diag.range.end = {i, static_cast<int>(line.length())};
      diag.severity = LspDiagnosticSeverity::Information;
      diag.source = "markdown-lint";
      diag.code = "MD032";
      diag.message = "Lists should be surrounded by blank lines";
      diagnostics.append(diag);
    }

    prevWasList = isList;
    prevWasBlank = isBlank;
  }

  return diagnostics;
}

QList<LspDiagnostic> MarkdownTools::checkOverlongLines(const QString &text,
                                                       int maxLength) {
  QList<LspDiagnostic> diagnostics;
  const QStringList lines = text.split('\n');
  bool inFencedBlock = false;

  for (int i = 0; i < lines.size(); ++i) {
    const QString &line = lines[i];

    if (s_fencedBlockRe.match(line).hasMatch()) {
      inFencedBlock = !inFencedBlock;
      continue;
    }
    if (inFencedBlock)
      continue;

    if (s_headingRe.match(line).hasMatch())
      continue;

    if (s_tablePipeRe.match(line).hasMatch())
      continue;

    if (line.contains("](") || line.contains("!["))
      continue;

    if (line.length() > maxLength) {
      LspDiagnostic diag;
      diag.range.start = {i, maxLength};
      diag.range.end = {i, static_cast<int>(line.length())};
      diag.severity = LspDiagnosticSeverity::Hint;
      diag.source = "markdown-lint";
      diag.code = "MD013";
      diag.message = QString("Line length %1 exceeds %2 characters")
                         .arg(line.length())
                         .arg(maxLength);
      diagnostics.append(diag);
    }
  }

  return diagnostics;
}

QString MarkdownTools::wrapParagraph(const QString &text, int width) {
  if (width <= 0)
    return text;

  QStringList lines = text.split('\n');
  QStringList result;
  bool inFencedBlock = false;

  for (int i = 0; i < lines.size(); ++i) {
    const QString &line = lines[i];

    if (s_fencedBlockRe.match(line).hasMatch()) {
      inFencedBlock = !inFencedBlock;
      result.append(line);
      continue;
    }

    if (inFencedBlock || line.trimmed().isEmpty() ||
        s_headingRe.match(line).hasMatch() || line.trimmed().startsWith('>') ||
        s_tablePipeRe.match(line).hasMatch() ||
        s_checkboxRe.match(line).hasMatch() ||
        line.trimmed().startsWith("- ") || line.trimmed().startsWith("* ") ||
        line.trimmed().startsWith("+ ") ||
        s_orderedListRe.match(line).hasMatch() ||
        s_tocMarkerRe.match(line).hasMatch() ||
        s_tocEndMarkerRe.match(line).hasMatch()) {
      result.append(line);
      continue;
    }

    if (line.length() <= width) {
      result.append(line);
      continue;
    }

    QStringList words = line.split(' ', Qt::SkipEmptyParts);
    QString currentLine;
    for (const QString &word : words) {
      if (currentLine.isEmpty()) {
        currentLine = word;
      } else if (currentLine.length() + 1 + word.length() <= width) {
        currentLine += " " + word;
      } else {
        result.append(currentLine);
        currentLine = word;
      }
    }
    if (!currentLine.isEmpty())
      result.append(currentLine);
  }

  return result.join('\n');
}

QString MarkdownTools::normalizeHeadingSpacing(const QString &text) {
  QStringList lines = text.split('\n');
  QStringList result;
  bool inFencedBlock = false;

  for (int i = 0; i < lines.size(); ++i) {
    const QString &line = lines[i];

    if (s_fencedBlockRe.match(line).hasMatch()) {
      inFencedBlock = !inFencedBlock;
      result.append(line);
      continue;
    }

    if (inFencedBlock) {
      result.append(line);
      continue;
    }

    bool isHeading = s_headingRe.match(line).hasMatch();

    if (isHeading && i > 0) {
      while (!result.isEmpty() && result.last().trimmed().isEmpty())
        result.removeLast();
      result.append("");
    }

    result.append(line);

    if (isHeading && i + 1 < lines.size()) {
      int next = i + 1;
      while (next < lines.size() && lines[next].trimmed().isEmpty())
        ++next;
      if (next < lines.size() && next > i + 1) {
        result.append("");
        i = next - 1;
      } else if (next == i + 1) {
        result.append("");
      }
    }
  }

  return result.join('\n');
}

QString MarkdownTools::normalizeBulletIndentation(const QString &text) {
  static const QRegularExpression bulletRe(R"(^(\s*)([-*+])\s+(.*)$)");
  QStringList lines = text.split('\n');
  QStringList result;
  bool inFencedBlock = false;

  for (const QString &line : lines) {
    if (s_fencedBlockRe.match(line).hasMatch()) {
      inFencedBlock = !inFencedBlock;
      result.append(line);
      continue;
    }

    if (inFencedBlock) {
      result.append(line);
      continue;
    }

    QRegularExpressionMatch match = bulletRe.match(line);
    if (match.hasMatch()) {
      QString indent = match.captured(1);
      QString content = match.captured(3);
      int indentLevel = indent.length() / 2;
      QString normalizedIndent;
      for (int j = 0; j < indentLevel; ++j)
        normalizedIndent += "  ";
      result.append(normalizedIndent + "- " + content);
    } else {
      result.append(line);
    }
  }

  return result.join('\n');
}

QString MarkdownTools::formatCodeFences(const QString &text) {
  static const QRegularExpression fenceOpenRe(R"(^(`{3,}|~{3,})(.*)$)");
  QStringList lines = text.split('\n');
  QStringList result;
  bool inFencedBlock = false;
  QString currentFenceType;

  for (const QString &line : lines) {
    QRegularExpressionMatch match = fenceOpenRe.match(line);
    if (match.hasMatch()) {
      QString fence = match.captured(1);
      QString lang = match.captured(2).trimmed();

      if (!inFencedBlock) {
        inFencedBlock = true;
        currentFenceType = QString(3, fence[0]);
        result.append(currentFenceType + (lang.isEmpty() ? "" : lang));
      } else {
        result.append(currentFenceType);
        inFencedBlock = false;
        currentFenceType.clear();
      }
    } else {
      result.append(line);
    }
  }

  return result.join('\n');
}

QList<MarkdownLink> MarkdownTools::extractLinks(const QString &text) {
  QList<MarkdownLink> links;
  const QStringList lines = text.split('\n');
  bool inFencedBlock = false;

  for (int i = 0; i < lines.size(); ++i) {
    const QString &line = lines[i];

    if (s_fencedBlockRe.match(line).hasMatch()) {
      inFencedBlock = !inFencedBlock;
      continue;
    }
    if (inFencedBlock)
      continue;

    QRegularExpressionMatchIterator it = s_linkRe.globalMatch(line);
    while (it.hasNext()) {
      QRegularExpressionMatch match = it.next();
      int pos = match.capturedStart();
      if (pos > 0 && line[pos - 1] == '!')
        continue;

      MarkdownLink link;
      link.text = match.captured(1);
      link.target = match.captured(2);
      link.lineNumber = i;
      link.column = static_cast<int>(match.capturedStart());
      link.isImage = false;
      links.append(link);
    }
  }

  return links;
}

QList<MarkdownLink> MarkdownTools::extractImages(const QString &text) {
  QList<MarkdownLink> images;
  const QStringList lines = text.split('\n');
  bool inFencedBlock = false;

  for (int i = 0; i < lines.size(); ++i) {
    const QString &line = lines[i];

    if (s_fencedBlockRe.match(line).hasMatch()) {
      inFencedBlock = !inFencedBlock;
      continue;
    }
    if (inFencedBlock)
      continue;

    QRegularExpressionMatchIterator it = s_imageRe.globalMatch(line);
    while (it.hasNext()) {
      QRegularExpressionMatch match = it.next();
      MarkdownLink img;
      img.text = match.captured(1);
      img.target = match.captured(2);
      img.lineNumber = i;
      img.column = static_cast<int>(match.capturedStart());
      img.isImage = true;
      images.append(img);
    }
  }

  return images;
}

QList<MarkdownFootnote> MarkdownTools::extractFootnotes(const QString &text) {
  static const QRegularExpression defRe(R"(^\[\^([^\]]+)\]:\s*(.*)$)");
  static const QRegularExpression refRe(R"(\[\^([^\]]+)\])");
  QMap<QString, MarkdownFootnote> footnoteMap;
  const QStringList lines = text.split('\n');
  bool inFencedBlock = false;

  for (int i = 0; i < lines.size(); ++i) {
    const QString &line = lines[i];

    if (s_fencedBlockRe.match(line).hasMatch()) {
      inFencedBlock = !inFencedBlock;
      continue;
    }
    if (inFencedBlock)
      continue;

    QRegularExpressionMatch defMatch = defRe.match(line);
    if (defMatch.hasMatch()) {
      QString label = defMatch.captured(1);
      if (!footnoteMap.contains(label)) {
        MarkdownFootnote fn;
        fn.label = label;
        fn.text = defMatch.captured(2);
        fn.definitionLine = i;
        footnoteMap[label] = fn;
      } else {
        footnoteMap[label].definitionLine = i;
        footnoteMap[label].text = defMatch.captured(2);
      }
      continue;
    }

    QRegularExpressionMatchIterator it = refRe.globalMatch(line);
    while (it.hasNext()) {
      QRegularExpressionMatch match = it.next();
      QString label = match.captured(1);
      if (line.trimmed().startsWith("[^" + label + "]:"))
        continue;
      if (!footnoteMap.contains(label)) {
        MarkdownFootnote fn;
        fn.label = label;
        fn.definitionLine = -1;
        footnoteMap[label] = fn;
      }
      footnoteMap[label].referenceLines.append(i);
    }
  }

  return footnoteMap.values();
}

MarkdownFrontmatter MarkdownTools::parseFrontmatter(const QString &text) {
  MarkdownFrontmatter fm;
  fm.startLine = -1;
  fm.endLine = -1;

  const QStringList lines = text.split('\n');
  if (lines.isEmpty() || lines[0].trimmed() != "---")
    return fm;

  fm.startLine = 0;
  QStringList content;

  for (int i = 1; i < lines.size(); ++i) {
    if (lines[i].trimmed() == "---" || lines[i].trimmed() == "...") {
      fm.endLine = i;
      break;
    }
    content.append(lines[i]);
  }

  if (fm.endLine < 0)
    return MarkdownFrontmatter{QMap<QString, QString>(), -1, -1, ""};

  fm.rawContent = content.join('\n');

  static const QRegularExpression kvRe(R"(^([^:]+):\s*(.*)$)");
  for (const QString &line : content) {
    QRegularExpressionMatch match = kvRe.match(line);
    if (match.hasMatch()) {
      fm.fields[match.captured(1).trimmed()] = match.captured(2).trimmed();
    }
  }

  return fm;
}

QList<MarkdownOutlineEntry>
MarkdownTools::generateDocumentOutline(const QString &text) {
  QList<MarkdownHeading> headings = extractHeadings(text);
  QList<MarkdownOutlineEntry> root;
  QList<MarkdownOutlineEntry *> stack;

  for (const MarkdownHeading &h : headings) {
    MarkdownOutlineEntry entry;
    entry.level = h.level;
    entry.text = h.text;
    entry.anchor = h.anchor;
    entry.lineNumber = h.lineNumber;

    while (!stack.isEmpty() && stack.last()->level >= h.level)
      stack.removeLast();

    if (stack.isEmpty()) {
      root.append(entry);
      stack.append(&root.last());
    } else {
      stack.last()->children.append(entry);
      stack.append(&stack.last()->children.last());
    }
  }

  return root;
}

int MarkdownTools::wordCount(const QString &text) {
  QStringList lines = text.split('\n');
  int count = 0;
  bool inFencedBlock = false;

  for (const QString &line : lines) {
    if (s_fencedBlockRe.match(line).hasMatch()) {
      inFencedBlock = !inFencedBlock;
      continue;
    }
    if (inFencedBlock)
      continue;

    if (s_tocMarkerRe.match(line).hasMatch() ||
        s_tocEndMarkerRe.match(line).hasMatch())
      continue;

    if (line.trimmed().startsWith("---") && line.trimmed().length() == 3)
      continue;

    QString cleaned = line;
    cleaned.replace(QRegularExpression(R"(\[([^\]]*)\]\([^)]*\))"), "\\1");
    cleaned.replace(QRegularExpression(R"(!\[[^\]]*\]\([^)]*\))"), "");
    cleaned.replace(QRegularExpression(R"(`[^`]+`)"), "");
    cleaned.replace(QRegularExpression(R"(\*{1,3}|_{1,3}|~~|==)"), "");
    cleaned.replace(QRegularExpression(R"(^#{1,6}\s+)"), "");
    cleaned.replace(QRegularExpression(R"(^\s*[-*+]\s+)"), "");
    cleaned.replace(QRegularExpression(R"(^\s*\d+\.\s+)"), "");
    cleaned.replace(QRegularExpression(R"(^\s*>\s*)"), "");
    cleaned.replace(QRegularExpression(R"(^\s*\[([ xX])\]\s*)"), "");

    QStringList words =
        cleaned.split(QRegularExpression(R"(\s+)"), Qt::SkipEmptyParts);
    count += words.size();
  }

  return count;
}

int MarkdownTools::readingTimeMinutes(const QString &text, int wordsPerMinute) {
  if (wordsPerMinute <= 0)
    return 0;
  int words = wordCount(text);
  int minutes = (words + wordsPerMinute - 1) / wordsPerMinute;
  return qMax(1, minutes);
}

QString MarkdownTools::escapeHtml(const QString &text) {
  QString result = text;
  result.replace('&', "&amp;");
  result.replace('<', "&lt;");
  result.replace('>', "&gt;");
  result.replace('"', "&quot;");
  return result;
}

QString MarkdownTools::processInlineFormatting(const QString &text) {
  QString result = escapeHtml(text);

  result.replace(QRegularExpression(R"(\*\*\*(.+?)\*\*\*)"),
                 "<strong><em>\\1</em></strong>");
  result.replace(QRegularExpression(R"(\*\*(.+?)\*\*)"),
                 "<strong>\\1</strong>");
  result.replace(QRegularExpression(R"(\*(.+?)\*)"), "<em>\\1</em>");
  result.replace(QRegularExpression(R"(~~(.+?)~~)"), "<del>\\1</del>");
  result.replace(QRegularExpression(R"(`([^`]+)`)"), "<code>\\1</code>");

  result.replace(QRegularExpression(R"(!\[([^\]]*)\]\(([^)]*)\))"),
                 "<img src=\"\\2\" alt=\"\\1\" style=\"max-width:100%;\">");

  result.replace(QRegularExpression(R"(\[([^\]]+)\]\(([^)]*)\))"),
                 "<a href=\"\\2\">\\1</a>");

  return result;
}

QString MarkdownTools::toHtml(const QString &markdown,
                              const QString &basePath) {
  QStringList lines = markdown.split('\n');
  QString html;
  QTextStream stream(&html);

  stream << "<!DOCTYPE html><html><head><meta charset=\"utf-8\">" << "<style>"
         << "body { font-family: -apple-system, BlinkMacSystemFont, "
            "'Segoe UI', Roboto, sans-serif;"
         << " line-height: 1.6; padding: 20px; max-width: 800px; "
            "margin: 0 auto;"
         << " color: #c9d1d9; background-color: #0d1117; }"
         << "h1,h2,h3,h4,h5,h6 { color: #e6edf3; border-bottom: 1px solid "
            "#30363d; padding-bottom: 0.3em; }"
         << "code { background: #161b22; padding: 2px 6px; border-radius: "
            "3px; font-size: 0.9em; }"
         << "pre { background: #161b22; padding: 16px; border-radius: 6px; "
            "overflow-x: auto; }"
         << "pre code { background: none; padding: 0; }"
         << "blockquote { border-left: 4px solid #30363d; margin: 0; "
            "padding: 0 16px; color: #8b949e; }"
         << "table { border-collapse: collapse; width: 100%; }"
         << "th, td { border: 1px solid #30363d; padding: 8px 12px; }"
         << "th { background: #161b22; }" << "img { max-width: 100%; }"
         << "a { color: #58a6ff; }"
         << "hr { border: none; border-top: 1px solid #30363d; }"
         << ".task-list-item { list-style: none; }"
         << ".task-list-item input { margin-right: 0.5em; }"
         << "</style></head><body>\n";

  bool inFencedBlock = false;
  bool inBlockquote = false;
  bool inList = false;
  bool inOrderedList = false;
  bool inTable = false;
  bool inTableHeader = true;
  QString fenceDelimiter;

  for (int i = 0; i < lines.size(); ++i) {
    QString line = lines[i];

    if (inFencedBlock) {
      if (line.trimmed().startsWith(fenceDelimiter)) {
        stream << "</code></pre>\n";
        inFencedBlock = false;
        fenceDelimiter.clear();
      } else {
        stream << escapeHtml(line) << "\n";
      }
      continue;
    }

    QRegularExpressionMatch fenceMatch = s_fencedBlockRe.match(line);
    if (fenceMatch.hasMatch()) {
      if (inList) {
        stream << (inOrderedList ? "</ol>\n" : "</ul>\n");
        inList = false;
      }
      if (inBlockquote) {
        stream << "</blockquote>\n";
        inBlockquote = false;
      }
      inFencedBlock = true;
      fenceDelimiter = fenceMatch.captured(1);
      QString lang = line.mid(fenceDelimiter.length()).trimmed();
      stream << "<pre><code"
             << (lang.isEmpty()
                     ? ""
                     : " class=\"language-" + escapeHtml(lang) + "\"")
             << ">";
      continue;
    }

    if (line.trimmed().isEmpty()) {
      if (inList) {
        stream << (inOrderedList ? "</ol>\n" : "</ul>\n");
        inList = false;
      }
      if (inBlockquote) {
        stream << "</blockquote>\n";
        inBlockquote = false;
      }
      if (inTable) {
        stream << "</table>\n";
        inTable = false;
        inTableHeader = true;
      }
      continue;
    }

    QRegularExpressionMatch headingMatch = s_headingRe.match(line);
    if (headingMatch.hasMatch()) {
      if (inList) {
        stream << (inOrderedList ? "</ol>\n" : "</ul>\n");
        inList = false;
      }
      int level = headingMatch.captured(1).length();
      QString text = headingMatch.captured(2).trimmed();
      QString anchor = generateAnchor(text);
      stream << "<h" << level << " id=\"" << escapeHtml(anchor) << "\">"
             << processInlineFormatting(text) << "</h" << level << ">\n";
      continue;
    }

    if (line.trimmed().startsWith('>')) {
      if (!inBlockquote) {
        inBlockquote = true;
        stream << "<blockquote>\n";
      }
      QString content = line.trimmed().mid(1).trimmed();
      stream << "<p>" << processInlineFormatting(content) << "</p>\n";
      continue;
    }

    if (s_tablePipeRe.match(line).hasMatch()) {
      if (s_tableSepRe.match(line).hasMatch())
        continue;

      if (!inTable) {
        inTable = true;
        inTableHeader = true;
        stream << "<table>\n";
      }

      QString trimmed = line.mid(1);
      if (trimmed.endsWith('|'))
        trimmed.chop(1);
      QStringList cells = trimmed.split('|');

      if (inTableHeader) {
        stream << "<thead><tr>";
        for (const QString &cell : cells)
          stream << "<th>" << processInlineFormatting(cell.trimmed())
                 << "</th>";
        stream << "</tr></thead><tbody>\n";
        inTableHeader = false;
      } else {
        stream << "<tr>";
        for (const QString &cell : cells)
          stream << "<td>" << processInlineFormatting(cell.trimmed())
                 << "</td>";
        stream << "</tr>\n";
      }
      continue;
    }

    if (line.trimmed() == "---" || line.trimmed() == "***" ||
        line.trimmed() == "___") {
      stream << "<hr>\n";
      continue;
    }

    QRegularExpressionMatch checkboxMatch = s_checkboxRe.match(line);
    if (checkboxMatch.hasMatch()) {
      if (!inList) {
        inList = true;
        inOrderedList = false;
        stream << "<ul>\n";
      }
      QString state = checkboxMatch.captured(2);
      QString content = checkboxMatch.captured(3).trimmed();
      bool checked = (state == "x" || state == "X");
      stream << "<li class=\"task-list-item\"><input type=\"checkbox\" disabled"
             << (checked ? " checked" : "") << "> "
             << processInlineFormatting(content) << "</li>\n";
      continue;
    }

    QRegularExpressionMatch olMatch = s_orderedListRe.match(line);
    if (olMatch.hasMatch()) {
      if (!inList || !inOrderedList) {
        if (inList)
          stream << "</ul>\n";
        inList = true;
        inOrderedList = true;
        stream << "<ol>\n";
      }
      stream << "<li>" << processInlineFormatting(olMatch.captured(2).trimmed())
             << "</li>\n";
      continue;
    }

    if (line.trimmed().startsWith("- ") || line.trimmed().startsWith("* ") ||
        line.trimmed().startsWith("+ ")) {
      if (!inList || inOrderedList) {
        if (inList)
          stream << "</ol>\n";
        inList = true;
        inOrderedList = false;
        stream << "<ul>\n";
      }
      QString content = line.trimmed().mid(2);
      stream << "<li>" << processInlineFormatting(content) << "</li>\n";
      continue;
    }

    if (inBlockquote) {
      stream << "</blockquote>\n";
      inBlockquote = false;
    }

    stream << "<p>" << processInlineFormatting(line) << "</p>\n";
  }

  if (inFencedBlock)
    stream << "</code></pre>\n";
  if (inList)
    stream << (inOrderedList ? "</ol>\n" : "</ul>\n");
  if (inBlockquote)
    stream << "</blockquote>\n";
  if (inTable)
    stream << "</tbody></table>\n";

  stream << "</body></html>";

  if (!basePath.isEmpty()) {
    QString absBase = "file://" + QDir(basePath).absolutePath() + "/";
    html.replace(
        QRegularExpression(R"delim(src="(?!https?://|data:)([^"]+)")delim"),
        "src=\"" + absBase + "\\1\"");
    html.replace(QRegularExpression(
                     R"delim(href="(?!https?://|mailto:|#)([^"]+)")delim"),
                 "href=\"" + absBase + "\\1\"");
  }

  return html;
}
