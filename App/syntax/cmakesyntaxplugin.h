#ifndef CMAKESYNTAXPLUGIN_H
#define CMAKESYNTAXPLUGIN_H

#include "shellsyntaxplugin.h"

class CMakeSyntaxPlugin : public ShellSyntaxPlugin {
public:
  QString languageId() const override { return "cmake"; }
  QString languageName() const override { return "CMake"; }
  QStringList fileExtensions() const override {
    return {"cmake", "cmakelists.txt"};
  }
};

#endif
