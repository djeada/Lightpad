#include "htmlsyntaxplugin.h"
#include <QRegularExpression>

QStringList HtmlSyntaxPlugin::getTags()
{
    return {
        "html", "head", "body", "title", "meta", "link", "script", "style",
        "div", "span", "p", "a", "img", "ul", "ol", "li", "table", "tr", "td", "th",
        "form", "input", "button", "select", "option", "textarea", "label",
        "header", "footer", "nav", "main", "section", "article", "aside",
        "h1", "h2", "h3", "h4", "h5", "h6", "br", "hr", "pre", "code",
        "strong", "em", "b", "i", "u", "small", "sub", "sup",
        "iframe", "video", "audio", "source", "canvas", "svg"
    };
}

QStringList HtmlSyntaxPlugin::getAttributes()
{
    return {
        "id", "class", "style", "src", "href", "alt", "title", "type", "name",
        "value", "placeholder", "disabled", "readonly", "required", "checked",
        "selected", "multiple", "action", "method", "target", "rel", "width",
        "height", "colspan", "rowspan", "data", "role", "aria", "tabindex"
    };
}

QVector<SyntaxRule> HtmlSyntaxPlugin::syntaxRules() const
{
    QVector<SyntaxRule> rules;

    // Tag names (in angle brackets)
    for (const QString& tag : getTags()) {
        SyntaxRule rule;
        rule.pattern = QRegularExpression("</?\\s*" + tag + "\\b", QRegularExpression::CaseInsensitiveOption);
        rule.name = "keyword_0";
        rules.append(rule);
    }

    // Attribute names
    for (const QString& attr : getAttributes()) {
        SyntaxRule rule;
        rule.pattern = QRegularExpression("\\b" + attr + "(?=\\s*=)", QRegularExpression::CaseInsensitiveOption);
        rule.name = "keyword_1";
        rules.append(rule);
    }

    // Closing angle brackets
    SyntaxRule closeTagRule;
    closeTagRule.pattern = QRegularExpression("/?>");
    closeTagRule.name = "keyword_0";
    rules.append(closeTagRule);

    // Attribute values (double quotes)
    SyntaxRule stringRule;
    stringRule.pattern = QRegularExpression("\"[^\"]*\"");
    stringRule.name = "string";
    rules.append(stringRule);

    // Attribute values (single quotes)
    SyntaxRule singleQuoteRule;
    singleQuoteRule.pattern = QRegularExpression("'[^']*'");
    singleQuoteRule.name = "string";
    rules.append(singleQuoteRule);

    // DOCTYPE
    SyntaxRule doctypeRule;
    doctypeRule.pattern = QRegularExpression("<!DOCTYPE[^>]*>", QRegularExpression::CaseInsensitiveOption);
    doctypeRule.name = "keyword_2";
    rules.append(doctypeRule);

    // HTML entities
    SyntaxRule entityRule;
    entityRule.pattern = QRegularExpression("&[A-Za-z0-9#]+;");
    entityRule.name = "keyword_2";
    rules.append(entityRule);

    return rules;
}

QVector<MultiLineBlock> HtmlSyntaxPlugin::multiLineBlocks() const
{
    QVector<MultiLineBlock> blocks;

    // HTML comments
    MultiLineBlock commentBlock;
    commentBlock.startPattern = QRegularExpression("<!--");
    commentBlock.endPattern = QRegularExpression("-->");
    blocks.append(commentBlock);

    return blocks;
}

QStringList HtmlSyntaxPlugin::keywords() const
{
    QStringList all;
    all << getTags() << getAttributes();
    return all;
}
