#include "lightpadsyntaxhighlighter.h"
#include <QDebug>

const QString keyWords_Cpp_0 = ":/resources/highlight/Cpp/0.txt";
const QString keyWords_Cpp_1 = ":/resources/highlight/Cpp/1.txt";
const QString keyWords_Cpp_2 = ":/resources/highlight/Cpp/2.txt";
const QString keyWords_Js_0 = ":/resources/highlight/JavaScript/0.txt";
const QString keyWords_Js_1 = ":/resources/highlight/JavaScript/1.txt";
const QString keyWords_Js_2 = ":/resources/highlight/JavaScript/2.txt";
const QString keyWords_Py_0 = ":/resources/highlight/Python/0.txt";
const QString keyWords_Py_1 = ":/resources/highlight/Python/1.txt";
const QString keyWords_Py_2 = ":/resources/highlight/Python/2.txt";

QString cutEndOfLine(QString line) {
    if (line.size() > 2) {
        if (line.indexOf("\r") > 0 )
            return line.left(line.size() - 2);

        else
            return line.left(line.size() - 1);
    }

   return line;
}

static void loadkeywordPatterns(QStringList& keywordPatterns, QString path) {

    QFile TextFile(path);

    if (TextFile.open(QIODevice::ReadOnly)) {
        while (!TextFile.atEnd()) {
                QString line = TextFile.readLine();
                keywordPatterns.append("\\b" +  cutEndOfLine(line) + "\\b");
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

static void loadHighlightingRules(QVector<HighlightingRule>& highlightingRules, const QStringList& keywordPatterns_0, const QStringList& keywordPatterns_1, const QStringList& keywordPatterns_2, const QString& searchKeyword, QRegularExpression singleLineComment){
    QTextCharFormat keywordFormat;
    keywordFormat.setForeground(Qt::darkGreen);
    keywordFormat.setFontWeight(QFont::Bold);

    for (auto &pattern : keywordPatterns_0)
        highlightingRules.append(HighlightingRule(QRegularExpression(pattern), keywordFormat));

    QTextCharFormat keywordFormat_1;
    keywordFormat_1.setForeground(Qt::darkYellow);
    keywordFormat_1.setFontWeight(QFont::Bold);

    for (auto &pattern : keywordPatterns_1)
        highlightingRules.append(HighlightingRule(QRegularExpression(pattern), keywordFormat_1));

    QTextCharFormat keywordFormat_2;
    keywordFormat_2.setForeground(Qt::darkMagenta);

    for (auto &pattern : keywordPatterns_2)
        highlightingRules.append(HighlightingRule(QRegularExpression(pattern), keywordFormat_2));

    QTextCharFormat numberFormat;
    numberFormat.setForeground(Qt::darkYellow);
    highlightingRules.append(HighlightingRule(QRegularExpression(QStringLiteral("\\b[-+.,]*\\d{1,}f*\\b")), numberFormat));

    QTextCharFormat classFormat;

    classFormat.setForeground(Qt::darkMagenta);
    classFormat.setFontWeight(QFont::Bold);
    highlightingRules.append(HighlightingRule(QRegularExpression(QStringLiteral("\\bQ[A-Za-z]+\\b")), classFormat));

    QTextCharFormat quotationFormat;
    quotationFormat.setForeground(Qt::darkGreen);
    highlightingRules.append(HighlightingRule(QRegularExpression(QStringLiteral("\".*\"")), quotationFormat));

    QTextCharFormat functionFormat;
    functionFormat.setFontItalic(true);
    functionFormat.setForeground(Qt::blue);
    highlightingRules.append(HighlightingRule(QRegularExpression(QStringLiteral("\\b[A-Za-z0-9_]+(?=\\()")), functionFormat));

    QTextCharFormat singleLineCommentFormat;
    singleLineCommentFormat.setForeground(Qt::gray);
    highlightingRules.append(HighlightingRule(singleLineComment, singleLineCommentFormat));

    if (!searchKeyword.isEmpty()) {
        QTextCharFormat searchFormat;
        searchFormat.setBackground(QColor("#646464"));
        highlightingRules.append(HighlightingRule(QRegularExpression(searchKeyword,  QRegularExpression::CaseInsensitiveOption), searchFormat));
    }
}


QVector<HighlightingRule> highlightingRulesCpp(const QString& searchKeyword)
{
    QVector<HighlightingRule> highlightingRules;
    QStringList keywordPatterns_0;
    QStringList keywordPatterns_1;
    QStringList keywordPatterns_2;
    loadkeywordPatterns(keywordPatterns_0, keyWords_Cpp_0);
    loadkeywordPatterns(keywordPatterns_1, keyWords_Cpp_1);
    loadkeywordPatterns(keywordPatterns_2, keyWords_Cpp_2);

    if (!keywordPatterns_0.isEmpty() && !keywordPatterns_1.isEmpty() && !keywordPatterns_2.isEmpty())
        loadHighlightingRules(highlightingRules, keywordPatterns_0, keywordPatterns_1, keywordPatterns_2, searchKeyword, QRegularExpression(QStringLiteral("//[^\n]*")));

    return highlightingRules;
}

QVector<HighlightingRule> highlightingRulesJs(const QString& searchKeyword)
{
    QVector<HighlightingRule> highlightingRules;
    QStringList keywordPatterns_0;
    QStringList keywordPatterns_1;
    QStringList keywordPatterns_2;
    loadkeywordPatterns(keywordPatterns_0, keyWords_Js_0);
    loadkeywordPatterns(keywordPatterns_1, keyWords_Js_1);
    loadkeywordPatterns(keywordPatterns_2, keyWords_Js_2);

    if (!keywordPatterns_0.isEmpty() && !keywordPatterns_1.isEmpty() && !keywordPatterns_2.isEmpty())
        loadHighlightingRules(highlightingRules, keywordPatterns_0, keywordPatterns_1, keywordPatterns_2, searchKeyword, QRegularExpression(QStringLiteral("//[^\n]*")));

    return highlightingRules;
}

QVector<HighlightingRule> highlightingRulesPy(const QString& searchKeyword)
{
    QVector<HighlightingRule> highlightingRules;
    QStringList keywordPatterns_0;
    QStringList keywordPatterns_1;
    QStringList keywordPatterns_2;
    loadkeywordPatterns(keywordPatterns_0, keyWords_Py_0);
    loadkeywordPatterns(keywordPatterns_1, keyWords_Py_1);
    loadkeywordPatterns(keywordPatterns_2, keyWords_Py_2);

    if (!keywordPatterns_0.isEmpty() && !keywordPatterns_1.isEmpty() && !keywordPatterns_2.isEmpty())
        loadHighlightingRules(highlightingRules, keywordPatterns_0, keywordPatterns_1, keywordPatterns_2, searchKeyword, QRegularExpression(QStringLiteral("#[^\n]*")));

    return highlightingRules;
}

