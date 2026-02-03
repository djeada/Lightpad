#include "yamlsyntaxplugin.h"
#include <QRegularExpression>

QVector<SyntaxRule> YamlSyntaxPlugin::syntaxRules() const {
  QVector<SyntaxRule> rules;

  // Keys (before colon)
  SyntaxRule keyRule;
  keyRule.pattern = QRegularExpression("^\\s*[A-Za-z_][A-Za-z0-9_-]*(?=\\s*:)");
  keyRule.name = "keyword_0";
  rules.append(keyRule);

  // Quoted keys
  SyntaxRule quotedKeyRule;
  quotedKeyRule.pattern = QRegularExpression("^\\s*[\"'][^\"']+[\"'](?=\\s*:)");
  quotedKeyRule.name = "keyword_0";
  rules.append(quotedKeyRule);

  // Boolean values
  SyntaxRule boolRule;
  boolRule.pattern =
      QRegularExpression("\\b(true|false|yes|no|on|off)\\b",
                         QRegularExpression::CaseInsensitiveOption);
  boolRule.name = "keyword_1";
  rules.append(boolRule);

  // Null values
  SyntaxRule nullRule;
  nullRule.pattern = QRegularExpression("\\b(null|~)\\b");
  nullRule.name = "keyword_2";
  rules.append(nullRule);

  // Numbers
  SyntaxRule numberRule;
  numberRule.pattern = QRegularExpression("-?\\d+(\\.\\d+)?([eE][+-]?\\d+)?");
  numberRule.name = "number";
  rules.append(numberRule);

  // Anchors and aliases
  SyntaxRule anchorRule;
  anchorRule.pattern = QRegularExpression("[&*][A-Za-z_][A-Za-z0-9_-]*");
  anchorRule.name = "keyword_1";
  rules.append(anchorRule);

  // Tags
  SyntaxRule tagRule;
  tagRule.pattern = QRegularExpression("!![A-Za-z]+|![A-Za-z_][A-Za-z0-9_-]*");
  tagRule.name = "keyword_2";
  rules.append(tagRule);

  // String literals (double quotes)
  SyntaxRule stringRule;
  stringRule.pattern = QRegularExpression("\"[^\"]*\"");
  stringRule.name = "string";
  rules.append(stringRule);

  // String literals (single quotes)
  SyntaxRule singleQuoteRule;
  singleQuoteRule.pattern = QRegularExpression("'[^']*'");
  singleQuoteRule.name = "string";
  rules.append(singleQuoteRule);

  // Document markers (--- and ...)
  SyntaxRule docMarkerRule;
  docMarkerRule.pattern = QRegularExpression("^(---|\\.\\.\\.)$");
  docMarkerRule.name = "keyword_0";
  rules.append(docMarkerRule);

  // List indicators (-)
  SyntaxRule listRule;
  listRule.pattern = QRegularExpression("^\\s*-\\s");
  listRule.name = "keyword_2";
  rules.append(listRule);

  // Comments
  SyntaxRule commentRule;
  commentRule.pattern = QRegularExpression("#[^\n]*");
  commentRule.name = "comment";
  rules.append(commentRule);

  return rules;
}

QVector<MultiLineBlock> YamlSyntaxPlugin::multiLineBlocks() const {
  QVector<MultiLineBlock> blocks;
  // YAML doesn't have multi-line block comments
  return blocks;
}

QStringList YamlSyntaxPlugin::keywords() const {
  return {"true", "false", "yes", "no", "on", "off", "null"};
}
