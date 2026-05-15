#ifndef GLSLSYNTAXPLUGIN_H
#define GLSLSYNTAXPLUGIN_H

#include "basesyntaxplugin.h"
#include <QVector>

class GlslSyntaxPlugin : public BaseSyntaxPlugin {
public:
  QString languageId() const override { return "glsl"; }
  QString languageName() const override { return "GLSL"; }
  QStringList fileExtensions() const override {
    return {"glsl", "vert", "frag",  "geom",  "comp",  "tesc", "tese",
            "rgen", "rint", "rahit", "rchit", "rmiss", "rcall"};
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
