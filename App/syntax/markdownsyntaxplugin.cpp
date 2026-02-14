#include "markdownsyntaxplugin.h"
#include <QRegularExpression>

QVector<SyntaxRule> MarkdownSyntaxPlugin::syntaxRules() const {
  QVector<SyntaxRule> rules;

  SyntaxRule headerRule;
  headerRule.pattern = QRegularExpression("^#{1,6}\\s.*$");
  headerRule.name = "keyword_0";
  rules.append(headerRule);

  SyntaxRule setextRule;
  setextRule.pattern = QRegularExpression("^(={3,}|-{3,})\\s*$");
  setextRule.name = "keyword_0";
  rules.append(setextRule);

  SyntaxRule boldItalicRule;
  boldItalicRule.pattern =
      QRegularExpression("\\*\\*\\*[^*]+\\*\\*\\*|___[^_]+___");
  boldItalicRule.name = "keyword_1";
  rules.append(boldItalicRule);

  SyntaxRule boldRule;
  boldRule.pattern = QRegularExpression("\\*\\*[^*]+\\*\\*|__[^_]+__");
  boldRule.name = "keyword_1";
  rules.append(boldRule);

  SyntaxRule italicRule;
  italicRule.pattern = QRegularExpression("(?<!\\*)\\*(?!\\*)([^*]+)\\*(?!\\*)"
                                          "|(?<!_)_(?!_)([^_]+)_(?!_)");
  italicRule.name = "keyword_2";
  rules.append(italicRule);

  SyntaxRule strikeRule;
  strikeRule.pattern = QRegularExpression("~~[^~]+~~");
  strikeRule.name = "comment";
  rules.append(strikeRule);

  SyntaxRule highlightRule;
  highlightRule.pattern = QRegularExpression("==[^=]+=={1,2}");
  highlightRule.name = "keyword_1";
  rules.append(highlightRule);

  SyntaxRule subscriptRule;
  subscriptRule.pattern = QRegularExpression("(?<!~)~(?!~)[^~]+~(?!~)");
  subscriptRule.name = "keyword_2";
  rules.append(subscriptRule);

  SyntaxRule superscriptRule;
  superscriptRule.pattern = QRegularExpression("\\^[^^]+\\^");
  superscriptRule.name = "keyword_2";
  rules.append(superscriptRule);

  SyntaxRule inlineCodeDoubleRule;
  inlineCodeDoubleRule.pattern = QRegularExpression("``[^`]+``");
  inlineCodeDoubleRule.name = "string";
  rules.append(inlineCodeDoubleRule);

  SyntaxRule inlineCodeRule;
  inlineCodeRule.pattern = QRegularExpression("`[^`]+`");
  inlineCodeRule.name = "string";
  rules.append(inlineCodeRule);

  SyntaxRule fencedLangRule;
  fencedLangRule.pattern = QRegularExpression("^(`{3,}|~{3,})[A-Za-z0-9_+-]+");
  fencedLangRule.name = "string";
  rules.append(fencedLangRule);

  SyntaxRule imageRule;
  imageRule.pattern = QRegularExpression("!\\[[^\\]]*\\]\\([^)]*\\)");
  imageRule.name = "function";
  rules.append(imageRule);

  SyntaxRule linkRule;
  linkRule.pattern = QRegularExpression("\\[[^\\]]+\\]\\([^)]*\\)");
  linkRule.name = "function";
  rules.append(linkRule);

  SyntaxRule refLinkRule;
  refLinkRule.pattern = QRegularExpression("\\[[^\\]]+\\]\\[[^\\]]*\\]");
  refLinkRule.name = "function";
  rules.append(refLinkRule);

  SyntaxRule refDefRule;
  refDefRule.pattern = QRegularExpression("^\\s{0,3}\\[[^\\]]+\\]:\\s+\\S+.*$");
  refDefRule.name = "function";
  rules.append(refDefRule);

  SyntaxRule autolinkRule;
  autolinkRule.pattern =
      QRegularExpression("<(https?://[^>]+|[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\."
                         "[A-Za-z]{2,})>");
  autolinkRule.name = "function";
  rules.append(autolinkRule);

  SyntaxRule urlRule;
  urlRule.pattern = QRegularExpression("https?://[^\\s<>\"'\\)]+");
  urlRule.name = "string";
  rules.append(urlRule);

  SyntaxRule quoteRule;
  quoteRule.pattern = QRegularExpression("^\\s{0,3}(>\\s?)+.*$");
  quoteRule.name = "comment";
  rules.append(quoteRule);

  SyntaxRule unorderedListRule;
  unorderedListRule.pattern = QRegularExpression("^\\s*[*+-]\\s");
  unorderedListRule.name = "keyword_2";
  rules.append(unorderedListRule);

  SyntaxRule orderedListRule;
  orderedListRule.pattern = QRegularExpression("^\\s*\\d+[.)]\\s");
  orderedListRule.name = "keyword_2";
  rules.append(orderedListRule);

  SyntaxRule taskListRule;
  taskListRule.pattern = QRegularExpression(
      "^\\s*[*+-]\\s+\\[([ xX])\\]", QRegularExpression::CaseInsensitiveOption);
  taskListRule.name = "keyword_1";
  rules.append(taskListRule);

  SyntaxRule hrRule;
  hrRule.pattern = QRegularExpression("^\\s{0,3}([-*_]\\s*){3,}$");
  hrRule.name = "keyword_0";
  rules.append(hrRule);

  SyntaxRule tableSepRule;
  tableSepRule.pattern =
      QRegularExpression("^\\|?(\\s*:?-{3,}:?\\s*\\|)+\\s*:?-{3,}:?\\s*\\|?$");
  tableSepRule.name = "keyword_0";
  rules.append(tableSepRule);

  SyntaxRule tablePipeRule;
  tablePipeRule.pattern = QRegularExpression("\\|");
  tablePipeRule.name = "keyword_2";
  rules.append(tablePipeRule);

  SyntaxRule footnoteRefRule;
  footnoteRefRule.pattern = QRegularExpression("\\[\\^[^\\]]+\\]");
  footnoteRefRule.name = "function";
  rules.append(footnoteRefRule);

  SyntaxRule footnoteDefRule;
  footnoteDefRule.pattern = QRegularExpression("^\\[\\^[^\\]]+\\]:\\s.*$");
  footnoteDefRule.name = "function";
  rules.append(footnoteDefRule);

  SyntaxRule defListRule;
  defListRule.pattern = QRegularExpression("^:\\s+.+$");
  defListRule.name = "keyword_2";
  rules.append(defListRule);

  SyntaxRule htmlTagRule;
  htmlTagRule.pattern =
      QRegularExpression("</?[A-Za-z][A-Za-z0-9]*(?:\\s[^>]*)?\\/?>",
                         QRegularExpression::CaseInsensitiveOption);
  htmlTagRule.name = "keyword_2";
  rules.append(htmlTagRule);

  SyntaxRule htmlEntityRule;
  htmlEntityRule.pattern = QRegularExpression("&[A-Za-z0-9#]+;");
  htmlEntityRule.name = "keyword_2";
  rules.append(htmlEntityRule);

  SyntaxRule escapeRule;
  escapeRule.pattern = QRegularExpression("\\\\[\\\\`*_{}\\[\\]()#+\\-.!|~>]");
  escapeRule.name = "comment";
  rules.append(escapeRule);

  SyntaxRule inlineMathRule;
  inlineMathRule.pattern =
      QRegularExpression("(?<!\\$)\\$(?!\\$)[^$\\n]+\\$(?!\\$)");
  inlineMathRule.name = "number";
  rules.append(inlineMathRule);

  SyntaxRule displayMathRule;
  displayMathRule.pattern = QRegularExpression("\\$\\$[^$]+\\$\\$");
  displayMathRule.name = "number";
  rules.append(displayMathRule);

  SyntaxRule emojiRule;
  emojiRule.pattern = QRegularExpression(":[A-Za-z0-9_+-]+:");
  emojiRule.name = "string";
  rules.append(emojiRule);

  SyntaxRule frontMatterRule;
  frontMatterRule.pattern = QRegularExpression("^---\\s*$");
  frontMatterRule.name = "keyword_0";
  rules.append(frontMatterRule);

  return rules;
}

