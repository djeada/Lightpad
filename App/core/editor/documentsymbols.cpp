#include "documentsymbols.h"

#include <QRegularExpression>
#include <QSet>
#include <QStringList>

namespace DocumentSymbols {

namespace {

const QSet<QString> &controlKeywords() {
  static const QSet<QString> k = {
      "if",       "for",           "while",    "switch",  "return",  "else",
      "catch",    "sizeof",        "do",       "case",    "throw",   "new",
      "delete",   "defined",       "typeof",   "using",   "typedef", "template",
      "operator", "static_assert", "decltype", "alignof", "assert"};
  return k;
}

bool isHashLanguage(const QString &l) {
  return l == "py" || l == "sh" || l == "yaml" || l == "make" || l == "cmake" ||
         l == "dockerfile";
}

QString stripLine(const QString &line, bool slashComments, bool hashComments,
                  bool &inBlockComment) {
  QString out;
  out.reserve(line.size());
  QChar quote;
  for (int i = 0; i < line.size(); ++i) {
    const QChar c = line.at(i);
    if (inBlockComment) {
      if (c == '*' && i + 1 < line.size() && line.at(i + 1) == '/') {
        inBlockComment = false;
        ++i;
      }
      continue;
    }
    if (!quote.isNull()) {
      if (c == '\\') {
        ++i;
      } else if (c == quote) {
        quote = QChar();
      }
      continue;
    }
    if (c == '"' || c == '\'' || (c == '`' && slashComments)) {
      quote = c;
      continue;
    }
    if (slashComments && c == '/' && i + 1 < line.size()) {
      if (line.at(i + 1) == '/') {
        break;
      }
      if (line.at(i + 1) == '*') {
        inBlockComment = true;
        ++i;
        continue;
      }
    }
    if (hashComments && c == '#') {
      break;
    }
    out.append(c);
  }
  return out;
}

void scanPython(const QStringList &lines, QList<Symbol> &out) {
  static const QRegularExpression def(
      R"(^(\s*)(?:async\s+)?def\s+([A-Za-z_]\w*)\s*\()");
  static const QRegularExpression cls(R"(^(\s*)class\s+([A-Za-z_]\w*))");
  QList<int> classIndents;
  bool inTriple = false;
  for (int i = 0; i < lines.size(); ++i) {
    const QString &line = lines[i];
    const int tripleCount = line.count(QStringLiteral("\"\"\"")) +
                            line.count(QStringLiteral("'''"));
    const bool wasInTriple = inTriple;
    if (tripleCount % 2 == 1) {
      inTriple = !inTriple;
    }
    if (wasInTriple) {
      continue;
    }
    if (line.trimmed().isEmpty() || line.trimmed().startsWith('#')) {
      continue;
    }
    int indent = 0;
    while (indent < line.size() && line.at(indent).isSpace()) {
      ++indent;
    }
    while (!classIndents.isEmpty() && classIndents.last() >= indent) {
      classIndents.removeLast();
    }
    auto m = cls.match(line);
    if (m.hasMatch()) {
      out.append({m.captured(2), Kind::Class, i, m.capturedStart(2)});
      classIndents.append(indent);
      continue;
    }
    m = def.match(line);
    if (m.hasMatch()) {
      out.append({m.captured(2),
                  classIndents.isEmpty() ? Kind::Function : Kind::Method, i,
                  m.capturedStart(2)});
    }
  }
}

void scanMarkdown(const QStringList &lines, QList<Symbol> &out) {
  static const QRegularExpression heading(R"(^(#{1,6})\s+(.+?)\s*#*\s*$)");
  bool fenced = false;
  for (int i = 0; i < lines.size(); ++i) {
    if (lines[i].startsWith("```") || lines[i].startsWith("~~~")) {
      fenced = !fenced;
      continue;
    }
    if (fenced) {
      continue;
    }
    const auto m = heading.match(lines[i]);
    if (m.hasMatch()) {
      out.append({m.captured(2), Kind::Heading, i, m.capturedStart(2)});
    }
  }
}

void scanBraceLanguage(const QStringList &lines, const QString &language,
                       QList<Symbol> &out) {
  const bool rustLike = language == "rust";
  const bool goLike = language == "go";
  const bool scriptLike = language == "js" || language == "ts";

  static const QRegularExpression classRe(
      R"(^\s*(?:export\s+|default\s+|abstract\s+|public\s+|private\s+|protected\s+|final\s+|static\s+|pub\s+|pub\(crate\)\s+)*(class|struct|interface|enum|union|namespace|trait|impl|object)\s+(?:class\s+|struct\s+)?([A-Za-z_][\w:<>]*))");
  static const QRegularExpression goType(
      R"(^\s*type\s+([A-Za-z_]\w*)\s+(struct|interface))");
  static const QRegularExpression goFunc(
      R"(^\s*func\s+(?:\([^)]*\)\s*)?([A-Za-z_]\w*)\s*[\[(])");
  static const QRegularExpression rustFn(
      R"(^\s*(?:pub(?:\([^)]*\))?\s+)?(?:async\s+|const\s+|unsafe\s+)*fn\s+([A-Za-z_]\w*))");
  static const QRegularExpression jsFunc(
      R"(^\s*(?:export\s+)?(?:default\s+)?(?:async\s+)?function\s*\*?\s*([A-Za-z_$][\w$]*)\s*[<(])");
  static const QRegularExpression jsArrow(
      R"(^\s*(?:export\s+)?(?:const|let|var)\s+([A-Za-z_$][\w$]*)\s*(?::[^=]+)?=\s*(?:async\s*)?(?:function\b|\([^)]*\)\s*(?::[^=]+)?=>|[A-Za-z_$][\w$]*\s*=>))");
  static const QRegularExpression jsMethod(
      R"(^\s*(?:public\s+|private\s+|protected\s+|static\s+|async\s+|get\s+|set\s+|readonly\s+)*([A-Za-z_$][\w$]*)\s*\([^)]*\)\s*(?::\s*[^{]+)?\{\s*$)");
  static const QRegularExpression cFunc(
      R"(^\s*(?:[\w:<>,*&~\[\]]+\s+)+\**&*\s*(~?[A-Za-z_][\w:]*(?:::~?[A-Za-z_]\w*)*|operator\s*[^\s(]+)\s*\()");
  static const QRegularExpression cCtor(
      R"(^\s*([A-Za-z_]\w*::~?[A-Za-z_]\w*)\s*\()");
  static const QRegularExpression cInClassCtor(
      R"(^\s*(~?[A-Za-z_]\w*)\s*\([^;]*$)");

  enum Scope { Container, Other };
  QList<Scope> stack;
  QList<QString> containerNames;
  bool inBlock = false;

  for (int i = 0; i < lines.size(); ++i) {
    const QString code = stripLine(lines[i], true, false, inBlock);
    const QString trimmed = code.trimmed();
    bool atTop = true;
    for (Scope s : stack) {
      if (s != Container) {
        atTop = false;
      }
    }

    bool openedContainer = false;
    if (!trimmed.isEmpty() && atTop && !trimmed.startsWith('#')) {
      QRegularExpressionMatch m;
      if (goLike && (m = goType.match(code)).hasMatch()) {
        out.append({m.captured(1), Kind::Class, i, m.capturedStart(1)});
      } else if (goLike && (m = goFunc.match(code)).hasMatch()) {
        out.append({m.captured(1), Kind::Function, i, m.capturedStart(1)});
      } else if (rustLike && (m = rustFn.match(code)).hasMatch()) {
        out.append({m.captured(1),
                    stack.isEmpty() ? Kind::Function : Kind::Method, i,
                    m.capturedStart(1)});
      } else if ((m = classRe.match(code)).hasMatch() &&
                 !(trimmed.endsWith(';') && !trimmed.contains('{'))) {
        if (!(goLike || language == "py")) {
          out.append({m.captured(2), Kind::Class, i, m.capturedStart(2)});
          openedContainer = true;
        }
      } else if (scriptLike && (m = jsFunc.match(code)).hasMatch()) {
        out.append({m.captured(1), Kind::Function, i, m.capturedStart(1)});
      } else if (scriptLike && (m = jsArrow.match(code)).hasMatch()) {
        out.append({m.captured(1), Kind::Function, i, m.capturedStart(1)});
      } else if (scriptLike && !stack.isEmpty() &&
                 (m = jsMethod.match(code)).hasMatch() &&
                 !controlKeywords().contains(m.captured(1)) &&
                 m.captured(1) != "constructor_") {
        out.append({m.captured(1), Kind::Method, i, m.capturedStart(1)});
      } else if (!scriptLike && !rustLike && !goLike) {
        const bool endsStatement = trimmed.endsWith(';');
        const int eq = trimmed.indexOf('=');
        const int paren = trimmed.indexOf('(');
        if (!endsStatement && paren > 0 && (eq < 0 || eq > paren) &&
            !trimmed.contains(QStringLiteral("<<"))) {
          if ((m = cFunc.match(code)).hasMatch()) {
            const QString name = m.captured(1);
            const QString first = name.section("::", 0, 0);
            if (!controlKeywords().contains(first) &&
                !controlKeywords().contains(
                    trimmed.section(QRegularExpression("\\s+"), 0, 0))) {
              out.append({name, stack.isEmpty() ? Kind::Function : Kind::Method,
                          i, m.capturedStart(1)});
            }
          } else if ((m = cCtor.match(code)).hasMatch()) {
            out.append({m.captured(1), Kind::Function, i, m.capturedStart(1)});
          } else if (!stack.isEmpty() &&
                     (m = cInClassCtor.match(code)).hasMatch() &&
                     !controlKeywords().contains(m.captured(1))) {
            out.append({m.captured(1), Kind::Method, i, m.capturedStart(1)});
          }
        }
      }
    }

    for (const QChar c : code) {
      if (c == '{') {
        stack.append(openedContainer ? Container : Other);
        openedContainer = false;
      } else if (c == '}' && !stack.isEmpty()) {
        stack.removeLast();
      }
    }
  }
}

} // namespace

QList<Symbol> scan(const QString &text, const QString &language) {
  QList<Symbol> out;
  const QStringList lines = text.split('\n');
  if (language == "py") {
    scanPython(lines, out);
  } else if (language == "md") {
    scanMarkdown(lines, out);
  } else if (isHashLanguage(language) || language == "json" ||
             language == "html" || language == "css" || language == "sql") {

  } else {
    scanBraceLanguage(lines, language, out);
  }
  return out;
}

} // namespace DocumentSymbols
