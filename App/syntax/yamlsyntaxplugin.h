#ifndef YAMLSYNTAXPLUGIN_H
#define YAMLSYNTAXPLUGIN_H

#include "basesyntaxplugin.h"
#include <QVector>

/**
 * @brief Built-in YAML syntax highlighting plugin
 */
class YamlSyntaxPlugin : public BaseSyntaxPlugin {
public:
    QString languageId() const override { return "yaml"; }
    QString languageName() const override { return "YAML"; }
    QStringList fileExtensions() const override { return {"yaml", "yml"}; }
    
    QVector<SyntaxRule> syntaxRules() const override;
    QVector<MultiLineBlock> multiLineBlocks() const override;
    QStringList keywords() const override;
    
    QPair<QString, QPair<QString, QString>> commentStyle() const override {
        return {"#", {"", ""}};
    }
};

#endif // YAMLSYNTAXPLUGIN_H
