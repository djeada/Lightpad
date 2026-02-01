#include "pluginbasedsyntaxhighlighter.h"
#include "../core/logging/logger.h"

PluginBasedSyntaxHighlighter::PluginBasedSyntaxHighlighter(
    ISyntaxPlugin* plugin,
    const Theme& theme,
    const QString& searchKeyword,
    QTextDocument* parent)
    : QSyntaxHighlighter(parent)
    , m_theme(theme)
    , m_searchKeyword(searchKeyword)
    , m_firstVisibleBlock(0)
    , m_lastVisibleBlock(1000)
{
    if (!plugin) {
        Logger::instance().warning("PluginBasedSyntaxHighlighter created with null plugin");
        return;
    }

    loadRulesFromPlugin(plugin, theme);

    // Setup search highlight format
    m_searchFormat.setBackground(QColor("#646464"));
}

void PluginBasedSyntaxHighlighter::setSearchKeyword(const QString& keyword)
{
    m_searchKeyword = keyword;
    rehighlight();
}

void PluginBasedSyntaxHighlighter::loadRulesFromPlugin(ISyntaxPlugin* plugin, const Theme& theme)
{
    if (!plugin) {
        return;
    }

    // Load syntax rules from plugin
    m_rules = plugin->syntaxRules();
    
    // Apply theme colors to each rule
    for (SyntaxRule& rule : m_rules) {
        rule.format = applyThemeToFormat(rule, theme);
    }

    // Load multi-line blocks
    m_multiLineBlocks = plugin->multiLineBlocks();
    
    // Apply theme colors to multi-line blocks
    for (MultiLineBlock& block : m_multiLineBlocks) {
        // Use single-line comment color for multi-line comments by default
        block.format.setForeground(theme.singleLineCommentFormat);
    }

    Logger::instance().info(QString("Loaded %1 rules and %2 multi-line blocks from plugin '%3'")
        .arg(m_rules.size())
        .arg(m_multiLineBlocks.size())
        .arg(plugin->languageName()));
}

QTextCharFormat PluginBasedSyntaxHighlighter::applyThemeToFormat(const SyntaxRule& rule, const Theme& theme)
{
    QTextCharFormat format = rule.format;
    QString ruleName = rule.name.toLower();

    // Apply colors based on rule name/type
    if (ruleName.contains("keyword") || ruleName.contains("preprocessor") || ruleName.contains("directive")) {
        if (ruleName.contains("0") || ruleName.contains("primary")) {
            format.setForeground(theme.keywordFormat_0);
            format.setFontWeight(QFont::Bold);
        } else if (ruleName.contains("1") || ruleName.contains("secondary")) {
            format.setForeground(theme.keywordFormat_1);
            format.setFontWeight(QFont::Bold);
        } else if (ruleName.contains("2") || ruleName.contains("tertiary")) {
            format.setForeground(theme.keywordFormat_2);
        } else {
            format.setForeground(theme.keywordFormat_0);
            format.setFontWeight(QFont::Bold);
        }
    } else if (ruleName.contains("number")) {
        format.setForeground(theme.numberFormat);
    } else if (ruleName.contains("string") || ruleName.contains("quotation")) {
        format.setForeground(theme.quotationFormat);
    } else if (ruleName.contains("comment")) {
        format.setForeground(theme.singleLineCommentFormat);
    } else if (ruleName.contains("function")) {
        format.setForeground(theme.functionFormat);
        format.setFontItalic(true);
    } else if (ruleName.contains("class") || ruleName.contains("type")
               || ruleName.contains("scope") || ruleName.contains("scoped")) {
        format.setForeground(theme.classFormat);
        format.setFontWeight(QFont::Bold);
    }

    return format;
}

void PluginBasedSyntaxHighlighter::highlightBlock(const QString& text)
{
    // Fast path: skip empty lines
    if (text.isEmpty()) {
        setCurrentBlockState(previousBlockState());
        return;
    }
    
    // Skip highlighting for blocks far outside the viewport
    // This dramatically improves performance for large files
    int blockNum = currentBlock().blockNumber();
    if (!isBlockVisible(blockNum)) {
        // Just set the block state for multi-line tracking
        // but skip the expensive regex matching
        setCurrentBlockState(previousBlockState());
        return;
    }
    
    // Apply single-line rules
    for (const SyntaxRule& rule : m_rules) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    // Apply multi-line blocks (comments, strings, etc.)
    for (const MultiLineBlock& block : m_multiLineBlocks) {
        setCurrentBlockState(0);

        int startIndex = 0;
        if (previousBlockState() != 1) {
            startIndex = text.indexOf(block.startPattern);
        }

        while (startIndex >= 0) {
            QRegularExpressionMatch match = block.endPattern.match(text, startIndex);
            int endIndex = match.capturedStart();
            int blockLength = 0;

            if (endIndex == -1) {
                setCurrentBlockState(1);
                blockLength = text.length() - startIndex;
            } else {
                blockLength = endIndex - startIndex + match.capturedLength();
            }

            setFormat(startIndex, blockLength, block.format);
            startIndex = text.indexOf(block.startPattern, startIndex + blockLength);
        }
    }

    // Apply search keyword highlighting (on top of everything else)
    if (!m_searchKeyword.isEmpty()) {
        QRegularExpression searchPattern(m_searchKeyword, QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatchIterator matchIterator = searchPattern.globalMatch(text);
        
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), m_searchFormat);
        }
    }
}

bool PluginBasedSyntaxHighlighter::isBlockVisible(int blockNumber) const
{
    // Include buffer around viewport for smooth scrolling
    int minBlock = m_firstVisibleBlock - VIEWPORT_BUFFER;
    int maxBlock = m_lastVisibleBlock + VIEWPORT_BUFFER;
    
    return (blockNumber >= minBlock && blockNumber <= maxBlock);
}
