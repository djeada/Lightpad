#ifndef DEBUGVALUEINSPECTOR_H
#define DEBUGVALUEINSPECTOR_H

#include "../../dap/dapclient.h"
#include "../../settings/theme.h"
#include <QHash>
#include <QPointer>
#include <QWidget>

class QLabel;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTabWidget;
class QTableWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QSpinBox;

class DebugValueInspector : public QWidget {
  Q_OBJECT
public:
  explicit DebugValueInspector(QWidget *parent = nullptr);
  void setDapClient(DapClient *client);
  void setValue(const DapVariable &value, int frameId = -1,
                const QString &lengthExpression = QString());
  void focusArrayControls();
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
  bool isNative() const;
  void applyArrayView();
  void viewArray();
  void loadArrayPage(QTreeWidgetItem *parent, int start);
  void loadStringPage(QTreeWidgetItem *parent, int start);
  void receiveElement(int seq, const DapVariable &value);
  void refreshDisplay();
  void clearValue();
  void renderValue(const DapVariable &value, bool preserveView = false,
                   const QString &lengthExpression = {});
  void refreshValue(bool explicitRequest = false);
  void receiveFrames(int threadId, const QList<DapStackFrame> &frames);
  void bindFrame(const QList<DapStackFrame> &frames);
  void rememberView();
  void restoreView();
  QString itemPath(const QTreeWidgetItem *item) const;
  void updateText();
  void updateItem(QTreeWidgetItem *item, const DapVariable &value);
  QString displayValue(const QString &value, const QString &type) const;
  QString itemDisplayValue(const QTreeWidgetItem *item) const;
  static bool isIndexed(const DapVariable &value);
  static bool isIndex(const QString &name);
  static qlonglong indexForName(QString name);
  QTreeWidgetItem *appendValue(QTreeWidgetItem *parent,
                               const DapVariable &value);

  QPointer<DapClient> m_client;
  QString m_expression;
  QString m_valueName;
  QString m_frameName;
  QString m_frameSource;
  int m_threadId = 0;
  int m_frameFromBottom = -1;
  int m_refreshRequest = 0;
  bool m_waitingForFrame = false;
  bool m_explicitRefresh = false;
  QCheckBox *m_liveUpdates = nullptr;
  QPushButton *m_refresh = nullptr;
  struct NodeView {
    int children = 0;
    bool expanded = false;
    bool loaded = false;
  };
  QHash<QString, NodeView> m_savedNodes;
  QString m_selectedPath;
  bool m_restoringView = false;
  bool m_restoreView = false;
  int m_treeScroll = 0;
  int m_tableScroll = 0;
  int m_savedTab = 0;
  DapVariable m_value;
  QHash<int, Request> m_requests;
  QHash<int, QTreeWidgetItem *> m_elementRequests;
  int m_frameId = -1;
  int m_lengthRequest = 0;
  bool m_lengthResolved = false;
  bool m_bufferView = false;
  int m_bufferColumns = 1;
  int m_bufferLayout = 0;
  int m_readErrors = 0;
  QString m_bufferElementType;
  QByteArray m_stringBytes;
  bool m_stringView = false;
  QWidget *m_arrayControls = nullptr;
  QLineEdit *m_arrayExpression = nullptr;
  QLineEdit *m_lengthExpression = nullptr;
  QComboBox *m_arrayLayout = nullptr;
  QSpinBox *m_arrayCount = nullptr;
  QSpinBox *m_arrayColumns = nullptr;
  QComboBox *m_elementType = nullptr;
  QComboBox *m_displayFormat = nullptr;
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