QVector<MultiLineBlock> MarkdownSyntaxPlugin::multiLineBlocks() const {
  QVector<MultiLineBlock> blocks;

  MultiLineBlock codeBlock;
  codeBlock.startPattern = QRegularExpression("^`{3,}");
  codeBlock.endPattern = QRegularExpression("^`{3,}\\s*$");
  blocks.append(codeBlock);

  MultiLineBlock tildeBlock;
  tildeBlock.startPattern = QRegularExpression("^~{3,}");
  tildeBlock.endPattern = QRegularExpression("^~{3,}\\s*$");
  blocks.append(tildeBlock);

  MultiLineBlock commentBlock;
  commentBlock.startPattern = QRegularExpression("<!--");
  commentBlock.endPattern = QRegularExpression("-->");
  blocks.append(commentBlock);

  MultiLineBlock mathBlock;
  mathBlock.startPattern = QRegularExpression("^\\$\\$\\s*$");
  mathBlock.endPattern = QRegularExpression("^\\$\\$\\s*$");
  blocks.append(mathBlock);

  return blocks;
}

QStringList MarkdownSyntaxPlugin::keywords() const {
  return {

      "**bold**",    "*italic*",    "~~strikethrough~~",
      "# ",          "## ",         "### ",
      "#### ",       "##### ",      "###### ",
      "- ",          "* ",          "1. ",
      "- [ ] ",      "- [x] ",      "> ",
      "```",         "---",         "***",
      "[text](url)", "![alt](url)", "[text][ref]",
      "[^footnote]", "`code`",      "==highlight==",
      "$$math$$",    "$inline$",    "| col |",
  };
}
