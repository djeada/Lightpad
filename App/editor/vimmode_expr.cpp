#include "vimmode.h"

#include <QTextBlock>
#include <cmath>

namespace {

struct Val {
  enum Type { Num, Str, List } type = Num;
  qint64 n = 0;
  QString s;
  QList<Val> l;

  static Val num(qint64 v) {
    Val r;
    r.type = Num;
    r.n = v;
    return r;
  }
  static Val str(const QString &v) {
    Val r;
    r.type = Str;
    r.s = v;
    return r;
  }
  static Val list(const QList<Val> &v) {
    Val r;
    r.type = List;
    r.l = v;
    return r;
  }
};

qint64 strToNum(const QString &s) {
  int i = 0;
  while (i < s.size() && s[i].isSpace())
    ++i;
  bool neg = false;
  if (i < s.size() && (s[i] == '-' || s[i] == '+')) {
    neg = s[i] == '-';
    ++i;
  }
  qint64 v = 0;
  if (i + 1 < s.size() && s[i] == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X')) {
    i += 2;
    while (i < s.size() && s[i].isLetterOrNumber() &&
           QString("0123456789abcdefABCDEF").contains(s[i]))
      v = v * 16 + QString(s[i++]).toInt(nullptr, 16);
  } else {
    while (i < s.size() && s[i].isDigit())
      v = v * 10 + s[i++].digitValue();
  }
  return neg ? -v : v;
}

QString quoteString(const QString &s) {
  QString q = s;
  q.replace("'", "''");
  return "'" + q + "'";
}

QString toStr(const Val &v);

QString toRepr(const Val &v) {
  if (v.type == Val::Str)
    return quoteString(v.s);
  if (v.type == Val::List) {
    QStringList parts;
    for (const Val &e : v.l)
      parts << toRepr(e);
    return "[" + parts.join(", ") + "]";
  }
  return QString::number(v.n);
}

QString toStr(const Val &v) {
  switch (v.type) {
  case Val::Num:
    return QString::number(v.n);
  case Val::Str:
    return v.s;
  case Val::List:
    return toRepr(v);
  }
  return QString();
}

qint64 toNum(const Val &v) {
  if (v.type == Val::Num)
    return v.n;
  if (v.type == Val::Str)
    return strToNum(v.s);
  return 0;
}

bool truthy(const Val &v) { return toNum(v) != 0; }

} // namespace

class VimExprEvaluator {
public:
  VimExprEvaluator(VimMode *vim, const QString &text,
                   const QRegularExpressionMatch *match)
      : m_vim(vim), m_text(text), m_match(match) {}

  bool evaluate(Val &out) {
    skipSpace();
    out = parseTernary();
    skipSpace();
    if (!m_error.isEmpty())
      return false;
    if (m_pos < m_text.size()) {
      m_error = QString("E15: Invalid expression: \"%1\"").arg(m_text);
      return false;
    }
    return true;
  }
  QString error() const { return m_error; }

  static QString asString(const Val &v) { return toStr(v); }
  static QStringList asLines(const Val &v, bool *isList) {
    if (v.type == Val::List) {
      if (isList)
        *isList = true;
      QStringList lines;
      for (const Val &e : v.l)
        lines << toStr(e);
      return lines;
    }
    if (isList)
      *isList = false;
    return QStringList() << toStr(v);
  }

private:
  VimMode *m_vim;
  QString m_text;
  const QRegularExpressionMatch *m_match;
  int m_pos = 0;
  QString m_error;

  void fail(const QString &msg) {
    if (m_error.isEmpty())
      m_error = msg;
  }
  void skipSpace() {
    while (m_pos < m_text.size() && m_text[m_pos].isSpace())
      ++m_pos;
  }
  bool peekIs(const QString &s) const {
    return m_text.mid(m_pos, s.size()) == s;
  }
  bool accept(const QString &s) {
    skipSpace();
    if (peekIs(s)) {
      m_pos += s.size();
      return true;
    }
    return false;
  }

