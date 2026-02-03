#include "completionitemmodel.h"

CompletionItemModel::CompletionItemModel(QObject *parent)
    : QAbstractListModel(parent) {}

int CompletionItemModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return m_items.size();
}

QVariant CompletionItemModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
    return QVariant();
  }

  const CompletionItem &item = m_items.at(index.row());

  switch (role) {
  case LabelRole:
    return item.label;
  case DetailRole:
    return item.detail;
  case DocumentationRole:
    return item.documentation;
  case KindRole:
    return static_cast<int>(item.kind);
  case InsertTextRole:
    return item.effectiveInsertText();
  case IsSnippetRole:
    return item.isSnippet;
  case PriorityRole:
    return item.priority;
  case IconRole:
    if (!item.icon.isNull()) {
      return item.icon;
    }
    return iconForKind(item.kind);
  default:
    return QVariant();
  }
}

void CompletionItemModel::setItems(const QList<CompletionItem> &items) {
  beginResetModel();
  m_items = items;
  endResetModel();
}

void CompletionItemModel::clear() {
  beginResetModel();
  m_items.clear();
  endResetModel();
}

CompletionItem CompletionItemModel::itemAt(int index) const {
  if (index < 0 || index >= m_items.size()) {
    return CompletionItem();
  }
  return m_items.at(index);
}

QIcon CompletionItemModel::iconForKind(CompletionItemKind kind) const {
  // Return a simple colored icon based on kind
  // In a real implementation, these would be loaded from resources
  QString iconName;
  switch (kind) {
  case CompletionItemKind::Text:
    iconName = "text";
    break;
  case CompletionItemKind::Method:
  case CompletionItemKind::Function:
    iconName = "function";
    break;
  case CompletionItemKind::Constructor:
    iconName = "constructor";
    break;
  case CompletionItemKind::Field:
  case CompletionItemKind::Variable:
    iconName = "variable";
    break;
  case CompletionItemKind::Class:
  case CompletionItemKind::Interface:
  case CompletionItemKind::Struct:
    iconName = "class";
    break;
  case CompletionItemKind::Module:
    iconName = "module";
    break;
  case CompletionItemKind::Property:
    iconName = "property";
    break;
  case CompletionItemKind::Enum:
  case CompletionItemKind::EnumMember:
    iconName = "enum";
    break;
  case CompletionItemKind::Keyword:
    iconName = "keyword";
    break;
  case CompletionItemKind::Snippet:
    iconName = "snippet";
    break;
  case CompletionItemKind::Constant:
    iconName = "constant";
    break;
  default:
    iconName = "text";
    break;
  }

  // Try to load from resources, return empty if not found
  QString path = QString(":/icons/completion/%1.png").arg(iconName);
  QIcon icon(path);
  return icon;
}
