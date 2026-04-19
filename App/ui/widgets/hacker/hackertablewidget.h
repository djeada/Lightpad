#ifndef HACKERTABLEWIDGET_H
#define HACKERTABLEWIDGET_H

#include "hackerwidget.h"
#include <QHeaderView>
#include <QStyledItemDelegate>
#include <QTableWidget>

class HackerTableDelegate : public QStyledItemDelegate {
  Q_OBJECT
public:
  explicit HackerTableDelegate(QObject *parent = nullptr);

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;
};

class HackerTableHeader : public QHeaderView {
  Q_OBJECT
public:
  explicit HackerTableHeader(Qt::Orientation orientation,
                             QWidget *parent = nullptr);

protected:
  void paintSection(QPainter *painter, const QRect &rect,
                    int logicalIndex) const override;
};

class HackerTableWidget : public HackerWidget {
  Q_OBJECT
public:
  explicit HackerTableWidget(QWidget *parent = nullptr);
  explicit HackerTableWidget(int rows, int columns, QWidget *parent = nullptr);

  void setRowCount(int rows);
  void setColumnCount(int columns);
  int rowCount() const;
  int columnCount() const;
  void setItem(int row, int column, QTableWidgetItem *item);
  QTableWidgetItem *item(int row, int column) const;
  void setHorizontalHeaderLabels(const QStringList &labels);
  void setVerticalHeaderLabels(const QStringList &labels);
  void setColumnWidth(int column, int width);
  void setRowHeight(int row, int height);
  void clear();
  void clearContents();
  void setSelectionMode(QAbstractItemView::SelectionMode mode);
  void setSelectionBehavior(QAbstractItemView::SelectionBehavior behavior);
  void setAlternatingRowColors(bool enable);
  void setSortingEnabled(bool enable);
  QTableWidget *innerTable() const { return m_table; }

  QSize sizeHint() const override;

signals:
  void cellClicked(int row, int column);
  void cellDoubleClicked(int row, int column);
  void currentCellChanged(int currentRow, int currentColumn, int previousRow,
                          int previousColumn);
  void itemSelectionChanged();

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  QTableWidget *m_table;
  HackerTableDelegate *m_delegate;
  HackerTableHeader *m_horizontalHeader;
  void setupTable();
  void setupTable(int rows, int columns);
};

#endif
