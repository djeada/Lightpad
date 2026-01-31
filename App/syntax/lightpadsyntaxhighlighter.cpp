#include "lightpadsyntaxhighlighter.h"
#include <QDebug>
#include <QFile>

const QString keyWords_Cpp_0 = ":/resources/highlight/Cpp/0.txt";
const QString keyWords_Cpp_1 = ":/resources/highlight/Cpp/1.txt";
const QString keyWords_Cpp_2 = ":/resources/highlight/Cpp/2.txt";
const QString keyWords_Js_0 = ":/resources/highlight/JavaScript/0.txt";
const QString keyWords_Js_1 = ":/resources/highlight/JavaScript/1.txt";
const QString keyWords_Js_2 = ":/resources/highlight/JavaScript/2.txt";
const QString keyWords_Py_0 = ":/resources/highlight/Python/0.txt";
const QString keyWords_Py_1 = ":/resources/highlight/Python/1.txt";
const QString keyWords_Py_2 = ":/resources/highlight/Python/2.txt";

QString cutEndOfLine(QString line)
{
    if (line.size() > 2) {
        if (line.indexOf("\r") > 0)
            return line.left(line.size() - 2);

        else
            return line.left(line.size() - 1);
    }

    return line;
}

static void loadkeywordPatterns(QStringList& keywordPatterns, QString path)
{

    QFile TextFile(path);

    if (TextFile.open(QIODevice::ReadOnly)) {
        while (!TextFile.atEnd()) {
            QString line = TextFile.readLine();
            keywordPatterns.append("\\b" + cutEndOfLine(line) + "\\b");
        }

        TextFile.close();
    }
}

HighlightingRule::HighlightingRule(QRegularExpression pattern, QTextCharFormat format)
    : pattern(pattern)
    , format(format)
{
}

LightpadSyntaxHighlighter::LightpadSyntaxHighlighter(QVector<HighlightingRule> highlightingRules, QRegularExpression commentStartExpression, QRegularExpression commentEndExpression, QTextDocument* parent)
    : QSyntaxHighlighter(parent)
    , highlightingRules(highlightingRules)
    , commentStartExpression(commentStartExpression)
    , commentEndExpression(commentEndExpression)
    , m_firstVisibleBlock(0)
    , m_lastVisibleBlock(1000)
{
    multiLineCommentFormat.setForeground(Qt::gray);
}

bool LightpadSyntaxHighlighter::isBlockVisible(int blockNumber) const
{
    // Include buffer around viewport for smooth scrolling
    int minBlock = m_firstVisibleBlock - VIEWPORT_BUFFER;
    int maxBlock = m_lastVisibleBlock + VIEWPORT_BUFFER;
    
    return (blockNumber >= minBlock && blockNumber <= maxBlock);
}

