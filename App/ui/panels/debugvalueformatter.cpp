#include "debugvalueformatter.h"

#include <QHash>
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

QString DebugValueFormatter::scalar(const QString &value, const QString &type,
                                    DisplayFormat display) {
  if (type.contains("char") && pointerDepth(type) == 1) {
    QByteArray bytes;
    if (stringBytes(value, &bytes))
      return value.mid(value.indexOf('"')).trimmed();
  }
  if (display == DisplayFormat::Natural || type.contains('*') ||
      type.contains('[') || type.contains("float") || type.contains("double"))
    return value;

  static const QRegularExpression integer(
      QStringLiteral("^([+-]?)(0[xX][0-9a-fA-F]+|0[bB][01]+|[0-9]+)"
                     "(?:\\s+.*)?$"));
  auto match = integer.match(value.trimmed());
  bool negative = false;
  quint64 number = 0;
  bool ok = false;
  if (match.hasMatch()) {
    negative = match.captured(1) == QLatin1String("-");
    QString digits = match.captured(2);
    int base = 10;
    if (digits.startsWith("0x", Qt::CaseInsensitive)) {
      base = 16;
      digits.remove(0, 2);
    } else if (digits.startsWith("0b", Qt::CaseInsensitive)) {
      base = 2;
      digits.remove(0, 2);
    }
    number = digits.toULongLong(&ok, base);
  } else if (type.contains("char")) {
    QString text = value.trimmed();
    if (text.startsWith('L') || text.startsWith('u') || text.startsWith('U'))
      text.remove(0, 1);
    if (text.size() >= 3 && text.front() == '\'' && text.back() == '\'') {
      text = text.mid(1, text.size() - 2);
      if (text.size() == 1) {
        number = text.front().unicode();
        ok = true;
      } else {
        static const QHash<QString, uint> escapes = {
            {"\\0", 0},   {"\\a", 7},  {"\\b", 8},  {"\\t", 9},
            {"\\n", 10},  {"\\v", 11}, {"\\f", 12}, {"\\r", 13},
            {"\\\\", 92}, {"\\'", 39}, {"\\\"", 34}};
        if (escapes.contains(text)) {
          number = escapes.value(text);
          ok = true;
        } else if (text.startsWith("\\x")) {
          number = text.mid(2).toULongLong(&ok, 16);
        } else if (text.startsWith('\\')) {
          number = text.mid(1).toULongLong(&ok, 8);
        }
      }
    }
  }
  if (!ok)
    return value;

  const QString sign = negative && number ? QStringLiteral("-") : QString();
  switch (display) {
  case DisplayFormat::Decimal:
    return sign + QString::number(number);
  case DisplayFormat::Hexadecimal:
    return sign + "0x" + QString::number(number, 16);
  case DisplayFormat::Binary:
    return sign + "0b" + QString::number(number, 2);
  case DisplayFormat::Character: {
    if (negative) {
      if (!type.contains("char") || number > 128)
        return value;
      number = 256 - number;
    }
    static const QHash<quint64, QString> escapes = {
        {0, "\\0"},  {7, "\\a"},  {8, "\\b"},  {9, "\\t"},  {10, "\\n"},
        {11, "\\v"}, {12, "\\f"}, {13, "\\r"}, {39, "\\'"}, {92, "\\\\"}};
    if (escapes.contains(number))
      return "'" + escapes.value(number) + "'";
    if (number >= 32 && number < 127)
      return "'" + QString(QChar(static_cast<ushort>(number))) + "'";
    if (number <= 255)
      return QStringLiteral("'\\x%1'").arg(number, 2, 16, QLatin1Char('0'));
    if (number <= 0x10ffff)
      return QStringLiteral("'\\%1%2'")
          .arg(number <= 0xffff ? "u" : "U")
          .arg(number, number <= 0xffff ? 4 : 8, 16, QLatin1Char('0'));
    return value;
  }
  default:
    return value;
  }
}

