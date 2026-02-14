#ifndef JAVASYNTAXPLUGIN_H
#define JAVASYNTAXPLUGIN_H

#include "basesyntaxplugin.h"
#include <QVector>

class JavaSyntaxPlugin : public BaseSyntaxPlugin {
public:
  QString languageId() const override { return "java"; }
  QString languageName() const override { return "Java"; }
  QStringList fileExtensions() const override { return {"java"}; }

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
