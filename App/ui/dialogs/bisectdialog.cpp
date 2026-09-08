#include "bisectdialog.h"
#include "../../git/gitintegration.h"
#include "../uimetrics.h"
#include "../uistylehelper.h"
#include "themedmessagebox.h"
#include <QComboBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

BisectDialog::BisectDialog(GitIntegration *git, const Theme &theme,
                           QWidget *parent)
    : StyledDialog(parent), m_git(git), m_goodCombo(nullptr),
      m_badCombo(nullptr), m_commandEdit(nullptr), m_statusLabel(nullptr),
      m_guidanceLabel(nullptr), m_progressLabel(nullptr), m_outputView(nullptr),
      m_logList(nullptr), m_startButton(nullptr), m_goodButton(nullptr),
      m_badButton(nullptr), m_skipButton(nullptr), m_runButton(nullptr),
      m_inspectButton(nullptr), m_resetButton(nullptr), m_closeButton(nullptr) {
  setWindowTitle(tr("Guided bisect"));
  setMinimumSize(860, 620);
  resize(980, 700);

  buildUi();
  setKeyboardDefault(nullptr);
  applyTheme(theme);
  reload();
}

void BisectDialog::buildUi() {
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(UiMetrics::SpaceLg, UiMetrics::SpaceLg,
                             UiMetrics::SpaceLg, UiMetrics::SpaceLg);
  layout->setSpacing(UiMetrics::SpaceMd);

  QHBoxLayout *endpoints = new QHBoxLayout();
  QLabel *goodLabel = new QLabel(tr("Behaviour was right at:"), this);
  goodLabel->setObjectName(QStringLiteral("bisectGoodLabel"));
  endpoints->addWidget(goodLabel);

  m_goodCombo = new QComboBox(this);
  m_goodCombo->setObjectName(QStringLiteral("bisectGoodCombo"));
  m_goodCombo->setEditable(true);
  endpoints->addWidget(m_goodCombo, 1);

  QLabel *badLabel = new QLabel(tr("and wrong at:"), this);
  badLabel->setObjectName(QStringLiteral("bisectBadLabel"));
  endpoints->addWidget(badLabel);

  m_badCombo = new QComboBox(this);
  m_badCombo->setObjectName(QStringLiteral("bisectBadCombo"));
  m_badCombo->setEditable(true);
  endpoints->addWidget(m_badCombo, 1);

  m_startButton = new QPushButton(tr("Start"), this);
  m_startButton->setObjectName(QStringLiteral("bisectStartButton"));
  connect(m_startButton, &QPushButton::clicked, this, &BisectDialog::onStart);

  for (QComboBox *combo : {m_goodCombo, m_badCombo}) {
    connect(combo, &QComboBox::editTextChanged, this,
            &BisectDialog::updateStartEnabled);
  }
  endpoints->addWidget(m_startButton);
  layout->addLayout(endpoints);

  m_statusLabel = new QLabel(this);
  m_statusLabel->setObjectName(QStringLiteral("bisectStatusLabel"));
  m_statusLabel->setWordWrap(true);
  layout->addWidget(m_statusLabel);

  m_guidanceLabel = new QLabel(this);
  m_guidanceLabel->setObjectName(QStringLiteral("bisectGuidanceLabel"));
  m_guidanceLabel->setWordWrap(true);
  layout->addWidget(m_guidanceLabel);

  m_progressLabel = new QLabel(this);
  m_progressLabel->setObjectName(QStringLiteral("bisectProgressLabel"));
  m_progressLabel->setWordWrap(true);
  layout->addWidget(m_progressLabel);

  QHBoxLayout *verdicts = new QHBoxLayout();
  const auto addButton = [&](QPushButton **button, const QString &text,
                             const QString &name, const QString &tip) {
    *button = new QPushButton(text, this);
    (*button)->setObjectName(name);
    (*button)->setToolTip(tip);
    verdicts->addWidget(*button);
  };
  addButton(&m_goodButton, tr("Good here"), QStringLiteral("bisectGoodButton"),
            tr("The behaviour is right at this commit."));
  addButton(&m_badButton, tr("Bad here"), QStringLiteral("bisectBadButton"),
            tr("The behaviour is wrong at this commit."));
  addButton(&m_skipButton, tr("Cannot test this one"),
            QStringLiteral("bisectSkipButton"),
            tr("Use this when the commit will not build. Skipped commits "
               "widen the answer to a range."));
  addButton(&m_inspectButton, tr("Open the suspect"),
            QStringLiteral("bisectInspectButton"),
            tr("Show the commit the search settled on."));
  connect(m_goodButton, &QPushButton::clicked, this, &BisectDialog::onGood);
  connect(m_badButton, &QPushButton::clicked, this, &BisectDialog::onBad);
  connect(m_skipButton, &QPushButton::clicked, this, &BisectDialog::onSkip);
  connect(m_inspectButton, &QPushButton::clicked, this,
          &BisectDialog::onInspectSuspect);
  verdicts->addStretch();
  layout->addLayout(verdicts);

  QHBoxLayout *automated = new QHBoxLayout();
  QLabel *commandLabel = new QLabel(tr("Or run a test for each commit:"), this);
  commandLabel->setObjectName(QStringLiteral("bisectCommandLabel"));
  automated->addWidget(commandLabel);

  m_commandEdit = new QLineEdit(this);
  m_commandEdit->setObjectName(QStringLiteral("bisectCommandEdit"));
  m_commandEdit->setPlaceholderText(
      tr("e.g. make test — exit 0 good, 1-127 bad, 125 cannot test"));
  automated->addWidget(m_commandEdit, 1);

  m_runButton = new QPushButton(tr("Run it"), this);
  m_runButton->setObjectName(QStringLiteral("bisectRunButton"));
  m_runButton->setToolTip(
      tr("Runs the command at every candidate and reads the verdict from its "
         "exit code."));
  connect(m_runButton, &QPushButton::clicked, this,
          &BisectDialog::onRunAutomated);
  automated->addWidget(m_runButton);
  layout->addLayout(automated);

  m_outputView = new QPlainTextEdit(this);
  m_outputView->setObjectName(QStringLiteral("bisectOutputView"));
  m_outputView->setReadOnly(true);
  m_outputView->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

  m_outputView->setPlaceholderText(
      tr("Git's own output appears here once the search starts — the commit "
         "it checked out, and how many steps are left."));
  layout->addWidget(m_outputView, 1);

  m_logList = new QListWidget(this);
  m_logList->setObjectName(QStringLiteral("bisectLogList"));
  m_logList->setSelectionMode(QAbstractItemView::NoSelection);
  m_logList->setFocusPolicy(Qt::NoFocus);
  m_logList->setMaximumHeight(110);
  m_logList->setToolTip(
      tr("The bisect log. Git keeps it, which is why a search survives "
         "closing this window."));
  layout->addWidget(m_logList);

  QHBoxLayout *footer = new QHBoxLayout();
  m_resetButton = new QPushButton(tr("Reset the search"), this);
  m_resetButton->setObjectName(QStringLiteral("bisectResetButton"));
  m_resetButton->setToolTip(
      tr("Ends the bisect and puts your checkout back where it started."));
  connect(m_resetButton, &QPushButton::clicked, this, &BisectDialog::onReset);
  footer->addWidget(m_resetButton);
  footer->addStretch();

  m_closeButton = new QPushButton(tr("Close"), this);
  m_closeButton->setObjectName(QStringLiteral("bisectCloseButton"));
  connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
  footer->addWidget(m_closeButton);
  layout->addLayout(footer);
}

