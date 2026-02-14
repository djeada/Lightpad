#ifndef SHELLSYNTAXPLUGIN_H
#define SHELLSYNTAXPLUGIN_H

#include "basesyntaxplugin.h"
#include <QVector>

class ShellSyntaxPlugin : public BaseSyntaxPlugin {
public:
  QString languageId() const override { return "sh"; }
  QString languageName() const override { return "Shell"; }
  QStringList fileExtensions() const override {
    return {"sh", "bash", "zsh", "fish"};
  }

  QVector<SyntaxRule> syntaxRules() const override;
  QVector<MultiLineBlock> multiLineBlocks() const override;
  QStringList keywords() const override;

  QPair<QString, QPair<QString, QString>> commentStyle() const override {
    return {"#", {"", ""}};
  }

private:
  static QStringList getPrimaryKeywords();
  static QStringList getSecondaryKeywords();
  static QStringList getTertiaryKeywords();
};

#endif
