#include "csssyntaxplugin.h"
#include <QRegularExpression>

QStringList CssSyntaxPlugin::getProperties() {
  return {"color",
          "background",
          "background-color",
          "background-image",
          "border",
          "border-radius",
          "margin",
          "padding",
          "width",
          "height",
          "min-width",
          "max-width",
          "min-height",
          "max-height",
          "display",
          "position",
          "top",
          "right",
          "bottom",
          "left",
          "float",
          "clear",
          "overflow",
          "z-index",
          "font",
          "font-family",
          "font-size",
          "font-weight",
          "font-style",
          "text-align",
          "text-decoration",
          "line-height",
          "letter-spacing",
          "flex",
          "flex-direction",
          "justify-content",
          "align-items",
          "flex-wrap",
          "grid",
          "grid-template-columns",
          "grid-template-rows",
          "gap",
          "transform",
          "transition",
          "animation",
          "opacity",
          "visibility",
          "cursor",
          "box-shadow",
          "outline",
          "content"};
}

QStringList CssSyntaxPlugin::getValues() {
  return {"none",      "auto",      "inherit",      "initial",  "unset",
          "block",     "inline",    "inline-block", "flex",     "grid",
          "hidden",    "visible",   "absolute",     "relative", "fixed",
          "sticky",    "static",    "center",       "left",     "right",
          "top",       "bottom",    "transparent",  "solid",    "dashed",
          "dotted",    "bold",      "normal",       "italic",   "underline",
          "uppercase", "lowercase", "nowrap",       "wrap",     "pointer",
          "default",   "row",       "column"};
}

QStringList CssSyntaxPlugin::getAtRules() {
  return {"@import",   "@media",     "@keyframes", "@font-face", "@charset",
          "@supports", "@namespace", "@page",      "@viewport"};
}

QVector<SyntaxRule> CssSyntaxPlugin::syntaxRules() const {
  QVector<SyntaxRule> rules;

  for (const QString &rule : getAtRules()) {
    SyntaxRule atRule;
    atRule.pattern = QRegularExpression(rule + "\\b");
    atRule.name = "keyword_0";
    rules.append(atRule);
  }

  for (const QString &prop : getProperties()) {
    SyntaxRule rule;
    rule.pattern = QRegularExpression("\\b" + prop + "(?=\\s*:)");
    rule.name = "keyword_1";
    rules.append(rule);
  }

  for (const QString &val : getValues()) {
    SyntaxRule rule;
    rule.pattern = QRegularExpression("\\b" + val + "\\b");
    rule.name = "keyword_2";
    rules.append(rule);
  }

  SyntaxRule classRule;
  classRule.pattern = QRegularExpression("\\.[A-Za-z_][A-Za-z0-9_-]*");
  classRule.name = "function";
  rules.append(classRule);

  SyntaxRule idRule;
  idRule.pattern = QRegularExpression("#[A-Za-z_][A-Za-z0-9_-]*(?![;])");
  idRule.name = "function";
  rules.append(idRule);

  SyntaxRule pseudoRule;
  pseudoRule.pattern = QRegularExpression(":[A-Za-z-]+");
  pseudoRule.name = "keyword_2";
  rules.append(pseudoRule);

  SyntaxRule numberRule;
  numberRule.pattern = QRegularExpression(
      "[-+]?\\d*\\.?\\d+(%|px|em|rem|vh|vw|pt|cm|mm|in|s|ms)?");
  numberRule.name = "number";
  rules.append(numberRule);

  SyntaxRule hexColorRule;
  hexColorRule.pattern = QRegularExpression("#[0-9A-Fa-f]{3,8}\\b");
  hexColorRule.name = "number";
  rules.append(hexColorRule);

  SyntaxRule stringRule;
  stringRule.pattern = QRegularExpression("\"[^\"]*\"");
  stringRule.name = "string";
  rules.append(stringRule);

  SyntaxRule singleQuoteRule;
  singleQuoteRule.pattern = QRegularExpression("'[^']*'");
  singleQuoteRule.name = "string";
  rules.append(singleQuoteRule);

  SyntaxRule urlRule;
  urlRule.pattern = QRegularExpression("\\burl\\([^)]*\\)");
  urlRule.name = "string";
  rules.append(urlRule);

  return rules;
}

QVector<MultiLineBlock> CssSyntaxPlugin::multiLineBlocks() const {
  QVector<MultiLineBlock> blocks;

  MultiLineBlock commentBlock;
  commentBlock.startPattern = QRegularExpression("/\\*");
  commentBlock.endPattern = QRegularExpression("\\*/");
  blocks.append(commentBlock);

  return blocks;
}

QStringList CssSyntaxPlugin::keywords() const {
  QStringList all;
  all << getProperties() << getValues() << getAtRules();
  return all;
}
