# Autocomplete System Redesign Plan

## Executive Summary

This document outlines a detailed plan to replace Lightpad's current autocomplete system with a more scalable, maintainable, and feature-rich architecture. The current implementation uses a monolithic Qt QCompleter with a hardcoded word list. The proposed design introduces a provider-based architecture with LSP integration, language-specific completions, and support for future extensibility.

---

## 1. Current System Analysis

### 1.1 Architecture Overview

The current autocomplete system consists of three tightly-coupled components:

```
┌─────────────────────────────────────────────────────────────────┐
│                      MainWindow                                  │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │ Hardcoded wordList (QStringList)                         │   │
│  │ - 100+ keywords across C++, Python, JavaScript           │   │
│  │ - Sorted and deduplicated                                │   │
│  └────────────────────────┬─────────────────────────────────┘   │
│                           │                                      │
│                           ▼                                      │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │ QCompleter                                                │   │
│  │ - CaseInsensitive matching                               │   │
│  │ - PopupCompletion mode                                   │   │
│  │ - Shared across all TextArea instances                   │   │
│  └────────────────────────┬─────────────────────────────────┘   │
│                           │                                      │
└───────────────────────────┼──────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                        TextArea                                  │
│  - setCompleter() / completer()                                 │
│  - insertCompletion()                                           │
│  - textUnderCursor()                                            │
│  - keyPressEvent() handles Ctrl+Space and auto-trigger          │
└─────────────────────────────────────────────────────────────────┘
```

### 1.2 Current Implementation Files

| File | Component | Lines of Code |
|------|-----------|---------------|
| `App/ui/mainwindow.cpp` | Word list initialization, QCompleter setup | ~50 lines |
| `App/core/textarea.cpp` | Completer integration, key handling | ~80 lines |
| `App/core/textarea.h` | Completer member variable and methods | ~10 lines |
| `App/lsp/lspclient.cpp` | LSP completion (unused in UI) | ~50 lines |
| `App/plugins/isyntaxplugin.h` | Plugin `keywords()` interface (unused) | ~5 lines |

### 1.3 Limitations of Current System

| Issue | Description | Impact |
|-------|-------------|--------|
| **No Language Filtering** | Shows all keywords regardless of file type | Poor UX - Python keywords in .cpp files |
| **Hardcoded Word List** | Keywords defined inline in mainwindow.cpp | Difficult to maintain and extend |
| **No Context Awareness** | Cannot suggest variable names, functions | Limited utility for actual coding |
| **Single Completer Instance** | Shared across all editor tabs | Cannot have language-specific behavior |
| **Unused LSP Integration** | LspClient has completion but not connected to UI | Wasted infrastructure |
| **No Plugin Integration** | ISyntaxPlugin::keywords() never called | Plugin system underutilized |
| **No Priority/Ranking** | Simple alphabetical ordering | Relevant suggestions not prioritized |
| **No Snippet Support** | Plain text insertion only | Cannot expand templates |
| **No Documentation** | No inline docs for suggestions | Users must look up APIs |
| **Tight Coupling** | QCompleter embedded in MainWindow | Hard to test and modify |

---

## 2. Proposed Architecture

### 2.1 High-Level Design

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         CompletionEngine (New)                               │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                    CompletionProviderRegistry                        │   │
│  │  Manages multiple completion providers, prioritizes and merges       │   │
│  └──────────────────────────────┬──────────────────────────────────────┘   │
│                                 │                                           │
│      ┌──────────────────────────┼──────────────────────────────┐           │
│      │                          │                              │           │
│      ▼                          ▼                              ▼           │
│  ┌────────────┐          ┌────────────┐               ┌────────────┐       │
│  │ Keyword    │          │ LSP        │               │ Snippet    │       │
│  │ Provider   │          │ Provider   │               │ Provider   │       │
│  └────────────┘          └────────────┘               └────────────┘       │
│      │                          │                              │           │
│      ▼                          ▼                              ▼           │
│  ┌────────────┐          ┌────────────┐               ┌────────────┐       │
│  │ Language   │          │ LspClient  │               │ Snippet    │       │
│  │ Keywords   │          │ (existing) │               │ Database   │       │
│  │ Registry   │          └────────────┘               └────────────┘       │
│  └────────────┘                                                            │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                      CompletionContext                               │   │
│  │  Document info, cursor position, language, prefix, trigger kind     │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                      CompletionItem                                  │   │
│  │  label, insertText, kind, detail, documentation, priority, snippet  │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                         CompletionWidget (New)                               │
│  Custom popup replacing QCompleter::popup()                                  │
│  - Rich item rendering (icons, detail, docs)                                │
│  - Fuzzy filtering                                                          │
│  - Keyboard navigation                                                      │
│  - Documentation preview panel                                              │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                        TextArea (Modified)                                   │
│  - Owns CompletionEngine instead of QCompleter                              │
│  - Minimal changes to key handling                                          │
│  - Delegates completion logic to engine                                     │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Core Components