  Val parseTernary() {
    Val cond = parseOr();
    skipSpace();
    if (m_pos < m_text.size() && m_text[m_pos] == '?') {
      ++m_pos;
      Val a = parseTernary();
      if (!accept(":")) {
        fail("E109: Missing ':' after '?'");
        return a;
      }
      Val b = parseTernary();
      return truthy(cond) ? a : b;
    }
    return cond;
  }
  Val parseOr() {
    Val v = parseAnd();
    while (accept("||")) {
      Val r = parseAnd();
      v = Val::num(truthy(v) || truthy(r));
    }
    return v;
  }
  Val parseAnd() {
    Val v = parseCompare();
    while (accept("&&")) {
      Val r = parseCompare();
      v = Val::num(truthy(v) && truthy(r));
    }
    return v;
  }
  Val parseCompare() {
    Val v = parseAdd();
    while (true) {
      skipSpace();
      static const QStringList ops = {"==", "!=", ">=", "<=", "=~", "!~",
                                      ">",  "<"};
      QString op;
      for (const QString &o : ops) {
        if (peekIs(o)) {
          op = o;
          break;
        }
      }
      if (op.isEmpty())
        break;
      m_pos += op.size();
      bool ignoreCase = false;
      bool matchCase = false;
      if (m_pos < m_text.size() && m_text[m_pos] == '#') {
        matchCase = true;
        ++m_pos;
      } else if (m_pos < m_text.size() && m_text[m_pos] == '?') {
        ignoreCase = true;
        ++m_pos;
      }
      Val r = parseAdd();
      bool result = false;
      if (op == "=~" || op == "!~") {
        QRegularExpression re = m_vim->compilePattern(toStr(r));
        if (ignoreCase)
          re.setPatternOptions(re.patternOptions() |
                               QRegularExpression::CaseInsensitiveOption);
        if (matchCase)
          re.setPatternOptions(re.patternOptions() &
                               ~QRegularExpression::CaseInsensitiveOption);
        result = re.match(toStr(v)).hasMatch();
        if (op == "!~")
          result = !result;
      } else if (v.type == Val::Str && r.type == Val::Str) {
        const int cmp = QString::compare(
            v.s, r.s,
            (ignoreCase || (!matchCase && m_vim->m_ignoreCase))
                ? Qt::CaseInsensitive
                : Qt::CaseSensitive);
        result = op == "==" ? cmp == 0
                 : op == "!=" ? cmp != 0
                 : op == ">"  ? cmp > 0
                 : op == ">=" ? cmp >= 0
                 : op == "<"  ? cmp < 0
                              : cmp <= 0;
      } else {
        const qint64 a = toNum(v), b = toNum(r);
        result = op == "==" ? a == b
                 : op == "!=" ? a != b
                 : op == ">"  ? a > b
                 : op == ">=" ? a >= b
                 : op == "<"  ? a < b
                              : a <= b;
      }
      v = Val::num(result);
    }
    return v;
  }
  Val parseAdd() {
    Val v = parseMul();
    while (true) {
      skipSpace();
      if (peekIs("..")) {
        m_pos += 2;
        v = Val::str(toStr(v) + toStr(parseMul()));
      } else if (peekIs(".") && !(m_pos + 1 < m_text.size() &&
                                  m_text[m_pos + 1].isDigit())) {
        ++m_pos;
        v = Val::str(toStr(v) + toStr(parseMul()));
      } else if (peekIs("+")) {
        ++m_pos;
        Val r = parseMul();
        if (v.type == Val::List && r.type == Val::List)
          v = Val::list(v.l + r.l);
        else
          v = Val::num(toNum(v) + toNum(r));
      } else if (peekIs("-")) {
        ++m_pos;
        v = Val::num(toNum(v) - toNum(parseMul()));
      } else {
        break;
      }
    }
    return v;
  }
  Val parseMul() {
    Val v = parseUnary();
    while (true) {
      skipSpace();
      if (peekIs("*")) {
        ++m_pos;
        v = Val::num(toNum(v) * toNum(parseUnary()));
      } else if (peekIs("/")) {
        ++m_pos;
        const qint64 d = toNum(parseUnary());
        const qint64 a = toNum(v);
        v = Val::num(d == 0 ? (a == 0 ? Q_INT64_C(-9223372036854775807) - 1
                                      : (a > 0 ? Q_INT64_C(9223372036854775807)
                                               : -Q_INT64_C(9223372036854775807)))
                            : a / d);
      } else if (peekIs("%")) {
        ++m_pos;
        const qint64 d = toNum(parseUnary());
        v = Val::num(d == 0 ? 0 : toNum(v) % d);
      } else {
        break;
      }
    }
    return v;
  }
  Val parseUnary() {
    skipSpace();
    if (peekIs("!")) {
      ++m_pos;
      return Val::num(!truthy(parseUnary()));
    }
    if (peekIs("-")) {
      ++m_pos;
      return Val::num(-toNum(parseUnary()));
    }
    if (peekIs("+")) {
      ++m_pos;
      return Val::num(toNum(parseUnary()));
    }
    return parsePostfix();
  }
  Val parsePostfix() {
    Val v = parsePrimary();
    while (m_error.isEmpty()) {
      if (m_pos < m_text.size() && m_text[m_pos] == '[') {
        ++m_pos;
        skipSpace();
        bool hasFirst = !peekIs(":");
        qint64 a = 0;
        if (hasFirst)
          a = toNum(parseTernary());
        skipSpace();
        if (peekIs(":")) {
          ++m_pos;
          skipSpace();
          qint64 b = -1;
          bool hasSecond = !peekIs("]");
          if (hasSecond)
            b = toNum(parseTernary());
          skipSpace();
          if (!accept("]")) {
            fail("E111: Missing ']'");
            return v;
          }
          if (!hasFirst)
            a = 0;
          if (v.type == Val::List) {
            const int size = v.l.size();
            if (a < 0)
              a += size;
            if (hasSecond && b < 0)
              b += size;
            if (!hasSecond)
              b = size - 1;
            QList<Val> out;
            for (qint64 k = qMax<qint64>(0, a); k <= b && k < size; ++k)
              out << v.l[k];
            v = Val::list(out);
          } else {
            const QString s = toStr(v);
            const int size = s.size();
            if (a < 0)
              a = qMax<qint64>(0, a + size);
            if (hasSecond && b < 0)
              b += size;
            if (!hasSecond)
              b = size - 1;
            v = Val::str(b >= a ? s.mid(int(a), int(b - a + 1)) : QString());
          }
        } else {
          if (!accept("]")) {
            fail("E111: Missing ']'");
            return v;
          }
          if (v.type == Val::List) {
            if (a < 0)
              a += v.l.size();
            if (a < 0 || a >= v.l.size()) {
              fail("E684: List index out of range");
              return v;
            }
            v = v.l[int(a)];
          } else {
            const QString s = toStr(v);
            if (a < 0)
              a += s.size();
            v = Val::str(a >= 0 && a < s.size() ? QString(s[int(a)])
                                                : QString());
          }
        }
      } else {
        break;
      }
    }
    return v;
  }

