#ifndef ICOMPLETIONPROVIDER_H
#define ICOMPLETIONPROVIDER_H

#include "completioncontext.h"
#include "completionitem.h"
#include <QList>
#include <QString>
#include <QStringList>
#include <functional>

class ICompletionProvider {
public:
  virtual ~ICompletionProvider() = default;

  virtual QString id() const = 0;

  virtual QString displayName() const = 0;

  virtual int basePriority() const = 0;

  virtual QStringList supportedLanguages() const = 0;

  virtual bool supportsLanguage(const QString &languageId) const {
    QStringList langs = supportedLanguages();
    if (langs.contains("*")) {
      return true;
    }
    return langs.contains(languageId, Qt::CaseInsensitive);
  }

  virtual QStringList triggerCharacters() const { return {}; }

  virtual int minimumPrefixLength() const { return 1; }

  virtual void requestCompletions(
      const CompletionContext &context,
      std::function<void(const QList<CompletionItem> &)> callback) = 0;

  virtual void
  resolveItem(const CompletionItem &item,
              std::function<void(const CompletionItem &)> callback) {
    callback(item);
  }

  virtual void cancelPendingRequests() {}

  virtual bool isEnabled() const { return true; }

  virtual void setEnabled(bool enabled) { Q_UNUSED(enabled); }
};

#endif