#### 2.2.1 CompletionItem

A rich data structure for completion suggestions:

```cpp
// App/completion/completionitem.h
struct CompletionItem {
    QString label;              // Display text
    QString insertText;         // Text to insert (may differ for snippets)
    QString filterText;         // Text used for matching (default: label)
    CompletionItemKind kind;    // Function, Variable, Keyword, Snippet, etc.
    QString detail;             // Short detail (e.g., type signature)
    QString documentation;      // Full documentation (Markdown supported)
    int priority;               // Sorting priority (lower = higher priority)
    bool isSnippet;             // Whether insertText contains placeholders
    QString sortText;           // Custom sort key
    QIcon icon;                 // Optional custom icon

    enum CompletionItemKind {
        Text = 1, Method, Function, Constructor, Field, Variable,
        Class, Interface, Module, Property, Unit, Value, Enum,
        Keyword, Snippet, Color, File, Reference, Folder,
        EnumMember, Constant, Struct, Event, Operator, TypeParameter
    };
};
```

#### 2.2.2 CompletionContext

Context for completion requests:

```cpp
// App/completion/completioncontext.h
struct CompletionContext {
    QString documentUri;        // File URI
    QString languageId;         // Language identifier (cpp, python, js)
    QString prefix;             // Current word being typed
    int line;                   // Cursor line (0-based)
    int column;                 // Cursor column (0-based)
    QString lineText;           // Full text of current line
    TriggerKind triggerKind;    // How completion was triggered

    enum TriggerKind {
        Invoked,                // Explicit (Ctrl+Space)
        TriggerCharacter,       // Triggered by specific character (. -> ::)
        TriggerForIncomplete    // Re-triggered for incomplete results
    };
};
```

#### 2.2.3 ICompletionProvider Interface

Abstract interface for completion providers:

```cpp
// App/completion/icompletionprovider.h
class ICompletionProvider {
public:
    virtual ~ICompletionProvider() = default;

    // Provider metadata
    virtual QString id() const = 0;
    virtual QString displayName() const = 0;
    virtual int basePriority() const = 0;  // Default priority for items

    // Language support
    virtual QStringList supportedLanguages() const = 0;
    virtual bool supportsLanguage(const QString& languageId) const;

    // Trigger characters (e.g., "." for member access)
    virtual QStringList triggerCharacters() const { return {}; }

    // Completion request (async via signal/callback)
    virtual void requestCompletions(
        const CompletionContext& context,
        std::function<void(const QList<CompletionItem>&)> callback
    ) = 0;

    // Optional: resolve additional details for an item
    virtual void resolveItem(
        const CompletionItem& item,
        std::function<void(const CompletionItem&)> callback
    ) { callback(item); }
};
```

#### 2.2.4 CompletionProviderRegistry

Central registry managing all providers:

```cpp
// App/completion/completionproviderregistry.h
class CompletionProviderRegistry : public QObject {
    Q_OBJECT

public:
    static CompletionProviderRegistry& instance();

    void registerProvider(std::shared_ptr<ICompletionProvider> provider);
    void unregisterProvider(const QString& providerId);

    QList<std::shared_ptr<ICompletionProvider>> providersForLanguage(
        const QString& languageId
    ) const;

    QStringList allTriggerCharacters(const QString& languageId) const;

signals:
    void providerRegistered(const QString& providerId);
    void providerUnregistered(const QString& providerId);

private:
    QList<std::shared_ptr<ICompletionProvider>> m_providers;
};
```

