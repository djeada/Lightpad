#ifndef WGSLSYNTAXPLUGIN_H
#define WGSLSYNTAXPLUGIN_H

#include "basesyntaxplugin.h"
#include <QVector>

class WgslSyntaxPlugin : public BaseSyntaxPlugin {
public:
  QString languageId() const override { return "wgsl"; }
  QString languageName() const override { return "WGSL"; }
  QStringList fileExtensions() const override { return {"wgsl"}; }

  QVector<SyntaxRule> syntaxRules() const override;
  QVector<MultiLineBlock> multiLineBlocks() const override;
  QStringList keywords() const override;

  QPair<QString, QPair<QString, QString>> commentStyle() const override {
    return {"//", {"", ""}};
  }

private:
  static QStringList getPrimaryKeywords();
  static QStringList getSecondaryKeywords();
  static QStringList getTertiaryKeywords();
};

#endif
