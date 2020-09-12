#include "lightpadsyntaxhighlighter.h"
#include <QDebug>

const QString keyWords_Cpp_1 = ":/resources/highlight/Cpp/0.txt";

static void loadKeyWordPatterns(QStringList& keywordPatterns, QString path) {

    QFile TextFile(path);

    if (TextFile.open(QIODevice::ReadOnly)) {
        while (!TextFile.atEnd()) {
                QString line = TextFile.readLine();
                if (line.size() > 3) {
                    if ( line.indexOf("\r") > 0 )
                        keywordPatterns.append("\\b" + line.left(line.size() - 2) + "\\b");

                    else
                        keywordPatterns.append("\\b" + line.left(line.size() - 1) + "\\b");
                }
       }

        TextFile.close();
    }
}

HighlightingRule::HighlightingRule(QRegularExpression pattern, QTextCharFormat format) :
    pattern(pattern),
    format(format) {
}

LightpadSyntaxHighlighter::LightpadSyntaxHighlighter(QVector<HighlightingRule> highlightingRules, QRegularExpression commentStartExpression, QRegularExpression commentEndExpression, QTextDocument* parent):
    QSyntaxHighlighter(parent),
    highlightingRules(highlightingRules),
    commentStartExpression(commentStartExpression),
    commentEndExpression(commentEndExpression) {
        multiLineCommentFormat.setForeground(Qt::gray);
}

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

QVector<HighlightingRule> highlightingRulesCpp(QString searchKeyword)
{
    QVector<HighlightingRule> highlightingRules;
    QStringList keywordPatterns;
    loadKeyWordPatterns(keywordPatterns, keyWords_Cpp_1);

    if (!keywordPatterns.isEmpty()) {

        QTextCharFormat keywordFormat;
        QTextCharFormat classFormat;
        QTextCharFormat singleLineCommentFormat;
        QTextCharFormat quotationFormat;
        QTextCharFormat functionFormat;

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

        singleLineCommentFormat.setForeground(Qt::gray);
        highlightingRules.append(HighlightingRule(QRegularExpression(QStringLiteral("//[^\n]*")), singleLineCommentFormat));

        if (!searchKeyword.isEmpty()) {
            QTextCharFormat searchFormat;
            searchFormat.setBackground(QColor("#646464"));
            highlightingRules.append(HighlightingRule(QRegularExpression(searchKeyword,  QRegularExpression::CaseInsensitiveOption), searchFormat));
        }
    }

    return highlightingRules;
}
