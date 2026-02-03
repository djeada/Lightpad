#ifndef HTMLSYNTAXPLUGIN_H
#define HTMLSYNTAXPLUGIN_H

#include "basesyntaxplugin.h"
#include <QVector>

/**
 * @brief Built-in HTML syntax highlighting plugin
 */
class HtmlSyntaxPlugin : public BaseSyntaxPlugin {
public:
  QString languageId() const override { return "html"; }
  QString languageName() const override { return "HTML"; }
  QStringList fileExtensions() const override {
    return {"html", "htm", "xhtml"};
  }

  QVector<SyntaxRule> syntaxRules() const override;
  QVector<MultiLineBlock> multiLineBlocks() const override;
  QStringList keywords() const override;

  QPair<QString, QPair<QString, QString>> commentStyle() const override {
    return {"", {"<!--", "-->"}};
  }

private:
  static QStringList getTags();
  static QStringList getAttributes();
};

#endif // HTMLSYNTAXPLUGIN_H
