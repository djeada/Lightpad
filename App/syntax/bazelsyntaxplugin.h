#ifndef BAZELSYNTAXPLUGIN_H
#define BAZELSYNTAXPLUGIN_H

#include "shellsyntaxplugin.h"

class BazelSyntaxPlugin : public ShellSyntaxPlugin {
public:
  QString languageId() const override { return "bazel"; }
  QString languageName() const override { return "Bazel"; }
  QStringList fileExtensions() const override { return {"bzl", "bazel"}; }
};

#endif
