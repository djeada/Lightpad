#include "markdownsyntaxplugin.h"
#include <QRegularExpression>

QVector<SyntaxRule> MarkdownSyntaxPlugin::syntaxRules() const
{
    QVector<SyntaxRule> rules;

    // Headers (# ## ### etc.)
    SyntaxRule headerRule;
    headerRule.pattern = QRegularExpression("^#{1,6}\\s.*$");
    headerRule.name = "keyword_0";
    rules.append(headerRule);

    // Bold text (**text** or __text__)
    SyntaxRule boldRule;
    boldRule.pattern = QRegularExpression("\\*\\*[^*]+\\*\\*|__[^_]+__");
    boldRule.name = "keyword_1";
    rules.append(boldRule);

    // Italic text (*text* or _text_)
    SyntaxRule italicRule;
    italicRule.pattern = QRegularExpression("\\*[^*]+\\*|_[^_]+_");
    italicRule.name = "keyword_2";
    rules.append(italicRule);

    // Inline code (`code`)
    SyntaxRule inlineCodeRule;
    inlineCodeRule.pattern = QRegularExpression("`[^`]+`");
    inlineCodeRule.name = "string";
    rules.append(inlineCodeRule);

    // Links [text](url)
    SyntaxRule linkRule;
    linkRule.pattern = QRegularExpression("\\[[^\\]]+\\]\\([^)]+\\)");
    linkRule.name = "function";
    rules.append(linkRule);

    // Image links ![alt](url)
    SyntaxRule imageRule;
    imageRule.pattern = QRegularExpression("!\\[[^\\]]*\\]\\([^)]+\\)");
    imageRule.name = "function";
    rules.append(imageRule);

    // Reference links [text][ref]
    SyntaxRule refLinkRule;
    refLinkRule.pattern = QRegularExpression("\\[[^\\]]+\\]\\[[^\\]]*\\]");
    refLinkRule.name = "function";
    rules.append(refLinkRule);

    // Block quotes (> text)
    SyntaxRule quoteRule;
    quoteRule.pattern = QRegularExpression("^>.*$");
    quoteRule.name = "comment";
    rules.append(quoteRule);

    // Unordered list items (* or - or +)
    SyntaxRule unorderedListRule;
    unorderedListRule.pattern = QRegularExpression("^\\s*[*+-]\\s");
    unorderedListRule.name = "keyword_2";
    rules.append(unorderedListRule);

    // Ordered list items (1. 2. etc.)
    SyntaxRule orderedListRule;
    orderedListRule.pattern = QRegularExpression("^\\s*\\d+\\.\\s");
    orderedListRule.name = "keyword_2";
    rules.append(orderedListRule);

    // Horizontal rules (--- or *** or ___)
    SyntaxRule hrRule;
    hrRule.pattern = QRegularExpression("^(---+|\\*\\*\\*+|___+)$");
    hrRule.name = "keyword_0";
    rules.append(hrRule);

    // Strikethrough (~~text~~)
    SyntaxRule strikeRule;
    strikeRule.pattern = QRegularExpression("~~[^~]+~~");
    strikeRule.name = "comment";
    rules.append(strikeRule);

    // URLs (http:// or https://)
    SyntaxRule urlRule;
    urlRule.pattern = QRegularExpression("https?://[^\\s<>\"']+");
    urlRule.name = "string";
    rules.append(urlRule);

    return rules;
}

QVector<MultiLineBlock> MarkdownSyntaxPlugin::multiLineBlocks() const
{
    QVector<MultiLineBlock> blocks;

    // Fenced code blocks (```)
    MultiLineBlock codeBlock;
    codeBlock.startPattern = QRegularExpression("^```");
    codeBlock.endPattern = QRegularExpression("^```");
    blocks.append(codeBlock);

    // HTML comments
    MultiLineBlock commentBlock;
    commentBlock.startPattern = QRegularExpression("<!--");
    commentBlock.endPattern = QRegularExpression("-->");
    blocks.append(commentBlock);

    return blocks;
}

QStringList MarkdownSyntaxPlugin::keywords() const
{
    return {};
}
