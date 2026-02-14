#ifndef COMPLETIONCONTEXT_H
#define COMPLETIONCONTEXT_H

#include <QString>

enum class CompletionTriggerKind {

  Invoked = 1,

  TriggerCharacter = 2,

  TriggerForIncomplete = 3
};

struct CompletionContext {

  QString documentUri;

  QString languageId;

  QString prefix;

  int line = 0;

  int column = 0;

  QString lineText;

  QString triggerCharacter;

  CompletionTriggerKind triggerKind = CompletionTriggerKind::Invoked;

  bool isAutoComplete = false;

  bool isValid() const {

    return !prefix.isEmpty() ||
           triggerKind == CompletionTriggerKind::TriggerCharacter;
  }

  bool isLanguage(const QString &lang) const {
    return languageId.compare(lang, Qt::CaseInsensitive) == 0;
  }

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

#endif
