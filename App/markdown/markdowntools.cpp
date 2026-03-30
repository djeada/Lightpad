#include "markdowntools.h"
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QStringList>
#include <QTextStream>

static const QRegularExpression s_headingRe(R"(^(#{1,6})\s+(.+)$)");
static const QRegularExpression s_fencedBlockRe(R"(^(`{3,}|~{3,}))");
static const QRegularExpression s_checkboxRe(
    R"(^(\s*[-*+]\s+)\[([ xX])\](.*)$)");
static const QRegularExpression s_orderedListRe(R"(^(\s*)\d+\.\s+(.*)$)");
static const QRegularExpression s_linkRe(
    R"(\[([^\]]*)\]\(([^)]*)\))");
static const QRegularExpression s_imageRe(
    R"(!\[([^\]]*)\]\(([^)]*)\))");
static const QRegularExpression s_imageNoAltRe(
    R"(!\[\]\(([^)]*)\))");
static const QRegularExpression s_trailingSpaceRe(R"([ \t]+$)");
static const QRegularExpression s_tocMarkerRe(
    R"(^\s*<!--\s*TOC\s*-->\s*$)");
static const QRegularExpression s_tocEndMarkerRe(
    R"(^\s*<!--\s*/TOC\s*-->\s*$)");
static const QRegularExpression s_tablePipeRe(R"(^\|.*\|$)");
static const QRegularExpression s_tableSepRe(
    R"(^\|[\s:]*-[-\s:|]*\|$)");

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

      result.append(match.captured(1) +
                    QString::number(counters[indent]) + ". " + content);
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

  return diagnostics;
}

QList<LspDiagnostic> MarkdownTools::checkDuplicateHeadings(const QString &text) {
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
      diag.message =
          QString("Duplicate heading '%1' (first seen on line %2)")
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
        diag.range.start = {i, match.capturedStart(2)};
        diag.range.end = {i, match.capturedEnd(2)};
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
      diag.range.start = {i, match.capturedStart()};
      diag.range.end = {i, match.capturedEnd()};
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
      diag.range.start = {i, match.capturedStart()};
      diag.range.end = {i, match.capturedEnd()};
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
      diag.message =
          QString("Heading level skipped: h%1 after h%2")
              .arg(headings[i].level)
              .arg(headings[i - 1].level);
      diagnostics.append(diag);
    }
  }

  return diagnostics;
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
  result.replace(QRegularExpression(R"(`([^`]+)`)"),
                 "<code>\\1</code>");

  result.replace(
      QRegularExpression(R"(!\[([^\]]*)\]\(([^)]*)\))"),
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

  stream << "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
         << "<style>"
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
         << "th { background: #161b22; }"
         << "img { max-width: 100%; }"
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
             << (lang.isEmpty() ? "" : " class=\"language-" + escapeHtml(lang) + "\"")
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
    QString absBase =
        "file://" + QDir(basePath).absolutePath() + "/";
    html.replace(
        QRegularExpression(R"delim(src="(?!https?://|data:)([^"]+)")delim"),
        "src=\"" + absBase + "\\1\"");
    html.replace(
        QRegularExpression(R"delim(href="(?!https?://|mailto:|#)([^"]+)")delim"),
        "href=\"" + absBase + "\\1\"");
  }

  return html;
}