QString DebugValueFormatter::childExpression(const QString &parent,
                                             const QString &parentType,
                                             const QString &name) {
  if (parent.isEmpty())
    return {};
  static const QRegularExpression index(QStringLiteral("^\\[?([0-9]+)\\]?$"));
  const auto match = index.match(name.trimmed());
  if (match.hasMatch())
    return QStringLiteral("(%1)[%2]").arg(parent, match.captured(1));
  if (name == QLatin1String("value") &&
      (parentType.contains('*') || parentType.contains('&')))
    return QStringLiteral("(*(%1))").arg(parent);
  static const QRegularExpression member(
      QStringLiteral("^[A-Za-z_][A-Za-z_0-9]*$"));
  if (member.match(name).hasMatch())
    return QStringLiteral("(%1)%2%3")
        .arg(parent, parentType.contains('*') ? "->" : ".", name);
  return {};
}

int DebugValueFormatter::arrayLength(const QString &type) {
  static const QRegularExpression dimension(
      QStringLiteral("\\[\\s*([0-9]+)\\s*\\]"));
  const auto match = dimension.match(type);
  if (!match.hasMatch())
    return 0;
  bool ok = false;
  const int count = match.captured(1).toInt(&ok);
  return ok && count > 0 ? count : 0;
}

QString DebugValueFormatter::pointeeType(const QString &type) {
  static const QRegularExpression pointer(QStringLiteral(
      "\\*\\s*(?:(?:const|volatile|restrict|__restrict)\\s*)*$"));
  const auto match = pointer.match(type.trimmed());
  return match.hasMatch() ? type.trimmed().left(match.capturedStart()).trimmed()
                          : QString();
}

int DebugValueFormatter::pointerDepth(QString type) {
  int depth = 0;
  for (QString pointee = pointeeType(type); !pointee.isEmpty();
       pointee = pointeeType(pointee))
    ++depth;
  return depth;
}

bool DebugValueFormatter::stringBytes(const QString &value, QByteArray *bytes) {
  const int opening = value.indexOf('"');
  if (opening < 0)
    return false;
  QByteArray result;
  const QString text = value.mid(opening).trimmed();
  static const QHash<QChar, char> escapes = {
      {'a', '\a'}, {'b', '\b'}, {'f', '\f'},  {'n', '\n'}, {'r', '\r'},
      {'t', '\t'}, {'v', '\v'}, {'\\', '\\'}, {'"', '"'},  {'\'', '\''}};
  for (int i = 1; i < text.size(); ++i) {
    if (result.size() > 65536)
      return false;
    QChar c = text.at(i);
    if (c == '"') {
      if (i != text.size() - 1)
        return false;
      *bytes = result;
      return true;
    }
    if (c != '\\') {
      QString character(c);
      if (c.isHighSurrogate() && i + 1 < text.size() &&
          text.at(i + 1).isLowSurrogate())
        character += text.at(++i);
      result += character.toUtf8();
      continue;
    }
    if (++i >= text.size())
      return false;
    c = text.at(i);
    if (escapes.contains(c)) {
      result += escapes.value(c);
    } else if (c >= '0' && c <= '7') {
      QString digits(c);
      for (int n = 1; n < 3 && i + 1 < text.size() && text.at(i + 1) >= '0' &&
                      text.at(i + 1) <= '7';
           ++n)
        digits += text.at(++i);
      result += static_cast<char>(digits.toUInt(nullptr, 8));
    } else if (c == 'x') {
      QString digits;
      while (
          i + 1 < text.size() &&
          QStringLiteral("0123456789abcdef").contains(text.at(i + 1).toLower()))
        digits += text.at(++i);
      bool ok = false;
      const uint byte = digits.toUInt(&ok, 16);
      if (!ok || byte > 255)
        return false;
      result += static_cast<char>(byte);
    } else {
      return false;
    }
    if (result.size() > 65536)
      return false;
  }
  return false;
}
