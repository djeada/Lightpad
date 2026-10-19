#ifndef DEBUGVALUEINSPECTOR_H
#define DEBUGVALUEINSPECTOR_H

#include "../../dap/dapclient.h"
#include "../../settings/theme.h"
#include <QHash>
#include <QPointer>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTabWidget;
class QTableWidget;
class QTreeWidget;
class QTreeWidgetItem;

class DebugValueInspector : public QWidget {
  Q_OBJECT
public:
  explicit DebugValueInspector(QWidget *parent = nullptr);
  void setDapClient(DapClient *client);
  void setValue(const DapVariable &value);
  void clear();
  void invalidate();
  void applyTheme(const Theme &theme);

signals:
  void addWatchRequested(const QString &expression);

private:
  struct Request {
    QTreeWidgetItem *item = nullptr;
    int start = 0;
    bool prefixRetry = false;
  };
  void loadChildren(QTreeWidgetItem *item, int start = 0,
                    int referenceOverride = 0);
  void receiveChildren(int seq, const QList<DapVariable> &variables);
  void updateTable();
  void filterTree();
  void copySelection();
  static bool isIndexed(const DapVariable &value);
  static bool isIndex(const QString &name);
  static qlonglong indexForName(QString name);
  QTreeWidgetItem *appendValue(QTreeWidgetItem *parent,
                               const DapVariable &value);

  QPointer<DapClient> m_client;
  DapVariable m_value;
  QHash<int, Request> m_requests;
  QLabel *m_title = nullptr;
  QLabel *m_status = nullptr;
  QLineEdit *m_filter = nullptr;
  QTabWidget *m_tabs = nullptr;
  QTreeWidget *m_tree = nullptr;
  QTableWidget *m_table = nullptr;
  QPlainTextEdit *m_text = nullptr;
  QPushButton *m_watch = nullptr;
  QTreeWidgetItem *m_root = nullptr;
  bool m_live = false;
  bool m_autoTable = true;
  int m_itemCount = 0;
};

#endif
