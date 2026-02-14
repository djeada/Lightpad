#ifndef RUSTSYNTAXPLUGIN_H
#define RUSTSYNTAXPLUGIN_H

#include "basesyntaxplugin.h"
#include <QVector>

class RustSyntaxPlugin : public BaseSyntaxPlugin {
public:
  QString languageId() const override { return "rust"; }
  QString languageName() const override { return "Rust"; }
  QStringList fileExtensions() const override { return {"rs"}; }

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
