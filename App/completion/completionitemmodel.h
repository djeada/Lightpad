#ifndef COMPLETIONITEMMODEL_H
#define COMPLETIONITEMMODEL_H

#include "completionitem.h"
#include <QAbstractListModel>
#include <QList>

/**
 * @brief Model for completion items in a list view
 *
 * Provides a Qt model interface for displaying CompletionItems
 * in a QListView or similar widget.
 */
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

  /**
   * @brief Set the completion items
   * @param items Items to display
   */
  void setItems(const QList<CompletionItem> &items);

  /**
   * @brief Clear all items
   */
  void clear();

  /**
   * @brief Get item at index
   * @param index Row index
   * @return Item at index, or invalid item if out of range
   */
  CompletionItem itemAt(int index) const;

  /**
   * @brief Get number of items
   */
  int count() const { return m_items.size(); }

private:
  QIcon iconForKind(CompletionItemKind kind) const;

  QList<CompletionItem> m_items;
};

#endif // COMPLETIONITEMMODEL_H
