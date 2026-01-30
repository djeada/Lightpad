# Creating Custom Syntax Highlighting Plugins for Lightpad

This guide explains how to create custom syntax highlighting plugins for Lightpad using the plugin-based architecture.

## Overview

Lightpad's new plugin-based syntax highlighting system allows you to add support for any programming language without modifying the core editor code. Plugins are C++ classes that implement the `ISyntaxPlugin` interface.

## Quick Start

### 1. Create Your Plugin Class

Create a header file for your language plugin (e.g., `rustsyntaxplugin.h`):

```cpp
#ifndef RUSTSYNTAXPLUGIN_H
#define RUSTSYNTAXPLUGIN_H

#include "basesyntaxplugin.h"
#include <QVector>

class RustSyntaxPlugin : public BaseSyntaxPlugin {
public:
    QString languageId() const override { return "rust"; }
    QString languageName() const override { return "Rust"; }
    QStringList fileExtensions() const override { return {"rs"}; }
    
    QVector<SyntaxRule> syntaxRules() const override;
    QVector<MultiLineBlock> multiLineBlocks() const override;
    QStringList keywords() const override;
    
    QPair<QString, QPair<QString, QString>> commentStyle() const override {
        return {"//", {"/*", "*/"}};
    }

private:
    static QStringList getPrimaryKeywords();
    static QStringList getSecondaryKeywords();
};

#endif
```

### 2. Implement The Plugin

Create the implementation file (e.g., `rustsyntaxplugin.cpp`):

```cpp
#include "rustsyntaxplugin.h"
#include <QRegularExpression>

QStringList RustSyntaxPlugin::getPrimaryKeywords()
{
    return {
        "as", "async", "await", "break", "const", "continue", "crate",
        "dyn", "else", "enum", "extern", "false", "fn", "for", "if",
        "impl", "in", "let", "loop", "match", "mod", "move", "mut",
        "pub", "ref", "return", "self", "Self", "static", "struct",
        "super", "trait", "true", "type", "unsafe", "use", "where", "while"
    };
}

QStringList RustSyntaxPlugin::getSecondaryKeywords()
{
    return {
        "bool", "char", "f32", "f64", "i8", "i16", "i32", "i64", "i128",
        "isize", "str", "u8", "u16", "u32", "u64", "u128", "usize"
    };
}

QVector<SyntaxRule> RustSyntaxPlugin::syntaxRules() const
{
    QVector<SyntaxRule> rules;

    // Primary keywords
    for (const QString& keyword : getPrimaryKeywords()) {
        SyntaxRule rule;
        rule.pattern = QRegularExpression("\\b" + keyword + "\\b");
        rule.name = "keyword_0";
        rules.append(rule);
    }

    // Secondary keywords (types)
    for (const QString& keyword : getSecondaryKeywords()) {
        SyntaxRule rule;
        rule.pattern = QRegularExpression("\\b" + keyword + "\\b");
        rule.name = "keyword_1";
        rules.append(rule);
    }

    // Numbers
    SyntaxRule numberRule;
    numberRule.pattern = QRegularExpression("\\b[-+.,]*\\d{1,}[uif]*(8|16|32|64|128)?\\b");
    numberRule.name = "number";
    rules.append(numberRule);

    // String literals
    SyntaxRule stringRule;
    stringRule.pattern = QRegularExpression("\".*\"");
    stringRule.name = "string";
    rules.append(stringRule);

    // Function calls
    SyntaxRule functionRule;
    functionRule.pattern = QRegularExpression("\\b[A-Za-z0-9_]+(?=\\()");
    functionRule.name = "function";
    rules.append(functionRule);

    // Macros
    SyntaxRule macroRule;
    macroRule.pattern = QRegularExpression("\\b[A-Za-z0-9_]+!");
    macroRule.name = "keyword_1";
    rules.append(macroRule);

    // Single-line comments
    SyntaxRule commentRule;
    commentRule.pattern = QRegularExpression("//[^\n]*");
    commentRule.name = "comment";
    rules.append(commentRule);

    return rules;
}

QVector<MultiLineBlock> RustSyntaxPlugin::multiLineBlocks() const
{
    QVector<MultiLineBlock> blocks;

    // Multi-line comments
    MultiLineBlock commentBlock;
    commentBlock.startPattern = QRegularExpression("/\\*");
    commentBlock.endPattern = QRegularExpression("\\*/");
    blocks.append(commentBlock);

    return blocks;
}

QStringList RustSyntaxPlugin::keywords() const
{
    QStringList all;
    all << getPrimaryKeywords() << getSecondaryKeywords();
    return all;
}
```

### 3. Register Your Plugin

In `main.cpp` or a plugin initialization function:

```cpp
#include "syntax/syntaxpluginregistry.h"
#include "rustsyntaxplugin.h"

void registerCustomPlugins()
{
    auto& registry = SyntaxPluginRegistry::instance();
    registry.registerPlugin(std::make_unique<RustSyntaxPlugin>());
}
```

## API Reference

### ISyntaxPlugin Interface

Required methods:

- **`languageId()`**: Returns a unique identifier for the language (e.g., "cpp", "py", "rust")
- **`languageName()`**: Returns the display name (e.g., "C++", "Python", "Rust")
- **`fileExtensions()`**: Returns a list of file extensions without dots (e.g., ["rs", "rslib"])
- **`syntaxRules()`**: Returns highlighting rules for single-line patterns
- **`multiLineBlocks()`**: Returns definitions for multi-line blocks like comments
- **`keywords()`**: Returns all keywords for autocomplete (optional)
- **`commentStyle()`**: Returns comment syntax (single-line, multi-line start/end)

