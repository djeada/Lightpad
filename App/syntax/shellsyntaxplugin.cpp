#include "shellsyntaxplugin.h"
#include <QRegularExpression>

QStringList ShellSyntaxPlugin::getPrimaryKeywords() {
  return {"if",   "then",     "else",   "elif",  "fi",    "case",
          "esac", "for",      "while",  "until", "do",    "done",
          "in",   "function", "select", "time",  "coproc"};
}

QStringList ShellSyntaxPlugin::getSecondaryKeywords() {
  return {"break",   "continue", "return", "exit",     "shift",   "source",
          "alias",   "unalias",  "export", "readonly", "declare", "local",
          "typeset", "unset",    "set",    "shopt",    "trap",    "eval",
          "exec",    "true",     "false"};
}

QStringList ShellSyntaxPlugin::getTertiaryKeywords() {
  return {"echo",  "printf", "read", "cd",   "pwd",  "pushd", "popd",
          "dirs",  "ls",     "cat",  "grep", "sed",  "awk",   "find",
          "xargs", "sort",   "uniq", "head", "tail", "wc",    "cut",
          "paste", "tr",     "test", "expr"};
}

QVector<SyntaxRule> ShellSyntaxPlugin::syntaxRules() const {
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

  SyntaxRule varRule;
  varRule.pattern =
      QRegularExpression("\\$[A-Za-z_][A-Za-z0-9_]*|\\$\\{[^}]+\\}");
  varRule.name = "keyword_1";
  rules.append(varRule);

  SyntaxRule specialVarRule;
  specialVarRule.pattern = QRegularExpression("\\$[0-9@#?$!*-]");
  specialVarRule.name = "keyword_1";
  rules.append(specialVarRule);

  SyntaxRule numberRule;
  numberRule.pattern = QRegularExpression("\\b\\d+\\b");
  numberRule.name = "number";
  rules.append(numberRule);

  SyntaxRule stringRule;
  stringRule.pattern = QRegularExpression("\"[^\"]*\"");
  stringRule.name = "string";
  rules.append(stringRule);

  SyntaxRule singleQuoteRule;
  singleQuoteRule.pattern = QRegularExpression("'[^']*'");
  singleQuoteRule.name = "string";
  rules.append(singleQuoteRule);

  SyntaxRule functionRule;
  functionRule.pattern =
      QRegularExpression("\\b[A-Za-z_][A-Za-z0-9_]*(?=\\s*\\(\\))");
  functionRule.name = "function";
  rules.append(functionRule);

  SyntaxRule shebangRule;
  shebangRule.pattern = QRegularExpression("^#!.*$");
  shebangRule.name = "comment";
  rules.append(shebangRule);

  SyntaxRule commentRule;
  commentRule.pattern = QRegularExpression("#[^\n]*");
  commentRule.name = "comment";
  rules.append(commentRule);

  return rules;
}

QVector<MultiLineBlock> ShellSyntaxPlugin::multiLineBlocks() const {
  QVector<MultiLineBlock> blocks;

  return blocks;
}

QStringList ShellSyntaxPlugin::keywords() const {
  QStringList all;
  all << getPrimaryKeywords() << getSecondaryKeywords()
      << getTertiaryKeywords();
  return all;
}
