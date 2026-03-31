#include "dockerfilesyntaxplugin.h"
#include <QRegularExpression>

QStringList DockerfileSyntaxPlugin::getInstructions() {
  return {"FROM",    "RUN",         "CMD",        "LABEL",
          "EXPOSE",  "ENV",         "ADD",        "COPY",
          "ENTRYPOINT", "VOLUME",   "USER",       "WORKDIR",
          "ARG",     "ONBUILD",     "STOPSIGNAL", "HEALTHCHECK",
          "SHELL",   "MAINTAINER"};
}

QVector<SyntaxRule> DockerfileSyntaxPlugin::syntaxRules() const {
  QVector<SyntaxRule> rules;

  for (const QString &instr : getInstructions()) {
    SyntaxRule rule;
    rule.pattern = QRegularExpression(
        "^\\s*" + instr + "\\b",
        QRegularExpression::CaseInsensitiveOption |
            QRegularExpression::MultilineOption);
    rule.name = "keyword_0";
    rules.append(rule);
  }

  SyntaxRule asRule;
  asRule.pattern = QRegularExpression(
      "\\bAS\\b", QRegularExpression::CaseInsensitiveOption);
  asRule.name = "keyword_1";
  rules.append(asRule);

  SyntaxRule flagRule;
  flagRule.pattern = QRegularExpression("--[a-zA-Z][a-zA-Z0-9_-]*");
  flagRule.name = "keyword_2";
  rules.append(flagRule);

  SyntaxRule varBraceRule;
  varBraceRule.pattern = QRegularExpression("\\$\\{[^}]+\\}");
  varBraceRule.name = "keyword_1";
  rules.append(varBraceRule);

  SyntaxRule varRule;
  varRule.pattern = QRegularExpression("\\$[A-Za-z_][A-Za-z0-9_]*");
  varRule.name = "keyword_1";
  rules.append(varRule);

  SyntaxRule portRule;
  portRule.pattern = QRegularExpression("\\b\\d+(/(?:tcp|udp))?\\b");
  portRule.name = "number";
  rules.append(portRule);

  SyntaxRule stringRule;
  stringRule.pattern = QRegularExpression("\"(?:[^\"\\\\]|\\\\.)*\"");
  stringRule.name = "string";
  rules.append(stringRule);

  SyntaxRule singleQuoteRule;
  singleQuoteRule.pattern = QRegularExpression("'(?:[^'\\\\]|\\\\.)*'");
  singleQuoteRule.name = "string";
  rules.append(singleQuoteRule);

  SyntaxRule jsonArrayRule;
  jsonArrayRule.pattern = QRegularExpression("\\[.*?\\]");
  jsonArrayRule.name = "string";
  rules.append(jsonArrayRule);

  SyntaxRule imageTagRule;
  imageTagRule.pattern = QRegularExpression(
      "(?<=FROM\\s)[a-zA-Z0-9_./-]+(?::[a-zA-Z0-9_./-]+)?",
      QRegularExpression::CaseInsensitiveOption);
  imageTagRule.name = "function";
  rules.append(imageTagRule);

  SyntaxRule commentRule;
  commentRule.pattern = QRegularExpression(
      "^\\s*#[^\n]*", QRegularExpression::MultilineOption);
  commentRule.name = "comment";
  rules.append(commentRule);

  SyntaxRule parserDirectiveRule;
  parserDirectiveRule.pattern = QRegularExpression(
      "^#\\s*(syntax|escape|check)\\s*=.*$",
      QRegularExpression::MultilineOption);
  parserDirectiveRule.name = "keyword_2";
  rules.append(parserDirectiveRule);

  return rules;
}

QVector<MultiLineBlock> DockerfileSyntaxPlugin::multiLineBlocks() const {
  QVector<MultiLineBlock> blocks;
  return blocks;
}

QStringList DockerfileSyntaxPlugin::keywords() const {
  return getInstructions();
}
