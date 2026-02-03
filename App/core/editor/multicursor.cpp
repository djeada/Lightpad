#include "multicursor.h"
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextDocument>
#include <algorithm>

MultiCursorHandler::MultiCursorHandler(QPlainTextEdit *editor)
    : m_editor(editor) {}

void MultiCursorHandler::addCursorAbove() {
  if (!m_editor)
    return;

  QTextCursor cursor = m_editor->textCursor();
  int col = cursor.positionInBlock();

  if (cursor.blockNumber() > 0) {
    QTextCursor newCursor = cursor;
    newCursor.movePosition(QTextCursor::PreviousBlock);

    // Try to maintain column position
    int lineLength = newCursor.block().text().length();
    int targetCol = qMin(col, lineLength);
    newCursor.movePosition(QTextCursor::StartOfBlock);
    newCursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                           targetCol);

    m_extraCursors.append(cursor);
    m_editor->setTextCursor(newCursor);
  }
}

void MultiCursorHandler::addCursorBelow() {
  if (!m_editor)
    return;

  QTextCursor cursor = m_editor->textCursor();
  int col = cursor.positionInBlock();

  if (cursor.blockNumber() < m_editor->document()->blockCount() - 1) {
    QTextCursor newCursor = cursor;
    newCursor.movePosition(QTextCursor::NextBlock);

    // Try to maintain column position
    int lineLength = newCursor.block().text().length();
    int targetCol = qMin(col, lineLength);
    newCursor.movePosition(QTextCursor::StartOfBlock);
    newCursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                           targetCol);

    m_extraCursors.append(cursor);
    m_editor->setTextCursor(newCursor);
  }
}

void MultiCursorHandler::addCursorAtNextOccurrence() {
  if (!m_editor)
    return;

  QTextCursor cursor = m_editor->textCursor();
  QString word;

  if (cursor.hasSelection()) {
    word = cursor.selectedText();
  } else {
    cursor.select(QTextCursor::WordUnderCursor);
    word = cursor.selectedText();
    m_editor->setTextCursor(cursor);
  }

  if (word.isEmpty())
    return;

  m_lastSelectedWord = word;

  // Find next occurrence after current cursor
  int startSearchPos = cursor.selectionEnd();

  // Search forward from cursor
  QTextDocument *doc = m_editor->document();
  QTextCursor found = doc->find(word, startSearchPos);

  // Wrap around if not found
  if (found.isNull()) {
    found = doc->find(word, 0);
  }

  if (!found.isNull() && found.selectionStart() != cursor.selectionStart()) {
    m_extraCursors.append(cursor);
    m_editor->setTextCursor(found);
  }
}

void MultiCursorHandler::addCursorsToAllOccurrences() {
  if (!m_editor)
    return;

  QTextCursor cursor = m_editor->textCursor();
  QString word;

  if (cursor.hasSelection()) {
    word = cursor.selectedText();
  } else {
    cursor.select(QTextCursor::WordUnderCursor);
    word = cursor.selectedText();
  }

  if (word.isEmpty())
    return;

  m_lastSelectedWord = word;
  m_extraCursors.clear();

  // Find all occurrences
  QTextDocument *doc = m_editor->document();
  QTextCursor searchCursor(doc);
  QTextCursor firstCursor;
  bool first = true;

  while (true) {
    QTextCursor found = doc->find(word, searchCursor);
    if (found.isNull())
      break;

    if (first) {
      firstCursor = found;
      first = false;
    } else {
      m_extraCursors.append(found);
    }

    searchCursor = found;
    searchCursor.movePosition(QTextCursor::Right);
  }

  if (!first) {
    m_editor->setTextCursor(firstCursor);
  }
}

void MultiCursorHandler::clearExtraCursors() {
  m_extraCursors.clear();
  m_lastSelectedWord.clear();
}

bool MultiCursorHandler::hasMultipleCursors() const {
  return !m_extraCursors.isEmpty();
}

int MultiCursorHandler::cursorCount() const {
  return m_extraCursors.size() + 1;
}

void MultiCursorHandler::applyToAllCursors(
    const std::function<void(QTextCursor &)> &operation) {
  if (!m_editor)
    return;

  // Apply to main cursor
  QTextCursor mainCursor = m_editor->textCursor();
  operation(mainCursor);
  m_editor->setTextCursor(mainCursor);

  // Apply to extra cursors
  for (QTextCursor &cursor : m_extraCursors) {
    operation(cursor);
  }

  mergeOverlappingCursors();
}

void MultiCursorHandler::updateExtraSelections(const QColor &highlightColor) {
  if (!m_editor)
    return;

  QList<QTextEdit::ExtraSelection> selections = m_editor->extraSelections();

  // Add extra cursor highlights
  for (const QTextCursor &cursor : m_extraCursors) {
    QTextEdit::ExtraSelection selection;
    selection.cursor = cursor;

    if (cursor.hasSelection()) {
      // Highlight selection
      selection.format.setBackground(QColor(38, 79, 120));
    } else {
      // Show cursor line
      selection.format.setBackground(highlightColor.lighter(110));
      selection.format.setProperty(QTextFormat::FullWidthSelection, false);
    }

    selections.append(selection);
  }

  m_editor->setExtraSelections(selections);
}

void MultiCursorHandler::mergeOverlappingCursors() {
  if (m_extraCursors.isEmpty() || !m_editor)
    return;

  QTextCursor mainCursor = m_editor->textCursor();
  QList<QTextCursor> allCursors;
  allCursors.append(mainCursor);
  allCursors.append(m_extraCursors);

  // Sort by position
  std::sort(allCursors.begin(), allCursors.end(),
            [](const QTextCursor &a, const QTextCursor &b) {
              return a.position() < b.position();
            });

  // Remove duplicates
  QList<QTextCursor> unique;
  for (const QTextCursor &cursor : allCursors) {
    bool duplicate = false;
    for (const QTextCursor &existing : unique) {
      if (cursor.position() == existing.position()) {
        duplicate = true;
        break;
      }
    }
    if (!duplicate) {
      unique.append(cursor);
    }
  }

  if (unique.size() > 0) {
    m_editor->setTextCursor(unique.first());
    m_extraCursors = unique.mid(1);
  }
}
