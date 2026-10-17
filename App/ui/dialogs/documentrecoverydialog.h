#ifndef DOCUMENTRECOVERYDIALOG_H
#define DOCUMENTRECOVERYDIALOG_H

#include "../../core/io/recoverystore.h"
#include "styleddialog.h"
#include <functional>

class QTreeWidget;
class QLineEdit;
class QLabel;
class QPlainTextEdit;
class QPushButton;

class DocumentRecoveryDialog : public StyledDialog {
  Q_OBJECT
public:
  using Action = std::function<bool(const Recovery::Draft &, QString *)>;
  explicit DocumentRecoveryDialog(QWidget *parent = nullptr);
  void setActions(std::function<Recovery::Scan()> provider, Action restore,
                  Action discard);
  void applyTheme(const Theme &theme) override;
  void refresh();

private:
  std::function<Recovery::Scan()> m_provider;
  Action m_restore, m_discard;
  QVector<Recovery::Draft> m_drafts;
  QStringList m_warnings;
  QTreeWidget *m_list;
  QLineEdit *m_filter;
  QPlainTextEdit *m_preview;
  QLabel *m_title, *m_description, *m_details, *m_status;
  QPushButton *m_restoreButton, *m_discardButton, *m_warningButton;
  QVector<Recovery::Draft> selected() const;
  void updateSelection();
  void showPreview();
  void runAction(bool restore);
};
#endif
