#include "gitmergeplan.h"

#include <QMap>
#include <QObject>

namespace {

constexpr int kIncomingLogLimit = 40;

QString remoteOf(const QString &ref) {
  const int slash = ref.indexOf(QLatin1Char('/'));
  return slash <= 0 ? QString() : ref.left(slash);
}

QString shortNameOf(const QString &ref) {
  const int slash = ref.indexOf(QLatin1Char('/'));
  return slash <= 0 ? ref : ref.mid(slash + 1);
}

QString commitCount(int count) {
  return count == 1 ? QObject::tr("1 commit")
                    : QObject::tr("%1 commits").arg(count);
}

QString fileCount(int count) {
  return count == 1 ? QObject::tr("1 file")
                    : QObject::tr("%1 files").arg(count);
}

} // namespace

QString GitMergeSource::whereLabel() const {
  if (!isRemote) {
    return QObject::tr("the copy on this machine");
  }
  return QObject::tr("the copy on %1").arg(remoteName);
}

bool GitMergeChoice::hasLocal() const {
  for (const GitMergeSource &copy : copies) {
    if (!copy.isRemote) {
      return true;
    }
  }
  return false;
}

bool GitMergeChoice::hasRemote() const {
  for (const GitMergeSource &copy : copies) {
    if (copy.isRemote) {
      return true;
    }
  }
  return false;
}

bool GitMergeChoice::copiesDisagree() const {
  QString first;
  for (const GitMergeSource &copy : copies) {
    if (first.isEmpty()) {
      first = copy.summary.shortHash;
    } else if (copy.summary.shortHash != first) {
      return true;
    }
  }
  return false;
}

const GitMergeSource *GitMergeChoice::preferred() const {
  for (const GitMergeSource &copy : copies) {
    if (!copy.isRemote) {
      return &copy;
    }
  }
  return copies.isEmpty() ? nullptr : &copies.first();
}

bool GitMergePlan::canStart() const {
  return valid && blockers.isEmpty() &&
         relation != GitMergeRelation::AlreadyUpToDate;
}

QString gitMergeRelationName(GitMergeRelation relation) {
  switch (relation) {
  case GitMergeRelation::Unknown:
    return QObject::tr("unknown");
  case GitMergeRelation::Unrelated:
    return QObject::tr("no shared history");
  case GitMergeRelation::AlreadyUpToDate:
    return QObject::tr("nothing new to bring in");
  case GitMergeRelation::FastForward:
    return QObject::tr("a clean catch-up");
  case GitMergeRelation::Diverged:
    return QObject::tr("both sides moved on");
  }
  return QString();
}

QString GitMergePlan::headline() const {
  if (!valid) {
    return QObject::tr("Pick a branch to bring in");
  }

  switch (relation) {
  case GitMergeRelation::AlreadyUpToDate:
    return QObject::tr("%1 already has everything from %2")
        .arg(targetRef, sourceRef);
  case GitMergeRelation::Unrelated:
    return QObject::tr("%1 and %2 have nothing in common")
        .arg(sourceRef, targetRef);
  case GitMergeRelation::FastForward:
    return QObject::tr("Catch %1 up to %2 (%3)")
        .arg(targetRef, sourceRef, commitCount(incomingCommits));
  case GitMergeRelation::Diverged:
    return QObject::tr("Bring %1 from %2 into %3")
        .arg(commitCount(incomingCommits), sourceRef, targetRef);
  case GitMergeRelation::Unknown:
    break;
  }
  return QObject::tr("Merge %1 into %2").arg(sourceRef, targetRef);
}

QString GitMergePlan::outcomeSentence() const {
  if (!valid) {
    return QString();
  }

  switch (relation) {
  case GitMergeRelation::AlreadyUpToDate:
    return QObject::tr(
               "Nothing will change. Every commit on %1 is already part of %2.")
        .arg(sourceRef, targetRef);
  case GitMergeRelation::Unrelated:
    return QObject::tr(
        "These two branches were never related, so Git has no common "
        "starting point to compare them from. Almost every file will "
        "look contested.");
  case GitMergeRelation::FastForward:
    return QObject::tr(
               "You have made no commits of your own since the two branches "
               "split, so %1 simply moves forward to where %2 already is. "
               "There is nothing to conflict with.")
        .arg(targetRef, sourceRef);
  case GitMergeRelation::Diverged:
    return QObject::tr(
               "You have %1 that %2 does not, and %2 has %3 that you do not. "
               "Git will weave both histories together and create one merge "
               "commit on %4.")
        .arg(commitCount(localOnlyCommits), sourceRef,
             commitCount(incomingCommits), targetRef);
  case GitMergeRelation::Unknown:
    break;
  }
  return QString();
}

QString GitMergePlan::conflictSentence() const {
  if (!valid) {
    return QString();
  }
  if (relation == GitMergeRelation::AlreadyUpToDate) {
    return QObject::tr("No files change, so nothing can go wrong.");
  }
  if (relation == GitMergeRelation::FastForward) {
    return QObject::tr("A catch-up never conflicts.");
  }
  if (likelyConflicts.isEmpty()) {
    return changedFiles.size() == 1
               ? QObject::tr("Git expects to merge that one file on its own, "
                             "with nothing for you to decide.")
               : QObject::tr("Git expects to merge all %1 files on its own, "
                             "with nothing for you to decide.")
                     .arg(changedFiles.size());
  }
  return likelyConflicts.size() == 1
             ? QObject::tr("1 of these files will need your decision.")
             : QObject::tr("%1 of these files will need your decision.")
                   .arg(likelyConflicts.size());
}

