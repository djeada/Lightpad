#include "completioninsertion.h"
#include "snippetregistry.h"
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextDocument>

namespace {
bool identifierCharacter(QChar ch) {
  return ch.isLetterOrNumber() || ch == '_' || ch == '$';
}

int identifierStart(const QTextCursor &cursor) {
  const QString line = cursor.block().text();
  int start = cursor.positionInBlock();
  while (start > 0 && identifierCharacter(line.at(start - 1)))
    --start;
  return cursor.block().position() + start;
}

int position(QTextDocument *document, int line, int column) {
  if (line < 0 || column < 0)
    return -1;
  const auto block = document->findBlockByNumber(line);
  return block.isValid() && column <= block.text().size()
             ? block.position() + column
             : -1;
}
} // namespace

QString CompletionInsertion::prefix(const QTextCursor &cursor) {
  if (cursor.isNull())
    return {};
  return cursor.block().text().mid(identifierStart(cursor) -
                                       cursor.block().position(),
                                   cursor.position() - identifierStart(cursor));
}

bool CompletionInsertion::apply(QTextCursor &cursor,
                                const CompletionItem &item) {
  if (cursor.isNull())
    return false;
  QTextDocument *document = cursor.document();
  int start = cursor.selectionStart();
  int end = cursor.selectionEnd();
  if (item.textEditRange) {
    const auto range = *item.textEditRange;
    start = position(document, range.startLine, range.startCharacter);
    end = position(document, range.endLine, range.endCharacter);
    if (start < 0 || end < start || cursor.position() < start ||
        cursor.position() > end)
      return false;
  } else if (!cursor.hasSelection()) {
    start = identifierStart(cursor);
    const QString line = cursor.block().text();
    int column = cursor.positionInBlock();
    while (column < line.size() && identifierCharacter(line.at(column)))
      ++column;
    end = cursor.block().position() + column;
  }

  const QString raw = item.effectiveInsertText();
  if (raw.isEmpty() && !item.textEditRange)
    return false;
  Snippet::Expansion expanded;
  if (item.isSnippet)
    expanded = Snippet::expandWithSelection(raw);
  else {
    expanded.text = raw;
    expanded.cursorOffset = raw.size();
  }

  const bool callable = item.kind == CompletionItemKind::Function ||
                        item.kind == CompletionItemKind::Method ||
                        item.kind == CompletionItemKind::Constructor;
  static const QRegularExpression identifier(
      QStringLiteral("^(?:::)?[\\p{L}_$][\\p{L}\\p{N}_$]*(?:(?:\\.|::|->)[\\p{"
                     "L}_$][\\p{L}\\p{N}_$]*)*$"));
  static const QRegularExpression referenceContext(QStringLiteral(
      "^\\s*(?:from\\b.*\\bimport\\b|import\\b|using\\b|use\\b|#include\\b)"));
  const QString before = document->findBlock(start).text().left(
      start - document->findBlock(start).position());
  const bool addCall = callable && identifier.match(expanded.text).hasMatch() &&
                       !referenceContext.match(before).hasMatch() &&
                       !before.trimmed().endsWith('&');
  int next = end;
  while (next < document->characterCount() - 1 &&
         (document->characterAt(next) == ' ' ||
          document->characterAt(next) == '\t'))
    ++next;
  const bool existingCall = document->characterAt(next) == '(';
  const int open = expanded.text.indexOf('(');
  const bool simpleCall = open > 0 && expanded.text.endsWith(')') &&
                          identifier.match(expanded.text.left(open)).hasMatch();
  if (callable && existingCall && (addCall || simpleCall)) {
    if (simpleCall)
      expanded.text.truncate(open);
    expanded.cursorOffset = expanded.text.size() + (next - end) + 1;
    expanded.selectionLength = 0;
  } else if (addCall) {
    expanded.text += QStringLiteral("()");
    expanded.cursorOffset = expanded.text.size() - 1;
    expanded.selectionLength = 0;
  } else if (callable && !expanded.hasTabstop && expanded.text.endsWith("()")) {
    expanded.cursorOffset = expanded.text.size() - 1;
  }

  QTextCursor edit(cursor);
  edit.beginEditBlock();
  edit.setPosition(start);
  edit.setPosition(end, QTextCursor::KeepAnchor);
  edit.insertText(expanded.text);
  edit.endEditBlock();
  edit.setPosition(start + expanded.cursorOffset);
  edit.setPosition(start + expanded.cursorOffset + expanded.selectionLength,
                   QTextCursor::KeepAnchor);
  cursor = edit;
  return true;
}
