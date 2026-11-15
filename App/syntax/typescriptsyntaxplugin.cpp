#include "typescriptsyntaxplugin.h"
#include <QRegularExpression>

QStringList TypeScriptSyntaxPlugin::getPrimaryKeywords() {
  return {"var",       "let",       "const",      "function",  "class",
          "interface", "type",      "enum",       "namespace", "module",
          "declare",   "abstract",  "implements", "extends",   "public",
          "private",   "protected", "readonly",   "static"};
}

QStringList TypeScriptSyntaxPlugin::getSecondaryKeywords() {
  return {"any",      "boolean", "break",      "case",    "catch", "continue",
          "debugger", "default", "delete",     "do",      "else",  "export",
          "false",    "finally", "for",        "from",    "get",   "if",
          "import",   "in",      "instanceof", "keyof",   "new",   "null",
          "number",   "object",  "of",         "return",  "set",   "string",
          "super",    "switch",  "symbol",     "this",    "throw", "true",
          "try",      "typeof",  "undefined",  "unknown", "void",  "while",
          "with",     "yield",   "async",      "await",   "never", "bigint",
          "as",       "is",      "infer",      "asserts"};
}

QStringList TypeScriptSyntaxPlugin::getTertiaryKeywords() {
  return {"Array",     "Boolean",  "Date",     "Error",       "Function",
          "JSON",      "Map",      "Math",     "Number",      "Object",
          "Promise",   "RegExp",   "Set",      "String",      "Symbol",
          "WeakMap",   "WeakSet",  "console",  "document",    "window",
          "Partial",   "Required", "Readonly", "Record",      "Pick",
          "Omit",      "Exclude",  "Extract",  "NonNullable", "Parameters",
          "ReturnType"};
}

QVector<SyntaxRule> TypeScriptSyntaxPlugin::syntaxRules() const {
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
      QRegularExpression("\\b[-+]?\\d[\\d_]*(\\.\\d+)?([eE][+-]?\\d+)?n?\\b");
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

  SyntaxRule templateRule;
  templateRule.pattern = QRegularExpression("`[^`]*`");
  templateRule.name = "string";
  rules.append(templateRule);

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

  SyntaxRule decoratorRule;
  decoratorRule.pattern = QRegularExpression("@[A-Za-z_][A-Za-z0-9_]*");
  decoratorRule.name = "keyword_1";
  rules.append(decoratorRule);

  SyntaxRule commentRule;
  commentRule.pattern = QRegularExpression("//[^\n]*");
  commentRule.name = "comment";
  rules.append(commentRule);

  SyntaxRule typeDefinition;
  typeDefinition.pattern = QRegularExpression(
      R"(\b(?:class|interface|enum|type)\s+([A-Za-z_$][\w$]*))");
  typeDefinition.name = "type_definition";
  typeDefinition.captureGroup = 1;
  rules.append(typeDefinition);

  return rules;
}

QVector<MultiLineBlock> TypeScriptSyntaxPlugin::multiLineBlocks() const {
  QVector<MultiLineBlock> blocks;

  MultiLineBlock commentBlock;
  commentBlock.startPattern = QRegularExpression("/\\*");
  commentBlock.endPattern = QRegularExpression("\\*/");
  blocks.append(commentBlock);

  return blocks;
}

QStringList TypeScriptSyntaxPlugin::keywords() const {
  QStringList all;
  all << getPrimaryKeywords() << getSecondaryKeywords()
      << getTertiaryKeywords();
  return all;
}
