#include "pythonsyntaxplugin.h"
#include <QRegularExpression>

QStringList PythonSyntaxPlugin::getPrimaryKeywords() {
  return {"False",  "None",     "True",  "and",    "as",       "assert",
          "async",  "await",    "break", "class",  "continue", "def",
          "del",    "elif",     "else",  "except", "finally",  "for",
          "from",   "global",   "if",    "import", "in",       "is",
          "lambda", "nonlocal", "not",   "or",     "pass",     "raise",
          "return", "try",      "while", "with",   "yield"};
}

QStringList PythonSyntaxPlugin::getSecondaryKeywords() {
  return {"int", "float", "str", "bool", "list", "dict", "tuple", "set"};
}

QStringList PythonSyntaxPlugin::getTertiaryKeywords() {
  return {"self", "super", "__init__"};
}

QVector<SyntaxRule> PythonSyntaxPlugin::syntaxRules() const {
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
  numberRule.pattern = QRegularExpression("\\b[-+.,]*\\d{1,}f*\\b");
  numberRule.name = "number";
  rules.append(numberRule);

  SyntaxRule stringRule;
  stringRule.pattern = QRegularExpression("\".*\"");
  stringRule.name = "string";
  rules.append(stringRule);

  SyntaxRule singleQuoteRule;
  singleQuoteRule.pattern = QRegularExpression("'.*'");
  singleQuoteRule.name = "string";
  rules.append(singleQuoteRule);

  SyntaxRule functionRule;
  functionRule.pattern = QRegularExpression("\\b[A-Za-z0-9_]+(?=\\()");
  functionRule.name = "function";
  rules.append(functionRule);

  SyntaxRule decoratorRule;
  decoratorRule.pattern = QRegularExpression("@[A-Za-z0-9_]+");
  decoratorRule.name = "keyword_1";
  rules.append(decoratorRule);

  SyntaxRule commentRule;
  commentRule.pattern = QRegularExpression("#[^\n]*");
  commentRule.name = "comment";
  rules.append(commentRule);

  return rules;
}

QVector<MultiLineBlock> PythonSyntaxPlugin::multiLineBlocks() const {
  QVector<MultiLineBlock> blocks;

  MultiLineBlock singleQuoteBlock;
  singleQuoteBlock.startPattern = QRegularExpression("'''");
  singleQuoteBlock.endPattern = QRegularExpression("'''");
  blocks.append(singleQuoteBlock);

  MultiLineBlock doubleQuoteBlock;
  doubleQuoteBlock.startPattern = QRegularExpression("\"\"\"");
  doubleQuoteBlock.endPattern = QRegularExpression("\"\"\"");
  blocks.append(doubleQuoteBlock);

  return blocks;
}

QStringList PythonSyntaxPlugin::keywords() const {
  QStringList all;
  all << getPrimaryKeywords() << getSecondaryKeywords()
      << getTertiaryKeywords();
  return all;
}
