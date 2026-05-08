#ifndef METALSYNTAXPLUGIN_H
#define METALSYNTAXPLUGIN_H

#include "basesyntaxplugin.h"
#include <QVector>

class MetalSyntaxPlugin : public BaseSyntaxPlugin {
public:
  QString languageId() const override { return "metal"; }
  QString languageName() const override { return "Metal"; }
  QStringList fileExtensions() const override { return {"metal"}; }

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
