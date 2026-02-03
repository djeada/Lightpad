#ifndef CSSSYNTAXPLUGIN_H
#define CSSSYNTAXPLUGIN_H

#include "basesyntaxplugin.h"
#include <QVector>

/**
 * @brief Built-in CSS syntax highlighting plugin
 */
class CssSyntaxPlugin : public BaseSyntaxPlugin {
public:
  QString languageId() const override { return "css"; }
  QString languageName() const override { return "CSS"; }
  QStringList fileExtensions() const override {
    return {"css", "scss", "sass", "less"};
  }

  QVector<SyntaxRule> syntaxRules() const override;
  QVector<MultiLineBlock> multiLineBlocks() const override;
  QStringList keywords() const override;

  QPair<QString, QPair<QString, QString>> commentStyle() const override {
    return {"", {"/*", "*/"}};
  }

private:
  static QStringList getProperties();
  static QStringList getValues();
  static QStringList getAtRules();
};

#endif // CSSSYNTAXPLUGIN_H
