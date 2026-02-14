#ifndef GOSYNTAXPLUGIN_H
#define GOSYNTAXPLUGIN_H

#include "basesyntaxplugin.h"
#include <QVector>

class GoSyntaxPlugin : public BaseSyntaxPlugin {
public:
  QString languageId() const override { return "go"; }
  QString languageName() const override { return "Go"; }
  QStringList fileExtensions() const override { return {"go"}; }

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
