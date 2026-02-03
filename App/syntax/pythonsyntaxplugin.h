#ifndef PYTHONSYNTAXPLUGIN_H
#define PYTHONSYNTAXPLUGIN_H

#include "basesyntaxplugin.h"
#include <QVector>

/**
 * @brief Built-in Python syntax highlighting plugin
 */
class PythonSyntaxPlugin : public BaseSyntaxPlugin {
public:
  QString languageId() const override { return "py"; }
  QString languageName() const override { return "Python"; }
  QStringList fileExtensions() const override { return {"py", "pyw", "pyi"}; }

  QVector<SyntaxRule> syntaxRules() const override;
  QVector<MultiLineBlock> multiLineBlocks() const override;
  QStringList keywords() const override;

  QPair<QString, QPair<QString, QString>> commentStyle() const override {
    return {"#", {"'''", "'''"}};
  }

private:
  static QStringList getPrimaryKeywords();
  static QStringList getSecondaryKeywords();
  static QStringList getTertiaryKeywords();
};

#endif // PYTHONSYNTAXPLUGIN_H
