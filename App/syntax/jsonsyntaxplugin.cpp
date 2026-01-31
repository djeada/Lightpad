#include "jsonsyntaxplugin.h"
#include <QRegularExpression>

QVector<SyntaxRule> JsonSyntaxPlugin::syntaxRules() const
{
    QVector<SyntaxRule> rules;

    // Object keys (quoted strings before colon)
    SyntaxRule keyRule;
    keyRule.pattern = QRegularExpression("\"[^\"]*\"(?=\\s*:)");
    keyRule.name = "keyword_0";
    rules.append(keyRule);

    // Boolean values
    SyntaxRule boolRule;
    boolRule.pattern = QRegularExpression("\\b(true|false)\\b");
    boolRule.name = "keyword_1";
    rules.append(boolRule);

    // Null value
    SyntaxRule nullRule;
    nullRule.pattern = QRegularExpression("\\bnull\\b");
    nullRule.name = "keyword_2";
    rules.append(nullRule);

    // Numbers
    SyntaxRule numberRule;
    numberRule.pattern = QRegularExpression("-?\\d+(\\.\\d+)?([eE][+-]?\\d+)?");
    numberRule.name = "number";
    rules.append(numberRule);

    // String values
    SyntaxRule stringRule;
    stringRule.pattern = QRegularExpression("\"[^\"]*\"");
    stringRule.name = "string";
    rules.append(stringRule);

    // JSONC comments (single line)
    SyntaxRule commentRule;
    commentRule.pattern = QRegularExpression("//[^\n]*");
    commentRule.name = "comment";
    rules.append(commentRule);

    return rules;
}

QVector<MultiLineBlock> JsonSyntaxPlugin::multiLineBlocks() const
{
    QVector<MultiLineBlock> blocks;

    // JSONC multi-line comments
    MultiLineBlock commentBlock;
    commentBlock.startPattern = QRegularExpression("/\\*");
    commentBlock.endPattern = QRegularExpression("\\*/");
    blocks.append(commentBlock);

    return blocks;
}

QStringList JsonSyntaxPlugin::keywords() const
{
    return {"true", "false", "null"};
}
