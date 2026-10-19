#include "debugvalueformatter.h"

#include <QRegularExpression>
#include <QStringList>

namespace {
QString indent(int depth) { return QString(depth * 4, QLatin1Char(' ')); }

QString formatPart(const QString &input, int depth, int width) {
  const QString text = input.trimmed();
  if (depth > 16 || text.isEmpty()) {
    return text;
  }

  int opening = -1;
  QChar quote;
  bool escaped = false;
  for (int i = 0; i < text.size(); ++i) {
    const QChar c = text.at(i);
    if (!quote.isNull()) {
      if (escaped)
        escaped = false;
      else if (c == '\\')
        escaped = true;
      else if (c == quote)
        quote = {};
    } else if (c == '\'' || c == '"') {
      quote = c;
    } else if (c == '[' || c == '{' || c == '(') {
      opening = i;
      break;
    }
  }
  if (opening < 0)
    return text;

  QString stack;
  QStringList parts;
  int start = opening + 1;
  bool nested = false;
  quote = {};
  escaped = false;
  for (int i = opening; i < text.size(); ++i) {
    const QChar c = text.at(i);
    if (!quote.isNull()) {
      if (escaped)
        escaped = false;
      else if (c == '\\')
        escaped = true;
      else if (c == quote)
        quote = {};
      continue;
    }
    if (c == '\'' || c == '"') {
      quote = c;
    } else if (c == '[' || c == '{' || c == '(') {
      if (!stack.isEmpty())
        nested = true;
      stack += c;
    } else if (c == ']' || c == '}' || c == ')') {
      const QChar expected = c == ']' ? '[' : c == '}' ? '{' : '(';
      if (stack.isEmpty() || stack.back() != expected)
        return text;
      stack.chop(1);
      if (stack.isEmpty()) {

        if (i != text.size() - 1)
          return text;
        const QString last = text.mid(start, i - start).trimmed();
        if (!last.isEmpty())
          parts << last;
      }
    } else if (c == ',' && stack.size() == 1) {
      parts << text.mid(start, i - start).trimmed();
      start = i + 1;
    }
  }
  if (!stack.isEmpty() || !quote.isNull() || parts.isEmpty())
    return text;
  if (!nested && text.size() + depth * 4 <= width)
    return text;

  QStringList lines;
  for (const QString &part : parts) {
    lines << indent(depth + 1) + formatPart(part, depth + 1, width);
  }
  const bool tupleComma = text.at(opening) == '(' && parts.size() == 1 &&
                          text.left(text.size() - 1).trimmed().endsWith(',');
  return text.left(opening + 1) + '\n' + lines.join(",\n") +
         (tupleComma ? ",\n" : "\n") + indent(depth) + text.back();
}
} // namespace

QString DebugValueFormatter::format(const QString &value, int width) {
  constexpr int maxChars = 65536;
  if (value.size() > maxChars) {
    return value.left(maxChars) +
           QStringLiteral("\n… [text preview limited to 65,536 characters]");
  }
  return formatPart(value, 0, width);
}

QString DebugValueFormatter::summary(const QString &value, int length) {
  QString text = value;
  text.replace('\n', QStringLiteral("\\n"));
  text.replace('\r', QStringLiteral("\\r"));
  return text.size() > length ? text.left(length) + QChar(0x2026) : text;
}

QString DebugValueFormatter::tableCell(const QString &value) {

  static const QRegularExpression scalar(
      QStringLiteral("^np\\.(?:float|u?int)(?:8|16|32|64|128|256)?\\(([-+0-9."
                     "eE]+|nan|[-+]?inf)\\)$"));
  const auto match = scalar.match(value);
  return match.hasMatch() ? match.captured(1) : value;
}
