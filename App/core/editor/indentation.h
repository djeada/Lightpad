#ifndef EDITORINDENTATION_H
#define EDITORINDENTATION_H

#include <QString>
#include <QTextCursor>

namespace EditorIndentation {

struct Options {
  QString language;
  int width = 4;
  bool enabled = true;
  bool expandTabs = false;
  bool preferTabs = false;
};

// Python requires spaces; Make recipes require literal tabs.
bool useSpaces(const QString &language, bool preference);

int visualColumn(const QString &text, int width);
QString leadingWhitespace(const QString &text);

// Replace a selection, split at the caret, and indent in a single undo step.
void insertNewline(QTextCursor &cursor, const Options &options);

// Indent/outdent selected lines, or insert to the next tab stop.
void tab(QTextCursor &cursor, int width, bool expandTabs, bool outdent = false);

// Only handles an unselected caret in leading spaces/tabs. Other deletion is
// left to the editor, including joining lines at column zero.
bool backspace(QTextCursor &cursor, int width);

} // namespace EditorIndentation

#endif
