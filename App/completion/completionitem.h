#ifndef COMPLETIONITEM_H
#define COMPLETIONITEM_H

#include <QIcon>
#include <QString>

enum class CompletionItemKind {
  Text = 1,
  Method = 2,
  Function = 3,
  Constructor = 4,
  Field = 5,
  Variable = 6,
  Class = 7,
  Interface = 8,
  Module = 9,
  Property = 10,
  Unit = 11,
  Value = 12,
  Enum = 13,
  Keyword = 14,
  Snippet = 15,
  Color = 16,
  File = 17,
  Reference = 18,
  Folder = 19,
  EnumMember = 20,
  Constant = 21,
  Struct = 22,
  Event = 23,
  Operator = 24,
  TypeParameter = 25
};

struct CompletionItem {

  QString label;

  QString insertText;

  QString filterText;

  CompletionItemKind kind = CompletionItemKind::Text;

  QString detail;

  QString documentation;

  int priority = 100;

  bool isSnippet = false;

  QString sortText;

  QIcon icon;

  QString providerId;

  QString effectiveFilterText() const {
    return filterText.isEmpty() ? label : filterText;
  }

  QString effectiveSortText() const {
    return sortText.isEmpty() ? label : sortText;
  }

  QString effectiveInsertText() const {
    return insertText.isEmpty() ? label : insertText;
  }

  bool operator<(const CompletionItem &other) const {
    if (priority != other.priority) {
      return priority < other.priority;
    }
    return effectiveSortText().compare(other.effectiveSortText(),
                                       Qt::CaseInsensitive) < 0;
  }

  bool operator==(const CompletionItem &other) const {
    return label == other.label && providerId == other.providerId;
  }
};

#endif
