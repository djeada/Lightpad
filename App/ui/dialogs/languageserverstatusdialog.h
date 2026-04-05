#ifndef LANGUAGESERVERSTATUSDIALOG_H
#define LANGUAGESERVERSTATUSDIALOG_H

#include "../../settings/theme.h"
#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class LanguageFeatureManager;

class LanguageServerStatusDialog : public QDialog {
  Q_OBJECT

public:
  explicit LanguageServerStatusDialog(
      const QString &languageId, const QString &workspaceFolder,
      const QString &filePath, const QString &effectiveLanguageId,
      const QString &overrideLanguageId, LanguageFeatureManager *manager,
      const Theme &theme, QWidget *parent = nullptr);

signals:
  void configurationApplied(const QString &languageAssociation);
  void pythonEnvironmentRequested();

private slots:
  void refreshDetails();
  void saveAndRetry();

private:
  QString m_languageId;
  QString m_workspaceFolder;
  QString m_filePath;
  QString m_effectiveLanguageId;
  QString m_overrideLanguageId;
  LanguageFeatureManager *m_manager;
  Theme m_theme;

  QLabel *m_statusBanner;
  QCheckBox *m_enabledCheck;
  QComboBox *m_languageCombo;
  QLineEdit *m_commandEdit;
  QLineEdit *m_argumentsEdit;
  QPlainTextEdit *m_detailsEdit;
  QPushButton *m_pythonEnvironmentButton;
  QPushButton *m_saveButton;
  QPushButton *m_closeButton;
};

#endif
