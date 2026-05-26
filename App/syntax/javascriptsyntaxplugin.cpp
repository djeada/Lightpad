#include "javascriptsyntaxplugin.h"
#include <QRegularExpression>

QStringList JavaScriptSyntaxPlugin::getPrimaryKeywords() {
  return {"var", "let", "const", "function", "class", "interface"};
}

QStringList JavaScriptSyntaxPlugin::getSecondaryKeywords() {
  return {"abstract",     "arguments", "await",      "boolean",   "break",
          "byte",         "case",      "catch",      "char",      "continue",
          "debugger",     "default",   "delete",     "do",        "double",
          "else",         "enum",      "eval",       "export",    "extends",
          "false",        "final",     "finally",    "float",     "for",
          "goto",         "if",        "implements", "import",    "in",
          "instanceof",   "int",       "long",       "native",    "new",
          "null",         "package",   "private",    "protected", "public",
          "return",       "short",     "static",     "super",     "switch",
          "synchronized", "this",      "throw",      "throws",    "transient",
          "true",         "try",       "typeof",     "void",      "volatile",
          "while",        "with",      "yield",      "async"};
}

QStringList JavaScriptSyntaxPlugin::getTertiaryKeywords() {
  return {"Array",    "Date",     "hasOwnProperty", "Infinity",
          "isFinite", "isNaN",    "isPrototypeOf",  "Math",
          "NaN",      "Number",   "Object",         "prototype",
          "String",   "toString", "undefined",      "valueOf"};
}

QVector<SyntaxRule> JavaScriptSyntaxPlugin::syntaxRules() const {
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

  SyntaxRule numberRule;
  numberRule.pattern = QRegularExpression(
      "\\b(?:0[xX][0-9a-fA-F_]+|0[oO][0-7_]+|"
      "0[bB][01_]+|\\d[\\d_]*(?:\\.\\d[\\d_]*)?(?:[eE][+-]?\\d[\\d_]*)?"
      ")n?\\b");
  numberRule.name = "number";
  rules.append(numberRule);

  SyntaxRule regexRule;
  regexRule.pattern =
      QRegularExpression("(?<=[=(:,;!&|?]\\s{0,4})/(?!\\/|\\*)(?:\\\\.|"
                         "[^/\\\\\\n])+/[gimsuy]*");
  regexRule.name = "regex";
  rules.append(regexRule);

  SyntaxRule templateRule;
  templateRule.pattern = QRegularExpression("`(?:\\\\.|[^`\\\\])*`");
  templateRule.name = "interpolated_string";
  rules.append(templateRule);

  SyntaxRule escapeRule;
  escapeRule.pattern = QRegularExpression(
      "\\\\(?:[bfnrtv\\\\'\"`0]|x[0-9a-fA-F]{2}|u[0-9a-fA-F]{4}|"
      "u\\{[0-9a-fA-F]+\\})");
  escapeRule.name = "escape";
  rules.append(escapeRule);

  SyntaxRule stringRule;
  stringRule.pattern = QRegularExpression("\"(?:\\\\.|[^\"\\\\])*\"");
  stringRule.name = "string";
  rules.append(stringRule);

  SyntaxRule singleQuoteRule;
  singleQuoteRule.pattern = QRegularExpression("'(?:\\\\.|[^'\\\\])*'");
  singleQuoteRule.name = "string";
  rules.append(singleQuoteRule);

  SyntaxRule operatorRule;
  operatorRule.pattern = QRegularExpression(
      "(?:===|!==|==|!=|<=|>=|=>|\\+\\+|--|&&|\\|\\||\\?\\.|\\.{3}|"
      "\\?\\?|[+\\-*/%&|^~!=<>]=?|\\?)");
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

QVector<MultiLineBlock> JavaScriptSyntaxPlugin::multiLineBlocks() const {
  QVector<MultiLineBlock> blocks;

  MultiLineBlock commentBlock;
  commentBlock.startPattern = QRegularExpression("/\\*");
  commentBlock.endPattern = QRegularExpression("\\*/");
  blocks.append(commentBlock);

  return blocks;
}

QStringList JavaScriptSyntaxPlugin::keywords() const {
  QStringList all;
  all << getPrimaryKeywords() << getSecondaryKeywords()
      << getTertiaryKeywords();
  return all;
}
