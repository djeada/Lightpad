# Plugin Development Guide

This guide explains how to create plugins for Lightpad.

## Overview

Lightpad's plugin system allows you to extend the editor with:
- New syntax highlighting rules
- Custom themes
- Additional tools and features

Plugins are Qt plugins that implement specific interfaces.

## Plugin Types

### Base Plugin (IPlugin)

All plugins must implement the `IPlugin` interface:

```cpp
#include "plugins/iplugin.h"

class MyPlugin : public QObject, public IPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID IPlugin_iid FILE "myplugin.json")
    Q_INTERFACES(IPlugin)
    
public:
    PluginMetadata metadata() const override {
        return {
            "com.example.myplugin",  // Unique ID
            "My Plugin",              // Display name
            "1.0.0",                  // Version
            "Author Name",            // Author
            "Plugin description",     // Description
            "tool",                   // Category
            {}                        // Dependencies
        };
    }
    
    bool initialize() override {
        // Setup plugin
        return true;
    }
    
    void shutdown() override {
        // Cleanup
    }
    
    bool isLoaded() const override {
        return m_loaded;
    }
    
private:
    bool m_loaded = false;
};
```

### Syntax Plugin (ISyntaxPlugin)

For adding syntax highlighting:

```cpp
#include "plugins/isyntaxplugin.h"

class RustSyntaxPlugin : public QObject, public ISyntaxPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID ISyntaxPlugin_iid FILE "rustsyntax.json")
    Q_INTERFACES(ISyntaxPlugin)
    
public:
    PluginMetadata metadata() const override {
        return {
            "com.lightpad.syntax.rust",
            "Rust Syntax",
            "1.0.0",
            "Lightpad Team",
            "Rust language syntax highlighting",
            "syntax",
            {}
        };
    }
    
    QString languageId() const override {
        return "rust";
    }
    
    QString languageName() const override {
        return "Rust";
    }
    
    QStringList fileExtensions() const override {
        return {"rs"};
    }
    
    QVector<SyntaxRule> syntaxRules() const override {
        QVector<SyntaxRule> rules;
        
        // Keywords
        QTextCharFormat keywordFormat;
        keywordFormat.setForeground(Qt::blue);
        keywordFormat.setFontWeight(QFont::Bold);
        
        QStringList keywords = {
            "fn", "let", "mut", "const", "if", "else",
            "match", "loop", "while", "for", "return",
            "struct", "enum", "impl", "trait", "pub",
            "use", "mod", "crate", "self", "super"
        };
        
        for (const QString& keyword : keywords) {
            SyntaxRule rule;
            rule.pattern = QRegularExpression("\\b" + keyword + "\\b");
            rule.format = keywordFormat;
            rules.append(rule);
        }
        
        // Strings
        QTextCharFormat stringFormat;
        stringFormat.setForeground(Qt::darkGreen);
        rules.append({QRegularExpression("\".*\""), stringFormat, "string"});
        
        // Comments
        QTextCharFormat commentFormat;
        commentFormat.setForeground(Qt::gray);
        rules.append({QRegularExpression("//[^\n]*"), commentFormat, "comment"});
        
        return rules;
    }
    
    QVector<MultiLineBlock> multiLineBlocks() const override {
        QTextCharFormat commentFormat;
        commentFormat.setForeground(Qt::gray);
        
        return {{
            QRegularExpression("/\\*"),
            QRegularExpression("\\*/"),
            commentFormat
        }};
    }
    
    QStringList keywords() const override {
        return {
            "fn", "let", "mut", "const", "if", "else",
            "match", "loop", "while", "for", "return"
        };
    }
    
    QPair<QString, QPair<QString, QString>> commentStyle() const override {
        return {"//", {"/*", "*/"}};
    }
    
    // IPlugin interface
    bool initialize() override { m_loaded = true; return true; }
    void shutdown() override { m_loaded = false; }
    bool isLoaded() const override { return m_loaded; }
    
private:
    bool m_loaded = false;
};
```

## Plugin Metadata File