  QString parseDoubleQuoted() {
    QString out;
    ++m_pos;
    while (m_pos < m_text.size() && m_text[m_pos] != '"') {
      QChar c = m_text[m_pos++];
      if (c != '\\' || m_pos >= m_text.size()) {
        out += c;
        continue;
      }
      QChar d = m_text[m_pos++];
      switch (d.unicode()) {
      case 'n':
        out += '\n';
        break;
      case 'r':
        out += '\r';
        break;
      case 't':
        out += '\t';
        break;
      case 'e':
        out += QChar(0x1b);
        break;
      case 'b':
        out += QChar(0x08);
        break;
      case 'f':
        out += QChar(0x0c);
        break;
      case 'x':
      case 'X':
      case 'u':
      case 'U': {
        const int maxDigits = (d == 'x' || d == 'X') ? 2 : (d == 'u' ? 4 : 8);
        int digits = 0;
        uint value = 0;
        while (m_pos < m_text.size() && digits < maxDigits &&
               QString("0123456789abcdefABCDEF").contains(m_text[m_pos])) {
          value = value * 16 + QString(m_text[m_pos]).toUInt(nullptr, 16);
          ++m_pos;
          ++digits;
        }
        if (digits == 0)
          out += d;
        else
          out += QString::fromUcs4(reinterpret_cast<const char32_t *>(&value),
                                   1);
        break;
      }
      case '<': {
        int close = m_text.indexOf('>', m_pos);
        if (close > 0) {
          const QStringList toks =
              VimMode::parseKeyNotation("<" + m_text.mid(m_pos, close - m_pos) +
                                        ">");
          if (toks.size() == 1) {
            const QString t = toks[0];
            QString raw;
            if (t == "<Esc>")
              raw = QString(QChar(0x1b));
            else if (t == "<CR>")
              raw = "\r";
            else if (t == "<Tab>")
              raw = "\t";
            else if (t == "<BS>")
              raw = QString(QChar(0x08));
            else if (t.size() == 5 && t.startsWith("<C-"))
              raw = QString(QChar(t[3].unicode() - 'a' + 1));
            else
              raw = t;
            out += raw;
            m_pos = close + 1;
            break;
          }
        }
        out += '<';
        break;
      }
      default:
        out += d;
        break;
      }
    }
    if (m_pos >= m_text.size())
      fail("E114: Missing quote");
    else
      ++m_pos;
    return out;
  }

