#ifndef MULTICURSOR_H
#define MULTICURSOR_H

#include <QColor>
#include <QList>
#include <QTextCursor>
#include <functional>

class QPlainTextEdit;
class QTextDocument;

class MultiCursorHandler {
public:
  explicit MultiCursorHandler(QPlainTextEdit *editor);

  void addCursorAbove();

  void addCursorBelow();

  void addCursorAtNextOccurrence();

  void addCursorsToAllOccurrences();

  void clearExtraCursors();

  bool hasMultipleCursors() const;

  int cursorCount() const;

  const QList<QTextCursor> &extraCursors() const { return m_extraCursors; }

  QList<QTextCursor> &extraCursorsRef() { return m_extraCursors; }

  void applyToAllCursors(const std::function<void(QTextCursor &)> &operation);

  void updateExtraSelections(const QColor &highlightColor);

  QString lastSelectedWord() const { return m_lastSelectedWord; }

private:
  void mergeOverlappingCursors();

  QPlainTextEdit *m_editor;
  QList<QTextCursor> m_extraCursors;
  QString m_lastSelectedWord;
};

#endif
