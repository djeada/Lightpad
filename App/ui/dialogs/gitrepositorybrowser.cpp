#include "gitrepositorybrowser.h"

#include "../../git/gitintegration.h"
#include "../gitui.h"
#include "compareanythingdialog.h"
#include "gitdiffdialog.h"
#include "gitlogdialog.h"
#include "gitobjectexplorerdialog.h"
#include <QButtonGroup>
#include <QDir>
#include <QGuiApplication>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QSplitter>
#include <QVBoxLayout>

GitRepositoryBrowser::GitRepositoryBrowser(const QString &repository,
                                           const Theme &theme, QWidget *parent)
    : QWidget(parent, Qt::Window), m_theme(theme),
      m_git(new GitIntegration(this)) {
  setObjectName("gitRepositoryBrowser");
  setAttribute(Qt::WA_DeleteOnClose);
  setWindowTitle(tr("Git Explorer — %1").arg(QDir(repository).dirName()));
  m_git->setRepositoryPath(repository);
  m_git->setAutoRefreshInterval(0);
  QPalette palette = this->palette();
  palette.setColor(QPalette::Window, theme.backgroundColor);
  palette.setColor(QPalette::WindowText, theme.foregroundColor);
  setPalette(palette);
  setAutoFillBackground(true);
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(12, 12, 12, 12);
  auto *bar = new QHBoxLayout;
  auto *title = new QLabel(
      tr("Git Explorer  /  %1").arg(QDir(repository).dirName()), this);
  QFont font = title->font();
  font.setBold(true);
  font.setPointSize(font.pointSize() + 2);
  title->setFont(font);
  title->setToolTip(repository);
  title->setWordWrap(true);
  title->setMinimumWidth(0);
  bar->addWidget(title, 1);
  m_modes = new QButtonGroup(this);
  const QStringList modes{tr("Objects"), tr("History"), tr("Split view")};
  for (int i = 0; i < modes.size(); ++i) {
    auto *button = new QPushButton(modes[i], this);
    button->setObjectName(QStringLiteral("gitBrowserMode%1").arg(i));
    button->setCheckable(true);
    m_modes->addButton(button, i);
    bar->addWidget(button);
  }
  connect(m_modes, &QButtonGroup::idClicked, this,
          &GitRepositoryBrowser::setMode);
  layout->addLayout(bar);
  m_splitter = new QSplitter(Qt::Horizontal, this);
  m_splitter->setObjectName("gitBrowserSplitter");
  m_splitter->setChildrenCollapsible(false);
  m_objects = new GitObjectExplorerDialog(repository, theme, m_splitter);
  m_history = new GitLogDialog(m_git, theme, m_splitter);
  for (GitDialog *page : {static_cast<GitDialog *>(m_objects),
                          static_cast<GitDialog *>(m_history)}) {
    page->setWindowFlags(Qt::Widget);
    page->setModal(false);
    page->setMinimumSize(0, 0);
    page->setProperty("gitEmbedded", true);
    for (auto *button : page->findChildren<QPushButton *>())
      if (button->property("gitDismiss").toBool())
        button->hide();
    m_splitter->addWidget(page);
  }
  m_splitter->setSizes({1000, 800});
  layout->addWidget(m_splitter, 1);
  connect(m_objects, &GitObjectExplorerDialog::viewCommitRequested, this,
          &GitRepositoryBrowser::showHistory);
  connect(m_history, &GitLogDialog::inspectObjectRequested, this,
          &GitRepositoryBrowser::showObject);
  connect(m_history, &GitLogDialog::objectSelected, this,
          [this](const QString &hash) {
            if (m_objects->isVisible()) {
              m_objects->searchHash(hash);
              m_objects->focusDag();
            }
          });
  connect(m_history, &GitLogDialog::repositoryChanged, this,
          &GitRepositoryBrowser::repositoryChanged);
  connect(m_history, &GitLogDialog::viewCommitDiff, this,
          [this](const QString &hash) { openDiff(hash, {}); });
  connect(m_history, &GitLogDialog::viewCommitFileDiff, this,
          &GitRepositoryBrowser::openDiff);
  connect(m_history, &GitLogDialog::compareRequested, this,
          [this, theme](const QString &from, const QString &to) {
            auto *compare = new CompareAnythingDialog(m_git, theme, this);
            compare->setAttribute(Qt::WA_DeleteOnClose);
            compare->setModal(false);
            compare->setEndpoints(GitCompareEndpoint::commit(from, from),
                                  GitCompareEndpoint::commit(to, to));
            connect(compare, &CompareAnythingDialog::fileOpenRequested, this,
                    &GitRepositoryBrowser::fileOpenRequested);
            compare->show();
          });
  GitUi::polish(this, theme);
  for (auto *button : m_modes->buttons())
    button->setStyleSheet(
        button->styleSheet() +
        QStringLiteral(
            "QPushButton:checked { background: %1; border: 2px solid %2; }")
            .arg(theme.accentSoftColor.name(), theme.accentColor.name()));
  const QSize available =
      QGuiApplication::primaryScreen()->availableGeometry().size();
  resize(qMin(1680, available.width() - 40),
         qMin(960, available.height() - 40));
  setMinimumSize(640, 600);
  setMode(0);
}

void GitRepositoryBrowser::openDiff(const QString &hash,
                                    const QString &filePath) {
  GitDiffDialog *diff = nullptr;
  for (auto *candidate :
       findChildren<GitDiffDialog *>(QString(), Qt::FindDirectChildrenOnly))
    if (candidate->property("commitHash").toString() == hash)
      diff = candidate;
  if (!diff) {
    diff = new GitDiffDialog(m_git, hash, GitDiffDialog::DiffTarget::Commit,
                             false, m_theme, this);
    diff->setAttribute(Qt::WA_DeleteOnClose);
    diff->setModal(false);
    diff->setProperty("commitHash", hash);
    diff->setDiffText(m_git->getCommitDiff(hash));
  }
  if (!filePath.isEmpty())
    diff->selectFile(filePath);
  diff->show();
  diff->raise();
  diff->activateWindow();
}

QString GitRepositoryBrowser::repositoryPath() const {
  return m_git->repositoryPath();
}

void GitRepositoryBrowser::setMode(int mode) {
  m_modes->button(mode)->setChecked(true);
  m_objects->setVisible(mode != 1);
  m_history->setVisible(mode != 0);
}

void GitRepositoryBrowser::showObject(const QString &hash) {
  if (!hash.isEmpty())
    m_objects->searchHash(hash);
  else if (m_objects->currentObjectHash().isEmpty())
    m_objects->searchHash(m_git->getCommitDetails("HEAD").hash);
  if (!m_objects->isVisible())
    setMode(width() >= 1500 ? 2 : 0);
  m_objects->focusDag();
}

void GitRepositoryBrowser::showHistory(const QString &hash) {
  setMode(width() >= 1500 ? 2 : 1);
  if (!hash.isEmpty())
    m_history->selectCommit(hash);
}
