#ifndef DBRESULTVIEW_H
#define DBRESULTVIEW_H

#include "../../database/dbtypes.h"
#include "../../database/resultexporter.h"
#include "../../settings/theme.h"

#include <QAbstractTableModel>
#include <QLabel>
#include <QLineEdit>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QToolButton>
#include <QWidget>

class DbResultModel : public QAbstractTableModel {
  Q_OBJECT

public:
  explicit DbResultModel(QObject *parent = nullptr);

  void setResult(const DbResultSet &result);
  const DbResultSet &result() const { return m_result; }
  void setTheme(const Theme &theme);

  int rowCount(const QModelIndex &parent = {}) const override;
  int columnCount(const QModelIndex &parent = {}) const override;
  QVariant data(const QModelIndex &index, int role) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role) const override;
  bool isNumericColumn(int column) const;

  enum Roles { RawValueRole = Qt::UserRole + 1, IsNullRole };

private:
  DbResultSet m_result;
  QVector<bool> m_numeric;
  Theme m_theme;
};

class DbResultSortProxy : public QSortFilterProxyModel {
  Q_OBJECT

public:
  using QSortFilterProxyModel::QSortFilterProxyModel;
  void setFilterText(const QString &text);
  QString filterText() const { return m_filter; }

protected:
  bool lessThan(const QModelIndex &left,
                const QModelIndex &right) const override;
  bool filterAcceptsRow(int row, const QModelIndex &parent) const override;

private:
  QString m_filter;
};

class DbResultView : public QWidget {
  Q_OBJECT

public:
  explicit DbResultView(QWidget *parent = nullptr);

  void setResult(const DbResultSet &result, DbEngine engine,
                 const QString &tableName = QString(), qint64 elapsedMs = -1);
  const DbResultSet &result() const { return m_model->result(); }
  void applyTheme(const Theme &theme);

  QTableView *table() const { return m_table; }
  DbResultModel *model() const { return m_model; }
  DbResultSortProxy *proxy() const { return m_proxy; }

  QString selectionAs(ResultExporter::Format format, bool withHeader) const;
  bool exportToFile(const QString &path, ResultExporter::Format format) const;

signals:
  void insightsRequested();
  void statusMessage(const QString &text);

private slots:
  void copySelection(bool withHeader);
  void copyAs(ResultExporter::Format format);
  void showCellValue(const QModelIndex &index);
  void showContextMenu(const QPoint &pos);
  void updateSummary();

private:
  void selectedRowsAndColumns(QVector<int> *rows, QVector<int> *columns) const;
  void fitColumns();

  DbResultModel *m_model;
  DbResultSortProxy *m_proxy;
  QTableView *m_table;
  QLineEdit *m_filter;
  QLabel *m_summary;
  QToolButton *m_copyButton;
  QToolButton *m_exportButton;
  QToolButton *m_insightsButton;
  DbEngine m_engine = DbEngine::PostgreSql;
  QString m_tableName;
  qint64 m_elapsedMs = -1;
  Theme m_theme;
};

#endif
