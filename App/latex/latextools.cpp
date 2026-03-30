#include "latextools.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QMap>
#include <QSet>
#include <QStringList>
#include <QTextStream>

static const QRegularExpression s_sectionRe(
    R"(\\(part|chapter|section|subsection|subsubsection|paragraph|subparagraph)\*?\{([^}]*)\})");
static const QRegularExpression s_labelRe(R"(\\label\{([^}]*)\})");
static const QRegularExpression s_refRe(
    R"(\\(ref|eqref|pageref|cite|citep|citet)\{([^}]*)\})");
static const QRegularExpression s_citeRe(
    R"(\\(cite|citep|citet|citeauthor|citeyear|citealt|citealp|parencite|textcite|autocite|fullcite)\{([^}]*)\})");
static const QRegularExpression s_includeRe(
    R"(\\(input|include|includeonly|includegraphics)(?:\[[^\]]*\])?\{([^}]*)\})");
static const QRegularExpression s_bibResourceRe(
    R"(\\(addbibresource|bibliography)\{([^}]*)\})");
static const QRegularExpression s_documentClassRe(
    R"(\\documentclass(?:\[[^\]]*\])?\{([^}]*)\})");
static const QRegularExpression s_usePackageRe(
    R"(\\usepackage(?:\[[^\]]*\])?\{([^}]*)\})");
static const QRegularExpression s_beginEnvRe(R"(\\begin\{([^}]*)\})");
static const QRegularExpression s_endEnvRe(R"(\\end\{([^}]*)\})");
static const QRegularExpression s_commentRe(R"((?:^|[^\\])%.*)");

static const QRegularExpression s_logErrorRe(R"(^!\s+(.*))");
static const QRegularExpression s_logLatexErrorRe(
    R"(^!\s+LaTeX Error:\s+(.*))");
static const QRegularExpression s_logWarningRe(
    R"(^LaTeX Warning:\s+(.*))");
static const QRegularExpression s_logPackageWarningRe(
    R"(^Package\s+(\S+)\s+Warning:\s+(.*))");
static const QRegularExpression s_logOverfullRe(
    R"(^(Overfull|Underfull)\s+\\[hv]box\s+.*)");
static const QRegularExpression s_logLineNumberRe(R"(\bl\.(\d+)\b)");
static const QRegularExpression s_logInputLineRe(
    R"(on input line\s+(\d+))");

static const QMap<QString, int> s_sectionLevels = {
    {"part", 0},           {"chapter", 1},    {"section", 2},
    {"subsection", 3},     {"subsubsection", 4},
    {"paragraph", 5},      {"subparagraph", 6}};

static int lineNumberAt(const QString &text, int position) {
  int line = 0;
  for (int i = 0; i < position && i < text.length(); ++i) {
    if (text[i] == '\n')
      ++line;
  }
  return line;
}

static bool isInComment(const QString &line, int col) {
  for (int i = 0; i < col && i < line.length(); ++i) {
    if (line[i] == '%' && (i == 0 || line[i - 1] != '\\'))
      return true;
  }
  return false;
}

QList<LatexSection> LatexTools::extractSections(const QString &text) {
  QList<LatexSection> sections;
  const QStringList lines = text.split('\n');

  for (int i = 0; i < lines.size(); ++i) {
    const QString &line = lines[i];

    QRegularExpressionMatchIterator it = s_sectionRe.globalMatch(line);
    while (it.hasNext()) {
      QRegularExpressionMatch match = it.next();

      if (isInComment(line, match.capturedStart()))
        continue;

      LatexSection sec;
      sec.level = s_sectionLevels.value(match.captured(1), 2);
      sec.text = match.captured(2).trimmed();
      sec.lineNumber = i;

      for (int j = i; j < qMin(i + 3, lines.size()); ++j) {
        QRegularExpressionMatch labelMatch = s_labelRe.match(lines[j]);
        if (labelMatch.hasMatch()) {
          sec.label = labelMatch.captured(1);
          break;
        }
      }

      sections.append(sec);
    }
  }

  return sections;
}

