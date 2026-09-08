#include "compilerdiagnosticparser.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace {

QString stripAnsi(const QString &text) {
  static const QRegularExpression ansi(
      QStringLiteral("\x1B\\[[0-9;?]*[ -/]*[@-~]"));
  QString cleaned = text;
  cleaned.remove(ansi);
  cleaned.remove(QLatin1Char('\r'));
  return cleaned;
}

LspDiagnosticSeverity severityFromKeyword(const QString &keyword) {
  const QString lowered = keyword.trimmed().toLower();
  if (lowered.endsWith(QLatin1String("error"))) {
    return LspDiagnosticSeverity::Error;
  }
  if (lowered == QLatin1String("warning")) {
    return LspDiagnosticSeverity::Warning;
  }

  return LspDiagnosticSeverity::Information;
}

QString resolvePath(const QString &rawPath, const QString &workingDirectory) {
  const QString trimmed = rawPath.trimmed();
  if (trimmed.isEmpty()) {
    return QString();
  }

  QFileInfo info(trimmed);
  if (info.isAbsolute()) {
    return QDir::cleanPath(info.absoluteFilePath());
  }
  if (workingDirectory.isEmpty()) {
    return QDir::cleanPath(trimmed);
  }
  return QDir::cleanPath(QDir(workingDirectory).absoluteFilePath(trimmed));
}

LspRange rangeFor(int line, int column) {
  LspRange range;
  range.start.line = qMax(0, line - 1);
  range.start.character = qMax(0, column - 1);
  range.end = range.start;
  return range;
}

} // namespace

QList<CompilerDiagnostic>
CompilerDiagnosticParser::parse(const QString &output,
                                const QString &workingDirectory) {

  static const QRegularExpression gccPattern(
      QStringLiteral("^\\s*(\\S(?:[^:]|:(?![ 0-9]))*?):(\\d+)(?::(\\d+))?:\\s+"
                     "(fatal error|error|warning|note|remark):\\s+(.*)$"),
      QRegularExpression::CaseInsensitiveOption);

  static const QRegularExpression msvcPattern(
      QStringLiteral("^\\s*(.+?)\\((\\d+)(?:,(\\d+))?\\)\\s*:\\s*"
                     "(fatal error|error|warning|note)\\s+([A-Za-z]+\\d+)\\s*:"
                     "\\s*(.*)$"),
      QRegularExpression::CaseInsensitiveOption);

  QList<CompilerDiagnostic> results;
  const QStringList lines = stripAnsi(output).split(QLatin1Char('\n'));

  for (const QString &line : lines) {
    if (line.trimmed().isEmpty()) {
      continue;
    }

    QString rawPath;
    int lineNumber = 0;
    int column = 0;
    QString keyword;
    QString message;
    QString code;

    QRegularExpressionMatch match = gccPattern.match(line);
    if (match.hasMatch()) {
      rawPath = match.captured(1);
      lineNumber = match.captured(2).toInt();
      column = match.captured(3).toInt();
      keyword = match.captured(4);
      message = match.captured(5).trimmed();
    } else {
      match = msvcPattern.match(line);
      if (!match.hasMatch()) {
        continue;
      }
      rawPath = match.captured(1);
      lineNumber = match.captured(2).toInt();
      column = match.captured(3).toInt();
      keyword = match.captured(4);
      code = match.captured(5);
      message = match.captured(6).trimmed();
    }

    const QString filePath = resolvePath(rawPath, workingDirectory);

    if (filePath.isEmpty() || !QFileInfo::exists(filePath) ||
        QFileInfo(filePath).isDir() || lineNumber <= 0 || message.isEmpty()) {
      continue;
    }

    CompilerDiagnostic entry;
    entry.filePath = filePath;
    entry.diagnostic.range = rangeFor(lineNumber, column);
    entry.diagnostic.severity = severityFromKeyword(keyword);
    entry.diagnostic.message = message;
    entry.diagnostic.source = QStringLiteral("build");
    entry.diagnostic.code = code;

    bool duplicate = false;
    for (const CompilerDiagnostic &existing : results) {
      if (existing.filePath == entry.filePath &&
          existing.diagnostic.range.start.line ==
              entry.diagnostic.range.start.line &&
          existing.diagnostic.range.start.character ==
              entry.diagnostic.range.start.character &&
          existing.diagnostic.message == entry.diagnostic.message) {
        duplicate = true;
        break;
      }
    }
    if (!duplicate) {
      results.append(entry);
    }
  }

  return results;
}

QMap<QString, QList<LspDiagnostic>>
CompilerDiagnosticParser::parseByFile(const QString &output,
                                      const QString &workingDirectory) {
  QMap<QString, QList<LspDiagnostic>> grouped;
  const QList<CompilerDiagnostic> diagnostics = parse(output, workingDirectory);
  for (const CompilerDiagnostic &entry : diagnostics) {
    grouped[entry.filePath].append(entry.diagnostic);
  }
  return grouped;
}
