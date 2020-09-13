#ifndef LIGHTPADSYNTAXHIGHLIGHTER_H
#define LIGHTPADSYNTAXHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QRegularExpression>

class HighlightingRule {

    public:
        HighlightingRule(QRegularExpression pattern, QTextCharFormat format);
        QRegularExpression pattern;
        QTextCharFormat format;
};

class LightpadSyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

    public:
        LightpadSyntaxHighlighter(QVector<HighlightingRule> highlightingRules, QRegularExpression commentStartExpression, QRegularExpression commentEndExpression, QTextDocument* parent = nullptr);

    protected:
        void highlightBlock(const QString &text) override;

    private:
        QVector<HighlightingRule> highlightingRules;
        QRegularExpression commentStartExpression;
        QRegularExpression commentEndExpression;
        QTextCharFormat multiLineCommentFormat;
};

QVector<HighlightingRule> highlightingRulesCpp(const QString& searchKeyword = "");
QVector<HighlightingRule> highlightingRulesJs(const QString& searchKeyword = "");
QVector<HighlightingRule> highlightingRulesPy(const QString& searchKeyword = "");

#endif // LIGHTPADSYNTAXHIGHLIGHTER_H
