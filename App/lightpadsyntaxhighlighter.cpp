#include "lightpadsyntaxhighlighter.h"

#include <QDebug>

HighlightingRule::HighlightingRule(QRegularExpression pattern, QTextCharFormat format) :
    pattern(pattern),
    format(format) {

}

LightpadSyntaxHighlighter::LightpadSyntaxHighlighter(QStringList patternList, QTextDocument* parent):
    QSyntaxHighlighter(parent),
    keywordPatterns(patternList)
{

    keywordFormat.setForeground(Qt::darkBlue);
    keywordFormat.setFontWeight(QFont::Bold);

    for (auto &pattern : keywordPatterns)
        highlightingRules.append(HighlightingRule(QRegularExpression(pattern), keywordFormat));

    classFormat.setForeground(Qt::darkMagenta);
    classFormat.setFontWeight(QFont::Bold);
    highlightingRules.append(HighlightingRule(QRegularExpression(QStringLiteral("\\bQ[A-Za-z]+\\b")), classFormat));

    quotationFormat.setForeground(Qt::darkGreen);
    highlightingRules.append(HighlightingRule(QRegularExpression(QStringLiteral("\".*\"")), quotationFormat));

    functionFormat.setFontItalic(true);
    functionFormat.setForeground(Qt::blue);
    highlightingRules.append(HighlightingRule(QRegularExpression(QStringLiteral("\\b[A-Za-z0-9_]+(?=\\()")), functionFormat));

    singleLineCommentFormat.setForeground(Qt::red);

    highlightingRules.append(HighlightingRule(QRegularExpression(QStringLiteral("//[^\n]*")), singleLineCommentFormat));

    multiLineCommentFormat.setForeground(Qt::red);
    commentStartExpression = QRegularExpression(QStringLiteral("/\\*"));
    commentEndExpression = QRegularExpression(QStringLiteral("\\*/"));
}

/*
void LightpadSyntaxHighlighter::setKeywordPattern(QStringList patternList)
{
    keywordPatterns = patternList;
}*/

void LightpadSyntaxHighlighter::highlightBlock(const QString &text) {

    for (const HighlightingRule &rule : qAsConst(highlightingRules)) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);

        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

   setCurrentBlockState(0);

   int startIndex = 0;
   if (previousBlockState() != 1)
       startIndex = text.indexOf(commentStartExpression);

   while (startIndex >= 0) {
       QRegularExpressionMatch match = commentEndExpression.match(text, startIndex);
       int endIndex = match.capturedStart();
       int commentLength = 0;

       if (endIndex == -1) {
           setCurrentBlockState(1);
           commentLength = text.length() - startIndex;
       }

       else
           commentLength = endIndex - startIndex + match.capturedLength();

       setFormat(startIndex, commentLength, multiLineCommentFormat);
       startIndex = text.indexOf(commentStartExpression, startIndex + commentLength);
   }
}
