#ifndef MARKDOWNTOOLS_H
#define MARKDOWNTOOLS_H

#include "../lsp/lspclient.h"
#include <QList>
#include <QPair>
#include <QRegularExpression>
#include <QString>

struct MarkdownHeading {
  int level;
  QString text;
  int lineNumber;
  QString anchor;
};

class MarkdownTools {
public:
  static QList<MarkdownHeading> extractHeadings(const QString &text);

  static QString generateAnchor(const QString &headingText);

  static QString generateToc(const QString &text, int maxDepth = 3);

  static QString insertOrUpdateToc(const QString &text, int maxDepth = 3);

  static QString toggleCheckbox(const QString &lineText);

  static int countTasks(const QString &text, int *completed = nullptr);

  static QString formatTable(const QString &tableText);

  static QString normalizeListNumbering(const QString &text);

  static QList<LspDiagnostic> lint(const QString &text,
                                   const QString &filePath = QString());

  static QString toHtml(const QString &markdown,
                        const QString &basePath = QString());

private:
  static QString escapeHtml(const QString &text);
  static QString processInlineFormatting(const QString &text);
  static QList<LspDiagnostic> checkDuplicateHeadings(const QString &text);
  static QList<LspDiagnostic> checkBrokenLocalLinks(const QString &text,
                                                     const QString &filePath);
  static QList<LspDiagnostic> checkMissingAltText(const QString &text);
  static QList<LspDiagnostic> checkTrailingSpaces(const QString &text);
  static QList<LspDiagnostic> checkInconsistentHeadingLevels(
      const QString &text);
};

#endif
