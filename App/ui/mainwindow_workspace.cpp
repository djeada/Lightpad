#include "mainwindow_internal.h"
#include <QResizeEvent>

namespace {
class WorkspaceHeader : public QWidget {
public:
  using QWidget::QWidget;
  QWidget *context = nullptr;
  QWidget *spacer = nullptr;

protected:
  void resizeEvent(QResizeEvent *event) override {
    QWidget::resizeEvent(event);
    if (context && spacer) {
      const bool compact = width() < 960;
      context->setVisible(!compact);
      spacer->setVisible(compact);
    }
  }
};
} // namespace

void MainWindow::setupWorkspaceChrome() {
  auto *central = qobject_cast<QVBoxLayout *>(ui->centralwidget->layout());
  auto *metadata = qobject_cast<QHBoxLayout *>(ui->backgroundBottom->layout());
  if (!central || !metadata)
    return;

  // One command row above the editor; one status row below every tool panel.
  auto *header = new WorkspaceHeader(ui->centralwidget);
  header->setObjectName("workspaceHeader");
  auto *commands = new QHBoxLayout(header);
  commands->setContentsMargins(8, 4, 8, 4);
  commands->setSpacing(6);

  auto *files = new QToolButton(header);
  files->setObjectName("workspaceFileSearch");
  files->setText(tr("Go to file…"));
  files->setToolTip(tr("Find a file in the workspace (Ctrl+P)"));
  files->setAccessibleName(tr("Go to file"));
  files->setAutoRaise(true);
  connect(files, &QToolButton::clicked, this, &MainWindow::showFileQuickOpen);
  commands->addWidget(files);

  if (breadcrumbWidget) {
    central->removeWidget(breadcrumbWidget);
    breadcrumbWidget->setMinimumWidth(60);
    breadcrumbWidget->setSizePolicy(QSizePolicy::Ignored,
                                    QSizePolicy::Preferred);
    commands->addWidget(breadcrumbWidget, 1);
    auto *spacer = new QWidget(header);
    spacer->setStyleSheet(QStringLiteral("background: transparent;"));
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    spacer->hide();
    commands->addWidget(spacer, 1);
    header->context = breadcrumbWidget;
    header->spacer = spacer;
  } else {
    commands->addStretch();
  }

  for (auto *button : {ui->runButton, ui->debugButton, ui->testButton}) {
    metadata->removeWidget(button);
    button->setMinimumWidth(100);
    button->setMaximumWidth(200);
    button->setIconSize(QSize(16, 16));
    button->setStyleSheet(QString());
    button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    commands->addWidget(button);
  }
  metadata->removeWidget(ui->magicButton);
  ui->magicButton->setText(tr("Format"));
  ui->magicButton->setStyleSheet(QString());
  ui->magicButton->setAccessibleName(tr("Format document"));
  ui->magicButton->setToolTip(tr("Format the current document"));
  ui->magicButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
  ui->magicButton->setIconSize(QSize(16, 16));
  commands->addWidget(ui->magicButton);
  central->insertWidget(0, header);

  central->removeWidget(ui->backgroundBottom);
  metadata->setContentsMargins(0, 0, 4, 0);
  metadata->setSpacing(2);
  ui->backgroundBottom->setMinimumHeight(0);
  ui->backgroundBottom->setSizePolicy(QSizePolicy::Preferred,
                                      QSizePolicy::Preferred);
  statusBar()->setSizeGripEnabled(false);
  // Leave room for transient save, recovery, and run messages. Normal status
  // widgets temporarily yield that space; editor metadata stays on the right.
  for (auto *indicator : {m_gitBranchLabel, m_gitSyncLabel, m_gitDirtyLabel}) {
    if (indicator) {
      statusBar()->removeWidget(indicator);
      statusBar()->addWidget(indicator);
    }
  }
  statusBar()->addPermanentWidget(ui->backgroundBottom);
}
