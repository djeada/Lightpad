#ifndef TYPESCRIPTSYNTAXPLUGIN_H
#define TYPESCRIPTSYNTAXPLUGIN_H

#include "basesyntaxplugin.h"
#include <QVector>

class TypeScriptSyntaxPlugin : public BaseSyntaxPlugin {
public:
  QString languageId() const override { return "ts"; }
  QString languageName() const override { return "TypeScript"; }
  QStringList fileExtensions() const override { return {"ts", "tsx"}; }

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

#endif
