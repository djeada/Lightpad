#ifndef QUERYHISTORYDIALOG_H
#define QUERYHISTORYDIALOG_H

#include "../../database/queryhistory.h"
#include "styleddialog.h"

class QueryHistoryDialog : public StyledDialog {
  Q_OBJECT

public:
  explicit QueryHistoryDialog(QueryHistory *history, QWidget *parent = nullptr);

  void setConnectionNames(const QStringList &names);
  void applyTheme(const Theme &theme) override;
  void reload();

signals:
  void insertRequested(const QString &sql);
  void runRequested(const QString &sql, const QString &connectionName);

private slots:
  void onSelectionChanged();
  void clearAll();

private:
  const QueryHistoryEntry *selectedEntry() const;

  QueryHistory *m_history;
  QLineEdit *m_search;
  QComboBox *m_connection;
  QListWidget *m_list;
  QPlainTextEdit *m_preview;
  QLabel *m_meta;
  QPushButton *m_insert;
  QPushButton *m_run;
  QPushButton *m_copy;
  QPushButton *m_clear;
  QVector<QueryHistoryEntry> m_shown;
};

#endif
