# Completion System

This module provides the new scalable autocompletion system for Lightpad.

## Architecture

The completion system uses a provider-based architecture:

```
┌─────────────────────────────────────────────────────────────────┐
│                      CompletionEngine                            │
│  Coordinates providers, merges results, manages lifecycle       │
└───────────────────────────┬─────────────────────────────────────┘
                            │
    ┌───────────────────────┼───────────────────────┐
    │                       │                       │
    ▼                       ▼                       ▼
┌──────────┐         ┌──────────┐           ┌──────────┐
│ Keyword  │         │   LSP    │           │ Snippet  │
│ Provider │         │ Provider │           │ Provider │
└──────────┘         └──────────┘           └──────────┘
```

## Components

### Core
- `CompletionItem` - Rich completion item data structure
- `CompletionContext` - Request context with position and trigger info
- `ICompletionProvider` - Interface for completion providers
- `CompletionProviderRegistry` - Singleton registry for providers
- `CompletionEngine` - Central orchestrator

### Providers
- `KeywordCompletionProvider` - Language keywords from registry
- `LspCompletionProvider` - LSP-based completions
- `PluginCompletionProvider` - Keywords from syntax plugins
- `SnippetCompletionProvider` - Code snippets with placeholders

### UI
- `CompletionWidget` - Popup widget for displaying completions
- `CompletionItemModel` - Qt model for completion items

### Registries
- `LanguageKeywordsRegistry` - Manages language keywords
- `SnippetRegistry` - Manages code snippets

## Usage

### Basic Usage

```cpp
#include "completion/completionengine.h"
#include "completion/completionproviderregistry.h"
#include "completion/providers/keywordcompletionprovider.h"

// Register providers
auto& registry = CompletionProviderRegistry::instance();
registry.registerProvider(std::make_shared<KeywordCompletionProvider>());

// Create engine
CompletionEngine* engine = new CompletionEngine(this);
engine->setLanguage("cpp");

// Connect to results
connect(engine, &CompletionEngine::completionsReady,
        this, &MyWidget::showCompletions);

// Request completions
CompletionContext ctx;
ctx.languageId = "cpp";
ctx.prefix = "for";
ctx.line = 10;
ctx.column = 5;
engine->requestCompletions(ctx);
```

### Creating a Custom Provider

```cpp
class MyProvider : public ICompletionProvider {
public:
    QString id() const override { return "my_provider"; }
    QString displayName() const override { return "My Provider"; }
    int basePriority() const override { return 50; }
    QStringList supportedLanguages() const override { return {"cpp"}; }
    
    void requestCompletions(
        const CompletionContext& context,
        std::function<void(const QList<CompletionItem>&)> callback
    ) override {
        QList<CompletionItem> items;
        
        // Generate items based on context
        CompletionItem item;
        item.label = "myCompletion";
        item.kind = CompletionItemKind::Function;
        item.priority = basePriority();
        item.providerId = id();
        items.append(item);
        
        callback(items);
    }
};

// Register the provider
registry.registerProvider(std::make_shared<MyProvider>());
```

## Priority Ranges

- **0-20**: LSP/context-aware (highest priority)
- **20-50**: Snippets
- **50-80**: Plugin keywords
- **80-100**: Generic keywords
- **100+**: Low priority suggestions

## Snippet Syntax

Snippets use VS Code-compatible placeholder syntax:

- `$1`, `$2`, etc. - Tabstops
- `${1:placeholder}` - Tabstops with default text
- `$0` - Final cursor position

Example:
```json
{
    "prefix": "for",
    "label": "For Loop",
    "body": "for (${1:int} ${2:i} = 0; $2 < ${3:count}; $2++) {\n\t$0\n}"
}
```

## Files

```
completion/
├── completionitem.h           # CompletionItem struct
├── completioncontext.h        # CompletionContext struct
├── icompletionprovider.h      # ICompletionProvider interface
├── completionproviderregistry.h/.cpp  # Provider registry
├── languagekeywordsregistry.h/.cpp    # Keyword registry
├── completionengine.h/.cpp    # Central engine
├── completionitemmodel.h/.cpp # Qt model
├── completionwidget.h/.cpp    # Popup widget
├── snippetregistry.h/.cpp     # Snippet registry
├── providers/
│   ├── keywordcompletionprovider.h/.cpp
│   ├── lspcompletionprovider.h/.cpp
│   ├── plugincompletionprovider.h/.cpp
│   └── snippetcompletionprovider.h/.cpp
└── README.md
```
