#include "gitsandbox.h"
#include "gitintegration.h"
#include <QObject>
#include <QQueue>
#include <QSet>

void GitSandbox::loadFrom(GitIntegration *git, int maxCommits) {
  m_commits.clear();
  m_refs.clear();
  m_journal.clear();
  m_plan.clear();
  m_syntheticCounter = 0;
  m_loaded = false;

  if (!git || !git->isValidRepository()) {
    return;
  }

  GitLogOptions options;
  options.allRefs = true;
  for (const GitCommitInfo &commit : git->getLogPage(options, 0, maxCommits)) {
    SandboxCommit node;
    node.id = commit.hash;
    node.shortHash = commit.shortHash;
    node.subject = commit.subject;
    node.parentIds = commit.parents;
    m_commits.append(node);
  }

  const QString headHash = git->getCommitDetails(QStringLiteral("HEAD")).hash;
  const QString currentBranch = git->currentBranch();
  for (const GitBranchInfo &branch : git->getBranches()) {
    SandboxRef ref;
    ref.name = branch.name;
    ref.isRemote = branch.isRemote;
    ref.isHead = !branch.isRemote && branch.name == currentBranch;
    ref.commitId =
        ref.isHead ? headHash : git->getCommitDetails(branch.name).hash;
    if (!ref.commitId.isEmpty()) {
      m_refs.append(ref);
    }
  }

  m_loadedCommits = m_commits;
  m_loadedRefs = m_refs;
  m_loaded = true;
}

void GitSandbox::reset() {
  m_commits = m_loadedCommits;
  m_refs = m_loadedRefs;
  m_journal.clear();
  m_plan.clear();
  m_syntheticCounter = 0;
}

const SandboxCommit *GitSandbox::commitById(const QString &id) const {
  for (const SandboxCommit &commit : m_commits) {
    if (commit.id == id) {
      return &commit;
    }
  }
  return nullptr;
}

const SandboxRef *GitSandbox::refByName(const QString &name) const {
  for (const SandboxRef &ref : m_refs) {
    if (ref.name == name) {
      return &ref;
    }
  }
  return nullptr;
}

QString GitSandbox::refTarget(const QString &name) const {
  const SandboxRef *ref = refByName(name);
  return ref ? ref->commitId : QString();
}

void GitSandbox::setRef(const QString &name, const QString &commitId) {
  for (SandboxRef &ref : m_refs) {
    if (ref.name == name) {
      ref.commitId = commitId;
      return;
    }
  }
}

QStringList GitSandbox::ancestorsOf(const QString &id) const {
  QStringList result;
  QSet<QString> seen;
  QQueue<QString> queue;
  queue.enqueue(id);

  while (!queue.isEmpty()) {
    const QString current = queue.dequeue();
    if (current.isEmpty() || seen.contains(current)) {
      continue;
    }
    seen.insert(current);
    result << current;
    if (const SandboxCommit *commit = commitById(current)) {
      for (const QString &parent : commit->parentIds) {
        queue.enqueue(parent);
      }
    }
  }
  return result;
}

QStringList GitSandbox::commitsOnlyIn(const QString &fromId,
                                      const QString &notInId) const {
  const QStringList mine = ancestorsOf(fromId);
  const QStringList theirs = ancestorsOf(notInId);
  const QSet<QString> exclude(theirs.constBegin(), theirs.constEnd());

  QStringList only;
  for (const QString &id : mine) {
    if (!exclude.contains(id)) {
      only << id;
    }
  }
  return only;
}

QString GitSandbox::addSynthetic(const QString &subject,
                                 const QStringList &parents,
                                 const QString &derivedFrom) {
  SandboxCommit commit;
  commit.id = QStringLiteral("sandbox-%1").arg(++m_syntheticCounter);
  commit.shortHash = QObject::tr("new");
  commit.subject = subject;
  commit.parentIds = parents;
  commit.synthetic = true;
  commit.derivedFrom = derivedFrom;
  m_commits.prepend(commit);
  return commit.id;
}

QString GitSandbox::simulateMerge(const QString &intoRef,
                                  const QString &fromRef) {
  const QString into = refTarget(intoRef);
  const QString from = refTarget(fromRef);
  if (into.isEmpty() || from.isEmpty()) {
    return QObject::tr("Both refs have to exist in the sandbox.");
  }

  const QStringList incoming = commitsOnlyIn(from, into);
  if (incoming.isEmpty()) {
    const QString message =
        QObject::tr("%1 already contains everything on %2; nothing would "
                    "happen.")
            .arg(intoRef, fromRef);
    m_journal << message;
    return message;
  }

  const QStringList outgoing = commitsOnlyIn(into, from);
  QString message;
  if (outgoing.isEmpty()) {

    setRef(intoRef, from);
    message =
        QObject::tr("%1 fast-forwards onto %2: the ref moves, no commit is "
                    "created and nothing is rewritten.")
            .arg(intoRef, fromRef);
  } else {
    const QString mergeId =
        addSynthetic(QObject::tr("Merge %1 into %2").arg(fromRef, intoRef),
                     {into, from}, QString());
    setRef(intoRef, mergeId);
    message = QObject::tr(
                  "%1 gains one merge commit joining its %2 commits to the %3 "
                  "from %4. Both sides keep their hashes.")
                  .arg(intoRef)
                  .arg(outgoing.size())
                  .arg(incoming.size())
                  .arg(fromRef);
  }

  GitOperationRequest request;
  request.kind = GitOperationKind::Merge;
  request.target = fromRef;
  m_plan.append(request);
  m_journal << message;
  return message;
}