void BisectDialog::setEndpoints(const QString &goodRef, const QString &badRef) {
  if (!goodRef.isEmpty()) {
    m_goodCombo->setEditText(goodRef);
  }
  if (!badRef.isEmpty()) {
    m_badCombo->setEditText(badRef);
  }
}

void BisectDialog::reload() {
  if (!m_git || !m_git->isValidRepository()) {
    return;
  }

  if (m_goodCombo->count() == 0) {
    QStringList refs;
    for (const GitBranchInfo &branch : m_git->getBranches()) {
      refs << branch.name;
    }
    for (const GitTagInfo &tag : m_git->getTags()) {
      refs << tag.name;
    }
    for (const GitCommitInfo &commit :
         m_git->getCommitLogPage(QStringLiteral("HEAD"), 0, 50)) {
      refs << QStringLiteral("%1 %2").arg(commit.shortHash, commit.subject);
    }
    m_goodCombo->addItems(refs);
    m_badCombo->addItems(refs);
    m_badCombo->setEditText(QStringLiteral("HEAD"));
  }

  m_state = buildBisectState(m_git);

  const bool searching = m_state.status == GitBisectStatus::Searching;
  const bool found = m_state.status == GitBisectStatus::Found;

  m_statusLabel->setText(searching
                             ? tr("<b>Bisecting</b> — on <code>%1</code> %2")
                                   .arg(m_state.currentHash.left(7),
                                        m_state.currentSubject.toHtmlEscaped())
                         : found ? tr("<b>Found</b> — <code>%1</code>")
                                       .arg(m_state.suspectHash.left(7))
                                 : tr("<b>Not started</b>"));
  m_guidanceLabel->setText(gitBisectGuidance(m_state));
  m_progressLabel->setText(searching || found
                               ? tr("%1 good, %2 bad, %3 skipped so far.")
                                     .arg(m_state.goodCount)
                                     .arg(m_state.badCount)
                                     .arg(m_state.skipCount)
                               : QString());

  m_logList->clear();
  for (const QString &line : m_state.log) {
    QListWidgetItem *item = new QListWidgetItem(line, m_logList);
    item->setFlags(Qt::ItemIsEnabled);
  }

  const bool running = m_git->isBisecting();
  updateStartEnabled();
  m_goodCombo->setEnabled(!running);
  m_badCombo->setEnabled(!running);
  for (QPushButton *button :
       {m_goodButton, m_badButton, m_skipButton, m_runButton}) {
    button->setEnabled(running);
  }
  m_inspectButton->setEnabled(!m_state.suspectHash.isEmpty());
  m_resetButton->setEnabled(running);
  applyTheme(m_theme);
}

