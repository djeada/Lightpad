#include "pythonsyntaxplugin.h"
#include <QRegularExpression>

namespace {

QString wordPattern(const QString &word) {
  return "\\b" + QRegularExpression::escape(word) + "\\b";
}

void appendWordRules(QVector<SyntaxRule> &rules, const QStringList &words,
                     const QString &name) {
  for (const QString &word : words) {
    SyntaxRule rule;
    rule.pattern = QRegularExpression(wordPattern(word));
    rule.name = name;
    rules.append(rule);
  }
}

void appendPatternRule(QVector<SyntaxRule> &rules, const QString &pattern,
                       const QString &name, int captureGroup = 0) {
  SyntaxRule rule;
  rule.pattern = QRegularExpression(pattern);
  rule.name = name;
  rule.captureGroup = captureGroup;
  rules.append(rule);
}

} // namespace

QStringList PythonSyntaxPlugin::getPrimaryKeywords() {
  return {"and",      "as",     "assert",   "async", "await",  "break",
          "case",     "class",  "continue", "def",   "del",    "elif",
          "else",     "except", "finally",  "for",   "from",   "global",
          "if",       "import", "in",       "is",    "lambda", "match",
          "nonlocal", "not",    "or",       "pass",  "raise",  "return",
          "try",      "while",  "with",     "yield"};
}

QStringList PythonSyntaxPlugin::getBuiltinConstants() {
  return {"Ellipsis", "False", "None", "NotImplemented", "True", "__debug__"};
}

QStringList PythonSyntaxPlugin::getBuiltinFunctions() {
  return {"abs",        "all",          "any",        "ascii",       "bin",
          "breakpoint", "callable",     "chr",        "classmethod", "dir",
          "enumerate",  "filter",       "format",     "getattr",     "hasattr",
          "hex",        "input",        "isinstance", "issubclass",  "iter",
          "len",        "map",          "max",        "min",         "next",
          "open",       "ord",          "pow",        "print",       "property",
          "range",      "repr",         "reversed",   "round",       "setattr",
          "sorted",     "staticmethod", "sum",        "super",       "vars",
          "zip"};
}

QStringList PythonSyntaxPlugin::getBuiltinTypes() {
  return {"bool",  "bytearray", "bytes", "complex", "dict",
          "float", "frozenset", "int",   "list",    "object",
          "set",   "str",       "tuple", "type"};
}

QStringList PythonSyntaxPlugin::getContextKeywords() { return {"cls", "self"}; }

QVector<SyntaxRule> PythonSyntaxPlugin::syntaxRules() const {
  QVector<SyntaxRule> rules;

  appendWordRules(rules, getPrimaryKeywords(), "keyword_0");
  appendWordRules(rules, getBuiltinConstants(), "builtin_constant");
  appendWordRules(rules, getBuiltinTypes(), "builtin_type");
  appendWordRules(rules, getContextKeywords(), "keyword_2");

  appendPatternRule(
      rules,
      "\\b(?:0[xX][0-9a-fA-F_]+|0[oO][0-7_]+|"
      "0[bB][01_]+|\\d[\\d_]*(?:\\.\\d[\\d_]*)?(?:[eE][+-]?\\d[\\d_]*)?"
      "[jJ]?)\\b",
      "number");

  appendPatternRule(
      rules, "(?<![A-Za-z0-9_])(?:[fF][rR]?|[rR][fF])\"(?:\\\\.|[^\"\\\\])*\"",
      "interpolated_string");
  appendPatternRule(
      rules, "(?<![A-Za-z0-9_])(?:[fF][rR]?|[rR][fF])'(?:\\\\.|[^'\\\\])*'",
      "interpolated_string");

  appendPatternRule(rules,
                    "\\\\(?:[abfnrtv\\\\'\"]|[0-7]{1,3}|x[0-9a-fA-F]{2}|"
                    "u[0-9a-fA-F]{4}|U[0-9a-fA-F]{8}|N\\{[^}]+\\})",
                    "escape");

  appendPatternRule(
      rules, "(?<![A-Za-z0-9_])(?:[rRuUbB]{0,2})\"(?:\\\\.|[^\"\\\\])*\"",
      "string");
  appendPatternRule(rules,
                    "(?<![A-Za-z0-9_])(?:[rRuUbB]{0,2})'(?:\\\\.|[^'\\\\])*'",
                    "string");

  appendPatternRule(rules,
                    "(?:\\*\\*|//|<<|>>|<=|>=|==|!=|:=|[+\\-*/%@&|^~<>=]=?|->)",
                    "operator");
  appendPatternRule(rules, "\\b[A-Za-z_][A-Za-z0-9_]*(?=\\()", "function");
  appendPatternRule(rules, "\\b[A-Z][A-Za-z0-9_]*(?=\\()", "type");
  appendPatternRule(rules, "(?<=\\.)[A-Za-z_][A-Za-z0-9_]*\\b(?!\\s*\\()",
                    "attribute");
  appendWordRules(rules, getBuiltinFunctions(), "builtin_function");
  appendPatternRule(rules, "\\b__[A-Za-z_][A-Za-z0-9_]*__\\b", "magic");
  appendPatternRule(rules, "\\bdef\\s+([A-Za-z_][A-Za-z0-9_]*)\\b",
                    "function_definition", 1);
  appendPatternRule(rules, "\\bclass\\s+([A-Za-z_][A-Za-z0-9_]*)\\b",
                    "class_definition", 1);
  appendPatternRule(rules,
                    "@[A-Za-z_][A-Za-z0-9_]*(?:\\.[A-Za-z_][A-Za-z0-9_]*)*",
                    "decorator");
  appendPatternRule(rules, "#[^\n]*", "comment");

  return rules;
}

QVector<MultiLineBlock> PythonSyntaxPlugin::multiLineBlocks() const {
  QVector<MultiLineBlock> blocks;

  MultiLineBlock singleQuoteBlock;
  singleQuoteBlock.startPattern =
      QRegularExpression("(?<![A-Za-z0-9_])(?:[rRuUbBfF]{0,3})'''");
  singleQuoteBlock.endPattern = QRegularExpression("'''");
  singleQuoteBlock.name = "string";
  blocks.append(singleQuoteBlock);

  MultiLineBlock doubleQuoteBlock;
  doubleQuoteBlock.startPattern =
      QRegularExpression("(?<![A-Za-z0-9_])(?:[rRuUbBfF]{0,3})\"\"\"");
  doubleQuoteBlock.endPattern = QRegularExpression("\"\"\"");
  doubleQuoteBlock.name = "string";
  blocks.append(doubleQuoteBlock);

  return blocks;
}

QStringList PythonSyntaxPlugin::keywords() const {
  QStringList all;
  all << getPrimaryKeywords() << getBuiltinConstants() << getBuiltinFunctions()
      << getBuiltinTypes() << getContextKeywords();
  return all;
}
