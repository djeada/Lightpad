#ifndef DEBUGINLINESCOPE_H
#define DEBUGINLINESCOPE_H

#include <QRegularExpression>
#include <QString>
#include <QStringList>

// Inline values belong to the stopped function: the editor shows them from
// the function's first line down to the stop line, never in other functions
// that happen to use the same names.
namespace DebugInlineScope {

// First line (1-based) of `functionName`'s definition enclosing stopLine,
// found by searching upwards for "name(" indented less than the stop line.
// `lines` holds the document's lines up to at least stopLine. Falls back to
// 40 lines above the stop line when the name is not usable (<module>,
// anonymous functions) or no definition is found.
inline int scopeStartLine(const QStringList &lines, int stopLine,
                          const QString &functionName) {
  const int fallback = qMax(1, stopLine - 40);
  if (stopLine < 1 || stopLine > lines.size()) {
    return fallback;
  }
  // "Account.deposit", "ns::Type::method(int)", "main.main" -> the bare name.
  QString name = functionName.trimmed();
  const int paren = name.indexOf(QLatin1Char('('));
  if (paren > 0) {
    name = name.left(paren);
  }
  name = name.section(QStringLiteral("::"), -1).section(QLatin1Char('.'), -1);
  const int angle = name.indexOf(QLatin1Char('<'));
  if (angle >= 0) {
    name = name.left(angle);
  }
  name = name.trimmed();
  static const QRegularExpression identifier(
      QStringLiteral("^[A-Za-z_$][A-Za-z0-9_$]*$"));
  if (!identifier.match(name).hasMatch()) {
    return fallback;
  }
  const QRegularExpression definition(
      QStringLiteral("(^|[^A-Za-z0-9_$])%1\\s*(<[^>]*>)?\\s*\\(")
          .arg(QRegularExpression::escape(name)));
  auto indentOf = [](const QString &text) {
    int indent = 0;
    for (const QChar c : text) {
      if (c == QLatin1Char(' ')) {
        ++indent;
      } else if (c == QLatin1Char('\t')) {
        indent += 4;
      } else {
        break;
      }
    }
    return indent;
  };
  const int stopIndent = indentOf(lines.at(stopLine - 1));
  for (int line = stopLine; line >= 1 && stopLine - line <= 400; --line) {
    const QString &text = lines.at(line - 1);
    if (!definition.match(text).hasMatch()) {
      continue;
    }
    // A definition encloses its body, so it is indented less than the stop
    // line; calls at the body's own depth are skipped. Stopping on the
    // signature itself (a function breakpoint) also counts.
    if (line == stopLine || indentOf(text) < stopIndent) {
      return line;
    }
  }
  return fallback;
}

} // namespace DebugInlineScope

#endif