QString GitSandbox::simulateRebase(const QString &branchRef,
                                   const QString &ontoRef) {
  const QString branch = refTarget(branchRef);
  const QString onto = refTarget(ontoRef);
  if (branch.isEmpty() || onto.isEmpty()) {
    return QObject::tr("Both refs have to exist in the sandbox.");
  }

  QStringList replayed = commitsOnlyIn(branch, onto);
  if (replayed.isEmpty()) {
    const QString message =
        QObject::tr("%1 has nothing of its own to replay.").arg(branchRef);
    m_journal << message;
    return message;
  }

  std::reverse(replayed.begin(), replayed.end());
  QString tip = onto;
  for (const QString &id : replayed) {
    const SandboxCommit *original = commitById(id);
    tip = addSynthetic(original ? original->subject
                                : QObject::tr("replayed commit"),
                       {tip}, id);
  }
  setRef(branchRef, tip);

  const QString message =
      QObject::tr(
          "%1 is recreated on top of %2 as %3 new commits. The originals are "
          "still there but nothing points at them any more — a rebase copies, "
          "it does not move.")
          .arg(branchRef, ontoRef)
          .arg(replayed.size());

  GitOperationRequest request;
  request.kind = GitOperationKind::Rebase;
  request.target = ontoRef;
  m_plan.append(request);
  m_journal << message;
  return message;
}

QString GitSandbox::simulateReset(const QString &refName,
                                  const QString &commitId) {
  if (refTarget(refName).isEmpty() || !commitById(commitId)) {
    return QObject::tr("The ref and the commit both have to be in the "
                       "sandbox.");
  }

  const QStringList dropped = commitsOnlyIn(refTarget(refName), commitId);
  setRef(refName, commitId);

  const QString message =
      dropped.isEmpty()
          ? QObject::tr("%1 already points there.").arg(refName)
          : QObject::tr("%1 moves back past %2 commits. Whether your files "
                        "and index follow depends on the reset mode, which "
                        "the sandbox does not model.")
                .arg(refName)
                .arg(dropped.size());

  GitOperationRequest request;
  request.kind = GitOperationKind::Reset;
  request.target = commitId;
  m_plan.append(request);
  m_journal << message;
  return message;
}

QString GitSandbox::simulateCherryPick(const QString &intoRef,
                                       const QString &commitId) {
  const QString into = refTarget(intoRef);
  const SandboxCommit *source = commitById(commitId);
  if (into.isEmpty() || !source) {
    return QObject::tr("The ref and the commit both have to be in the "
                       "sandbox.");
  }

  const QString newId = addSynthetic(source->subject, {into}, commitId);
  setRef(intoRef, newId);

  const QString message =
      QObject::tr("%1 gains a copy of %2 as a new commit. The original stays "
                  "where it is — this copies a change, not a commit.")
          .arg(intoRef, source->shortHash);

  GitOperationRequest request;
  request.kind = GitOperationKind::CherryPick;
  request.target = commitId;
  m_plan.append(request);
  m_journal << message;
  return message;
}

QString GitSandbox::simulateDeleteRef(const QString &refName) {
  const SandboxRef *ref = refByName(refName);
  if (!ref) {
    return QObject::tr("No such ref in the sandbox.");
  }
  if (ref->isHead) {
    return QObject::tr("The checked-out branch cannot be deleted.");
  }

  const QString target = ref->commitId;
  for (int i = 0; i < m_refs.size(); ++i) {
    if (m_refs.at(i).name == refName) {
      m_refs.removeAt(i);
      break;
    }
  }

  int orphaned = 0;
  const QStringList unreachable = unreachableCommitIds();
  for (const QString &id : unreachable) {
    if (ancestorsOf(target).contains(id)) {
      ++orphaned;
    }
  }

  const QString message =
      orphaned == 0
          ? QObject::tr("Deleting %1 removes only a name: every commit on it "
                        "is still reachable from another ref.")
                .arg(refName)
          : QObject::tr("Deleting %1 leaves %2 commits with no ref pointing "
                        "at them.")
                .arg(refName)
                .arg(orphaned);

  GitOperationRequest request;
  request.kind = GitOperationKind::BranchDelete;
  request.target = refName;
  m_plan.append(request);
  m_journal << message;
  return message;
}

QStringList GitSandbox::unreachableCommitIds() const {
  QSet<QString> reachable;
  for (const SandboxRef &ref : m_refs) {
    for (const QString &id : ancestorsOf(ref.commitId)) {
      reachable.insert(id);
    }
  }

  QStringList unreachable;
  for (const SandboxCommit &commit : m_commits) {
    if (!reachable.contains(commit.id)) {
      unreachable << commit.id;
    }
  }
  return unreachable;
}

QStringList GitSandbox::limitations() const {
  return {
      QObject::tr("Conflicts are not simulated. Whether a merge or rebase "
                  "actually applies cleanly depends on file content, which "
                  "the sandbox does not read."),
      QObject::tr("The working tree and the index are not modelled. A reset's "
                  "mode, and anything uncommitted, are outside this picture."),
      QObject::tr("Only the commits loaded into the sandbox are considered, so "
                  "reachability answers apply to that window of history."),
  };
}
