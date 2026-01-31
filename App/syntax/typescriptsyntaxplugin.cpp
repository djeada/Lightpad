#include "typescriptsyntaxplugin.h"
#include <QRegularExpression>

QStringList TypeScriptSyntaxPlugin::getPrimaryKeywords()
{
    return {
        "var", "let", "const", "function", "class", "interface", "type",
        "enum", "namespace", "module", "declare", "abstract", "implements",
        "extends", "public", "private", "protected", "readonly", "static"
    };
}

QStringList TypeScriptSyntaxPlugin::getSecondaryKeywords()
{
    return {
        "any", "boolean", "break", "case", "catch", "continue", "debugger",
        "default", "delete", "do", "else", "export", "false", "finally",
        "for", "from", "get", "if", "import", "in", "instanceof", "keyof",
        "new", "null", "number", "object", "of", "return", "set", "string",
        "super", "switch", "symbol", "this", "throw", "true", "try", "typeof",
        "undefined", "unknown", "void", "while", "with", "yield", "async", "await",
        "never", "bigint", "as", "is", "infer", "asserts"
    };
}

QStringList TypeScriptSyntaxPlugin::getTertiaryKeywords()
{
    return {
        "Array", "Boolean", "Date", "Error", "Function", "JSON", "Map",
        "Math", "Number", "Object", "Promise", "RegExp", "Set", "String",
        "Symbol", "WeakMap", "WeakSet", "console", "document", "window",
        "Partial", "Required", "Readonly", "Record", "Pick", "Omit",
        "Exclude", "Extract", "NonNullable", "Parameters", "ReturnType"
    };
}

QVector<SyntaxRule> TypeScriptSyntaxPlugin::syntaxRules() const
{
    QVector<SyntaxRule> rules;

    // Primary keywords
    for (const QString& keyword : getPrimaryKeywords()) {
        SyntaxRule rule;
        rule.pattern = QRegularExpression("\\b" + keyword + "\\b");
        rule.name = "keyword_0";
        rules.append(rule);
    }

    // Secondary keywords
    for (const QString& keyword : getSecondaryKeywords()) {
        SyntaxRule rule;
        rule.pattern = QRegularExpression("\\b" + keyword + "\\b");
        rule.name = "keyword_1";
        rules.append(rule);
    }

    // Tertiary keywords (built-in objects and utility types)
    for (const QString& keyword : getTertiaryKeywords()) {
        SyntaxRule rule;
        rule.pattern = QRegularExpression("\\b" + keyword + "\\b");
        rule.name = "keyword_2";
        rules.append(rule);
    }

    // Numbers
    SyntaxRule numberRule;
    numberRule.pattern = QRegularExpression("\\b[-+]?\\d[\\d_]*(\\.\\d+)?([eE][+-]?\\d+)?n?\\b");
    numberRule.name = "number";
    rules.append(numberRule);

    // String literals (double quotes)
    SyntaxRule stringRule;
    stringRule.pattern = QRegularExpression("\"[^\"]*\"");
    stringRule.name = "string";
    rules.append(stringRule);

    // String literals (single quotes)
    SyntaxRule singleQuoteRule;
    singleQuoteRule.pattern = QRegularExpression("'[^']*'");
    singleQuoteRule.name = "string";
    rules.append(singleQuoteRule);

    // Template literals (backticks)
    SyntaxRule templateRule;
    templateRule.pattern = QRegularExpression("`[^`]*`");
    templateRule.name = "string";
    rules.append(templateRule);

    // Function calls
    SyntaxRule functionRule;
    functionRule.pattern = QRegularExpression("\\b[A-Za-z_][A-Za-z0-9_]*(?=\\()");
    functionRule.name = "function";
    rules.append(functionRule);

    // Decorators
    SyntaxRule decoratorRule;
    decoratorRule.pattern = QRegularExpression("@[A-Za-z_][A-Za-z0-9_]*");
    decoratorRule.name = "keyword_1";
    rules.append(decoratorRule);

    // Single-line comments
    SyntaxRule commentRule;
    commentRule.pattern = QRegularExpression("//[^\n]*");
    commentRule.name = "comment";
    rules.append(commentRule);

    return rules;
}

QVector<MultiLineBlock> TypeScriptSyntaxPlugin::multiLineBlocks() const
{
    QVector<MultiLineBlock> blocks;

    // Multi-line comments
    MultiLineBlock commentBlock;
    commentBlock.startPattern = QRegularExpression("/\\*");
    commentBlock.endPattern = QRegularExpression("\\*/");
    blocks.append(commentBlock);

    return blocks;
}

QStringList TypeScriptSyntaxPlugin::keywords() const
{
    QStringList all;
    all << getPrimaryKeywords() << getSecondaryKeywords() << getTertiaryKeywords();
    return all;
}
