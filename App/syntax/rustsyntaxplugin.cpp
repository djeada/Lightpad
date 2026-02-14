#include "rustsyntaxplugin.h"
#include <QRegularExpression>

QStringList RustSyntaxPlugin::getPrimaryKeywords() {
  return {"as",     "async",  "await", "break",  "const",  "continue", "crate",
          "dyn",    "else",   "enum",  "extern", "false",  "fn",       "for",
          "if",     "impl",   "in",    "let",    "loop",   "match",    "mod",
          "move",   "mut",    "pub",   "ref",    "return", "self",     "Self",
          "static", "struct", "super", "trait",  "true",   "type",     "unsafe",
          "use",    "where",  "while"};
}

QStringList RustSyntaxPlugin::getSecondaryKeywords() {
  return {"bool",  "char",  "f32",    "f64",    "i8",     "i16", "i32", "i64",
          "i128",  "isize", "str",    "u8",     "u16",    "u32", "u64", "u128",
          "usize", "Box",   "Option", "Result", "String", "Vec"};
}

QStringList RustSyntaxPlugin::getTertiaryKeywords() {
  return {"Some",      "None",         "Ok",    "Err",          "println",
          "print",     "format",       "panic", "assert",       "assert_eq",
          "assert_ne", "debug_assert", "todo",  "unimplemented"};
}

QVector<SyntaxRule> RustSyntaxPlugin::syntaxRules() const {
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
      QRegularExpression("\\b[-+]?\\d[\\d_]*(\\.[\\d_]+)?([eE][+-]?\\d+)?([uif]"
                         "(8|16|32|64|128|size))?\\b");
  numberRule.name = "number";
  rules.append(numberRule);

  SyntaxRule stringRule;
  stringRule.pattern = QRegularExpression("\"[^\"]*\"");
  stringRule.name = "string";
  rules.append(stringRule);

  SyntaxRule charRule;
  charRule.pattern = QRegularExpression("'[^']*'");
  charRule.name = "string";
  rules.append(charRule);

  SyntaxRule functionRule;
  functionRule.pattern = QRegularExpression("\\b[A-Za-z_][A-Za-z0-9_]*(?=\\()");
  functionRule.name = "function";
  rules.append(functionRule);

  SyntaxRule macroRule;
  macroRule.pattern = QRegularExpression("\\b[A-Za-z_][A-Za-z0-9_]*!");
  macroRule.name = "keyword_1";
  rules.append(macroRule);

  SyntaxRule attributeRule;
  attributeRule.pattern = QRegularExpression("#\\[.*\\]");
  attributeRule.name = "keyword_2";
  rules.append(attributeRule);

  SyntaxRule commentRule;
  commentRule.pattern = QRegularExpression("//[^\n]*");
  commentRule.name = "comment";
  rules.append(commentRule);

  return rules;
}

QVector<MultiLineBlock> RustSyntaxPlugin::multiLineBlocks() const {
  QVector<MultiLineBlock> blocks;

  MultiLineBlock commentBlock;
  commentBlock.startPattern = QRegularExpression("/\\*");
  commentBlock.endPattern = QRegularExpression("\\*/");
  blocks.append(commentBlock);

  return blocks;
}

QStringList RustSyntaxPlugin::keywords() const {
  QStringList all;
  all << getPrimaryKeywords() << getSecondaryKeywords()
      << getTertiaryKeywords();
  return all;
}