  QString parseSingleQuoted() {
    QString out;
    ++m_pos;
    while (m_pos < m_text.size()) {
      if (m_text[m_pos] == '\'') {
        if (m_pos + 1 < m_text.size() && m_text[m_pos + 1] == '\'') {
          out += '\'';
          m_pos += 2;
          continue;
        }
        ++m_pos;
        return out;
      }
      out += m_text[m_pos++];
    }
    fail("E115: Missing quote");
    return out;
  }

  Val parsePrimary() {
    skipSpace();
    if (m_pos >= m_text.size()) {
      fail("E15: Invalid expression");
      return Val();
    }
    const QChar c = m_text[m_pos];
    if (c.isDigit()) {
      int start = m_pos;
      if (c == '0' && m_pos + 1 < m_text.size() &&
          (m_text[m_pos + 1] == 'x' || m_text[m_pos + 1] == 'X')) {
        m_pos += 2;
        while (m_pos < m_text.size() &&
               QString("0123456789abcdefABCDEF").contains(m_text[m_pos]))
          ++m_pos;
        return Val::num(m_text.mid(start + 2, m_pos - start - 2)
                            .toLongLong(nullptr, 16));
      }
      while (m_pos < m_text.size() && m_text[m_pos].isDigit())
        ++m_pos;
      return Val::num(m_text.mid(start, m_pos - start).toLongLong());
    }
    if (c == '"')
      return Val::str(parseDoubleQuoted());
    if (c == '\'')
      return Val::str(parseSingleQuoted());
    if (c == '(') {
      ++m_pos;
      Val v = parseTernary();
      if (!accept(")"))
        fail("E110: Missing ')'");
      return v;
    }
    if (c == '[') {
      ++m_pos;
      QList<Val> items;
      skipSpace();
      if (!peekIs("]")) {
        while (true) {
          items << parseTernary();
          skipSpace();
          if (peekIs(",")) {
            ++m_pos;
            skipSpace();
            if (peekIs("]"))
              break;
            continue;
          }
          break;
        }
      }
      if (!accept("]"))
        fail("E697: Missing end of List ']'");
      return Val::list(items);
    }
    if (c == '@' && m_pos + 1 < m_text.size()) {
      const QChar reg = m_text[m_pos + 1];
      m_pos += 2;
      return Val::str(m_vim->getRegister(reg).content);
    }
    if (c == '&') {
      ++m_pos;
      int start = m_pos;
      while (m_pos < m_text.size() && m_text[m_pos].isLetter())
        ++m_pos;
      const QString name = m_text.mid(start, m_pos - start);
      if (name == "tw" || name == "textwidth")
        return Val::num(m_vim->m_textWidth);
      if (name == "ts" || name == "tabstop")
        return Val::num(m_vim->m_tabStop);
      if (name == "sw" || name == "shiftwidth")
        return Val::num(m_vim->m_shiftWidth);
      if (name == "et" || name == "expandtab")
        return Val::num(m_vim->m_expandTab);
      if (name == "ic" || name == "ignorecase")
        return Val::num(m_vim->m_ignoreCase);
      fail(QString("E113: Unknown option: %1").arg(name));
      return Val();
    }
    if (c.isLetter() || c == '_') {
      int start = m_pos;
      while (m_pos < m_text.size() &&
             (m_text[m_pos].isLetterOrNumber() || m_text[m_pos] == '_' ||
              m_text[m_pos] == ':'))
        ++m_pos;
      const QString name = m_text.mid(start, m_pos - start);
      skipSpaceNoAdvanceCheck();
      if (m_pos < m_text.size() && m_text[m_pos] == '(') {
        ++m_pos;
        QList<Val> args;
        skipSpace();
        if (!peekIs(")")) {
          while (true) {
            args << parseTernary();
            skipSpace();
            if (peekIs(",")) {
              ++m_pos;
              continue;
            }
            break;
          }
        }
        if (!accept(")")) {
          fail("E116: Invalid arguments for function " + name);
          return Val();
        }
        return callFunction(name, args);
      }
      if (name == "v:true")
        return Val::num(1);
      if (name == "v:false")
        return Val::num(0);
      QString key = name;
      if (key.startsWith("g:"))
        key = key.mid(2);
      if (m_vim->m_variables.contains(key)) {
        const QString stored = m_vim->m_variables.value(key);
        VimExprEvaluator inner(m_vim, stored, nullptr);
        Val out;
        if (inner.evaluate(out))
          return out;
      }
      fail(QString("E121: Undefined variable: %1").arg(name));
      return Val();
    }
    fail(QString("E15: Invalid expression: \"%1\"").arg(m_text));
    return Val();
  }
  void skipSpaceNoAdvanceCheck() {}

