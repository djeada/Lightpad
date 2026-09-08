#include "gitrecovery.h"
#include "gitintegration.h"
#include <QObject>

QString gitRecoveryEventKindName(GitRecoveryEventKind kind) {
  switch (kind) {
  case GitRecoveryEventKind::Commit:
    return QObject::tr("Commit");
  case GitRecoveryEventKind::ResetMoved:
    return QObject::tr("Reset");
  case GitRecoveryEventKind::RebaseRewrote:
    return QObject::tr("Rebase");
  case GitRecoveryEventKind::HeadSwitched:
    return QObject::tr("Switched");
  case GitRecoveryEventKind::Merged:
    return QObject::tr("Merge");
  case GitRecoveryEventKind::CherryPicked:
    return QObject::tr("Cherry-pick");
  case GitRecoveryEventKind::Pulled:
    return QObject::tr("Pull");
  case GitRecoveryEventKind::BranchCreated:
    return QObject::tr("Branch created");
  case GitRecoveryEventKind::AmendedCommit:
    return QObject::tr("Amend");
  case GitRecoveryEventKind::Other:
    return QObject::tr("Ref moved");
  }
  return QString();
}

GitRecoveryEventKind classifyReflogAction(const QString &action) {
  const QString value = action.trimmed().toLower();
  if (value.startsWith(QLatin1String("rebase"))) {
    return GitRecoveryEventKind::RebaseRewrote;
  }
  if (value.startsWith(QLatin1String("reset"))) {
    return GitRecoveryEventKind::ResetMoved;
  }
  if (value.startsWith(QLatin1String("checkout")) ||
      value.startsWith(QLatin1String("switch"))) {
    return GitRecoveryEventKind::HeadSwitched;
  }
  if (value.startsWith(QLatin1String("merge"))) {
    return GitRecoveryEventKind::Merged;
  }
  if (value.startsWith(QLatin1String("cherry-pick"))) {
    return GitRecoveryEventKind::CherryPicked;
  }
  if (value.startsWith(QLatin1String("pull"))) {
    return GitRecoveryEventKind::Pulled;
  }
  if (value.startsWith(QLatin1String("branch:"))) {
    return GitRecoveryEventKind::BranchCreated;
  }
  if (value.startsWith(QLatin1String("commit (amend)"))) {
    return GitRecoveryEventKind::AmendedCommit;
  }
  if (value.startsWith(QLatin1String("commit"))) {
    return GitRecoveryEventKind::Commit;
  }
  return GitRecoveryEventKind::Other;
}

QString gitRecoveryEventDescription(const GitRecoveryEvent &event) {
  switch (event.kind) {
  case GitRecoveryEventKind::RebaseRewrote:
    return event.groupedCount > 1
               ? QObject::tr("A rebase rewrote %1 commits, ending at %2")
                     .arg(event.groupedCount)
                     .arg(event.shortTo())
               : QObject::tr("A rebase moved the branch to %1")
                     .arg(event.shortTo());
  case GitRecoveryEventKind::ResetMoved:
    return QObject::tr("A reset moved the branch from %1 to %2")
        .arg(event.shortFrom(), event.shortTo());
  case GitRecoveryEventKind::HeadSwitched:
    return QObject::tr("HEAD moved to %1 — %2")
        .arg(event.shortTo(), event.subject);
  case GitRecoveryEventKind::Commit:
    return QObject::tr("Committed %1 — %2").arg(event.shortTo(), event.subject);
  case GitRecoveryEventKind::AmendedCommit:
    return QObject::tr("Amended the last commit, replacing %1 with %2")
        .arg(event.shortFrom(), event.shortTo());
  case GitRecoveryEventKind::Merged:
    return QObject::tr("A merge moved the branch to %1").arg(event.shortTo());
  case GitRecoveryEventKind::CherryPicked:
    return QObject::tr("A cherry-pick added %1").arg(event.shortTo());
  case GitRecoveryEventKind::Pulled:
    return QObject::tr("A pull moved the branch to %1").arg(event.shortTo());
  case GitRecoveryEventKind::BranchCreated:
    return QObject::tr("A branch was created at %1").arg(event.shortTo());
  case GitRecoveryEventKind::Other:
    break;
  }
  return QObject::tr("%1 moved from %2 to %3")
      .arg(event.selector, event.shortFrom(), event.shortTo());
}

QString gitRecoveryGuarantee(const GitRecoveryEvent &event) {
  if (event.reachable) {
    return QObject::tr(
        "This commit is still reachable from a branch or tag, so it is not "
        "going anywhere.");
  }
  return QObject::tr(
      "No branch or tag points at this commit any more. The reflog still "
      "knows it, which is what makes it recoverable — but that entry expires "
      "and garbage collection eventually removes the commit. Give it a name "
      "to keep it.");
}

QList<GitRecoveryEvent> parseReflogEntries(const QString &output) {
  QList<GitRecoveryEvent> events;
  for (const QString &line :
       output.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
    const QStringList fields = line.split(QChar(0));
    if (fields.size() < 5) {
      continue;
    }
    GitRecoveryEvent event;
    event.selector = fields.at(0);
    event.rawAction = fields.at(1);
    event.toHash = fields.at(2);
    event.subject = fields.at(3);
    event.relativeDate = fields.at(4);
    event.kind = classifyReflogAction(event.rawAction);
    event.rawEntries << QStringLiteral("%1 %2: %3")
                            .arg(event.toHash.left(7), event.selector,
                                 event.rawAction);
    events.append(event);
  }

  for (int i = 0; i < events.size(); ++i) {
    events[i].fromHash =
        i + 1 < events.size() ? events.at(i + 1).toHash : QString();
  }

  return events;
}

QList<GitRecoveryEvent>
groupRecoveryEvents(const QList<GitRecoveryEvent> &events) {
  QList<GitRecoveryEvent> grouped;

  for (const GitRecoveryEvent &event : events) {

    const bool foldable = event.kind == GitRecoveryEventKind::RebaseRewrote;
    if (foldable && !grouped.isEmpty() &&
        grouped.last().kind == GitRecoveryEventKind::RebaseRewrote) {
      GitRecoveryEvent &last = grouped.last();
      ++last.groupedCount;
      last.fromHash = event.fromHash;
      last.rawEntries += event.rawEntries;
      continue;
    }
    grouped.append(event);
  }

  return grouped;
}

QList<GitRecoveryEvent> buildRecoveryTimeline(GitIntegration *git,
                                              int maxEntries) {
  if (!git || !git->isValidRepository()) {
    return {};
  }

  QList<GitRecoveryEvent> events =
      groupRecoveryEvents(parseReflogEntries(git->reflogRaw(maxEntries)));

  for (GitRecoveryEvent &event : events) {
    event.reachable = git->isCommitReachable(event.toHash);
  }

  return events;
}
