#include "javasyntaxplugin.h"
#include <QRegularExpression>

QStringList JavaSyntaxPlugin::getPrimaryKeywords() {
  return {"abstract",   "assert",       "boolean",   "break",      "byte",
          "case",       "catch",        "char",      "class",      "const",
          "continue",   "default",      "do",        "double",     "else",
          "enum",       "extends",      "final",     "finally",    "float",
          "for",        "goto",         "if",        "implements", "import",
          "instanceof", "int",          "interface", "long",       "native",
          "new",        "package",      "private",   "protected",  "public",
          "return",     "short",        "static",    "strictfp",   "super",
          "switch",     "synchronized", "this",      "throw",      "throws",
          "transient",  "try",          "void",      "volatile",   "while"};
}

QStringList JavaSyntaxPlugin::getSecondaryKeywords() {
  return {"true",     "false",      "null",      "var",   "record",
          "sealed",   "non-sealed", "permits",   "yield", "module",
          "requires", "exports",    "opens",     "uses",  "provides",
          "with",     "to",         "transitive"};
}

QStringList JavaSyntaxPlugin::getTertiaryKeywords() {
  return {"String",
          "Integer",
          "Boolean",
          "Double",
          "Float",
          "Long",
          "Short",
          "Byte",
          "Character",
          "Object",
          "Class",
          "System",
          "Math",
          "Thread",
          "Runnable",
          "Exception",
          "RuntimeException",
          "Error",
          "Throwable",
          "Override",
          "Deprecated",
          "SuppressWarnings",
          "FunctionalInterface"};
}

QVector<SyntaxRule> JavaSyntaxPlugin::syntaxRules() const {
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
      "\\b[-+]?\\d[\\d_]*(\\.\\d+)?([eE][+-]?\\d+)?[lLfFdD]?\\b");
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
      QStringLiteral("\\b(?!(?:%1)\\b)[A-Za-z_$][A-Za-z0-9_$]*(?=\\s*\\()")
          .arg(keywords().join('|')));
  functionRule.name = "function";
  rules.append(functionRule);

  SyntaxRule constructedType;
  constructedType.pattern = QRegularExpression(
      R"(\b(?:new|extends|implements|instanceof)\s+(?:[A-Za-z_$][\w$]*\s*\.\s*)*([A-Za-z_$][\w$]*))");
  constructedType.name = "type_reference";
  constructedType.captureGroup = 1;
  rules.append(constructedType);

  SyntaxRule annotationRule;
  annotationRule.pattern = QRegularExpression("@[A-Za-z_][A-Za-z0-9_]*");
  annotationRule.name = "keyword_1";
  rules.append(annotationRule);

  SyntaxRule commentRule;
  commentRule.pattern = QRegularExpression("//[^\n]*");
  commentRule.name = "comment";
  rules.append(commentRule);

  SyntaxRule typeDefinition;
  typeDefinition.pattern = QRegularExpression(
      R"(\b(?:class|interface|enum|record)\s+([A-Za-z_$][\w$]*))");
  typeDefinition.name = "type_definition";
  typeDefinition.captureGroup = 1;
  rules.append(typeDefinition);

  return rules;
}

QVector<MultiLineBlock> JavaSyntaxPlugin::multiLineBlocks() const {
  QVector<MultiLineBlock> blocks;

  MultiLineBlock commentBlock;
  commentBlock.startPattern = QRegularExpression("/\\*");
  commentBlock.endPattern = QRegularExpression("\\*/");
  blocks.append(commentBlock);

  return blocks;
}

QStringList JavaSyntaxPlugin::keywords() const {
  QStringList all;
  all << getPrimaryKeywords() << getSecondaryKeywords()
      << getTertiaryKeywords();
  return all;
}