#### 2.2.5 CompletionEngine

Main completion orchestrator:

```cpp
// App/completion/completionengine.h
class CompletionEngine : public QObject {
    Q_OBJECT

public:
    explicit CompletionEngine(QObject* parent = nullptr);

    void setLanguage(const QString& languageId);
    void requestCompletions(const CompletionContext& context);
    void cancelPendingRequests();

    // Configuration
    void setMinimumPrefixLength(int length);  // Default: 1
    void setAutoTriggerDelay(int ms);         // Default: 300ms
    void setMaxResults(int count);            // Default: 100

signals:
    void completionsReady(const QList<CompletionItem>& items);
    void completionsFailed(const QString& error);

private:
    void mergeAndSortResults();
    void filterResults(const QString& prefix);

    QString m_languageId;
    QList<CompletionItem> m_pendingItems;
    int m_pendingProviders;
    QTimer* m_debounceTimer;
};
```

#### 2.2.6 CompletionWidget

Custom popup widget for displaying completions:

```cpp
// App/completion/completionwidget.h
class CompletionWidget : public QWidget {
    Q_OBJECT

public:
    explicit CompletionWidget(QWidget* parent = nullptr);

    void setItems(const QList<CompletionItem>& items);
    void setPrefix(const QString& prefix);
    void show(const QPoint& position);

    // Navigation
    void selectNext();
    void selectPrevious();
    void selectPageDown();
    void selectPageUp();
    CompletionItem selectedItem() const;

    // Appearance
    void setMaxVisibleItems(int count);  // Default: 10
    void setShowDocumentation(bool show);
    void setShowIcons(bool show);

signals:
    void itemSelected(const CompletionItem& item);
    void itemAccepted(const CompletionItem& item);
    void cancelled();

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    QListView* m_listView;
    CompletionItemModel* m_model;
    QLabel* m_documentationPanel;
    QString m_prefix;
};
```

### 2.3 Built-in Providers

#### 2.3.1 KeywordCompletionProvider

Provides language keywords from a configurable registry:

```cpp
// App/completion/providers/keywordcompletionprovider.h
class KeywordCompletionProvider : public ICompletionProvider {
public:
    QString id() const override { return "keywords"; }
    QString displayName() const override { return "Keywords"; }
    int basePriority() const override { return 100; }
    QStringList supportedLanguages() const override { return {"*"}; }

    void requestCompletions(
        const CompletionContext& context,
        std::function<void(const QList<CompletionItem>&)> callback
    ) override;

private:
    LanguageKeywordsRegistry* m_registry;
};

// Separate registry for keyword data
class LanguageKeywordsRegistry {
public:
    static LanguageKeywordsRegistry& instance();

    void registerLanguage(const QString& languageId, const QStringList& keywords);
    void loadFromFile(const QString& jsonPath);
    QStringList keywords(const QString& languageId) const;

private:
    QMap<QString, QStringList> m_keywords;
};
```

#### 2.3.2 LspCompletionProvider

Bridges to existing LspClient:

```cpp
// App/completion/providers/lspcompletionprovider.h
class LspCompletionProvider : public ICompletionProvider {
public:
    explicit LspCompletionProvider(LspClient* client);

    QString id() const override { return "lsp"; }
    QString displayName() const override { return "Language Server"; }
    int basePriority() const override { return 10; }  // Highest priority
    QStringList supportedLanguages() const override;
    QStringList triggerCharacters() const override;

    void requestCompletions(
        const CompletionContext& context,
        std::function<void(const QList<CompletionItem>&)> callback
    ) override;

    void resolveItem(
        const CompletionItem& item,
        std::function<void(const CompletionItem&)> callback
    ) override;

private:
    LspClient* m_lspClient;
    QMap<int, std::function<void(const QList<CompletionItem>&)>> m_pendingCallbacks;
};
```

#### 2.3.3 SnippetCompletionProvider

Provides user and language snippets:

