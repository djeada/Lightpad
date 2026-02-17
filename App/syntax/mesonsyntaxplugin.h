#ifndef MESONSYNTAXPLUGIN_H
#define MESONSYNTAXPLUGIN_H

#include "shellsyntaxplugin.h"

class MesonSyntaxPlugin : public ShellSyntaxPlugin {
public:
  QString languageId() const override { return "meson"; }
  QString languageName() const override { return "Meson"; }
  QStringList fileExtensions() const override {
    return {"meson", "meson.build", "meson_options.txt"};
  }
};

#endif
