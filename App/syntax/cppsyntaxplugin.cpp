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

  // Lower priority than keywords: a type before a variable/parameter name.
  SyntaxRule declaredType;
  declaredType.pattern = QRegularExpression(
      R"(\b([A-Za-z_]\w*)(?=\s+(?:const\s+)?[A-Za-z_]\w*\s*[;=,()\[{}]))");
  declaredType.name = "type_reference";
  declaredType.captureGroup = 1;
  rules.append(declaredType);

  // A bare "left * right" is also an expression. Only infer pointer types
  // from a type-like spelling; declared local names are resolved by the index.
  SyntaxRule indirectType;
  indirectType.pattern = QRegularExpression(
      R"(\b([A-Z][A-Za-z0-9_]*|[A-Za-z_]\w*_t)(?=\s*(?:const\s+)?[*&]+\s*[A-Za-z_]\w*\s*[;=,()\[{}]))");
  indirectType.name = "type_reference";
  indirectType.captureGroup = 1;
  rules.append(indirectType);

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

  SyntaxRule rawStringRule;
  rawStringRule.pattern = QRegularExpression(
      R"lp((?:u8|u|U|L)?R"([^ ()\\\t\r\n]{0,16})\(.*?\)\1")lp");
  rawStringRule.name = "raw_string";
  rules.append(rawStringRule);

  SyntaxRule stringRule;
  stringRule.pattern = QRegularExpression("\"(?:\\\\.|[^\"\\\\])*\"");
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

  SyntaxRule constructedType;
  constructedType.pattern =
      QRegularExpression(R"(\bnew\s+(?:[A-Za-z_]\w*\s*::\s*)*([A-Za-z_]\w*))");
  constructedType.name = "type_reference";
  constructedType.captureGroup = 1;
  rules.append(constructedType);

  SyntaxRule castType;
  castType.pattern = QRegularExpression(
      R"(\b(?:static_cast|dynamic_cast|reinterpret_cast|const_cast)\s*<\s*(?:const\s+)?(?:[A-Za-z_]\w*\s*::\s*)*([A-Za-z_]\w*))");
  castType.name = "type_reference";
  castType.captureGroup = 1;
  rules.append(castType);

  SyntaxRule commentRule;
  commentRule.pattern = QRegularExpression("//[^\n]*");
  commentRule.name = "comment";
  rules.append(commentRule);

  // Capture the declared name, keeping the keyword in its own color.
  SyntaxRule typeDefinition0;
  typeDefinition0.pattern = QRegularExpression(
      R"(\b(?:struct|class|union|enum(?:\s+(?:class|struct))?)\s+(?:(?:\[\[[^\]]*\]\]|alignas\s*\([^)]*\))\s*)*([A-Za-z_]\w*))");
  typeDefinition0.name = "type_definition";
  typeDefinition0.captureGroup = 1;
  rules.append(typeDefinition0);

  SyntaxRule typeDefinition1;
  typeDefinition1.pattern =
      QRegularExpression(R"(\busing\s+([A-Za-z_]\w*)\s*=)");
  typeDefinition1.name = "type_definition";
  typeDefinition1.captureGroup = 1;
  rules.append(typeDefinition1);

  SyntaxRule typeDefinition2;
  typeDefinition2.pattern =
      QRegularExpression(R"(\btypedef\s+[^;{}]+\b([A-Za-z_]\w*)\s*;)");
  typeDefinition2.name = "type_definition";
  typeDefinition2.captureGroup = 1;
  rules.append(typeDefinition2);

  SyntaxRule anonymousTypedef;
  anonymousTypedef.pattern = QRegularExpression(
      R"(\btypedef\s+(?:struct|union|enum)\b[^;{}]*\{[^{}]*\}\s*([A-Za-z_]\w*)\s*;)");
  anonymousTypedef.name = "type_definition";
  anonymousTypedef.captureGroup = 1;
  rules.append(anonymousTypedef);

  SyntaxRule callbackTypedef;
  callbackTypedef.pattern =
      QRegularExpression(R"(\btypedef\b[^;{}]*\(\s*\*\s*([A-Za-z_]\w*)\s*\))");
  callbackTypedef.name = "type_definition";
  callbackTypedef.captureGroup = 1;
  rules.append(callbackTypedef);

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
