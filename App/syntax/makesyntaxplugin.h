#ifndef MAKESYNTAXPLUGIN_H
#define MAKESYNTAXPLUGIN_H

#include "shellsyntaxplugin.h"

class MakeSyntaxPlugin : public ShellSyntaxPlugin {
public:
  QString languageId() const override { return "make"; }
  QString languageName() const override { return "Make"; }
  QStringList fileExtensions() const override { return {"mk", "makefile"}; }
};

#endif