QList<LatexLabel> LatexTools::extractLabels(const QString &text) {
  QList<LatexLabel> labels;
  const QStringList lines = text.split('\n');

  for (int i = 0; i < lines.size(); ++i) {
    const QString &line = lines[i];

    QRegularExpressionMatchIterator it = s_labelRe.globalMatch(line);
    while (it.hasNext()) {
      QRegularExpressionMatch match = it.next();
      if (isInComment(line, match.capturedStart()))
        continue;

      LatexLabel lbl;
      lbl.name = match.captured(1);
      lbl.lineNumber = i;
      lbl.type = "label";
      labels.append(lbl);
    }
  }

  return labels;
}

QList<LatexLabel> LatexTools::extractReferences(const QString &text) {
  QList<LatexLabel> refs;
  const QStringList lines = text.split('\n');

  for (int i = 0; i < lines.size(); ++i) {
    const QString &line = lines[i];

    QRegularExpressionMatchIterator it = s_refRe.globalMatch(line);
    while (it.hasNext()) {
      QRegularExpressionMatch match = it.next();
      if (isInComment(line, match.capturedStart()))
        continue;

      QString keys = match.captured(2);
      QString type = match.captured(1);

      const QStringList parts = keys.split(',');
      for (const QString &key : parts) {
        QString trimmed = key.trimmed();
        if (trimmed.isEmpty())
          continue;

        LatexLabel ref;
        ref.name = trimmed;
        ref.lineNumber = i;
        ref.type = type;
        refs.append(ref);
      }
    }
  }

  return refs;
}

QList<LatexLabel> LatexTools::extractCitations(const QString &text) {
  QList<LatexLabel> cites;
  const QStringList lines = text.split('\n');

  for (int i = 0; i < lines.size(); ++i) {
    const QString &line = lines[i];

    QRegularExpressionMatchIterator it = s_citeRe.globalMatch(line);
    while (it.hasNext()) {
      QRegularExpressionMatch match = it.next();
      if (isInComment(line, match.capturedStart()))
        continue;

      QString keys = match.captured(2);
      const QStringList parts = keys.split(',');
      for (const QString &key : parts) {
        QString trimmed = key.trimmed();
        if (trimmed.isEmpty())
          continue;

        LatexLabel cite;
        cite.name = trimmed;
        cite.lineNumber = i;
        cite.type = "cite";
        cites.append(cite);
      }
    }
  }

  return cites;
}

QList<LatexInclude> LatexTools::extractIncludes(const QString &text) {
  QList<LatexInclude> includes;
  const QStringList lines = text.split('\n');

  for (int i = 0; i < lines.size(); ++i) {
    const QString &line = lines[i];

    QRegularExpressionMatchIterator it = s_includeRe.globalMatch(line);
    while (it.hasNext()) {
      QRegularExpressionMatch match = it.next();
      if (isInComment(line, match.capturedStart()))
        continue;

      LatexInclude inc;
      inc.path = match.captured(2);
      inc.lineNumber = i;
      inc.type = match.captured(1);
      includes.append(inc);
    }

    QRegularExpressionMatchIterator bibIt = s_bibResourceRe.globalMatch(line);
    while (bibIt.hasNext()) {
      QRegularExpressionMatch match = bibIt.next();
      if (isInComment(line, match.capturedStart()))
        continue;

      LatexInclude inc;
      inc.path = match.captured(2);
      inc.lineNumber = i;
      inc.type = match.captured(1);
      includes.append(inc);
    }
  }

  return includes;
}

QString LatexTools::detectDocumentClass(const QString &text) {
  const QStringList lines = text.split('\n');

  for (const QString &line : lines) {
    QRegularExpressionMatch match = s_documentClassRe.match(line);
    if (match.hasMatch() && !isInComment(line, match.capturedStart()))
      return match.captured(1);
  }

  return QString();
}

QStringList LatexTools::detectPackages(const QString &text) {
  QStringList packages;
  const QStringList lines = text.split('\n');

  for (const QString &line : lines) {
    QRegularExpressionMatchIterator it = s_usePackageRe.globalMatch(line);
    while (it.hasNext()) {
      QRegularExpressionMatch match = it.next();
      if (isInComment(line, match.capturedStart()))
        continue;

      QString pkgList = match.captured(1);
      const QStringList parts = pkgList.split(',');
      for (const QString &pkg : parts) {
        QString trimmed = pkg.trimmed();
        if (!trimmed.isEmpty())
          packages.append(trimmed);
      }
    }
  }

  return packages;
}

