#ifndef LIGHTPADSYNTAXHIGHLIGHTER_H
#define LIGHTPADSYNTAXHIGHLIGHTER_H

#include "../settings/theme.h"
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTimer>
#include <QPlainTextEdit>

class HighlightingRule {

public:
    HighlightingRule(QRegularExpression pattern, QTextCharFormat format);
    QRegularExpression pattern;
    QTextCharFormat format;
};

class LightpadSyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    LightpadSyntaxHighlighter(QVector<HighlightingRule> highlightingRules, QRegularExpression commentStartExpression, QRegularExpression commentEndExpression, QTextDocument* parent = nullptr);
    
    // Set the editor to enable viewport-aware highlighting
    void setEditor(QPlainTextEdit* editor) { m_editor = editor; }

protected:
    void highlightBlock(const QString& text) override;

private:
    bool isBlockVisible(int blockNumber) const;
    
    Theme colors;
    QVector<HighlightingRule> highlightingRules;
    QRegularExpression commentStartExpression;
    QRegularExpression commentEndExpression;
    QTextCharFormat multiLineCommentFormat;
    QPlainTextEdit* m_editor = nullptr;
    
    // For deferred highlighting of off-screen blocks
    static constexpr int VIEWPORT_BUFFER = 50; // Extra lines around viewport
};

private:
    Theme colors;
    QVector<HighlightingRule> highlightingRules;
    QRegularExpression commentStartExpression;
    QRegularExpression commentEndExpression;
    QTextCharFormat multiLineCommentFormat;
};

QString cutEndOfLine(QString line);
QVector<HighlightingRule> highlightingRulesCpp(Theme colors, const QString& searchKeyword = "");
QVector<HighlightingRule> highlightingRulesJs(Theme colors, const QString& searchKeyword = "");
QVector<HighlightingRule> highlightingRulesPy(Theme colors, const QString& searchKeyword = "");

#endif // LIGHTPADSYNTAXHIGHLIGHTER_H
