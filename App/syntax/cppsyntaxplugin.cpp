#include "cppsyntaxplugin.h"
#include <QRegularExpression>

QStringList CppSyntaxPlugin::getPrimaryKeywords() {
  return {"alignas",   "auto",      "bool",     "char",      "char8_t",
          "char16_t",  "char32_t",  "class",    "const",     "consteval",
          "constexpr", "constinit", "decltype", "double",    "enum",
          "explicit",  "export",    "extern",   "float",     "inline",
          "int",       "long",      "mutable",  "namespace", "register",
          "short",     "signed",    "sizeof",   "static",    "struct",
          "typedef",   "typename",  "union",    "unsigned",  "void",
          "volatile",  "wchar_t"};
}

QStringList CppSyntaxPlugin::getSecondaryKeywords() {
  return {"and",        "and_eq",       "asm",
          "bitand",     "bitor",        "break",
          "case",       "catch",        "compl",
          "const_cast", "continue",     "default",
          "delete",     "do",           "dynamic_cast",
          "else",       "for",          "friend",
          "goto",       "if",           "new",
          "not",        "not_eq",       "operator",
          "or",         "or_eq",        "private",
          "protected",  "public",       "reinterpret_cast",
          "return",     "static_cast",  "switch",
          "template",   "this",         "throw",
          "try",        "using",        "virtual",
          "while",      "xor",          "xor_eq",
          "concept",    "requires",     "co_await",
          "co_return",  "co_yield",     "nullptr",
          "noexcept",   "thread_local", "static_assert",
          "alignof",    "typeid"};
}

QStringList CppSyntaxPlugin::getTertiaryKeywords() {
  return {"true", "false", "NULL", "override", "final"};
}

QVector<SyntaxRule> CppSyntaxPlugin::syntaxRules() const {
  QVector<SyntaxRule> rules;

  for (const QString &keyword : getPrimaryKeywords()) {
    SyntaxRule rule;
    rule.pattern = QRegularExpression("\\b" + keyword + "\\b");
    rule.name = "keyword_0";
    rules.append(rule);
  }

  for (const QString &keyword : getSecondaryKeywords()) {
    SyntaxRule rule;
    rule.pattern = QRegularExpression("\\b" + keyword + "\\b");
    rule.name = "keyword_1";
    rules.append(rule);
  }

  for (const QString &keyword : getTertiaryKeywords()) {
    SyntaxRule rule;
    rule.pattern = QRegularExpression("\\b" + keyword + "\\b");
    rule.name = "keyword_2";
    rules.append(rule);
  }

  const QString preprocessorDirectives =
      QStringLiteral("include|define|undef|if|ifdef|ifndef|elif|else|endif|"
                     "pragma|error|warning|line");
  SyntaxRule preprocessorRule;
  preprocessorRule.pattern = QRegularExpression(
      QStringLiteral("^\\s*#\\s*(%1)\\b").arg(preprocessorDirectives));
  preprocessorRule.name = "preprocessor_directive";
  rules.append(preprocessorRule);

  const QString identifierPattern = QStringLiteral("[A-Za-z_][A-Za-z0-9_]*");
  SyntaxRule scopeQualifierRule;
  scopeQualifierRule.pattern =
      QRegularExpression(QStringLiteral("%1(?=::)").arg(identifierPattern));
  scopeQualifierRule.name = "scope_qualifier";
  rules.append(scopeQualifierRule);

  SyntaxRule scopedIdentifierRule;
  scopedIdentifierRule.pattern =
      QRegularExpression(QStringLiteral("(?<=::)%1").arg(identifierPattern));
  scopedIdentifierRule.name = "scoped_identifier";
  rules.append(scopedIdentifierRule);

  SyntaxRule numberRule;
  numberRule.pattern = QRegularExpression(
      "\\b(?:0[xX][0-9a-fA-F'][0-9a-fA-F']*|0[bB][01'][01']*|0[0-7']+|"
      "\\d[\\d']*(?:\\.\\d[\\d']*)?(?:[eE][+-]?\\d[\\d']*)?)[uUlLfF]*\\b");
  numberRule.name = "number";
  rules.append(numberRule);

  SyntaxRule qtClassRule;
  qtClassRule.pattern = QRegularExpression("\\bQ[A-Za-z]+\\b");
  qtClassRule.name = "class";
  rules.append(qtClassRule);

  SyntaxRule escapeRule;
  escapeRule.pattern = QRegularExpression(
      "\\\\(?:[abfnrtv\\\\'\"]|[0-7]{1,3}|x[0-9a-fA-F]+|u[0-9a-fA-F]{4}|"
      "U[0-9a-fA-F]{8})");
  escapeRule.name = "escape";
  rules.append(escapeRule);

  SyntaxRule stringRule;
  stringRule.pattern =
      QRegularExpression("\"(?:\\\\.|[^\"\\\\])*\"");
  stringRule.name = "string";
  rules.append(stringRule);

  SyntaxRule charLiteralRule;
  charLiteralRule.pattern = QRegularExpression("'(?:\\\\.|[^'\\\\])*'");
  charLiteralRule.name = "string";
  rules.append(charLiteralRule);

  SyntaxRule operatorRule;
  operatorRule.pattern = QRegularExpression(
      "(?:<<|>>|<=>|->|\\+\\+|--|&&|\\|\\||[+\\-*/%&|^~!=<>]=?|::|\\?)");
  operatorRule.name = "operator";
  rules.append(operatorRule);

  SyntaxRule functionRule;
  functionRule.pattern = QRegularExpression("\\b[A-Za-z_][A-Za-z0-9_]*(?=\\()");
  functionRule.name = "function";
  rules.append(functionRule);

  SyntaxRule commentRule;
  commentRule.pattern = QRegularExpression("//[^\n]*");
  commentRule.name = "comment";
  rules.append(commentRule);

  return rules;
}

QVector<MultiLineBlock> CppSyntaxPlugin::multiLineBlocks() const {
  QVector<MultiLineBlock> blocks;

  MultiLineBlock commentBlock;
  commentBlock.startPattern = QRegularExpression("/\\*");
  commentBlock.endPattern = QRegularExpression("\\*/");
  blocks.append(commentBlock);

  return blocks;
}

QStringList CppSyntaxPlugin::keywords() const {
  QStringList all;
  all << getPrimaryKeywords() << getSecondaryKeywords()
      << getTertiaryKeywords();
  return all;
}