QString LatexTools::detectMainFile(const QString &dirPath) {
  QDir dir(dirPath);
  if (!dir.exists())
    return QString();

  QDirIterator it(dirPath, {"*.tex"}, QDir::Files);
  while (it.hasNext()) {
    QString filePath = it.next();
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
      continue;

    QString content = QString::fromUtf8(file.readAll());
    if (s_documentClassRe.match(content).hasMatch())
      return filePath;
  }

  return QString();
}

QList<LatexLogEntry> LatexTools::parseLatexLog(const QString &logContent) {
  QList<LatexLogEntry> entries;
  const QStringList lines = logContent.split('\n');
  QString currentFile;

  for (int i = 0; i < lines.size(); ++i) {
    const QString &line = lines[i];

    int lineNum = 0;
    QRegularExpressionMatch lineMatch = s_logLineNumberRe.match(line);
    if (lineMatch.hasMatch())
      lineNum = lineMatch.captured(1).toInt();

    QRegularExpressionMatch inputLineMatch = s_logInputLineRe.match(line);
    if (inputLineMatch.hasMatch())
      lineNum = inputLineMatch.captured(1).toInt();

    QRegularExpressionMatch latexErrMatch = s_logLatexErrorRe.match(line);
    if (latexErrMatch.hasMatch()) {
      LatexLogEntry entry;
      entry.lineNumber = lineNum;
      entry.severity = 1;
      entry.message = latexErrMatch.captured(1).trimmed();
      entry.file = currentFile;
      entries.append(entry);
      continue;
    }

    QRegularExpressionMatch errMatch = s_logErrorRe.match(line);
    if (errMatch.hasMatch()) {
      LatexLogEntry entry;
      entry.lineNumber = lineNum;
      entry.severity = 1;
      entry.message = errMatch.captured(1).trimmed();
      entry.file = currentFile;

      if (lineNum == 0 && i + 1 < lines.size()) {
        QRegularExpressionMatch nextLineMatch =
            s_logLineNumberRe.match(lines[i + 1]);
        if (nextLineMatch.hasMatch())
          entry.lineNumber = nextLineMatch.captured(1).toInt();
      }

      entries.append(entry);
      continue;
    }

    QRegularExpressionMatch warnMatch = s_logWarningRe.match(line);
    if (warnMatch.hasMatch()) {
      LatexLogEntry entry;
      entry.lineNumber = lineNum;
      entry.severity = 2;
      entry.message = warnMatch.captured(1).trimmed();
      entry.file = currentFile;
      entries.append(entry);
      continue;
    }

    QRegularExpressionMatch pkgWarnMatch = s_logPackageWarningRe.match(line);
    if (pkgWarnMatch.hasMatch()) {
      LatexLogEntry entry;
      entry.lineNumber = lineNum;
      entry.severity = 2;
      entry.message = QString("Package %1: %2")
                          .arg(pkgWarnMatch.captured(1))
                          .arg(pkgWarnMatch.captured(2).trimmed());
      entry.file = currentFile;
      entries.append(entry);
      continue;
    }

    QRegularExpressionMatch boxMatch = s_logOverfullRe.match(line);
    if (boxMatch.hasMatch()) {
      LatexLogEntry entry;
      entry.lineNumber = lineNum;
      entry.severity = 3;
      entry.message = line.trimmed();
      entry.file = currentFile;
      entries.append(entry);
      continue;
    }
  }

  return entries;
}

QList<LspDiagnostic> LatexTools::lint(const QString &text,
                                      const QString &filePath) {
  QList<LspDiagnostic> diagnostics;

  diagnostics.append(checkUnclosedEnvironments(text));
  diagnostics.append(checkDuplicateLabels(text));
  diagnostics.append(checkUndefinedReferences(text));
  diagnostics.append(checkMissingDocumentClass(text, filePath));
  diagnostics.append(checkUnclosedMath(text));
  diagnostics.append(checkCommonTypos(text));

  return diagnostics;
}

bool LatexTools::isLatexFile(const QString &extension) {
  static const QSet<QString> s_extensions = {"tex", "ltx", "sty",
                                             "cls", "bib", "dtx", "ins"};
  return s_extensions.contains(extension.toLower());
}

QString LatexTools::generateOutline(const QString &text, int maxDepth) {
  QList<LatexSection> sections = extractSections(text);
  QString outline;
  QTextStream stream(&outline);

  for (const LatexSection &sec : sections) {
    if (sec.level > maxDepth)
      continue;

    QString indent;
    for (int i = 0; i < sec.level; ++i)
      indent += "  ";

    stream << indent << sec.text;
    if (!sec.label.isEmpty())
      stream << " [" << sec.label << "]";
    stream << "\n";
  }

  return outline;
}

