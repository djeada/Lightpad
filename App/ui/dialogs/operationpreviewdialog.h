#ifndef OPERATIONPREVIEWDIALOG_H
#define OPERATIONPREVIEWDIALOG_H

#include "../../git/gitoperationpreview.h"
#include "styleddialog.h"

class QCheckBox;
class QLabel;
class QListWidget;
class QPushButton;
class QVBoxLayout;

class OperationPreviewDialog : public StyledDialog {
  Q_OBJECT

public:
  OperationPreviewDialog(GitIntegration *git,
                         const GitOperationRequest &request, const Theme &theme,
                         QWidget *parent = nullptr);

  const GitOperationPreview &preview() const { return m_preview; }

  static bool confirm(GitIntegration *git, const GitOperationRequest &request,
                      const Theme &theme, QWidget *parent);

  static bool safeOperationsSkipConfirmation();
  static void setSafeOperationsSkipConfirmation(bool skip);

protected:
  void applyTheme(const Theme &theme) override;

private:
  void buildUi();
  void buildGraphs(QVBoxLayout *layout);
  void buildDetails(QVBoxLayout *layout);
  void fillGraph(QListWidget *list, const QList<PreviewNode> &nodes);

  GitIntegration *m_git;
  GitOperationPreview m_preview;

  QLabel *m_headlineLabel;
  QLabel *m_riskLabel;
  QLabel *m_rationaleLabel;
  QLabel *m_layersLabel;
  QLabel *m_beforeLabel;
  QLabel *m_afterLabel;
  QListWidget *m_beforeList;
  QListWidget *m_afterList;
  QListWidget *m_riskList;
  QLabel *m_commandLabel;
  QCheckBox *m_skipSafeCheck;
  QPushButton *m_proceedButton;
  QPushButton *m_cancelButton;
};

#endif
