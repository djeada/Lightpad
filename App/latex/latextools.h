#ifndef LATEXTOOLS_H
#define LATEXTOOLS_H

#include "../lsp/lspclient.h"
#include <QList>
#include <QPair>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

struct LatexSection {
  int level;
  QString text;
  int lineNumber;
  QString label;
};

struct LatexLabel {
  QString name;
  int lineNumber;
  QString type;
};

struct LatexInclude {
  QString path;
  int lineNumber;
  QString type;
};

struct LatexLogEntry {
  int lineNumber;
  int severity;
  QString message;
  QString file;
};

class LatexTools {
public:
  static QList<LatexSection> extractSections(const QString &text);

  static QList<LatexLabel> extractLabels(const QString &text);

  static QList<LatexLabel> extractReferences(const QString &text);

  static QList<LatexLabel> extractCitations(const QString &text);

  static QList<LatexInclude> extractIncludes(const QString &text);

  static QString detectDocumentClass(const QString &text);

  static QStringList detectPackages(const QString &text);

  static QString detectMainFile(const QString &dirPath);

  static QList<LatexLogEntry> parseLatexLog(const QString &logContent);

  static QList<LspDiagnostic> lint(const QString &text,
                                   const QString &filePath = QString());

  static bool isLatexFile(const QString &extension);

  static QString generateOutline(const QString &text, int maxDepth = 4);

  static QPair<int, int> findMatchingEnvironment(const QString &text,
                                                 int position);

  static QString resolveIncludePath(const QString &includePath,
                                    const QString &basePath);

private:
  static QList<LspDiagnostic> checkUnclosedEnvironments(const QString &text);
  static QList<LspDiagnostic> checkDuplicateLabels(const QString &text);
  static QList<LspDiagnostic> checkUndefinedReferences(const QString &text);
  static QList<LspDiagnostic>
  checkMissingDocumentClass(const QString &text, const QString &filePath);
  static QList<LspDiagnostic> checkUnclosedMath(const QString &text);
  static QList<LspDiagnostic> checkCommonTypos(const QString &text);
};

#endif
