#ifndef SQLCOMPLETIONPROVIDER_H
#define SQLCOMPLETIONPROVIDER_H

#include "../icompletionprovider.h"

#include <functional>

class SqlCompletionProvider : public ICompletionProvider {
public:
  using DocumentTextFn = std::function<QString(const QString &documentUri)>;

  SqlCompletionProvider() = default;

  QString id() const override { return "sql"; }
  QString displayName() const override { return "SQL"; }
  int basePriority() const override { return 5; }
  QStringList supportedLanguages() const override { return {"sql"}; }
  QStringList triggerCharacters() const override { return {"."}; }
  int minimumPrefixLength() const override { return 1; }

  void requestCompletions(
      const CompletionContext &context,
      std::function<void(const QList<CompletionItem> &)> callback) override;

  bool isEnabled() const override { return m_enabled; }
  void setEnabled(bool enabled) override { m_enabled = enabled; }

  void setDocumentTextProvider(DocumentTextFn fn) {
    m_documentText = std::move(fn);
  }

private:
  bool m_enabled = true;
  DocumentTextFn m_documentText;
};

#endif
