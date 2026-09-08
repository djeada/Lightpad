#ifndef GITWORKTREEMAP_H
#define GITWORKTREEMAP_H

#include "gitrepositorystate.h"
#include <QList>
#include <QString>

class GitIntegration;

struct GitWorktreeCard {
  QString path;
  QString name;

  QString branch;
  bool detached = false;
  QString headHash;
  QString headSubject;

  bool isMain = false;

  bool isCurrent = false;

  bool prunable = false;
  QString prunableReason;
  QString lockedReason;

  int staged = 0;
  int modified = 0;
  int untracked = 0;
  int conflicted = 0;
  int ahead = 0;
  int behind = 0;
  QString upstream;

  bool hasUncommittedWork() const {
    return staged > 0 || modified > 0 || untracked > 0 || conflicted > 0;
  }

  QString shortHead() const { return headHash.left(7); }
};

QList<GitWorktreeCard> parseWorktreeList(const QString &output);

QString gitWorktreeRemovalWarning(const GitWorktreeCard &card);

QString gitWorktreeSummary(const GitWorktreeCard &card);

QList<GitWorktreeCard> buildWorktreeMap(GitIntegration *git);

#endif
