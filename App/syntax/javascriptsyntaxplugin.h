#ifndef JAVASCRIPTSYNTAXPLUGIN_H
#define JAVASCRIPTSYNTAXPLUGIN_H

#include "basesyntaxplugin.h"
#include <QVector>

/**
 * @brief Built-in JavaScript syntax highlighting plugin
 */
class JavaScriptSyntaxPlugin : public BaseSyntaxPlugin {
public:
    QString languageId() const override { return "js"; }
    QString languageName() const override { return "JavaScript"; }
    QStringList fileExtensions() const override { return {"js", "jsx", "mjs", "cjs"}; }
    
    QVector<SyntaxRule> syntaxRules() const override;
    QVector<MultiLineBlock> multiLineBlocks() const override;
    QStringList keywords() const override;
    
    QPair<QString, QPair<QString, QString>> commentStyle() const override {
        return {"//", {"/*", "*/"}};
    }

private:
    static QStringList getPrimaryKeywords();
    static QStringList getSecondaryKeywords();
    static QStringList getTertiaryKeywords();
};

#endif // JAVASCRIPTSYNTAXPLUGIN_H
