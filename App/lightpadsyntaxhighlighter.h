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
    LightpadSyntaxHighlighter(QStringList patternList, QTextDocument* parent = nullptr);
    ~LightpadSyntaxHighlighter();
      //  void setKeywordPattern(QStringList patternList);

    protected:
        void highlightBlock(const QString &text) override;

    private:
        QVector<HighlightingRule> highlightingRules;
        QStringList keywordPatterns;
        QRegularExpression commentStartExpression;
        QRegularExpression commentEndExpression;
        QTextCharFormat keywordFormat;

        QTextCharFormat classFormat;
        QTextCharFormat singleLineCommentFormat;
        QTextCharFormat multiLineCommentFormat;
        QTextCharFormat quotationFormat;
        QTextCharFormat functionFormat;
};

#endif // LIGHTPADSYNTAXHIGHLIGHTER_H