```cpp
// App/completion/providers/snippetcompletionprovider.h
class SnippetCompletionProvider : public ICompletionProvider {
public:
    QString id() const override { return "snippets"; }
    QString displayName() const override { return "Snippets"; }
    int basePriority() const override { return 50; }
    QStringList supportedLanguages() const override { return {"*"}; }

    void requestCompletions(
        const CompletionContext& context,
        std::function<void(const QList<CompletionItem>&)> callback
    ) override;

    // Snippet management
    void loadSnippets(const QString& directory);
    void addSnippet(const QString& languageId, const Snippet& snippet);

private:
    SnippetRegistry* m_registry;
};
```

#### 2.3.4 PluginCompletionProvider

Integrates with ISyntaxPlugin::keywords():

```cpp
// App/completion/providers/plugincompletionprovider.h
class PluginCompletionProvider : public ICompletionProvider {
public:
    QString id() const override { return "plugins"; }
    QString displayName() const override { return "Plugin Keywords"; }
    int basePriority() const override { return 80; }
    QStringList supportedLanguages() const override;

    void requestCompletions(
        const CompletionContext& context,
        std::function<void(const QList<CompletionItem>&)> callback
    ) override;

private:
    SyntaxPluginRegistry* m_pluginRegistry;
};
```

---

## 3. Directory Structure

```
App/
├── completion/
│   ├── CMakeLists.txt
│   ├── completionitem.h
│   ├── completioncontext.h
│   ├── icompletionprovider.h
│   ├── completionproviderregistry.h
│   ├── completionproviderregistry.cpp
│   ├── completionengine.h
│   ├── completionengine.cpp
│   ├── completionwidget.h
│   ├── completionwidget.cpp
│   ├── completionitemmodel.h
│   ├── completionitemmodel.cpp
│   ├── completionitemstyler.h          // Icons, colors
│   ├── completionitemstyler.cpp
│   ├── languagekeywordsregistry.h
│   ├── languagekeywordsregistry.cpp
│   ├── snippetregistry.h
│   ├── snippetregistry.cpp
│   └── providers/
│       ├── keywordcompletionprovider.h
│       ├── keywordcompletionprovider.cpp
│       ├── lspcompletionprovider.h
│       ├── lspcompletionprovider.cpp
│       ├── snippetcompletionprovider.h
│       ├── snippetcompletionprovider.cpp
│       ├── plugincompletionprovider.h
│       └── plugincompletionprovider.cpp
├── data/
│   └── keywords/
│       ├── cpp.json
│       ├── python.json
│       ├── javascript.json
│       └── ...
```

---

## 4. Migration Strategy

### 4.1 Phase 1: Foundation (Week 1-2)

**Goal**: Create new completion infrastructure without modifying existing code.

**Tasks**:
1. Create `App/completion/` directory structure
2. Implement `CompletionItem` and `CompletionContext` data structures
3. Implement `ICompletionProvider` interface
4. Implement `CompletionProviderRegistry` singleton
5. Write unit tests for registry

**Files Created**:
- `App/completion/completionitem.h`
- `App/completion/completioncontext.h`
- `App/completion/icompletionprovider.h`
- `App/completion/completionproviderregistry.h`
- `App/completion/completionproviderregistry.cpp`
- `tests/unit/test_completionproviderregistry.cpp`

**Risk**: Low - No changes to existing code.

### 4.2 Phase 2: Keyword Provider (Week 2-3)

**Goal**: Migrate hardcoded word list to data-driven approach.

**Tasks**:
1. Create `LanguageKeywordsRegistry` class
2. Extract keywords from mainwindow.cpp to JSON files
3. Implement `KeywordCompletionProvider`
4. Write unit tests
5. Create JSON keyword files for C++, Python, JavaScript

**Files Created**:
- `App/completion/languagekeywordsregistry.h`
- `App/completion/languagekeywordsregistry.cpp`
- `App/completion/providers/keywordcompletionprovider.h`
- `App/completion/providers/keywordcompletionprovider.cpp`
- `App/data/keywords/cpp.json`
- `App/data/keywords/python.json`
- `App/data/keywords/javascript.json`
- `tests/unit/test_keywordcompletionprovider.cpp`

