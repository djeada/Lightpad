#ifndef MARKDOWNTOOLS_H
#define MARKDOWNTOOLS_H

#include "../lsp/lspclient.h"
#include <QList>
#include <QMap>
#include <QPair>
#include <QRegularExpression>
#include <QString>

struct MarkdownHeading {
  int level;
  QString text;
  int lineNumber;
  QString anchor;
};

struct MarkdownLink {
  QString text;
  QString target;
  int lineNumber;
  int column;
  bool isImage;
};

struct MarkdownFootnote {
  QString label;
  QString text;
  int definitionLine;
  QList<int> referenceLines;
};

struct MarkdownFrontmatter {
  QMap<QString, QString> fields;
  int startLine;
  int endLine;
  QString rawContent;
};

struct MarkdownOutlineEntry {
  int level;
  QString text;
  QString anchor;
  int lineNumber;
  QList<MarkdownOutlineEntry> children;
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

  static QString wrapParagraph(const QString &text, int width = 80);
  static QString normalizeHeadingSpacing(const QString &text);
  static QString normalizeBulletIndentation(const QString &text);
  static QString formatCodeFences(const QString &text);

  static QList<MarkdownLink> extractLinks(const QString &text);
  static QList<MarkdownLink> extractImages(const QString &text);
  static QList<MarkdownFootnote> extractFootnotes(const QString &text);
  static MarkdownFrontmatter parseFrontmatter(const QString &text);
  static QList<MarkdownOutlineEntry>
  generateDocumentOutline(const QString &text);

  static int wordCount(const QString &text);
  static int readingTimeMinutes(const QString &text, int wordsPerMinute = 200);

private:
  static QString escapeHtml(const QString &text);
  static QString processInlineFormatting(const QString &text);
  static QList<LspDiagnostic> checkDuplicateHeadings(const QString &text);
  static QList<LspDiagnostic> checkBrokenLocalLinks(const QString &text,
                                                    const QString &filePath);
  static QList<LspDiagnostic> checkBrokenImagePaths(const QString &text,
                                                    const QString &filePath);
  static QList<LspDiagnostic> checkMissingAltText(const QString &text);
  static QList<LspDiagnostic> checkTrailingSpaces(const QString &text);
  static QList<LspDiagnostic>
  checkInconsistentHeadingLevels(const QString &text);
  static QList<LspDiagnostic> checkMalformedLists(const QString &text);
  static QList<LspDiagnostic> checkOverlongLines(const QString &text,
                                                 int maxLength = 120);
};

#endif
