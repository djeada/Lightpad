#include "indentation.h"

#include <QRegularExpression>
#include <QSet>
#include <QTextBlock>
#include <QTextDocument>
#include <QVector>

namespace {
bool isIndent(QChar ch) { return ch == ' ' || ch == '\t'; }

QString normalizedIndent(const QString &indent,
                         const EditorIndentation::Options &options) {
  return options.expandTabs
             ? QString(EditorIndentation::visualColumn(indent, options.width),
                       ' ')
             : indent;
}

QString nextIndent(const QString &indent,
                   const EditorIndentation::Options &options) {
  const QString base = normalizedIndent(indent, options);
  return base +
         (!options.expandTabs && (options.preferTabs || indent.contains('\t'))
              ? QStringLiteral("\t")
              : QString(qMax(1, options.width), ' '));
}

struct Opening {
  QChar character;
  int line;
  int column;
  QString indent;
  bool followedByCode = false;
};

struct Context {
  QVector<Opening> openings;
  QString statement;
  QString statementIndent;
  QString lineCode;
  bool inString = false;
  bool continued = false;
};

bool matches(QChar open, QChar close) {
  return (open == '(' && close == ')') || (open == '[' && close == ']') ||
         (open == '{' && close == '}');
}

// Keep quotes and comments out of the indentation rules. In Python, the
// logical statement can start several physical lines before a closing `):`.
Context contextBefore(const QTextCursor &cursor,
                      const EditorIndentation::Options &options) {
  const bool python = options.language == "py";
  const bool sql = options.language == "sql";
  static const QSet<QString> hashComments = {
      "py", "yaml", "sh", "cmake", "make", "dockerfile", "bazel", "meson"};
  static const QSet<QString> slashComments = {"c",    "cpp",  "java",  "js",
                                              "ts",   "go",   "rust",  "css",
                                              "glsl", "hlsl", "metal", "wgsl"};
  Context context;
  QChar quote;
  bool tripleQuote = false;
  bool blockComment = false;
  bool escaped = false;
  for (QTextBlock block = cursor.document()->begin(); block.isValid();
       block = block.next()) {
    if (block.position() > cursor.position())
      break;
    const QString text =
        block.text().left(block == cursor.block() ? cursor.positionInBlock()
                                                  : block.text().size());
    if (context.openings.isEmpty() && !context.continued && quote.isNull()) {
      context.statement.clear();
      context.statementIndent = EditorIndentation::leadingWhitespace(text);
    }
    QString code(text.size(), ' ');
    for (int i = 0; i < text.size(); ++i) {
      const QChar ch = text[i];
      if (blockComment) {
        if (ch == '*' && i + 1 < text.size() && text[i + 1] == '/') {
          blockComment = false;
          ++i;
        }
        continue;
      }
      if (!quote.isNull()) {
        if (escaped) {
          escaped = false;
          continue;
        }
        if (ch == '\\') {
          escaped = true;
          continue;
        }
        if (ch == quote &&
            (!tripleQuote || (i + 2 < text.size() && text[i + 1] == quote &&
                              text[i + 2] == quote))) {
          if (tripleQuote)
            i += 2;
          quote = QChar();
          tripleQuote = false;
        }
        continue;
      }
      if (sql && ch == '-' && i + 1 < text.size() && text[i + 1] == '-')
        break;
      if ((hashComments.contains(options.language) && ch == '#') ||
          (slashComments.contains(options.language) && ch == '/' &&
           i + 1 < text.size() && text[i + 1] == '/'))
        break;
      if ((sql || slashComments.contains(options.language)) && ch == '/' &&
          i + 1 < text.size() && text[i + 1] == '*') {
        blockComment = true;
        ++i;
        continue;
      }
      // A string is a token, but its contents cannot open a block or a pair.
      if (ch == '\'' || ch == '"' ||
          (ch == '`' &&
           (sql || options.language == "js" || options.language == "ts"))) {
        code[i] = 's';
        if (!context.openings.isEmpty())
          context.openings.last().followedByCode = true;
        quote = ch;
        tripleQuote = python && i + 2 < text.size() && text[i + 1] == ch &&
                      text[i + 2] == ch;
        if (tripleQuote)
          i += 2;
        continue;
      }
      code[i] = ch;
      if (ch.isSpace())
        continue;
      if (!context.openings.isEmpty())
        context.openings.last().followedByCode = true;
      if (ch == '(' || ch == '[' || ch == '{') {
        context.openings.append(
            {ch, block.blockNumber(),
             EditorIndentation::visualColumn(text.left(i + 1), options.width),
             EditorIndentation::leadingWhitespace(text), false});
      } else if (!context.openings.isEmpty() &&
                 matches(context.openings.last().character, ch)) {
        context.openings.removeLast();
      }
    }
    context.lineCode = code.trimmed();
    context.statement += ' ' + context.lineCode;
    context.continued = python && context.lineCode.endsWith('\\');
    context.inString = !quote.isNull();
    if (block == cursor.block())
      break;
    // An ordinary unescaped Python/C string does not span physical lines.
    if (!tripleQuote && quote != '`' && !escaped)
      quote = QChar();
    escaped = false;
  }
  return context;
}
} // namespace

bool EditorIndentation::useSpaces(const QString &language, bool preference) {
  if (language == "py")
    return true;
  if (language == "make")
    return false;
  return preference;
}

int EditorIndentation::visualColumn(const QString &text, int width) {
  width = qMax(1, width);
  int column = 0;
  for (const QChar ch : text)
    column += ch == '\t' ? width - column % width : 1;
  return column;
}

