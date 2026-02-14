#ifndef MARKDOWNSYNTAXPLUGIN_H
#define MARKDOWNSYNTAXPLUGIN_H

#include "basesyntaxplugin.h"
#include <QVector>

class MarkdownSyntaxPlugin : public BaseSyntaxPlugin {
public:
  QString languageId() const override { return "md"; }
  QString languageName() const override { return "Markdown"; }
  QStringList fileExtensions() const override {
    return {"md", "markdown", "mdown", "mkd", "mkdn", "mdx"};
  }

  QVector<SyntaxRule> syntaxRules() const override;
  QVector<MultiLineBlock> multiLineBlocks() const override;
  QStringList keywords() const override;

  QPair<QString, QPair<QString, QString>> commentStyle() const override {
    return {"", {"<!--", "-->"}};
  }
};

#endif