QPair<int, int> LatexTools::findMatchingEnvironment(const QString &text,
                                                    int position) {
  if (position < 0 || position >= text.length())
    return {-1, -1};

  QRegularExpressionMatch beginMatch = s_beginEnvRe.match(text, position);
  if (beginMatch.hasMatch() && beginMatch.capturedStart() == position) {
    QString envName = beginMatch.captured(1);
    int depth = 1;
    int searchPos = beginMatch.capturedEnd();

    while (depth > 0 && searchPos < text.length()) {
      QRegularExpressionMatch nextBegin =
          s_beginEnvRe.match(text, searchPos);
      QRegularExpressionMatch nextEnd = s_endEnvRe.match(text, searchPos);

      if (!nextEnd.hasMatch())
        return {-1, -1};

      bool beginFirst = nextBegin.hasMatch() &&
                        nextBegin.capturedStart() < nextEnd.capturedStart();

      if (beginFirst && nextBegin.captured(1) == envName) {
        ++depth;
        searchPos = nextBegin.capturedEnd();
      } else if (!beginFirst && nextEnd.captured(1) == envName) {
        --depth;
        if (depth == 0)
          return {position, nextEnd.capturedStart()};
        searchPos = nextEnd.capturedEnd();
      } else {
        searchPos = beginFirst ? nextBegin.capturedEnd()
                               : nextEnd.capturedEnd();
      }
    }

    return {-1, -1};
  }

  QRegularExpressionMatch endMatch = s_endEnvRe.match(text, position);
  if (endMatch.hasMatch() && endMatch.capturedStart() == position) {
    QString envName = endMatch.captured(1);
    int depth = 1;
    int searchPos = position;

    while (depth > 0 && searchPos > 0) {
      int bestBeginPos = -1;
      int bestEndPos = -1;
      QRegularExpressionMatch bestBegin;
      QRegularExpressionMatch bestEnd;

      QRegularExpressionMatchIterator beginIt =
          s_beginEnvRe.globalMatch(text);
      while (beginIt.hasNext()) {
        QRegularExpressionMatch m = beginIt.next();
        if (m.capturedStart() < searchPos && m.captured(1) == envName) {
          if (m.capturedStart() > bestBeginPos) {
            bestBeginPos = m.capturedStart();
            bestBegin = m;
          }
        }
      }

      QRegularExpressionMatchIterator endIt = s_endEnvRe.globalMatch(text);
      while (endIt.hasNext()) {
        QRegularExpressionMatch m = endIt.next();
        if (m.capturedStart() < searchPos &&
            m.capturedStart() != position && m.captured(1) == envName) {
          if (m.capturedStart() > bestEndPos) {
            bestEndPos = m.capturedStart();
            bestEnd = m;
          }
        }
      }

      if (bestBeginPos < 0)
        return {-1, -1};

      if (bestEndPos > bestBeginPos) {
        ++depth;
        searchPos = bestEndPos;
      } else {
        --depth;
        if (depth == 0)
          return {bestBeginPos, position};
        searchPos = bestBeginPos;
      }
    }

    return {-1, -1};
  }

  return {-1, -1};
}

QString LatexTools::resolveIncludePath(const QString &includePath,
                                       const QString &basePath) {
  QDir baseDir(basePath);
  if (QFileInfo(basePath).isFile())
    baseDir = QFileInfo(basePath).absoluteDir();

  QString resolved = baseDir.absoluteFilePath(includePath);

  if (!QFileInfo::exists(resolved) && !resolved.endsWith(".tex"))
    resolved = baseDir.absoluteFilePath(includePath + ".tex");

  return QDir::cleanPath(resolved);
}

// --- Private lint helpers ---

