#ifndef INTEGRATIONADVISORDIALOG_H
#define INTEGRATIONADVISORDIALOG_H

#include "../../git/gitintegrationadvice.h"
#include "styleddialog.h"
#include <QMap>

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QRadioButton;
class QVBoxLayout;

class IntegrationAdvisorDialog : public StyledDialog {
  Q_OBJECT

public:
  IntegrationAdvisorDialog(GitIntegration *git, const Theme &theme,
                           QWidget *parent = nullptr);

  void setSource(const QString &sourceRef, const QString &singleCommit);

  const GitIntegrationAdvice &advice() const { return m_advice; }
  GitIntegrationIntent selectedIntent() const;
  bool expertMode() const;

signals:
  void repositoryChanged();

  void reviewAppliedChangesRequested();

protected:
  void applyTheme(const Theme &theme) override;

private slots:
  void reload();
  void onIntentChanged();
  void onExpertModeToggled(bool expert);
  void onShowFullPreview();
  void onExecute();

private:
  void buildUi();
  void buildSourceBar(QVBoxLayout *layout);
  void buildOptions(QVBoxLayout *layout);
  void buildFooter(QVBoxLayout *layout);
  void updateSelectedOption();
  void fillTopology(const GitIntegrationOption &option);
  const GitIntegrationOption *optionFor(GitIntegrationIntent intent) const;

  GitIntegration *m_git;
  GitIntegrationAdvice m_advice;

  QComboBox *m_sourceCombo;
  QComboBox *m_commitCombo;
  QLabel *m_targetLabel;
  QLabel *m_recommendationLabel;

  QMap<GitIntegrationIntent, QRadioButton *> m_radios;
  QMap<GitIntegrationIntent, QLabel *> m_consequences;

  QLabel *m_riskLabel;
  QLabel *m_commandLabel;
  QListWidget *m_topologyList;

  QCheckBox *m_expertCheck;
  QWidget *m_questionnaire;
  QWidget *m_expertBar;
  QPushButton *m_previewButton;
  QPushButton *m_executeButton;
  QPushButton *m_cancelButton;

  bool m_updating;
};

#endif