QString EditorIndentation::leadingWhitespace(const QString &text) {
  int length = 0;
  while (length < text.size() && isIndent(text[length]))
    ++length;
  return text.left(length);
}

void EditorIndentation::insertNewline(QTextCursor &cursor,
                                      const Options &options) {
  cursor.beginEditBlock();
  cursor.removeSelectedText();
  const QString before = cursor.block().text().left(cursor.positionInBlock());
  QString indent;
  QString closingIndent;
  bool splitPair = false;
  if (options.enabled) {
    const QString base = leadingWhitespace(before);
    indent = normalizedIndent(base, options);
    const Context context = before.size() == base.size()
                                ? Context{}
                                : contextBefore(cursor, options);
    if (!context.inString) {
      if (!context.openings.isEmpty()) {
        const Opening &opening = context.openings.last();
        if (opening.line == cursor.blockNumber()) {
          const bool align =
              opening.followedByCode &&
              (opening.character != '{' || options.language == "py");
          indent = align ? QString(opening.column, ' ')
                         : nextIndent(opening.indent, options);
        } else if (visualColumn(base, options.width) <=
                   visualColumn(opening.indent, options.width)) {
          indent = nextIndent(opening.indent, options);
        }
        const QString rest =
            cursor.block().text().mid(cursor.positionInBlock());
        const QString trimmedRest = rest.trimmed();
        splitPair = !context.lineCode.isEmpty() &&
                    context.lineCode.back() == opening.character &&
                    !trimmedRest.isEmpty() &&
                    matches(opening.character, trimmedRest.front());
        if (splitPair) {
          closingIndent = normalizedIndent(opening.indent, options);
          // The closer gets its own indentation, replacing only its whitespace.
          QTextCursor whitespace = cursor;
          whitespace.setPosition(cursor.position() +
                                     leadingWhitespace(rest).size(),
                                 QTextCursor::KeepAnchor);
          whitespace.removeSelectedText();
        }
      } else {
        static const QRegularExpression pythonBlock(
            "^(?:(?:async\\s+)?(?:def|for|with)\\b|"
            "(?:if|elif|else|while|class|try|except|finally|match|case)\\b)");
        if (context.lineCode.endsWith(':') &&
            ((options.language == "py" &&
              pythonBlock.match(context.statement.trimmed()).hasMatch()) ||
             options.language == "yaml")) {
          indent = nextIndent(context.statementIndent, options);
        } else if (context.continued && options.language == "py" &&
                   visualColumn(base, options.width) <=
                       visualColumn(context.statementIndent, options.width)) {
          indent = nextIndent(context.statementIndent, options);
        }
      }
    }
  }
  cursor.insertText('\n' + indent);
  if (splitPair) {
    const int position = cursor.position();
    cursor.insertText('\n' + closingIndent);
    cursor.setPosition(position);
  }
  cursor.endEditBlock();
}

bool EditorIndentation::backspace(QTextCursor &cursor, int width) {
  if (cursor.hasSelection() || cursor.positionInBlock() == 0)
    return false;
  const QString before = cursor.block().text().left(cursor.positionInBlock());
  if (leadingWhitespace(before).size() != before.size())
    return false;
  width = qMax(1, width);
  const int column = visualColumn(before, width);
  const int target = (column - 1) / width * width;
  int keep = 0;
  int keptColumn = 0;
  while (keep < before.size()) {
    const int next =
        keptColumn + (before[keep] == '\t' ? width - keptColumn % width : 1);
    if (next > target)
      break;
    keptColumn = next;
    ++keep;
  }
  cursor.beginEditBlock();
  cursor.setPosition(cursor.block().position() + keep, QTextCursor::KeepAnchor);
  cursor.removeSelectedText();
  cursor.endEditBlock();
  return true;
}

void EditorIndentation::tab(QTextCursor &cursor, int width, bool expandTabs,
                            bool outdent) {
  width = qMax(1, width);
  if (!cursor.hasSelection() && !outdent) {
    const int column = visualColumn(
        cursor.block().text().left(cursor.positionInBlock()), width);
    cursor.insertText(expandTabs ? QString(width - column % width, ' ')
                                 : QString("\t"));
    return;
  }
  const bool selected = cursor.hasSelection();
  const bool backwards = cursor.position() < cursor.anchor();
  auto *document = cursor.document();
  const int first = document->findBlock(cursor.selectionStart()).blockNumber();
  const int end = selected ? cursor.selectionEnd() - 1 : cursor.position();
  const int last = document->findBlock(end).blockNumber();
  const int oldColumn = cursor.positionInBlock();
  int removed = 0;
  cursor.beginEditBlock();
  for (int line = first; line <= last; ++line) {
    QTextBlock block = document->findBlockByNumber(line);
    QTextCursor edit(block);
    if (outdent) {
      const QString text = block.text();
      int count = 0;
      if (text.startsWith('\t'))
        count = 1;
      else
        while (count < width && count < text.size() && text[count] == ' ')
          ++count;
      edit.setPosition(block.position() + count, QTextCursor::KeepAnchor);
      edit.removeSelectedText();
      removed = count;
    } else {
      edit.insertText(expandTabs ? QString(width, ' ') : QString("\t"));
    }
  }
  cursor.endEditBlock();
  const int start = document->findBlockByNumber(first).position();
  if (selected) {
    const auto block = document->findBlockByNumber(last);
    const int finish = block.position() + block.length() - 1;
    cursor.setPosition(backwards ? finish : start);
    cursor.setPosition(backwards ? start : finish, QTextCursor::KeepAnchor);
  } else {
    cursor.setPosition(start + qMax(0, oldColumn - removed));
  }
}
