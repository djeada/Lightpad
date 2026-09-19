#ifndef GITAUTOREFRESH_H
#define GITAUTOREFRESH_H

#include "../../git/gitintegration.h"
#include <QApplication>
#include <QPointer>
#include <QTimer>
#include <QWidget>
#include <functional>

constexpr int GIT_AUTO_REFRESH_RETRY_MS = 1000;

inline void reloadOnExternalGitChanges(QWidget *view, GitIntegration *git,
                                       const std::function<void()> &reload) {
  if (!view || !git || !reload) {
    return;
  }

  auto *retry = new QTimer(view);
  retry->setSingleShot(true);
  retry->setInterval(GIT_AUTO_REFRESH_RETRY_MS);

  QPointer<GitIntegration> guardedGit(git);
  const auto attempt = [view, retry, guardedGit, reload]() {
    if (!guardedGit || !guardedGit->isValidRepository() || !view->isVisible()) {
      return;
    }
    QWidget *modal = QApplication::activeModalWidget();
    if (QApplication::activePopupWidget() ||
        (modal && modal != view->window())) {
      retry->start();
      return;
    }
    reload();
  };

  QObject::connect(retry, &QTimer::timeout, view, attempt);
  QObject::connect(git, &GitIntegration::externalChangesDetected, view,
                   attempt);
}

#endif