QList<LspDiagnostic> LatexTools::checkUnclosedEnvironments(const QString &text) {
  QList<LspDiagnostic> diagnostics;
  const QStringList lines = text.split('\n');

  struct EnvEntry {
    QString name;
    int line;
    int col;
  };
  QList<EnvEntry> stack;

  for (int i = 0; i < lines.size(); ++i) {
    const QString &line = lines[i];

    QRegularExpressionMatchIterator beginIt = s_beginEnvRe.globalMatch(line);
    while (beginIt.hasNext()) {
      QRegularExpressionMatch match = beginIt.next();
      if (isInComment(line, match.capturedStart()))
        continue;
      stack.append({match.captured(1), i,
                    static_cast<int>(match.capturedStart())});
    }

    QRegularExpressionMatchIterator endIt = s_endEnvRe.globalMatch(line);
    while (endIt.hasNext()) {
      QRegularExpressionMatch match = endIt.next();
      if (isInComment(line, match.capturedStart()))
        continue;

      QString envName = match.captured(1);
      bool found = false;

      for (int k = stack.size() - 1; k >= 0; --k) {
        if (stack[k].name == envName) {
          stack.removeAt(k);
          found = true;
          break;
        }
      }

      if (!found) {
        LspDiagnostic diag;
        diag.range.start = {i, static_cast<int>(match.capturedStart())};
        diag.range.end = {i, static_cast<int>(match.capturedEnd())};
        diag.severity = LspDiagnosticSeverity::Error;
        diag.source = "latex-lint";
        diag.code = "ENV001";
        diag.message =
            QString("\\end{%1} without matching \\begin{%1}")
                .arg(envName);
        diagnostics.append(diag);
      }
    }
  }

  for (const EnvEntry &e : stack) {
    LspDiagnostic diag;
    diag.range.start = {e.line, e.col};
    diag.range.end = {e.line, e.col};
    diag.severity = LspDiagnosticSeverity::Error;
    diag.source = "latex-lint";
    diag.code = "ENV002";
    diag.message =
        QString("\\begin{%1} without matching \\end{%1}").arg(e.name);
    diagnostics.append(diag);
  }

  return diagnostics;
}

QList<LspDiagnostic> LatexTools::checkDuplicateLabels(const QString &text) {
  QList<LspDiagnostic> diagnostics;
  QList<LatexLabel> labels = extractLabels(text);
  QMap<QString, int> seen;

  for (const LatexLabel &lbl : labels) {
    if (seen.contains(lbl.name)) {
      LspDiagnostic diag;
      diag.range.start = {lbl.lineNumber, 0};
      diag.range.end = {lbl.lineNumber, 0};
      diag.severity = LspDiagnosticSeverity::Error;
      diag.source = "latex-lint";
      diag.code = "LBL001";
      diag.message =
          QString("Duplicate label '%1' (first defined on line %2)")
              .arg(lbl.name)
              .arg(seen[lbl.name] + 1);
      diagnostics.append(diag);
    } else {
      seen[lbl.name] = lbl.lineNumber;
    }
  }

  return diagnostics;
}

QList<LspDiagnostic>
LatexTools::checkUndefinedReferences(const QString &text) {
  QList<LspDiagnostic> diagnostics;
  QList<LatexLabel> labels = extractLabels(text);
  QSet<QString> labelNames;
  for (const LatexLabel &lbl : labels)
    labelNames.insert(lbl.name);

  const QStringList lines = text.split('\n');
  static const QRegularExpression s_refOnlyRe(
      R"(\\(ref|eqref|pageref)\{([^}]*)\})");

  for (int i = 0; i < lines.size(); ++i) {
    const QString &line = lines[i];

    QRegularExpressionMatchIterator it = s_refOnlyRe.globalMatch(line);
    while (it.hasNext()) {
      QRegularExpressionMatch match = it.next();
      if (isInComment(line, match.capturedStart()))
        continue;

      QString refName = match.captured(2).trimmed();
      if (!refName.isEmpty() && !labelNames.contains(refName)) {
        LspDiagnostic diag;
        diag.range.start = {i, static_cast<int>(match.capturedStart())};
        diag.range.end = {i, static_cast<int>(match.capturedEnd())};
        diag.severity = LspDiagnosticSeverity::Warning;
        diag.source = "latex-lint";
        diag.code = "REF001";
        diag.message =
            QString("Undefined reference '%1'").arg(refName);
        diagnostics.append(diag);
      }
    }
  }

  return diagnostics;
}