void LightpadSyntaxHighlighter::highlightBlock(const QString& text)
{
    // Skip highlighting for blocks far outside the viewport
    // This dramatically improves performance for large files
    int blockNum = currentBlock().blockNumber();
    if (!isBlockVisible(blockNum)) {
        // Just set the block state for multi-line comment tracking
        // but skip the expensive regex matching
        setCurrentBlockState(previousBlockState());
        return;
    }

    for (const HighlightingRule& rule : qAsConst(highlightingRules)) {
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

static void loadHighlightingRules(QVector<HighlightingRule>& highlightingRules, const QStringList& keywordPatterns_0, const QStringList& keywordPatterns_1, const QStringList& keywordPatterns_2, const QString& searchKeyword, QRegularExpression singleLineComment, Theme colors)
{
    QTextCharFormat keywordFormat;
    keywordFormat.setForeground(colors.keywordFormat_0);
    keywordFormat.setFontWeight(QFont::Bold);

    for (auto& pattern : keywordPatterns_0)
        highlightingRules.append(HighlightingRule(QRegularExpression(pattern), keywordFormat));

    QTextCharFormat keywordFormat_1;
    keywordFormat_1.setForeground(colors.keywordFormat_1);
    keywordFormat_1.setFontWeight(QFont::Bold);

    for (auto& pattern : keywordPatterns_1)
        highlightingRules.append(HighlightingRule(QRegularExpression(pattern), keywordFormat_1));

    QTextCharFormat keywordFormat_2;
    keywordFormat_2.setForeground(colors.keywordFormat_2);

    for (auto& pattern : keywordPatterns_2)
        highlightingRules.append(HighlightingRule(QRegularExpression(pattern), keywordFormat_2));

    QTextCharFormat numberFormat;
    numberFormat.setForeground(colors.numberFormat);
    highlightingRules.append(HighlightingRule(QRegularExpression(QStringLiteral("\\b[-+.,]*\\d{1,}f*\\b")), numberFormat));

    QTextCharFormat classFormat;

    classFormat.setForeground(colors.classFormat);
    classFormat.setFontWeight(QFont::Bold);
    highlightingRules.append(HighlightingRule(QRegularExpression(QStringLiteral("\\bQ[A-Za-z]+\\b")), classFormat));

    QTextCharFormat quotationFormat;
    quotationFormat.setForeground(colors.quotationFormat);
    highlightingRules.append(HighlightingRule(QRegularExpression(QStringLiteral("\".*\"")), quotationFormat));

    QTextCharFormat functionFormat;
    functionFormat.setFontItalic(true);
    functionFormat.setForeground(colors.functionFormat);
    highlightingRules.append(HighlightingRule(QRegularExpression(QStringLiteral("\\b[A-Za-z0-9_]+(?=\\()")), functionFormat));

    QTextCharFormat singleLineCommentFormat;
    singleLineCommentFormat.setForeground(colors.singleLineCommentFormat);
    highlightingRules.append(HighlightingRule(singleLineComment, singleLineCommentFormat));

    if (!searchKeyword.isEmpty()) {
        QTextCharFormat searchFormat;
        searchFormat.setBackground(QColor("#646464"));
        highlightingRules.append(HighlightingRule(QRegularExpression(searchKeyword, QRegularExpression::CaseInsensitiveOption), searchFormat));
    }
}

QVector<HighlightingRule> highlightingRulesCpp(Theme colors, const QString& searchKeyword)
{
    QVector<HighlightingRule> highlightingRules;
    QStringList keywordPatterns_0;
    QStringList keywordPatterns_1;
    QStringList keywordPatterns_2;
    loadkeywordPatterns(keywordPatterns_0, keyWords_Cpp_0);
    loadkeywordPatterns(keywordPatterns_1, keyWords_Cpp_1);
    loadkeywordPatterns(keywordPatterns_2, keyWords_Cpp_2);

    if (!keywordPatterns_0.isEmpty() && !keywordPatterns_1.isEmpty() && !keywordPatterns_2.isEmpty())
        loadHighlightingRules(highlightingRules, keywordPatterns_0, keywordPatterns_1, keywordPatterns_2, searchKeyword, QRegularExpression(QStringLiteral("//[^\n]*")), colors);

    return highlightingRules;
}

QVector<HighlightingRule> highlightingRulesJs(Theme colors, const QString& searchKeyword)
{
    QVector<HighlightingRule> highlightingRules;
    QStringList keywordPatterns_0;
    QStringList keywordPatterns_1;
    QStringList keywordPatterns_2;
    loadkeywordPatterns(keywordPatterns_0, keyWords_Js_0);
    loadkeywordPatterns(keywordPatterns_1, keyWords_Js_1);
    loadkeywordPatterns(keywordPatterns_2, keyWords_Js_2);

    if (!keywordPatterns_0.isEmpty() && !keywordPatterns_1.isEmpty() && !keywordPatterns_2.isEmpty())
        loadHighlightingRules(highlightingRules, keywordPatterns_0, keywordPatterns_1, keywordPatterns_2, searchKeyword, QRegularExpression(QStringLiteral("//[^\n]*")), colors);

    return highlightingRules;
}

QVector<HighlightingRule> highlightingRulesPy(Theme colors, const QString& searchKeyword)
{
    QVector<HighlightingRule> highlightingRules;
    QStringList keywordPatterns_0;
    QStringList keywordPatterns_1;
    QStringList keywordPatterns_2;
    loadkeywordPatterns(keywordPatterns_0, keyWords_Py_0);
    loadkeywordPatterns(keywordPatterns_1, keyWords_Py_1);
    loadkeywordPatterns(keywordPatterns_2, keyWords_Py_2);

    if (!keywordPatterns_0.isEmpty() && !keywordPatterns_1.isEmpty() && !keywordPatterns_2.isEmpty())
        loadHighlightingRules(highlightingRules, keywordPatterns_0, keywordPatterns_1, keywordPatterns_2, searchKeyword, QRegularExpression(QStringLiteral("#[^\n]*")), colors);

    return highlightingRules;
}
