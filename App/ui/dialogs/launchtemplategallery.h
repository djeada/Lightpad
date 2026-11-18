#ifndef LAUNCHTEMPLATEGALLERY_H
#define LAUNCHTEMPLATEGALLERY_H

#include "../../run_templates/launchtemplatecatalog.h"
#include "styleddialog.h"

class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

// Picks a ready-made launch configuration: suggestions for the current file
// and project first, then every template grouped by category, filterable.
class LaunchTemplateGallery : public StyledDialog {
  Q_OBJECT
  friend class TestLaunchConfigurations;

public:
  LaunchTemplateGallery(const QString &languageId,
                        const QString &workspaceFolder,
                        QWidget *parent = nullptr);

  QString selectedTemplateId() const;

  void applyTheme(const Theme &theme) override;

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  void populate();
  void applyFilter(const QString &text);
  void updateDetails();
  void selectFirstVisibleTemplate();

  QString m_languageId;
  QString m_workspaceFolder;
  QLineEdit *m_search = nullptr;
  QTreeWidget *m_tree = nullptr;
  QLabel *m_title = nullptr;
  QLabel *m_description = nullptr;
  QLabel *m_preview = nullptr;
  QLabel *m_badge = nullptr;
  QPushButton *m_createButton = nullptr;
  QPushButton *m_cancelButton = nullptr;
};

#endif
