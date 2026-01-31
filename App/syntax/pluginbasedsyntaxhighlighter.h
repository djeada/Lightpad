#ifndef PLUGINBASEDSYNTAXHIGHLIGHTER_H
#define PLUGINBASEDSYNTAXHIGHLIGHTER_H

#include "../settings/theme.h"
#include "../plugins/isyntaxplugin.h"
#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QVector>

/**
 * @brief Plugin-based syntax highlighter
 * 
 * This is a modern, plugin-driven syntax highlighter that uses ISyntaxPlugin
 * to provide highlighting rules. It replaces the hardcoded language-specific
 * functions with a generic, data-driven approach.
 */
class PluginBasedSyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    /**
     * @brief Construct highlighter from a syntax plugin
     * @param plugin The syntax plugin providing highlighting rules
     * @param theme Color theme for syntax elements
     * @param searchKeyword Optional keyword to highlight in search results
     * @param parent Parent QTextDocument
     */
    PluginBasedSyntaxHighlighter(
        ISyntaxPlugin* plugin,
        const Theme& theme,
        const QString& searchKeyword = "",
        QTextDocument* parent = nullptr);

    /**
     * @brief Update the search keyword highlighting
     */
    void setSearchKeyword(const QString& keyword);

    /**
     * @brief Get the current search keyword
     */
    QString searchKeyword() const { return m_searchKeyword; }
    
    /**
     * @brief Set the visible block range for viewport-aware highlighting
     */
    void setVisibleBlockRange(int first, int last) { 
        m_firstVisibleBlock = first; 
        m_lastVisibleBlock = last; 
    }

protected:
    void highlightBlock(const QString& text) override;

private:
    /**
     * @brief Check if a block is within or near the visible viewport
     */
    bool isBlockVisible(int blockNumber) const;
    
    /**
     * @brief Convert plugin rules to internal format with theme colors
     */
    void loadRulesFromPlugin(ISyntaxPlugin* plugin, const Theme& theme);

    /**
     * @brief Apply theme colors to a format based on rule name/type
     */
    QTextCharFormat applyThemeToFormat(const SyntaxRule& rule, const Theme& theme);

    Theme m_theme;
    QString m_searchKeyword;
    
    // Single-line highlighting rules
    QVector<SyntaxRule> m_rules;
    
    // Multi-line blocks (comments, strings)
    QVector<MultiLineBlock> m_multiLineBlocks;
    
    // Search highlight format
    QTextCharFormat m_searchFormat;
    
    // Editor reference for viewport-aware highlighting
    int m_firstVisibleBlock = 0;
    int m_lastVisibleBlock = 1000;
    
    // Buffer around viewport for smooth scrolling
    static constexpr int VIEWPORT_BUFFER = 50;
};

#endif // PLUGINBASEDSYNTAXHIGHLIGHTER_H