**Files Modified**:
- `App/CMakeLists.txt` (add new sources)
- `App/highlight.qrc` or new `keywords.qrc` (add JSON resources)

**Risk**: Low - Adding new code, not modifying.

### 4.3 Phase 3: Completion Engine (Week 3-4)

**Goal**: Implement the central completion orchestrator.

**Tasks**:
1. Implement `CompletionEngine` class
2. Implement async provider coordination
3. Implement result merging and sorting
4. Implement fuzzy filtering
5. Write comprehensive tests

**Files Created**:
- `App/completion/completionengine.h`
- `App/completion/completionengine.cpp`
- `tests/unit/test_completionengine.cpp`

**Risk**: Medium - Core logic, needs thorough testing.

### 4.4 Phase 4: Completion Widget (Week 4-5)

**Goal**: Create custom popup replacing QCompleter::popup().

**Tasks**:
1. Create `CompletionItemModel` (QAbstractListModel)
2. Create `CompletionWidget` with rich rendering
3. Implement icon support via `CompletionItemStyler`
4. Add documentation panel
5. Write widget tests

**Files Created**:
- `App/completion/completionwidget.h`
- `App/completion/completionwidget.cpp`
- `App/completion/completionitemmodel.h`
- `App/completion/completionitemmodel.cpp`
- `App/completion/completionitemstyler.h`
- `App/completion/completionitemstyler.cpp`
- `tests/unit/test_completionwidget.cpp`

**Risk**: Medium - UI work, needs visual testing.

### 4.5 Phase 5: LSP Integration (Week 5-6)

**Goal**: Connect existing LspClient to new completion system.

**Tasks**:
1. Implement `LspCompletionProvider`
2. Bridge LspCompletionItem to CompletionItem
3. Handle async LSP responses
4. Implement item resolution for documentation
5. Write integration tests

**Files Created**:
- `App/completion/providers/lspcompletionprovider.h`
- `App/completion/providers/lspcompletionprovider.cpp`
- `tests/unit/test_lspcompletionprovider.cpp`

**Files Modified**:
- None - Provider uses existing LspClient

**Risk**: Medium - Async coordination with LSP.

### 4.6 Phase 6: TextArea Integration (Week 6-7)

**Goal**: Replace QCompleter with new system in TextArea.

**Tasks**:
1. Add CompletionEngine member to TextArea
2. Modify keyPressEvent() to use new engine
3. Replace popup handling with CompletionWidget
4. Maintain backward compatibility during transition
5. Update insertCompletion() for snippets

**Files Modified**:
- `App/core/textarea.h` (replace QCompleter* with CompletionEngine*)
- `App/core/textarea.cpp` (update completion handling)

**Files Removed** (from TextArea):
- `m_completer` member variable
- `setCompleter()` / `completer()` methods (deprecated first)

**Risk**: High - Core editor modification.

### 4.7 Phase 7: MainWindow Cleanup (Week 7)

**Goal**: Remove old completion code from MainWindow.

**Tasks**:
1. Remove hardcoded wordList
2. Remove QCompleter creation
3. Initialize CompletionProviderRegistry with providers
4. Update setupTextArea() to configure engine

**Files Modified**:
- `App/ui/mainwindow.h` (remove QCompleter* member)
- `App/ui/mainwindow.cpp` (remove wordList initialization)

**Risk**: Medium - Removing established code.

### 4.8 Phase 8: Plugin Provider (Week 8)

**Goal**: Integrate ISyntaxPlugin::keywords() into completion.

**Tasks**:
1. Implement `PluginCompletionProvider`
2. Query SyntaxPluginRegistry for plugins
3. Cache plugin keywords appropriately
4. Write tests

**Files Created**:
- `App/completion/providers/plugincompletionprovider.h`
- `App/completion/providers/plugincompletionprovider.cpp`
- `tests/unit/test_plugincompletionprovider.cpp`

**Risk**: Low - Extension of new system.

### 4.9 Phase 9: Snippet Support (Week 8-9)

**Goal**: Add snippet expansion capability.

