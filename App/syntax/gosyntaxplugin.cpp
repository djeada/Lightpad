#include "gosyntaxplugin.h"
#include <QRegularExpression>

QStringList GoSyntaxPlugin::getPrimaryKeywords() {
  return {"break",     "case",   "chan",    "const",       "continue",
          "default",   "defer",  "else",    "fallthrough", "for",
          "func",      "go",     "goto",    "if",          "import",
          "interface", "map",    "package", "range",       "return",
          "select",    "struct", "switch",  "type",        "var"};
}

QStringList GoSyntaxPlugin::getSecondaryKeywords() {
  return {"bool",    "byte",    "complex64", "complex128", "error",
          "float32", "float64", "int",       "int8",       "int16",
          "int32",   "int64",   "rune",      "string",     "uint",
          "uint8",   "uint16",  "uint32",    "uint64",     "uintptr"};
}

QStringList GoSyntaxPlugin::getTertiaryKeywords() {
  return {"true",    "false", "nil",     "iota", "append", "cap",  "close",
          "complex", "copy",  "delete",  "imag", "len",    "make", "new",
          "panic",   "print", "println", "real", "recover"};
}

QVector<SyntaxRule> GoSyntaxPlugin::syntaxRules() const {
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
  numberRule.pattern =
      QRegularExpression("\\b[-+]?\\d[\\d_]*(\\.\\d+)?([eE][+-]?\\d+)?i?\\b");
  numberRule.name = "number";
  rules.append(numberRule);

  SyntaxRule stringRule;
  stringRule.pattern = QRegularExpression("\"[^\"]*\"");
  stringRule.name = "string";
  rules.append(stringRule);

  SyntaxRule rawStringRule;
  rawStringRule.pattern = QRegularExpression("`[^`]*`");
  rawStringRule.name = "string";
  rules.append(rawStringRule);

  SyntaxRule charRule;
  charRule.pattern = QRegularExpression("'[^']*'");
  charRule.name = "string";
  rules.append(charRule);

  // Method calls are refined by the function rule below; field accesses keep
  // the member color, including chains such as mesh.position.x.
  SyntaxRule memberRule;
  memberRule.pattern = QRegularExpression(
      R"((?:(?<!\.)\.(?!\.)|->)\s*([A-Za-z_$][A-Za-z0-9_$]*))");
  memberRule.name = "member";
  memberRule.captureGroup = 1;
  rules.append(memberRule);

  SyntaxRule functionRule;
  functionRule.pattern = QRegularExpression(
      QStringLiteral("\\b(?!(?:%1)\\b)[A-Za-z_][A-Za-z0-9_]*(?=\\s*\\()")
          .arg(keywords().join('|')));
  functionRule.name = "function";
  rules.append(functionRule);

  SyntaxRule commentRule;
  commentRule.pattern = QRegularExpression("//[^\n]*");
  commentRule.name = "comment";
  rules.append(commentRule);

  // Capture the declared name, keeping the keyword in its own color.
  SyntaxRule typeDefinition0;
  typeDefinition0.pattern = QRegularExpression(R"(\btype\s+([A-Za-z_]\w*))");
  typeDefinition0.name = "type_definition";
  typeDefinition0.captureGroup = 1;
  rules.append(typeDefinition0);

  return rules;
}

QVector<MultiLineBlock> GoSyntaxPlugin::multiLineBlocks() const {
  QVector<MultiLineBlock> blocks;

  MultiLineBlock commentBlock;
  commentBlock.startPattern = QRegularExpression("/\\*");
  commentBlock.endPattern = QRegularExpression("\\*/");
  blocks.append(commentBlock);

  return blocks;
}

QStringList GoSyntaxPlugin::keywords() const {
  QStringList all;
  all << getPrimaryKeywords() << getSecondaryKeywords()
      << getTertiaryKeywords();
  return all;
}