QString gitMergePreferenceExplanation(GitMergeOptions::Preference preference,
                                      const QString &oursName,
                                      const QString &theirsName) {
  switch (preference) {
  case GitMergeOptions::Preference::Manual:
    return QObject::tr(
        "Git merges everything it can on its own and stops at the lines where "
        "the two branches disagree, so you can look at each one and choose. "
        "Nothing is thrown away without you seeing it.");
  case GitMergeOptions::Preference::PreferOurs:
    return QObject::tr(
               "Wherever the same lines disagree, the version already on %1 "
               "wins and the version from %2 is dropped without asking. "
               "Everything that does not clash still comes in.")
        .arg(oursName, theirsName);
  case GitMergeOptions::Preference::PreferTheirs:
    return QObject::tr(
               "Wherever the same lines disagree, the version from %1 wins and "
               "your version on %2 is dropped without asking. Everything that "
               "does not clash still comes in.")
        .arg(theirsName, oursName);
  }
  return QString();
}

QList<GitMergeChoice> gitMergeChoices(GitIntegration *git,
                                      const QString &targetRef) {
  QList<GitMergeChoice> choices;
  if (!git || !git->isValidRepository()) {
    return choices;
  }

  const QString current =
      targetRef.isEmpty() ? git->currentBranch() : targetRef;

  QList<QString> order;
  QMap<QString, GitMergeChoice> byName;

  for (const GitBranchInfo &branch : git->getBranches()) {
    if (branch.name.isEmpty() || branch.name.endsWith(QLatin1String("/HEAD"))) {
      continue;
    }

    GitMergeSource source;
    source.ref = branch.name;
    source.isRemote = branch.isRemote;
    source.isCurrent = branch.isCurrent;
    source.remoteName = branch.isRemote ? remoteOf(branch.name) : QString();
    source.displayName =
        branch.isRemote ? shortNameOf(branch.name) : branch.name;

    if (!source.isRemote && source.displayName == current) {

      continue;
    }

    source.summary = git->refSummary(branch.name);
    if (!source.summary.exists) {
      continue;
    }

    if (!byName.contains(source.displayName)) {
      GitMergeChoice choice;
      choice.name = source.displayName;
      byName.insert(source.displayName, choice);
      order.append(source.displayName);
    }

    GitMergeChoice &choice = byName[source.displayName];
    if (source.isRemote) {
      choice.copies.append(source);
    } else {
      choice.copies.prepend(source);
    }
  }

  for (const QString &name : order) {
    choices.append(byName.value(name));
  }
  return choices;
}

GitMergePlan buildGitMergePlan(GitIntegration *git, const QString &sourceRef,
                               const QString &targetRef) {
  GitMergePlan plan;
  if (!git || !git->isValidRepository() || sourceRef.trimmed().isEmpty()) {
    return plan;
  }

  plan.sourceRef = sourceRef.trimmed();
  plan.targetRef = targetRef.isEmpty() ? git->currentBranch() : targetRef;
  plan.source = git->refSummary(plan.sourceRef);
  plan.target = git->refSummary(
      plan.targetRef.isEmpty() ? QStringLiteral("HEAD") : plan.targetRef);

  if (!plan.source.exists) {
    plan.blockers.append(QObject::tr("Lightpad cannot find a branch called %1.")
                             .arg(plan.sourceRef));
    return plan;
  }

  plan.valid = true;
  if (!plan.target.exists) {
    plan.valid = false;
    plan.blockers.append(
        QObject::tr("The destination branch no longer exists."));
    return plan;
  }

  const GitRepositoryState state = git->repositoryState();
  plan.operationInProgress = state.operation != GitOperation::None;
  plan.workingTreeDirty = git->isDirty();

  if (plan.operationInProgress) {
    plan.blockers.append(
        QObject::tr("Another Git operation is still half finished. Finish or "
                    "cancel it before starting a merge."));
  }
  if (plan.workingTreeDirty) {
    plan.blockers.append(QObject::tr(
        "You have edits that are not committed yet. Commit or "
        "stash them first so they cannot be mixed into the merge."));
  }
  if (plan.targetRef.isEmpty()) {
    plan.blockers.append(
        QObject::tr("You are not on a branch right now, so there is nothing "
                    "for the merge to land on."));
  }

  plan.mergeBase =
      git->getMergeBase(plan.sourceRef, plan.targetRef.isEmpty()
                                            ? QStringLiteral("HEAD")
                                            : plan.targetRef)
          .trimmed();

  const QString target =
      plan.targetRef.isEmpty() ? QStringLiteral("HEAD") : plan.targetRef;

  if (plan.mergeBase.isEmpty()) {
    plan.relation = GitMergeRelation::Unrelated;
  } else {
    plan.mergeBaseSubject = git->refSummary(plan.mergeBase).subject;
    plan.incomingCommits = git->countCommitsNotIn(plan.sourceRef, target);
    plan.localOnlyCommits = git->countCommitsNotIn(target, plan.sourceRef);

    if (plan.incomingCommits == 0) {
      plan.relation = GitMergeRelation::AlreadyUpToDate;
    } else if (plan.localOnlyCommits == 0) {
      plan.relation = GitMergeRelation::FastForward;
    } else {
      plan.relation = GitMergeRelation::Diverged;
    }
  }

  if (plan.relation != GitMergeRelation::AlreadyUpToDate) {
    plan.changedFiles = git->filesChangedBetween(
        plan.mergeBase.isEmpty() ? target : plan.mergeBase, plan.sourceRef);
    plan.incomingLog =
        git->getCommitLog(kIncomingLogLimit,
                          QStringLiteral("%1..%2").arg(target, plan.sourceRef));
  }

  if (plan.relation == GitMergeRelation::Diverged ||
      plan.relation == GitMergeRelation::Unrelated) {
    plan.likelyConflicts = git->predictMergeConflicts(target, plan.sourceRef);
  }

  return plan;
}
