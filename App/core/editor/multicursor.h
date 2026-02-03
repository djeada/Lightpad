#ifndef MULTICURSOR_H
#define MULTICURSOR_H

#include <QColor>
#include <QList>
#include <QTextCursor>
#include <functional>

class QPlainTextEdit;
class QTextDocument;

/**
 * @brief Handles multiple cursor support for text editors
 *
 * Manages a list of extra cursors beyond the main cursor, enabling
 * simultaneous editing at multiple locations.
 */
class MultiCursorHandler {
public:
  explicit MultiCursorHandler(QPlainTextEdit *editor);

  /**
   * @brief Add a cursor on the line above the current cursor
   */
  void addCursorAbove();

  /**
   * @brief Add a cursor on the line below the current cursor
   */
  void addCursorBelow();

  /**
   * @brief Add a cursor at the next occurrence of the selected word
   */
  void addCursorAtNextOccurrence();

  /**
   * @brief Add cursors at all occurrences of the selected word
   */
  void addCursorsToAllOccurrences();

  /**
   * @brief Clear all extra cursors
   */
  void clearExtraCursors();

  /**
   * @brief Check if there are multiple cursors
   */
  bool hasMultipleCursors() const;

  /**
   * @brief Get total cursor count (main + extra)
   */
  int cursorCount() const;

  /**
   * @brief Get the list of extra cursors
   */
  const QList<QTextCursor> &extraCursors() const { return m_extraCursors; }

  /**
   * @brief Get mutable access to extra cursors list
   */
  QList<QTextCursor> &extraCursorsRef() { return m_extraCursors; }

  /**
   * @brief Apply an operation to all cursors (main and extra)
   */
  void applyToAllCursors(const std::function<void(QTextCursor &)> &operation);

  /**
   * @brief Update extra selections to highlight extra cursors
   * @param highlightColor Color to use for cursor line highlight
   */
  void updateExtraSelections(const QColor &highlightColor);

  /**
   * @brief Get the last selected word (for multi-select operations)
   */
  QString lastSelectedWord() const { return m_lastSelectedWord; }

private:
  void mergeOverlappingCursors();

  QPlainTextEdit *m_editor;
  QList<QTextCursor> m_extraCursors;
  QString m_lastSelectedWord;
};

#endif // MULTICURSOR_H
