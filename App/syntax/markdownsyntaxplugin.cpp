#include "markdownsyntaxplugin.h"
#include <QRegularExpression>

QVector<SyntaxRule> MarkdownSyntaxPlugin::syntaxRules() const {
  QVector<SyntaxRule> rules;

  // --- Headers ---

  // ATX headers (# through ######)
  SyntaxRule headerRule;
  headerRule.pattern = QRegularExpression("^#{1,6}\\s.*$");
  headerRule.name = "keyword_0";
  rules.append(headerRule);

  // Setext header underlines (=== or ---)
  SyntaxRule setextRule;
  setextRule.pattern = QRegularExpression("^(={3,}|-{3,})\\s*$");
  setextRule.name = "keyword_0";
  rules.append(setextRule);

  // --- Emphasis and strong emphasis ---

  // Bold + italic (***text*** or ___text___)
  SyntaxRule boldItalicRule;
  boldItalicRule.pattern =
      QRegularExpression("\\*\\*\\*[^*]+\\*\\*\\*|___[^_]+___");
  boldItalicRule.name = "keyword_1";
  rules.append(boldItalicRule);

  // Bold text (**text** or __text__)
  SyntaxRule boldRule;
  boldRule.pattern = QRegularExpression("\\*\\*[^*]+\\*\\*|__[^_]+__");
  boldRule.name = "keyword_1";
  rules.append(boldRule);

  // Italic text (*text* or _text_)
  SyntaxRule italicRule;
  italicRule.pattern = QRegularExpression("(?<!\\*)\\*(?!\\*)([^*]+)\\*(?!\\*)"
                                          "|(?<!_)_(?!_)([^_]+)_(?!_)");
  italicRule.name = "keyword_2";
  rules.append(italicRule);

  // Strikethrough (~~text~~)
  SyntaxRule strikeRule;
  strikeRule.pattern = QRegularExpression("~~[^~]+~~");
  strikeRule.name = "comment";
  rules.append(strikeRule);

  // Highlight (==text==) — extended syntax
  SyntaxRule highlightRule;
  highlightRule.pattern = QRegularExpression("==[^=]+=={1,2}");
  highlightRule.name = "keyword_1";
  rules.append(highlightRule);

  // Subscript (~text~) — extended syntax
  SyntaxRule subscriptRule;
  subscriptRule.pattern = QRegularExpression("(?<!~)~(?!~)[^~]+~(?!~)");
  subscriptRule.name = "keyword_2";
  rules.append(subscriptRule);

  // Superscript (^text^) — extended syntax
  SyntaxRule superscriptRule;
  superscriptRule.pattern = QRegularExpression("\\^[^^]+\\^");
  superscriptRule.name = "keyword_2";
  rules.append(superscriptRule);

  // --- Code ---

  // Inline code with double backticks (``code``)
  SyntaxRule inlineCodeDoubleRule;
  inlineCodeDoubleRule.pattern = QRegularExpression("``[^`]+``");
  inlineCodeDoubleRule.name = "string";
  rules.append(inlineCodeDoubleRule);

  // Inline code (`code`)
  SyntaxRule inlineCodeRule;
  inlineCodeRule.pattern = QRegularExpression("`[^`]+`");
  inlineCodeRule.name = "string";
  rules.append(inlineCodeRule);

  // Fenced code block opening with language identifier (```lang)
  SyntaxRule fencedLangRule;
  fencedLangRule.pattern = QRegularExpression("^(`{3,}|~{3,})[A-Za-z0-9_+-]+");
  fencedLangRule.name = "string";
  rules.append(fencedLangRule);

  // --- Links and images ---

  // Image links ![alt](url "title")
  SyntaxRule imageRule;
  imageRule.pattern = QRegularExpression("!\\[[^\\]]*\\]\\([^)]*\\)");
  imageRule.name = "function";
  rules.append(imageRule);

  // Inline links [text](url "title")
  SyntaxRule linkRule;
  linkRule.pattern = QRegularExpression("\\[[^\\]]+\\]\\([^)]*\\)");
  linkRule.name = "function";
  rules.append(linkRule);

  // Reference links [text][ref]
  SyntaxRule refLinkRule;
  refLinkRule.pattern = QRegularExpression("\\[[^\\]]+\\]\\[[^\\]]*\\]");
  refLinkRule.name = "function";
  rules.append(refLinkRule);

  // Reference link definitions [ref]: url "title"
  SyntaxRule refDefRule;
  refDefRule.pattern = QRegularExpression("^\\s{0,3}\\[[^\\]]+\\]:\\s+\\S+.*$");
  refDefRule.name = "function";
  rules.append(refDefRule);

  // Autolinks <url> or <email>
  SyntaxRule autolinkRule;
  autolinkRule.pattern =
      QRegularExpression("<(https?://[^>]+|[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\."
                         "[A-Za-z]{2,})>");
  autolinkRule.name = "function";
  rules.append(autolinkRule);

  // Bare URLs (http:// or https://)
  SyntaxRule urlRule;
  urlRule.pattern = QRegularExpression("https?://[^\\s<>\"'\\)]+");
  urlRule.name = "string";
  rules.append(urlRule);

  // --- Block-level elements ---

  // Block quotes (> text, including nested >>)
  SyntaxRule quoteRule;
  quoteRule.pattern = QRegularExpression("^\\s{0,3}(>\\s?)+.*$");
  quoteRule.name = "comment";
  rules.append(quoteRule);

  // Unordered list items (* or - or +)
  SyntaxRule unorderedListRule;
  unorderedListRule.pattern = QRegularExpression("^\\s*[*+-]\\s");
  unorderedListRule.name = "keyword_2";
  rules.append(unorderedListRule);

  // Ordered list items (1. 2. etc.)
  SyntaxRule orderedListRule;
  orderedListRule.pattern = QRegularExpression("^\\s*\\d+[.)]\\s");
  orderedListRule.name = "keyword_2";
  rules.append(orderedListRule);

  // Task list checkboxes (- [ ] or - [x])
  SyntaxRule taskListRule;
  taskListRule.pattern = QRegularExpression(
      "^\\s*[*+-]\\s+\\[([ xX])\\]", QRegularExpression::CaseInsensitiveOption);
  taskListRule.name = "keyword_1";
  rules.append(taskListRule);

  // Horizontal rules (--- or *** or ___)
  SyntaxRule hrRule;
  hrRule.pattern = QRegularExpression("^\\s{0,3}([-*_]\\s*){3,}$");
  hrRule.name = "keyword_0";
  rules.append(hrRule);

  // --- Tables (extended syntax) ---

  // Table header separator (|---|---|)
  SyntaxRule tableSepRule;
  tableSepRule.pattern =
      QRegularExpression("^\\|?(\\s*:?-{3,}:?\\s*\\|)+\\s*:?-{3,}:?\\s*\\|?$");
  tableSepRule.name = "keyword_0";
  rules.append(tableSepRule);

  // Table pipe delimiters
  SyntaxRule tablePipeRule;
  tablePipeRule.pattern = QRegularExpression("\\|");
  tablePipeRule.name = "keyword_2";
  rules.append(tablePipeRule);

  // --- Footnotes (extended syntax) ---

  // Footnote reference [^id]
  SyntaxRule footnoteRefRule;
  footnoteRefRule.pattern = QRegularExpression("\\[\\^[^\\]]+\\]");
  footnoteRefRule.name = "function";
  rules.append(footnoteRefRule);

  // Footnote definition [^id]: text
  SyntaxRule footnoteDefRule;
  footnoteDefRule.pattern = QRegularExpression("^\\[\\^[^\\]]+\\]:\\s.*$");
  footnoteDefRule.name = "function";
  rules.append(footnoteDefRule);

  // --- Definition lists (extended syntax) ---

  // Definition term prefix (: definition)
  SyntaxRule defListRule;
  defListRule.pattern = QRegularExpression("^:\\s+.+$");
  defListRule.name = "keyword_2";
  rules.append(defListRule);

  // --- Inline HTML ---

  // HTML tags within Markdown
  SyntaxRule htmlTagRule;
  htmlTagRule.pattern =
      QRegularExpression("</?[A-Za-z][A-Za-z0-9]*(?:\\s[^>]*)?\\/?>",
                         QRegularExpression::CaseInsensitiveOption);
  htmlTagRule.name = "keyword_2";
  rules.append(htmlTagRule);

  // HTML entities (&amp; etc.)
  SyntaxRule htmlEntityRule;
  htmlEntityRule.pattern = QRegularExpression("&[A-Za-z0-9#]+;");
  htmlEntityRule.name = "keyword_2";
  rules.append(htmlEntityRule);

  // --- Escape sequences ---

  // Backslash escapes (\* \_ \` etc.)
  SyntaxRule escapeRule;
  escapeRule.pattern = QRegularExpression("\\\\[\\\\`*_{}\\[\\]()#+\\-.!|~>]");
  escapeRule.name = "comment";
  rules.append(escapeRule);

  // --- Math (extended syntax) ---

  // Inline math ($expression$)
  SyntaxRule inlineMathRule;
  inlineMathRule.pattern =
      QRegularExpression("(?<!\\$)\\$(?!\\$)[^$\\n]+\\$(?!\\$)");
  inlineMathRule.name = "number";
  rules.append(inlineMathRule);

  // Display math ($$expression$$)
  SyntaxRule displayMathRule;
  displayMathRule.pattern = QRegularExpression("\\$\\$[^$]+\\$\\$");
  displayMathRule.name = "number";
  rules.append(displayMathRule);

  // --- Emoji shortcodes (extended syntax) ---

  // :emoji_name:
  SyntaxRule emojiRule;
  emojiRule.pattern = QRegularExpression(":[A-Za-z0-9_+-]+:");
  emojiRule.name = "string";
  rules.append(emojiRule);

  // --- YAML front matter delimiter ---

  // Opening/closing ---
  SyntaxRule frontMatterRule;
  frontMatterRule.pattern = QRegularExpression("^---\\s*$");
  frontMatterRule.name = "keyword_0";
  rules.append(frontMatterRule);

  return rules;
}