void BisectDialog::updateStartEnabled() {
  if (!m_startButton || !m_goodCombo || !m_badCombo || !m_git) {
    return;
  }
  m_startButton->setEnabled(!m_git->isBisecting() &&
                            !m_goodCombo->currentText().trimmed().isEmpty() &&
                            !m_badCombo->currentText().trimmed().isEmpty());
}

void BisectDialog::applyOutput(const QString &output) {
  m_outputView->setPlainText(output);
  parseBisectProgress(output, &m_state);
  reload();

  if (!m_state.suspectHash.isEmpty()) {
    m_statusLabel->setText(
        tr("<b>Found</b> — <code>%1</code>").arg(m_state.suspectHash.left(7)));
    m_inspectButton->setEnabled(true);
  }
  emit repositoryChanged();
}

void BisectDialog::onStart() {
  if (!m_git) {
    return;
  }
  const QString good = m_goodCombo->currentText().section(' ', 0, 0).trimmed();
  const QString bad = m_badCombo->currentText().section(' ', 0, 0).trimmed();
  if (good.isEmpty() || bad.isEmpty()) {
    ThemedMessageBox::information(
        this, tr("Two points are needed"),
        tr("Bisect searches between a commit you know was right and one you "
           "know was wrong."));
    return;
  }
  applyOutput(m_git->bisectStart(bad, good));
}

void BisectDialog::onGood() {
  if (m_git) {
    applyOutput(m_git->bisectMark(QStringLiteral("good")));
  }
}

void BisectDialog::onBad() {
  if (m_git) {
    applyOutput(m_git->bisectMark(QStringLiteral("bad")));
  }
}

void BisectDialog::onSkip() {
  if (m_git) {
    applyOutput(m_git->bisectMark(QStringLiteral("skip")));
  }
}

void BisectDialog::onRunAutomated() {
  if (!m_git) {
    return;
  }
  const QString command = m_commandEdit->text().trimmed();
  if (command.isEmpty()) {
    return;
  }

  int exitCode = 0;
  const QString output = m_git->bisectRun(command, &exitCode);
  applyOutput(output + QStringLiteral("\n\n") +
              tr("git bisect run finished with exit code %1.\n%2")
                  .arg(exitCode)
                  .arg(gitBisectExitCodeMeaning(exitCode)));
}

void BisectDialog::onReset() {
  if (m_git && m_git->bisectReset()) {
    m_outputView->clear();
    emit repositoryChanged();
    reload();
  }
}

void BisectDialog::onInspectSuspect() {
  if (!m_state.suspectHash.isEmpty()) {
    emit inspectCommitRequested(m_state.suspectHash);
  }
}

void BisectDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);
  setStyleSheet(UIStyleHelper::formDialogStyle(theme));

  for (QComboBox *combo : {m_goodCombo, m_badCombo}) {
    if (combo) {
      combo->setStyleSheet(UIStyleHelper::comboBoxStyle(theme));
    }
  }
  if (m_commandEdit) {
    m_commandEdit->setStyleSheet(UIStyleHelper::lineEditStyle(theme));
  }
  if (m_outputView) {
    m_outputView->setStyleSheet(
        QString("QPlainTextEdit { background: %1; color: %2; border: 1px "
                "solid %3; }")
            .arg(theme.surfaceColor.name(), theme.foregroundColor.name(),
                 theme.borderColor.name()));
  }
  if (m_logList) {
    m_logList->setStyleSheet(
        QString("QListWidget { background: %1; color: %2; border: 1px solid "
                "%3; }")
            .arg(theme.surfaceColor.name(),
                 theme.singleLineCommentFormat.name(),
                 theme.borderColor.name()));
  }
  if (m_statusLabel) {
    styleTitleLabel(m_statusLabel);
  }
  for (const char *name :
       {"bisectGoodLabel", "bisectBadLabel", "bisectCommandLabel",
        "bisectGuidanceLabel", "bisectProgressLabel"}) {
    if (QLabel *label = findChild<QLabel *>(QString::fromLatin1(name))) {
      styleSubduedLabel(label);
    }
  }
  if (m_goodButton) {
    stylePrimaryButton(m_goodButton);
  }
  for (QPushButton *button : {m_badButton, m_skipButton, m_inspectButton,
                              m_startButton, m_runButton, m_closeButton}) {
    if (button) {
      styleSecondaryButton(button);
    }
  }
  if (m_resetButton) {
    styleDangerButton(m_resetButton);
  }
}
