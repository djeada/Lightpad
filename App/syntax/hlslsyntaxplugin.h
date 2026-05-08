#ifndef HLSLSYNTAXPLUGIN_H
#define HLSLSYNTAXPLUGIN_H

#include "basesyntaxplugin.h"
#include <QVector>

class HlslSyntaxPlugin : public BaseSyntaxPlugin {
public:
  QString languageId() const override { return "hlsl"; }
  QString languageName() const override { return "HLSL"; }
  QStringList fileExtensions() const override {
    return {"hlsl", "fx", "fxh", "hlsli"};
  }

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
