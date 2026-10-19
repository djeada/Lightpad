#ifndef GITDIALOG_H
#define GITDIALOG_H

#include "../gitui.h"
#include "styleddialog.h"

class GitDialog : public StyledDialog {
public:
  explicit GitDialog(QWidget *parent = nullptr);
  using StyledDialog::applyTheme;
  void applyTheme(const Theme &theme) override;

protected:
  void showEvent(QShowEvent *event) override;
  void setGitDescription(const QString &description);
  void useGitHeading(QLabel *heading, QLabel *description = nullptr);

private:
  void prepareLayout();
  QLabel *m_gitHeading = nullptr;
  QLabel *m_gitDescription = nullptr;
  QString m_description;
};

#endif
