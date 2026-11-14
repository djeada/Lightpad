#ifndef DBSTORAGEVIEW_H
#define DBSTORAGEVIEW_H

#include "../../database/dbconnection.h"
#include "../../settings/theme.h"
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QSet>
#include <QTreeWidget>

class DbStorageView : public QWidget {
  Q_OBJECT
public:
  explicit DbStorageView(QWidget *parent = nullptr);
  void setConnection(DbConnection *connection);
  void selectTable(const QString &schema, const QString &table);
  void applyTheme(const Theme &theme);
  void fitColumns();
signals:
  void browseTableRequested(const QString &schema, const QString &table);
  void countRowsRequested(const QString &schema, const QString &table);

private:
  void refresh();
  void updateActions();
  QPointer<DbConnection> m_connection;
  QSet<quint64> m_requests;
  QTreeWidget *m_tree;
  QLabel *m_summary;
  QLabel *m_status;
  QPushButton *m_refresh;
  QPushButton *m_browse;
  QPushButton *m_count;
};
#endif