QVector<MultiLineBlock> MarkdownSyntaxPlugin::multiLineBlocks() const {
  QVector<MultiLineBlock> blocks;

  // Fenced code blocks (```)
  MultiLineBlock codeBlock;
  codeBlock.startPattern = QRegularExpression("^`{3,}");
  codeBlock.endPattern = QRegularExpression("^`{3,}\\s*$");
  blocks.append(codeBlock);

  // Fenced code blocks (~~~)
  MultiLineBlock tildeBlock;
  tildeBlock.startPattern = QRegularExpression("^~{3,}");
  tildeBlock.endPattern = QRegularExpression("^~{3,}\\s*$");
  blocks.append(tildeBlock);

  // HTML comments
  MultiLineBlock commentBlock;
  commentBlock.startPattern = QRegularExpression("<!--");
  commentBlock.endPattern = QRegularExpression("-->");
  blocks.append(commentBlock);

  // Display math blocks ($$...$$)
  MultiLineBlock mathBlock;
  mathBlock.startPattern = QRegularExpression("^\\$\\$\\s*$");
  mathBlock.endPattern = QRegularExpression("^\\$\\$\\s*$");
  blocks.append(mathBlock);

  return blocks;
}

QStringList MarkdownSyntaxPlugin::keywords() const {
  return {
      // Common Markdown syntax keywords for autocomplete
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
