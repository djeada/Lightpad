#ifndef COMPLETIONITEMMODEL_H
#define COMPLETIONITEMMODEL_H

#include "completionitem.h"
#include <QAbstractListModel>
#include <QList>

class CompletionItemModel : public QAbstractListModel {
  Q_OBJECT

public:
  enum Roles {
    LabelRole = Qt::DisplayRole,
    DetailRole = Qt::UserRole,
    DocumentationRole,
    KindRole,
    InsertTextRole,
    IsSnippetRole,
    PriorityRole,
    IconRole = Qt::DecorationRole
  };

  explicit CompletionItemModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;

  void setItems(const QList<CompletionItem> &items);

  void clear();

  CompletionItem itemAt(int index) const;

  int count() const { return m_items.size(); }

private:
  QIcon iconForKind(CompletionItemKind kind) const;

  QList<CompletionItem> m_items;
};

#endif
