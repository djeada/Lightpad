#ifndef HACKERLISTWIDGET_H
#define HACKERLISTWIDGET_H

#include "hackerwidget.h"
#include <QListWidget>
#include <QStyledItemDelegate>

class HackerListDelegate : public QStyledItemDelegate {
  Q_OBJECT
public:
  enum CustomRole {
    DescriptionRole = Qt::UserRole + 100,
    ShortcutRole    = Qt::UserRole + 101,
    IconRole        = Qt::UserRole + 102
  };

  explicit HackerListDelegate(QObject *parent = nullptr);

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;
  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override;
};

class HackerListWidget : public HackerWidget {
  Q_OBJECT
public:
  explicit HackerListWidget(QWidget *parent = nullptr);

  void addItem(const QString &text);
  void addItem(QListWidgetItem *item);
  QListWidgetItem *item(int row) const;
  int count() const;
  int currentRow() const;
  void setCurrentRow(int row);
  QListWidgetItem *currentItem() const;
  void clear();
  QListWidgetItem *takeItem(int row);
  void insertItem(int row, const QString &text);
  void insertItem(int row, QListWidgetItem *item);
  void scrollToItem(const QListWidgetItem *item);
  QListWidget *innerList() const { return m_list; }

  QSize sizeHint() const override;

signals:
  void currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
  void currentRowChanged(int currentRow);
  void itemActivated(QListWidgetItem *item);
  void itemClicked(QListWidgetItem *item);

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  QListWidget *m_list;
  HackerListDelegate *m_delegate;
  void setupList();
};

#endif