**Tasks**:
1. Define Snippet data structure
2. Implement SnippetRegistry with JSON loading
3. Implement SnippetCompletionProvider
4. Add snippet expansion to insertCompletion()
5. Support tabstop navigation

**Files Created**:
- `App/completion/snippetregistry.h`
- `App/completion/snippetregistry.cpp`
- `App/completion/providers/snippetcompletionprovider.h`
- `App/completion/providers/snippetcompletionprovider.cpp`
- `App/data/snippets/cpp.json`
- `App/data/snippets/python.json`
- `tests/unit/test_snippetcompletionprovider.cpp`

**Risk**: Medium - New feature with complex UI interactions.

### 4.10 Phase 10: Documentation & Cleanup (Week 9-10)

**Goal**: Finalize migration, update documentation.

**Tasks**:
1. Update AUTOCOMPLETION.md with new architecture
2. Add developer documentation for creating providers
3. Remove deprecated code paths
4. Performance optimization
5. Final integration testing

**Files Modified**:
- `App/AUTOCOMPLETION.md` (complete rewrite)

**Files Created**:
- `App/completion/README.md` (developer guide)

---

## 5. API Changes

### 5.1 Deprecated APIs

```cpp
// These will be deprecated in Phase 6, removed in Phase 10
TextArea::setCompleter(QCompleter* completer);  // Use setCompletionEngine()
TextArea::completer() const;                     // Use completionEngine()
```

### 5.2 New Public APIs

```cpp
// TextArea
void TextArea::setCompletionEngine(CompletionEngine* engine);
CompletionEngine* TextArea::completionEngine() const;
void TextArea::triggerCompletion();  // Programmatic trigger

// MainWindow / Application
CompletionProviderRegistry& Application::completionProviders();
void Application::registerCompletionProvider(std::shared_ptr<ICompletionProvider>);
```

---

## 6. Configuration

### 6.1 User Settings

Add to settings system:

```json
{
  "completion": {
    "autoTrigger": true,
    "autoTriggerDelay": 300,
    "minimumPrefixLength": 1,
    "maxResults": 100,
    "showDocumentation": true,
    "showIcons": true,
    "providers": {
      "lsp": { "enabled": true },
      "keywords": { "enabled": true },
      "snippets": { "enabled": true },
      "plugins": { "enabled": true }
    },
    "triggerCharacters": {
      "cpp": [".", "::", "->"],
      "python": ["."],
      "javascript": ["."]
    }
  }
}
```

### 6.2 Keyword JSON Format

```json
{
  "language": "cpp",
  "keywords": [
    {
      "label": "class",
      "kind": "Keyword",
      "detail": "C++ class declaration",
      "documentation": "Defines a class type"
    },
    {
      "label": "std::vector",
      "kind": "Class",
      "detail": "template<class T> class vector",
      "documentation": "Dynamic array container"
    }
  ]
}
```

### 6.3 Snippet JSON Format

```json
{
  "language": "cpp",
  "snippets": [
    {
      "prefix": "for",
      "label": "For Loop",
      "body": "for (${1:int} ${2:i} = 0; $2 < ${3:count}; $2++) {\n\t$0\n}",
      "description": "For loop with iterator"
    },
    {
      "prefix": "class",
      "label": "Class Definition",
      "body": "class ${1:ClassName} {\npublic:\n\t${1}();\n\t~${1}();\n\nprivate:\n\t$0\n};",
      "description": "C++ class with constructor and destructor"
    }
  ]
}
```

---

## 7. Testing Strategy

### 7.1 Unit Tests

| Component | Test File | Coverage Target |
|-----------|-----------|-----------------|
| CompletionProviderRegistry | test_completionproviderregistry.cpp | 95% |
| CompletionEngine | test_completionengine.cpp | 90% |
| KeywordCompletionProvider | test_keywordcompletionprovider.cpp | 90% |
| LspCompletionProvider | test_lspcompletionprovider.cpp | 85% |
| SnippetCompletionProvider | test_snippetcompletionprovider.cpp | 85% |
| LanguageKeywordsRegistry | test_languagekeywordsregistry.cpp | 90% |
| SnippetRegistry | test_snippetregistry.cpp | 90% |

