#ifndef SNIPPETCOMPLETIONPROVIDER_H
#define SNIPPETCOMPLETIONPROVIDER_H

#include "../icompletionprovider.h"

/**
 * @brief Completion provider for code snippets
 *
 * Provides snippet completions from the SnippetRegistry.
 */
class SnippetCompletionProvider : public ICompletionProvider {
public:
  SnippetCompletionProvider();

  QString id() const override { return "snippets"; }
  QString displayName() const override { return "Snippets"; }
  int basePriority() const override { return 50; }
  QStringList supportedLanguages() const override { return {"*"}; }
  int minimumPrefixLength() const override { return 2; }

  void requestCompletions(
      const CompletionContext &context,
      std::function<void(const QList<CompletionItem> &)> callback) override;

  bool isEnabled() const override { return m_enabled; }
  void setEnabled(bool enabled) override { m_enabled = enabled; }

private:
  bool m_enabled = true;
};

#endif // SNIPPETCOMPLETIONPROVIDER_H
