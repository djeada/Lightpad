#ifndef LATEXSYNTAXPLUGIN_H
#define LATEXSYNTAXPLUGIN_H

#include "basesyntaxplugin.h"
#include <QVector>

class LatexSyntaxPlugin : public BaseSyntaxPlugin {
public:
  QString languageId() const override { return "latex"; }
  QString languageName() const override { return "LaTeX"; }
  QStringList fileExtensions() const override {
    return {"tex", "sty", "cls", "bib", "dtx", "ins", "ltx"};
  }

  QVector<SyntaxRule> syntaxRules() const override;
  QVector<MultiLineBlock> multiLineBlocks() const override;
  QStringList keywords() const override;

  QPair<QString, QPair<QString, QString>> commentStyle() const override {
    return {"%", {"", ""}};
  }

private:
  static QStringList getSectioningCommands();
  static QStringList getEnvironmentNames();
  static QStringList getTextFormattingCommands();
  static QStringList getMathCommands();
  static QStringList getReferenceCommands();
  static QStringList getPackageCommands();
};

#endif
