#ifndef BASESYNTAXPLUGIN_H
#define BASESYNTAXPLUGIN_H

#include "../plugins/isyntaxplugin.h"

/**
 * @brief Base class for built-in syntax plugins
 * 
 * Provides default implementations of IPlugin methods for built-in syntax plugins
 * that don't need the full plugin loading/unloading lifecycle.
 */
class BaseSyntaxPlugin : public ISyntaxPlugin {
public:
    // IPlugin interface implementation
    PluginMetadata metadata() const override {
        PluginMetadata meta;
        meta.id = languageId();
        meta.name = languageName();
        meta.version = "1.0.0";
        meta.author = "Lightpad Team";
        meta.description = QString("Built-in %1 syntax highlighting").arg(languageName());
        meta.category = "syntax";
        return meta;
    }

    bool initialize() override { 
        // Built-in plugins are always initialized
        return true; 
    }

    void shutdown() override { 
        // Built-in plugins don't need cleanup
    }

    bool isLoaded() const override { 
        // Built-in plugins are always loaded
        return true; 
    }
};

#endif // BASESYNTAXPLUGIN_H
