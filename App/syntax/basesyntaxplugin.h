#ifndef BASESYNTAXPLUGIN_H
#define BASESYNTAXPLUGIN_H

#include "../plugins/isyntaxplugin.h"

class BaseSyntaxPlugin : public ISyntaxPlugin {
public:
  PluginMetadata metadata() const override {
    PluginMetadata meta;
    meta.id = languageId();
    meta.name = languageName();
    meta.version = "1.0.0";
    meta.author = "Lightpad Team";
    meta.description =
        QString("Built-in %1 syntax highlighting").arg(languageName());
    meta.category = "syntax";
    return meta;
  }

  bool initialize() override { return true; }

  void shutdown() override {}

  bool isLoaded() const override { return true; }
};

#endif
