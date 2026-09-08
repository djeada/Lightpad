#include "timetraveldialog.h"
#include "../../git/gitintegration.h"
#include "../uimetrics.h"
#include "../uistylehelper.h"
#include "themedmessagebox.h"
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

namespace {

QString objectNameFor(GitTimeTravelMode mode) {
  switch (mode) {
  case GitTimeTravelMode::InspectSnapshot:
    return QStringLiteral("timeTravelInspectRadio");
  case GitTimeTravelMode::RunnableWorktree:
    return QStringLiteral("timeTravelWorktreeRadio");
  case GitTimeTravelMode::StartBranch:
    return QStringLiteral("timeTravelBranchRadio");
  }
  return QString();
}

} // namespace

TimeTravelDialog::TimeTravelDialog(GitIntegration *git,
                                   const QString &commitHash,
                                   const Theme &theme, QWidget *parent)
    : StyledDialog(parent), m_git(git), m_commitHash(commitHash),
      m_headerLabel(nullptr), m_checkoutLabel(nullptr), m_commandLabel(nullptr),
      m_branchNameEdit(nullptr), m_worktreePathEdit(nullptr),
      m_graphButton(nullptr), m_executeButton(nullptr),
      m_cancelButton(nullptr) {
  if (m_git) {
    m_commit = m_git->getCommitDetails(commitHash);
  }
  m_options = gitTimeTravelOptions(commitHash, m_commit.shortHash);

  setWindowTitle(tr("Time travel"));
  setMinimumSize(720, 520);
  resize(820, 580);

  buildUi();
  setKeyboardDefault(m_executeButton);
  applyTheme(theme);
  updateSelectedOption();
}

void TimeTravelDialog::buildUi() {
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(UiMetrics::SpaceLg, UiMetrics::SpaceLg,
                             UiMetrics::SpaceLg, UiMetrics::SpaceLg);
  layout->setSpacing(UiMetrics::SpaceMd);

  m_headerLabel = new QLabel(this);
  m_headerLabel->setObjectName(QStringLiteral("timeTravelHeaderLabel"));
  m_headerLabel->setWordWrap(true);
  m_headerLabel->setText(
      tr("What would you like to do with <code>%1</code> — %2?")
          .arg(m_commit.shortHash.isEmpty() ? m_commitHash.left(7)
                                            : m_commit.shortHash,
               m_commit.subject.toHtmlEscaped()));
  layout->addWidget(m_headerLabel);

  for (const GitTimeTravelOption &option : m_options) {
    QRadioButton *radio = new QRadioButton(option.title, this);
    radio->setObjectName(objectNameFor(option.mode));
    connect(radio, &QRadioButton::toggled, this,
            &TimeTravelDialog::onModeChanged);
    layout->addWidget(radio);

    QLabel *explanation = new QLabel(option.explanation, this);
    explanation->setObjectName(objectNameFor(option.mode) +
                               QStringLiteral("Explanation"));
    explanation->setWordWrap(true);
    explanation->setIndent(24);
    layout->addWidget(explanation);

    m_radios.insert(option.mode, radio);
    m_explanations.insert(option.mode, explanation);
  }

  QHBoxLayout *branchRow = new QHBoxLayout();
  QLabel *branchLabel = new QLabel(tr("Branch name:"), this);
  branchLabel->setObjectName(QStringLiteral("timeTravelBranchLabel"));
  branchRow->addWidget(branchLabel);
  m_branchNameEdit = new QLineEdit(this);
  m_branchNameEdit->setObjectName(QStringLiteral("timeTravelBranchNameEdit"));
  m_branchNameEdit->setPlaceholderText(tr("e.g. investigate-regression"));
  branchRow->addWidget(m_branchNameEdit, 1);
  layout->addLayout(branchRow);

  QHBoxLayout *worktreeRow = new QHBoxLayout();
  QLabel *worktreeLabel = new QLabel(tr("Worktree folder:"), this);
  worktreeLabel->setObjectName(QStringLiteral("timeTravelWorktreeLabel"));
  worktreeRow->addWidget(worktreeLabel);
  m_worktreePathEdit = new QLineEdit(this);
  m_worktreePathEdit->setObjectName(
      QStringLiteral("timeTravelWorktreePathEdit"));
  if (m_git && !m_git->repositoryPath().isEmpty()) {
    const QDir root(m_git->repositoryPath());

    m_worktreePathEdit->setText(QDir::cleanPath(root.absoluteFilePath(
        QStringLiteral("../%1-at-%2")
            .arg(root.dirName(), m_commit.shortHash.isEmpty()
                                     ? m_commitHash.left(7)
                                     : m_commit.shortHash))));
  }
  worktreeRow->addWidget(m_worktreePathEdit, 1);
  layout->addLayout(worktreeRow);

  m_checkoutLabel = new QLabel(this);
  m_checkoutLabel->setObjectName(QStringLiteral("timeTravelCheckoutLabel"));
  m_checkoutLabel->setWordWrap(true);
  layout->addWidget(m_checkoutLabel);

  m_commandLabel = new QLabel(this);
  m_commandLabel->setObjectName(QStringLiteral("timeTravelCommandLabel"));
  m_commandLabel->setWordWrap(true);
  m_commandLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  layout->addWidget(m_commandLabel);

  layout->addStretch();

  QHBoxLayout *footer = new QHBoxLayout();
  m_graphButton = new QPushButton(tr("Show in graph"), this);
  m_graphButton->setObjectName(QStringLiteral("timeTravelGraphButton"));
  connect(m_graphButton, &QPushButton::clicked, this,
          [this]() { emit showInGraphRequested(m_commitHash); });
  footer->addWidget(m_graphButton);
  footer->addStretch();

  m_cancelButton = new QPushButton(tr("Cancel"), this);
  m_cancelButton->setObjectName(QStringLiteral("timeTravelCancelButton"));
  connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
  footer->addWidget(m_cancelButton);

  m_executeButton = new QPushButton(tr("Go"), this);
  m_executeButton->setObjectName(QStringLiteral("timeTravelExecuteButton"));
  connect(m_executeButton, &QPushButton::clicked, this,
          &TimeTravelDialog::onExecute);
  footer->addWidget(m_executeButton);

  layout->addLayout(footer);

  if (QRadioButton *radio =
          m_radios.value(GitTimeTravelMode::InspectSnapshot, nullptr)) {
    radio->setChecked(true);
  }
}

