#ifndef NINJASYNTAXPLUGIN_H
#define NINJASYNTAXPLUGIN_H

#include "shellsyntaxplugin.h"

class NinjaSyntaxPlugin : public ShellSyntaxPlugin {
public:
  QString languageId() const override { return "ninja"; }
  QString languageName() const override { return "Ninja"; }
  QStringList fileExtensions() const override { return {"ninja", "build.ninja"}; }
};

#endif
