#ifndef COMPLETIONCONTEXT_H
#define COMPLETIONCONTEXT_H

#include <QString>

/**
 * @brief How the completion was triggered
 */
enum class CompletionTriggerKind {
  /**
   * @brief Explicitly invoked (e.g., Ctrl+Space)
   */
  Invoked = 1,

  /**
   * @brief Triggered by a specific character (e.g., '.', '::', '->')
   */
  TriggerCharacter = 2,

  /**
   * @brief Re-triggered for incomplete results
   *
   * Used when the initial completion result was marked as incomplete
   * and more typing refined the context.
   */
  TriggerForIncomplete = 3
};

/**
 * @brief Context for completion requests
 *
 * Contains all information needed by completion providers to generate
 * relevant suggestions, including document location, language, and
 * the current word being typed.
 */
struct CompletionContext {
  /**
   * @brief URI of the document (file path or virtual URI)
   *
   * Format: file:///path/to/file.cpp or untitled:1
   */
  QString documentUri;

  /**
   * @brief Language identifier for the document
   *
   * Examples: "cpp", "python", "javascript", "rust"
   * Should match the language IDs used by syntax plugins.
   */
  QString languageId;

  /**
   * @brief Current word prefix being typed
   *
   * The portion of the word before the cursor that should be
   * used for filtering suggestions.
   */
  QString prefix;

  /**
   * @brief Cursor line position (0-based)
   */
  int line = 0;

  /**
   * @brief Cursor column position (0-based)
   */
  int column = 0;

  /**
   * @brief Full text of the current line
   *
   * Useful for providers that need additional context
   * beyond just the prefix.
   */
  QString lineText;

  /**
   * @brief Character that triggered completion (if TriggerCharacter)
   *
   * Empty for Invoked trigger kind.
   */
  QString triggerCharacter;

  /**
   * @brief How the completion was triggered
   */
  CompletionTriggerKind triggerKind = CompletionTriggerKind::Invoked;

  /**
   * @brief Whether this is an auto-completion (vs explicit)
   *
   * true for automatic popup while typing,
   * false for explicit Ctrl+Space invocation.
   */
  bool isAutoComplete = false;

  /**
   * @brief Check if context is valid for completion
   * @return true if minimum required fields are set
   */
  bool isValid() const {
    // At minimum we need a prefix to filter against
    // Language ID is optional but recommended
    return !prefix.isEmpty() ||
           triggerKind == CompletionTriggerKind::TriggerCharacter;
  }

  /**
   * @brief Check if this context is for a specific language
   * @param lang Language ID to check
   * @return true if context matches the language
   */
  bool isLanguage(const QString &lang) const {
    return languageId.compare(lang, Qt::CaseInsensitive) == 0;
  }

  /**
   * @brief Create a context for explicit invocation
   * @param uri Document URI
   * @param lang Language ID
   * @param prefix Current word prefix
   * @param line Cursor line
   * @param col Cursor column
   * @return Configured context
   */
  static CompletionContext createInvoked(const QString &uri,
                                         const QString &lang,
                                         const QString &prefix, int line,
                                         int col) {
    CompletionContext ctx;
    ctx.documentUri = uri;
    ctx.languageId = lang;
    ctx.prefix = prefix;
    ctx.line = line;
    ctx.column = col;
    ctx.triggerKind = CompletionTriggerKind::Invoked;
    ctx.isAutoComplete = false;
    return ctx;
  }

  /**
   * @brief Create a context for trigger character
   * @param uri Document URI
   * @param lang Language ID
   * @param trigger The trigger character
   * @param line Cursor line
   * @param col Cursor column
   * @return Configured context
   */
  static CompletionContext createTriggerChar(const QString &uri,
                                             const QString &lang,
                                             const QString &trigger, int line,
                                             int col) {
    CompletionContext ctx;
    ctx.documentUri = uri;
    ctx.languageId = lang;
    ctx.triggerCharacter = trigger;
    ctx.line = line;
    ctx.column = col;
    ctx.triggerKind = CompletionTriggerKind::TriggerCharacter;
    ctx.isAutoComplete = true;
    return ctx;
  }
};

#endif // COMPLETIONCONTEXT_H