  Val callFunction(const QString &name, const QList<Val> &a) {
    auto arg = [&](int i) -> Val { return i < a.size() ? a[i] : Val(); };
    auto S = [&](int i) { return toStr(arg(i)); };
    auto N = [&](int i) { return toNum(arg(i)); };
    if (name == "submatch") {
      const int idx = int(N(0));
      return Val::str(m_match && idx >= 0 && idx <= 9 ? m_match->captured(idx)
                                                      : QString());
    }
    if (name == "line") {
      const QString w = S(0);
      if (w == ".")
        return Val::num((m_vim->m_exprLine >= 0
                             ? m_vim->m_exprLine
                             : m_vim->lineOf(m_vim->cursorPos())) +
                        1);
      if (w == "$")
        return Val::num(m_vim->lineCount());
      if (w.size() == 2 && w[0] == '\'') {
        int pos = 0;
        if (m_vim->markPosition(w[1], pos))
          return Val::num(m_vim->lineOf(pos) + 1);
      }
      return Val::num(0);
    }
    if (name == "col") {
      const QString w = S(0);
      const int pos = m_vim->m_exprLine >= 0
                          ? m_vim->lineStart(m_vim->m_exprLine) +
                                qMax(0, m_vim->m_exprCol)
                          : m_vim->cursorPos();
      const int line = m_vim->lineOf(pos);
      if (w == ".")
        return Val::num(m_vim->lineText(line).left(pos - m_vim->lineStart(line))
                            .toUtf8()
                            .size() +
                        1);
      if (w == "$")
        return Val::num(m_vim->lineText(line).toUtf8().size() + 1);
      return Val::num(0);
    }
    if (name == "getline") {
      const QString w = S(0);
      int l = w == "." ? m_vim->lineOf(m_vim->cursorPos())
              : w == "$" ? m_vim->lineCount() - 1
                         : int(toNum(arg(0))) - 1;
      if (l < 0 || l >= m_vim->lineCount())
        return Val::str(QString());
      return Val::str(m_vim->lineText(l));
    }
    if (name == "strlen")
      return Val::num(S(0).toUtf8().size());
    if (name == "strchars" || name == "strcharlen")
      return Val::num(S(0).size());
    if (name == "strwidth" || name == "strdisplaywidth")
      return Val::num(S(0).size());
    if (name == "len") {
      if (arg(0).type == Val::List)
        return Val::num(arg(0).l.size());
      return Val::num(S(0).toUtf8().size());
    }
    if (name == "empty") {
      const Val v = arg(0);
      if (v.type == Val::List)
        return Val::num(v.l.isEmpty());
      if (v.type == Val::Num)
        return Val::num(v.n == 0);
      return Val::num(v.s.isEmpty());
    }
    if (name == "toupper")
      return Val::str(S(0).toUpper());
    if (name == "tolower")
      return Val::str(S(0).toLower());
    if (name == "repeat") {
      const qint64 count = N(1);
      if (arg(0).type == Val::List) {
        QList<Val> out;
        for (qint64 k = 0; k < count && out.size() < 1000000; ++k)
          out += arg(0).l;
        return Val::list(out);
      }
      if (count <= 0)
        return Val::str(QString());
      if (qint64(S(0).size()) * count > 10000000) {
        fail("E1: Result too large");
        return Val();
      }
      return Val::str(S(0).repeated(int(count)));
    }
    if (name == "abs")
      return Val::num(std::llabs(N(0)));
    if (name == "max" || name == "min") {
      qint64 best = 0;
      bool first = true;
      for (const Val &e : arg(0).l) {
        const qint64 v = toNum(e);
        if (first || (name == "max" ? v > best : v < best))
          best = v;
        first = false;
      }
      return Val::num(best);
    }
    if (name == "str2nr")
      return Val::num(strToNum(S(0)));
    if (name == "string")
      return Val::str(toRepr(arg(0)));
    if (name == "join") {
      QStringList parts;
      for (const Val &e : arg(0).l)
        parts << toStr(e);
      return Val::str(parts.join(a.size() > 1 ? S(1) : QString(" ")));
    }
    if (name == "split") {
      const QString text = S(0);
      QStringList parts;
      if (a.size() < 2 || S(1).isEmpty()) {
        parts = text.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
      } else {
        const QRegularExpression re = m_vim->compilePattern(S(1));
        parts = text.split(re);
        if (a.size() < 3 || !truthy(arg(2)))
          parts.removeAll(QString());
      }
      QList<Val> out;
      for (const QString &p : parts)
        out << Val::str(p);
      return Val::list(out);
    }
    if (name == "range") {
      QList<Val> out;
      qint64 start = 0, end = 0, stride = 1;
      if (a.size() == 1) {
        end = N(0) - 1;
      } else {
        start = N(0);
        end = N(1);
        if (a.size() > 2)
          stride = N(2);
      }
      if (stride == 0)
        stride = 1;
      for (qint64 k = start; stride > 0 ? k <= end : k >= end; k += stride) {
        out << Val::num(k);
        if (out.size() > 1000000)
          break;
      }
      return Val::list(out);
    }
    if (name == "reverse") {
      QList<Val> out = arg(0).l;
      std::reverse(out.begin(), out.end());
      return Val::list(out);
    }
    if (name == "substitute") {
      QRegularExpression re = m_vim->compilePattern(S(1));
      const QString flags = S(3);
      const bool global = flags.contains('g');
      const QString text = S(0);
      QString result;
      int last = 0;
      auto it = re.globalMatch(text);
      while (it.hasNext()) {
        auto m = it.next();
        result += text.mid(last, m.capturedStart() - last);
        result += m_vim->expandReplacement(S(2), m);
        last = m.capturedEnd();
        if (!global)
          break;
      }
      result += text.mid(last);
      return Val::str(result);
    }
    if (name == "matchstr" || name == "match" || name == "matchend") {
      const QRegularExpression re = m_vim->compilePattern(S(1));
      const auto m = re.match(S(0));
      if (name == "matchstr")
        return Val::str(m.hasMatch() ? m.captured(0) : QString());
      if (!m.hasMatch())
        return Val::num(-1);
      return Val::num(name == "match" ? m.capturedStart() : m.capturedEnd());
    }
    if (name == "strpart" || name == "strcharpart") {
      const QString s = S(0);
      const int start = int(N(1));
      return Val::str(a.size() > 2 ? s.mid(start, int(N(2))) : s.mid(start));
    }
    if (name == "stridx" || name == "strridx") {
      return Val::num(name == "stridx" ? S(0).indexOf(S(1), int(N(2)))
                                       : S(0).lastIndexOf(S(1)));
    }
    if (name == "trim")
      return Val::str(S(0).trimmed());
    if (name == "escape") {
      QString out;
      const QString chars = S(1);
      for (QChar ch : S(0)) {
        if (chars.contains(ch) || ch == '\\')
          out += '\\';
        out += ch;
      }
      return Val::str(out);
    }
    if (name == "tr") {
      QString out = S(0);
      const QString from = S(1), to = S(2);
      for (QChar &ch : out) {
        const int idx = from.indexOf(ch);
        if (idx >= 0 && idx < to.size())
          ch = to[idx];
      }
      return Val::str(out);
    }
    if (name == "char2nr")
      return Val::num(S(0).isEmpty() ? 0 : S(0).toUcs4()[0]);
    if (name == "nr2char") {
      const uint code = uint(N(0));
      return Val::str(
          QString::fromUcs4(reinterpret_cast<const char32_t *>(&code), 1));
    }
    if (name == "getreg") {
      const QString r = a.isEmpty() ? QString("\"") : S(0);
      return Val::str(m_vim->getRegister(r.isEmpty() ? QChar('"') : r[0])
                          .content);
    }
    if (name == "printf")
      return Val::str(formatPrintf(a));
    if (name == "indent") {
      const int l = int(N(0)) - 1;
      return Val::num(l >= 0 && l < m_vim->lineCount()
                          ? m_vim->indentWidth(m_vim->lineText(l))
                          : -1);
    }
    if (name == "virtcol")
      return Val::num(m_vim->vcolOf(m_vim->lineText(m_vim->lineOf(
                                        m_vim->cursorPos())),
                                    m_vim->colOf(m_vim->cursorPos())) +
                      1);
    if (name == "type")
      return Val::num(arg(0).type == Val::Num ? 0
                      : arg(0).type == Val::Str ? 1
                                                : 3);
    fail(QString("E117: Unknown function: %1").arg(name));
    return Val();
  }