Create a JSON metadata file (e.g., `myplugin.json`):

```json
{
    "id": "com.example.myplugin",
    "name": "My Plugin",
    "version": "1.0.0",
    "author": "Your Name",
    "description": "A sample plugin",
    "category": "tool",
    "dependencies": []
}
```

## Building Plugins

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.10)
project(MyPlugin)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Core Widgets)

# Include Lightpad headers
include_directories(${LIGHTPAD_INCLUDE_DIR}/plugins)

add_library(myplugin SHARED
    myplugin.cpp
    myplugin.json
)

target_link_libraries(myplugin Qt5::Core Qt5::Widgets)

# Install to plugin directory
install(TARGETS myplugin DESTINATION ${LIGHTPAD_PLUGIN_DIR})
```

### Build Commands

```bash
mkdir build && cd build
cmake -DLIGHTPAD_INCLUDE_DIR=/path/to/lightpad/App ..
cmake --build .
```

## Plugin Installation

Copy the compiled plugin to:

| Platform | Location |
|----------|----------|
| Linux    | `~/.local/share/lightpad/plugins/` |
| Windows  | `%APPDATA%/Local/Lightpad/plugins/` |
| macOS    | `~/Library/Application Support/Lightpad/plugins/` |

## Plugin Lifecycle

1. **Discovery:** PluginManager scans plugin directories
2. **Loading:** Plugin's shared library is loaded
3. **Initialization:** `initialize()` is called
4. **Running:** Plugin provides functionality
5. **Shutdown:** `shutdown()` is called before unloading
6. **Unloading:** Shared library is unloaded

## Best Practices

### Error Handling

```cpp
bool initialize() override {
    try {
        // Initialization code
        m_loaded = true;
        return true;
    } catch (const std::exception& e) {
        qWarning() << "Plugin init failed:" << e.what();
        return false;
    }
}
```

### Resource Cleanup

```cpp
void shutdown() override {
    // Release resources
    m_resources.clear();
    m_loaded = false;
}
```

### Settings Storage

```cpp
QJsonObject settings() const override {
    return m_settings;
}

void setSettings(const QJsonObject& settings) override {
    m_settings = settings;
    applySettings();
}
```

### Logging

```cpp
#include "core/logging/logger.h"

void doSomething() {
    LOG_INFO("Plugin: doing something");
    LOG_DEBUG("Plugin: detailed info");
    LOG_WARNING("Plugin: potential issue");
    LOG_ERROR("Plugin: error occurred");
}
```

## Debugging Plugins

### Enable Debug Logging

Set environment variable:
```bash
LIGHTPAD_LOG_LEVEL=debug ./Lightpad
```

### Check Plugin Loading

Check log output for:
```
[INFO] Loaded plugin: My Plugin v1.0.0
```

Or errors:
```
[ERROR] Failed to load plugin /path/to/plugin.so: <error message>
```

## Example Plugins

See the `examples/plugins/` directory for complete examples:
- `rust_syntax/` - Rust syntax highlighting
- `custom_theme/` - Custom color theme
- `word_count/` - Word count tool

## API Reference

### IPlugin Interface

| Method | Description |
|--------|-------------|
| `metadata()` | Returns plugin metadata |
| `initialize()` | Called when plugin is loaded |
| `shutdown()` | Called before plugin is unloaded |
| `isLoaded()` | Returns true if plugin is active |
| `settings()` | Returns plugin settings |
| `setSettings()` | Updates plugin settings |

### ISyntaxPlugin Interface

| Method | Description |
|--------|-------------|
| `languageId()` | Returns language identifier |
| `languageName()` | Returns display name |
| `fileExtensions()` | Returns supported extensions |
| `syntaxRules()` | Returns highlighting rules |
| `multiLineBlocks()` | Returns multi-line patterns |
| `keywords()` | Returns language keywords |
| `commentStyle()` | Returns comment syntax |

## Getting Help

- **GitHub Issues:** Report bugs or request features
- **Documentation:** Check the docs/ directory
- **Examples:** Study example plugins
