#ifndef DOCKERFILESYNTAXPLUGIN_H
#define DOCKERFILESYNTAXPLUGIN_H

#include "basesyntaxplugin.h"
#include <QVector>

class DockerfileSyntaxPlugin : public BaseSyntaxPlugin {
public:
  QString languageId() const override { return "dockerfile"; }
  QString languageName() const override { return "Dockerfile"; }
  QStringList fileExtensions() const override {
    return {"dockerfile", "containerfile"};
  }

  QVector<SyntaxRule> syntaxRules() const override;
  QVector<MultiLineBlock> multiLineBlocks() const override;
  QStringList keywords() const override;

  QPair<QString, QPair<QString, QString>> commentStyle() const override {
    return {"#", {"", ""}};
  }

private:
  static QStringList getInstructions();
};

#endif