  QString formatPrintf(const QList<Val> &a) {
    if (a.isEmpty())
      return QString();
    const QString fmt = toStr(a[0]);
    QString out;
    int argIndex = 1;
    for (int i = 0; i < fmt.size(); ++i) {
      if (fmt[i] != '%') {
        out += fmt[i];
        continue;
      }
      ++i;
      if (i >= fmt.size())
        break;
      if (fmt[i] == '%') {
        out += '%';
        continue;
      }
      bool left = false, zero = false, plus = false, space = false, alt = false;
      while (i < fmt.size() && QString("-0+ #").contains(fmt[i])) {
        if (fmt[i] == '-')
          left = true;
        else if (fmt[i] == '0')
          zero = true;
        else if (fmt[i] == '+')
          plus = true;
        else if (fmt[i] == ' ')
          space = true;
        else
          alt = true;
        ++i;
      }
      int width = 0;
      while (i < fmt.size() && fmt[i].isDigit())
        width = width * 10 + fmt[i++].digitValue();
      int precision = -1;
      if (i < fmt.size() && fmt[i] == '.') {
        ++i;
        precision = 0;
        while (i < fmt.size() && fmt[i].isDigit())
          precision = precision * 10 + fmt[i++].digitValue();
      }
      if (i >= fmt.size())
        break;
      const QChar conv = fmt[i];
      const Val v = argIndex < a.size() ? a[argIndex++] : Val();
      QString piece;
      bool numeric = false;
      if (conv == 'd' || conv == 'i') {
        const qint64 n = toNum(v);
        piece = QString::number(std::llabs(n));
        if (n < 0)
          piece.prepend('-');
        else if (plus)
          piece.prepend('+');
        else if (space)
          piece.prepend(' ');
        numeric = true;
      } else if (conv == 'x' || conv == 'X' || conv == 'o' || conv == 'b' ||
                 conv == 'B') {
        const int base = conv == 'o' ? 8 : (conv == 'b' || conv == 'B') ? 2 : 16;
        piece = QString::number(quint64(toNum(v)), base);
        if (conv == 'X')
          piece = piece.toUpper();
        if (alt && toNum(v) != 0)
          piece.prepend(conv == 'o' ? "0" : QString("0") + conv);
        numeric = true;
      } else if (conv == 'c') {
        const uint code = uint(toNum(v));
        piece = QString::fromUcs4(reinterpret_cast<const char32_t *>(&code), 1);
      } else if (conv == 's') {
        piece = toStr(v);
        if (precision >= 0)
          piece = piece.left(precision);
      } else {
        piece = toStr(v);
      }
      if (piece.size() < width) {
        const int pad = width - piece.size();
        if (left) {
          piece += QString(pad, ' ');
        } else if (zero && numeric) {
          const bool sign = !piece.isEmpty() &&
                            (piece[0] == '-' || piece[0] == '+' ||
                             piece[0] == ' ');
          piece.insert(sign ? 1 : 0, QString(pad, '0'));
        } else {
          piece.prepend(QString(pad, ' '));
        }
      }
      out += piece;
    }
    return out;
  }
};

bool VimMode::evalExpression(const QString &expr, QString *result,
                             bool *isList, const QRegularExpressionMatch *match,
                             QString *error) {
  VimExprEvaluator ev(this, expr, match);
  Val v;
  const bool ok = ev.evaluate(v);
  if (!ok) {
    if (error)
      *error = ev.error();
    return false;
  }
  bool list = false;
  const QStringList lines = VimExprEvaluator::asLines(v, &list);
  if (isList)
    *isList = list;
  if (result)
    *result = list ? lines.join('\n') : lines.value(0);
  return true;
}

bool VimMode::evalExpressionRepr(const QString &expr, QString *result,
                                 QString *error, bool rawString) {
  VimExprEvaluator ev(this, expr, nullptr);
  Val v;
  if (!ev.evaluate(v)) {
    if (error)
      *error = ev.error();
    return false;
  }
  if (result)
    *result = (v.type == Val::Str && rawString) ? v.s : toRepr(v);
  return true;
}