GitTimeTravelMode TimeTravelDialog::selectedMode() const {
  for (auto it = m_radios.constBegin(); it != m_radios.constEnd(); ++it) {
    if (it.value()->isChecked()) {
      return it.key();
    }
  }
  return GitTimeTravelMode::InspectSnapshot;
}

const GitTimeTravelOption *
TimeTravelDialog::optionFor(GitTimeTravelMode mode) const {
  for (const GitTimeTravelOption &option : m_options) {
    if (option.mode == mode) {
      return &option;
    }
  }
  return nullptr;
}

void TimeTravelDialog::onModeChanged() { updateSelectedOption(); }

void TimeTravelDialog::updateSelectedOption() {
  const GitTimeTravelMode mode = selectedMode();
  const GitTimeTravelOption *option = optionFor(mode);
  if (!option) {
    return;
  }

  m_branchNameEdit->setEnabled(mode == GitTimeTravelMode::StartBranch);
  m_worktreePathEdit->setEnabled(mode == GitTimeTravelMode::RunnableWorktree);

  m_checkoutLabel->setText(
      option->changesCheckout
          ? tr("⚠ This moves your checkout: the files you are working on are "
               "replaced.")
          : tr("✓ Your checkout stays where it is; nothing you are working on "
               "changes."));
  m_commandLabel->setText(
      tr("Command: <code>%1</code>").arg(option->command.toHtmlEscaped()));

  m_executeButton->setText(gitTimeTravelModeName(mode));
  applyTheme(m_theme);
}

void TimeTravelDialog::onExecute() {
  if (!m_git) {
    return;
  }

  switch (selectedMode()) {
  case GitTimeTravelMode::InspectSnapshot:
    emit inspectSnapshotRequested(m_commitHash);
    accept();
    return;

  case GitTimeTravelMode::RunnableWorktree: {
    const QString path = m_worktreePathEdit->text().trimmed();
    if (path.isEmpty()) {
      return;
    }
    if (QDir(path).exists() && !QDir(path).isEmpty()) {
      ThemedMessageBox::warning(
          this, tr("Folder is not empty"),
          tr("%1 already contains files. Pick an empty folder so nothing of "
             "yours is overwritten.")
              .arg(path));
      return;
    }
    if (m_git->addWorktree(path, m_commitHash, false)) {
      emit worktreeOpened(path, m_commitHash);
      emit repositoryChanged();
      accept();
    }
    return;
  }

  case GitTimeTravelMode::StartBranch: {
    const QString name = m_branchNameEdit->text().trimmed();
    if (name.isEmpty()) {
      ThemedMessageBox::information(
          this, tr("Name the branch"),
          tr("A branch needs a name — that name is what makes the work easy "
             "to find again."));
      m_branchNameEdit->setFocus();
      return;
    }
    if (m_git->createBranchFromCommit(name, m_commitHash, true)) {
      emit repositoryChanged();
      accept();
    }
    return;
  }
  }
}

void TimeTravelDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);
  setStyleSheet(UIStyleHelper::formDialogStyle(theme));

  if (m_headerLabel) {
    styleTitleLabel(m_headerLabel);
  }
  for (QRadioButton *radio : m_radios) {
    radio->setStyleSheet(UIStyleHelper::checkBoxStyle(theme) +
                         QString("QRadioButton { color: %1; }")
                             .arg(theme.foregroundColor.name()));
  }
  for (QLabel *label : m_explanations) {
    styleSubduedLabel(label);
  }
  for (const char *name : {"timeTravelBranchLabel", "timeTravelWorktreeLabel",
                           "timeTravelCommandLabel"}) {
    if (QLabel *label = findChild<QLabel *>(QString::fromLatin1(name))) {
      styleSubduedLabel(label);
    }
  }
  for (QLineEdit *edit : {m_branchNameEdit, m_worktreePathEdit}) {
    if (edit) {
      edit->setStyleSheet(UIStyleHelper::lineEditStyle(theme));
    }
  }

  const GitTimeTravelOption *option = optionFor(selectedMode());
  const bool moves = option && option->changesCheckout;
  if (m_checkoutLabel) {
    m_checkoutLabel->setStyleSheet(
        QString("color: %1;")
            .arg((moves ? theme.warningColor : theme.successColor).name()));
  }
  for (QPushButton *button : {m_graphButton, m_cancelButton}) {
    if (button) {
      styleSecondaryButton(button);
    }
  }
  if (m_executeButton) {
    stylePrimaryButton(m_executeButton);
  }
}