QList<LspDiagnostic>
LatexTools::checkMissingDocumentClass(const QString &text,
                                      const QString &filePath) {
  QList<LspDiagnostic> diagnostics;

  if (!filePath.isEmpty()) {
    QString ext = QFileInfo(filePath).suffix().toLower();
    if (ext == "sty" || ext == "cls" || ext == "dtx" || ext == "ins" ||
        ext == "bib")
      return diagnostics;
  }

  if (text.contains("\\input") || text.contains("\\include"))
    return diagnostics;

  if (detectDocumentClass(text).isEmpty()) {
    LspDiagnostic diag;
    diag.range.start = {0, 0};
    diag.range.end = {0, 0};
    diag.severity = LspDiagnosticSeverity::Information;
    diag.source = "latex-lint";
    diag.code = "DOC001";
    diag.message = "No \\documentclass found";
    diagnostics.append(diag);
  }

  return diagnostics;
}

QList<LspDiagnostic> LatexTools::checkUnclosedMath(const QString &text) {
  QList<LspDiagnostic> diagnostics;
  const QStringList lines = text.split('\n');
  bool inDisplayMath = false;
  int displayMathLine = 0;

  for (int i = 0; i < lines.size(); ++i) {
    const QString &line = lines[i];
    int inlineDollarCount = 0;
    int firstDollarCol = -1;

    for (int j = 0; j < line.length(); ++j) {
      if (line[j] == '%' && (j == 0 || line[j - 1] != '\\'))
        break;

      if (line[j] == '$' && (j == 0 || line[j - 1] != '\\')) {
        if (j + 1 < line.length() && line[j + 1] == '$') {
          inDisplayMath = !inDisplayMath;
          if (inDisplayMath)
            displayMathLine = i;
          ++j;
          continue;
        }

        ++inlineDollarCount;
        if (firstDollarCol < 0)
          firstDollarCol = j;
      }
    }

    if (inlineDollarCount % 2 != 0) {
      LspDiagnostic diag;
      diag.range.start = {i, firstDollarCol};
      diag.range.end = {i, firstDollarCol + 1};
      diag.severity = LspDiagnosticSeverity::Warning;
      diag.source = "latex-lint";
      diag.code = "MATH001";
      diag.message = "Possible unclosed inline math mode ($)";
      diagnostics.append(diag);
    }
  }

  if (inDisplayMath) {
    LspDiagnostic diag;
    diag.range.start = {displayMathLine, 0};
    diag.range.end = {displayMathLine, 0};
    diag.severity = LspDiagnosticSeverity::Warning;
    diag.source = "latex-lint";
    diag.code = "MATH002";
    diag.message = "Unclosed display math mode ($$)";
    diagnostics.append(diag);
  }

  return diagnostics;
}

QList<LspDiagnostic> LatexTools::checkCommonTypos(const QString &text) {
  QList<LspDiagnostic> diagnostics;
  const QStringList lines = text.split('\n');

  static const QList<QPair<QRegularExpression, QString>> s_typos = {
      {QRegularExpression(R"(\\being\{)"),
       "\\being should be \\begin"},
      {QRegularExpression(R"(\\ned\{)"),
       "\\ned should be \\end"},
      {QRegularExpression(R"(\\bigen\{)"),
       "\\bigen should be \\begin"},
      {QRegularExpression(R"(\\ednl?\{)"),
       "\\edn/\\ednl should be \\end"},
      {QRegularExpression(R"(\\usepackge\{)"),
       "\\usepackge should be \\usepackage"},
      {QRegularExpression(R"(\\docuemntclass\{)"),
       "\\docuemntclass should be \\documentclass"},
      {QRegularExpression(R"(\\documentcalss\{)"),
       "\\documentcalss should be \\documentclass"},
      {QRegularExpression(R"(\\incldue\{)"),
       "\\incldue should be \\include"},
      {QRegularExpression(R"(\\labek\{)"),
       "\\labek should be \\label"},
  };

  for (int i = 0; i < lines.size(); ++i) {
    const QString &line = lines[i];

    for (const auto &[re, msg] : s_typos) {
      QRegularExpressionMatchIterator it = re.globalMatch(line);
      while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        if (isInComment(line, match.capturedStart()))
          continue;

        LspDiagnostic diag;
        diag.range.start = {i, static_cast<int>(match.capturedStart())};
        diag.range.end = {i, static_cast<int>(match.capturedEnd())};
        diag.severity = LspDiagnosticSeverity::Error;
        diag.source = "latex-lint";
        diag.code = "TYPO001";
        diag.message = msg;
        diagnostics.append(diag);
      }
    }
  }

  return diagnostics;
}
