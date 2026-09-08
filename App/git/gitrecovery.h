#ifndef GITRECOVERY_H
#define GITRECOVERY_H

#include "gitcommittypes.h"
#include <QList>
#include <QString>
#include <QStringList>

class GitIntegration;

enum class GitRecoveryEventKind {
  Commit,
  ResetMoved,
  RebaseRewrote,
  HeadSwitched,
  Merged,
  CherryPicked,
  Pulled,
  BranchCreated,
  AmendedCommit,
  Other,
};

struct GitRecoveryEvent {
  GitRecoveryEventKind kind = GitRecoveryEventKind::Other;

  QString selector;

  QString fromHash;
  QString toHash;
  QString subject;
  QString relativeDate;

  QString rawAction;

  int groupedCount = 1;
  QStringList rawEntries;

  bool reachable = true;

  QString shortFrom() const { return fromHash.left(7); }
  QString shortTo() const { return toHash.left(7); }
};

QString gitRecoveryEventKindName(GitRecoveryEventKind kind);

GitRecoveryEventKind classifyReflogAction(const QString &action);

QString gitRecoveryEventDescription(const GitRecoveryEvent &event);

QString gitRecoveryGuarantee(const GitRecoveryEvent &event);

QList<GitRecoveryEvent> parseReflogEntries(const QString &output);

QList<GitRecoveryEvent>
groupRecoveryEvents(const QList<GitRecoveryEvent> &events);

QList<GitRecoveryEvent> buildRecoveryTimeline(GitIntegration *git,
                                              int maxEntries = 100);

#endif
