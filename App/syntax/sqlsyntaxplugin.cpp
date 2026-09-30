#include "sqlsyntaxplugin.h"
#include "../database/sqlkeywords.h"
#include <QRegularExpression>

namespace {

void addRule(QVector<SyntaxRule> &rules, const QString &pattern,
             const QString &name, int captureGroup = 0,
             QRegularExpression::PatternOptions options =
                 QRegularExpression::NoPatternOption) {
  SyntaxRule rule;
  rule.pattern = QRegularExpression(pattern, options);
  rule.name = name;
  rule.captureGroup = captureGroup;
  rules.append(rule);
}

QString alternation(const QStringList &words) {
  QStringList escaped;
  for (const QString &w : words) {
    escaped << QRegularExpression::escape(w);
  }
  return "\\b(?:" + escaped.join('|') + ")\\b";
}

} // namespace

QVector<SyntaxRule> SqlSyntaxPlugin::syntaxRules() const {
  QVector<SyntaxRule> rules;
  const auto ci = QRegularExpression::CaseInsensitiveOption;

  addRule(rules, alternation(SqlKeywords::keywords()), "keyword_0", 0, ci);
  addRule(rules, alternation(SqlKeywords::types()), "keyword_1", 0, ci);
  addRule(rules,
          "\\b(?:0[xX][0-9a-fA-F]+|\\d+(?:\\.\\d+)?(?:[eE][+-]?\\d+)?)\\b",
          "number");
  addRule(rules, "[A-Za-z_][A-Za-z0-9_$]*(?=\\s*\\()", "function");
  addRule(rules, alternation(SqlKeywords::functions()), "function", 0, ci);
  addRule(rules, "(?:<>|!=|<=|>=|\\|\\||::|[-+*/%=<>&|^~])", "operator");

  addRule(rules, "(?:@@?|:|\\$)[A-Za-z_0-9]+", "constant");

  addRule(rules, "\"(?:[^\"]|\"\")*\"", "class");
  addRule(rules, "`(?:[^`]|``)*`", "class");
  addRule(rules, "\\[[^\\]\\n]+\\]", "class");
  addRule(rules, "'(?:[^']|'')*'", "string");
  addRule(rules, "--[^\n]*", "comment");
  return rules;
}

QVector<MultiLineBlock> SqlSyntaxPlugin::multiLineBlocks() const {
  MultiLineBlock block;
  block.startPattern = QRegularExpression("/\\*");
  block.endPattern = QRegularExpression("\\*/");
  block.name = "comment";
  return {block};
}

QStringList SqlSyntaxPlugin::keywords() const {
  QStringList all = SqlKeywords::keywords();
  all << SqlKeywords::types() << SqlKeywords::functions();
  all.removeDuplicates();
  return all;
}
