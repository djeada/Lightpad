#ifndef CPPSYNTAXPLUGIN_H
#define CPPSYNTAXPLUGIN_H

#include "basesyntaxplugin.h"
#include <QVector>

/**
 * @brief Built-in C++ syntax highlighting plugin
 */
class CppSyntaxPlugin : public BaseSyntaxPlugin {
public:
    QString languageId() const override { return "cpp"; }
    QString languageName() const override { return "C++"; }
    QStringList fileExtensions() const override { return {"cpp", "cc", "cxx", "c", "h", "hpp", "hxx"}; }
    
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

#endif // CPPSYNTAXPLUGIN_H
