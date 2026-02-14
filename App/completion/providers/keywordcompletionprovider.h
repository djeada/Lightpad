#ifndef KEYWORDCOMPLETIONPROVIDER_H
#define KEYWORDCOMPLETIONPROVIDER_H

#include "../icompletionprovider.h"

class KeywordCompletionProvider : public ICompletionProvider {
public:
  KeywordCompletionProvider();

  QString id() const override { return "keywords"; }
  QString displayName() const override { return "Keywords"; }
  int basePriority() const override { return 100; }
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

#endif
