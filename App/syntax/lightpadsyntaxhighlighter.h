#ifndef LIGHTPADSYNTAXHIGHLIGHTER_H
#define LIGHTPADSYNTAXHIGHLIGHTER_H

#include "../settings/theme.h"
#include <QRegularExpression>
#include <QSyntaxHighlighter>

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

protected:
    void highlightBlock(const QString& text) override;

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