### 7.2 Integration Tests

- TextArea + CompletionEngine integration
- Multi-provider result merging
- LSP completion with mock server
- Keyboard navigation in popup

### 7.3 Manual Testing Checklist

- [ ] Ctrl+Space triggers completion in all file types
- [ ] Auto-trigger works after typing 1+ characters
- [ ] Completions are filtered by language
- [ ] LSP suggestions appear for supported languages
- [ ] Snippets expand correctly with tabstops
- [ ] Documentation panel shows for selected items
- [ ] Arrow key navigation works in popup
- [ ] Enter/Tab accepts completion
- [ ] Escape dismisses popup
- [ ] Multi-cursor with completion works

---

## 8. Performance Considerations

### 8.1 Optimization Targets

| Metric | Target | Current |
|--------|--------|---------|
| Popup appearance | < 100ms | ~50ms |
| Typing responsiveness | < 16ms | Good |
| Memory per provider | < 1MB | N/A |
| Filter update | < 10ms | ~5ms |

### 8.2 Optimization Strategies

1. **Lazy Loading**: Load keyword files on first use per language
2. **Caching**: Cache LSP completions for same prefix
3. **Debouncing**: Delay provider requests while typing fast
4. **Virtual Rendering**: Only render visible items in popup
5. **Background Processing**: Move filtering to worker thread if needed

---

## 9. Risks and Mitigations

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Breaking existing completion | Medium | High | Gradual migration, keep old code until new is stable |
| LSP integration complexity | Medium | Medium | Reuse existing LspClient, add adapter layer |
| Performance regression | Low | High | Benchmark each phase, optimize early |
| UI inconsistency | Low | Medium | Use existing Qt styling patterns |
| Scope creep | Medium | Medium | Strict phase boundaries, defer nice-to-haves |

---

## 10. Success Metrics

1. **Functionality**: All current features work identically or better
2. **Extensibility**: Adding a new provider takes < 100 lines of code
3. **Performance**: No perceptible slowdown in completion popup
4. **Maintainability**: Single file changes don't require touching others
5. **Test Coverage**: > 80% coverage on new code
6. **User Satisfaction**: Positive feedback on language-specific completions

---

## 11. Future Extensions

After completing this redesign, the following features become straightforward:

1. **AI Completion Provider**: Add provider wrapping Copilot-style completions
2. **Semantic Completions**: Provider using AST for variable/function names
3. **Import Suggestions**: Auto-import completions
4. **Path Completion**: File path completions in strings
5. **Color Picker**: Color completions in CSS
6. **Emoji Completion**: `:emoji:` style completions in Markdown
7. **Custom User Providers**: Plugin API for external providers

---

## 12. Appendix

### 12.1 Comparison with Other Editors

| Feature | Lightpad Current | Lightpad Proposed | VS Code | Sublime Text |
|---------|-----------------|-------------------|---------|--------------|
| Provider Architecture | ❌ | ✅ | ✅ | ✅ |
| Language Filtering | ❌ | ✅ | ✅ | ✅ |
| LSP Integration | ❌ (code exists) | ✅ | ✅ | ✅ (LSP-based) |
| Snippet Support | ❌ | ✅ | ✅ | ✅ |
| Documentation Popup | ❌ | ✅ | ✅ | Partial |
| Fuzzy Matching | ❌ | ✅ | ✅ | ✅ |
| Icon Display | ❌ | ✅ | ✅ | ✅ |
| Priority Sorting | ❌ | ✅ | ✅ | ✅ |
| Plugin Providers | ❌ | ✅ | ✅ | ✅ |

### 12.2 References

- [Language Server Protocol Specification - Completion](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_completion)
- [VS Code Completion API](https://code.visualstudio.com/api/references/vscode-api#CompletionItemProvider)
- [Qt QCompleter Documentation](https://doc.qt.io/qt-6/qcompleter.html)
- [Sublime Text Completions](https://www.sublimetext.com/docs/completions.html)

---

## 13. Changelog

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2026-01-30 | Copilot | Initial design document |