### SyntaxRule Structure

```cpp
struct SyntaxRule {
    QRegularExpression pattern;  // Pattern to match
    QTextCharFormat format;       // Formatting to apply
    QString name;                 // Rule name for theme mapping
};
```

**Rule Names** (automatically themed):
- `"keyword_0"` - Primary keywords (bold, color 0)
- `"keyword_1"` - Secondary keywords (bold, color 1)
- `"keyword_2"` - Tertiary keywords (color 2)
- `"number"` - Numeric literals
- `"string"` or `"quotation"` - String literals
- `"comment"` - Comments
- `"function"` - Function names
- `"class"` or `"type"` - Class/type names

### MultiLineBlock Structure

```cpp
struct MultiLineBlock {
    QRegularExpression startPattern;  // Block start pattern
    QRegularExpression endPattern;    // Block end pattern
    QTextCharFormat format;           // Formatting to apply
};
```

## Examples

### Simple Language (Shell Script)

```cpp
QVector<SyntaxRule> ShellSyntaxPlugin::syntaxRules() const
{
    QVector<SyntaxRule> rules;
    
    // Keywords
    QStringList keywords = {"if", "then", "else", "elif", "fi", "for", "while", "do", "done", "case", "esac"};
    for (const QString& kw : keywords) {
        SyntaxRule rule;
        rule.pattern = QRegularExpression("\\b" + kw + "\\b");
        rule.name = "keyword_0";
        rules.append(rule);
    }
    
    // Variables
    SyntaxRule varRule;
    varRule.pattern = QRegularExpression("\\$[A-Za-z0-9_]+");
    varRule.name = "keyword_1";
    rules.append(varRule);
    
    // Comments
    SyntaxRule commentRule;
    commentRule.pattern = QRegularExpression("#[^\n]*");
    commentRule.name = "comment";
    rules.append(commentRule);
    
    return rules;
}
```

### Language with Multiple String Types (JavaScript)

```cpp
// Regular strings
SyntaxRule stringRule;
stringRule.pattern = QRegularExpression("\".*\"");
stringRule.name = "string";
rules.append(stringRule);

// Single-quoted strings
SyntaxRule singleQuoteRule;
singleQuoteRule.pattern = QRegularExpression("'.*'");
singleQuoteRule.name = "string";
rules.append(singleQuoteRule);

// Template literals
SyntaxRule templateRule;
templateRule.pattern = QRegularExpression("`.*`");
templateRule.name = "string";
rules.append(templateRule);
```

## Best Practices

1. **Pattern Ordering**: More specific patterns should come before general ones
2. **Word Boundaries**: Use `\\b` for keyword matching to avoid partial matches
3. **Performance**: Keep regex patterns simple; complex patterns can slow down highlighting
4. **Testing**: Create unit tests for your plugin (see `test_syntaxpluginregistry.cpp`)
5. **Documentation**: Document any non-obvious regex patterns

## Testing Your Plugin

Create a test file (e.g., `test_rustsyntaxplugin.cpp`):

```cpp
#include <QtTest/QtTest>
#include "rustsyntaxplugin.h"

class TestRustSyntaxPlugin : public QObject {
    Q_OBJECT

private slots:
    void testLanguageId() {
        RustSyntaxPlugin plugin;
        QCOMPARE(plugin.languageId(), QString("rust"));
    }
    
    void testFileExtensions() {
        RustSyntaxPlugin plugin;
        QVERIFY(plugin.fileExtensions().contains("rs"));
    }
    
    void testKeywords() {
        RustSyntaxPlugin plugin;
        QStringList keywords = plugin.keywords();
        QVERIFY(keywords.contains("fn"));
        QVERIFY(keywords.contains("let"));
    }
};

QTEST_MAIN(TestRustSyntaxPlugin)
#include "test_rustsyntaxplugin.moc"
```

## Integration

### Built-in Plugins

For built-in plugins, add them to:
1. `App/syntax/` directory
2. `App/CMakeLists.txt`
3. Register in `main.cpp`

### External Plugins

External plugins can be compiled as shared libraries and loaded at runtime through the plugin manager (future feature).

## Troubleshooting

**Problem**: Highlighting doesn't work
- Check if plugin is registered: `SyntaxPluginRegistry::instance().isLanguageSupported("your_lang")`
- Verify file extension mapping is correct
- Check regex patterns with a regex tester

**Problem**: Colors are wrong
- Check rule names match theme color mappings
- Verify `applyThemeToFormat()` in `pluginbasedsyntaxhighlighter.cpp`

**Problem**: Performance issues
- Simplify regex patterns
- Reduce number of rules
- Consider combining similar patterns

## Contributing

To contribute a language plugin to Lightpad:
1. Create the plugin following this guide
2. Add comprehensive tests
3. Submit a pull request with your plugin
4. Include sample code files for testing

## Further Reading

- Qt Regular Expressions: https://doc.qt.io/qt-5/qregularexpression.html
- Qt Syntax Highlighting: https://doc.qt.io/qt-5/qsyntaxhighlighter.html
- Lightpad Plugin System: `App/plugins/README.md`
