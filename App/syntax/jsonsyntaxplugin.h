#ifndef JSONSYNTAXPLUGIN_H
#define JSONSYNTAXPLUGIN_H

#include "basesyntaxplugin.h"
#include <QVector>

/**
 * @brief Built-in JSON syntax highlighting plugin
 */
class JsonSyntaxPlugin : public BaseSyntaxPlugin {
public:
  QString languageId() const override { return "json"; }
  QString languageName() const override { return "JSON"; }
  QStringList fileExtensions() const override {
    return {"json", "jsonc", "geojson"};
  }

  QVector<SyntaxRule> syntaxRules() const override;
  QVector<MultiLineBlock> multiLineBlocks() const override;
  QStringList keywords() const override;

  QPair<QString, QPair<QString, QString>> commentStyle() const override {
    return {"//", {"/*", "*/"}};
  }
};

#endif // JSONSYNTAXPLUGIN_H
