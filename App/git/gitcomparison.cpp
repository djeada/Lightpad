#include "gitcomparison.h"
#include <QObject>

namespace {

GitCompareEndpoint make(GitCompareEndpoint::Kind kind, const QString &ref,
                        const QString &label) {
  GitCompareEndpoint endpoint;
  endpoint.kind = kind;
  endpoint.ref = ref;
  endpoint.label = label;
  return endpoint;
}

} // namespace

GitCompareEndpoint GitCompareEndpoint::workingTree() {
  return make(Kind::WorkingTree, QString(), QObject::tr("Working Tree"));
}

GitCompareEndpoint GitCompareEndpoint::index() {
  return make(Kind::Index, QString(), QObject::tr("Index (staged)"));
}

GitCompareEndpoint GitCompareEndpoint::head() {
  return make(Kind::Commit, QStringLiteral("HEAD"), QStringLiteral("HEAD"));
}

GitCompareEndpoint GitCompareEndpoint::commit(const QString &hash,
                                              const QString &label) {
  return make(Kind::Commit, hash, label.isEmpty() ? hash.left(7) : label);
}

GitCompareEndpoint GitCompareEndpoint::branch(const QString &name) {
  return make(Kind::Branch, name, name);
}

GitCompareEndpoint GitCompareEndpoint::remoteBranch(const QString &name) {
  return make(Kind::RemoteBranch, name, name);
}

GitCompareEndpoint GitCompareEndpoint::tag(const QString &name) {
  return make(Kind::Tag, name, name);
}

GitCompareEndpoint GitCompareEndpoint::stash(const QString &selector,
                                             const QString &message) {
  return make(Kind::Stash, selector,
              message.isEmpty()
                  ? selector
                  : QStringLiteral("%1 — %2").arg(selector, message));
}

QString GitComparisonFile::statusText() const {
  switch (status.toLatin1()) {
  case 'A':
    return QObject::tr("added");
  case 'D':
    return QObject::tr("deleted");
  case 'R':
    return QObject::tr("renamed");
  case 'C':
    return QObject::tr("copied");
  case 'T':
    return QObject::tr("type changed");
  case 'M':
  default:
    return QObject::tr("modified");
  }
}

int GitComparisonResult::additions() const {
  int total = 0;
  for (const GitComparisonFile &file : files) {
    total += file.additions;
  }
  return total;
}

int GitComparisonResult::deletions() const {
  int total = 0;
  for (const GitComparisonFile &file : files) {
    total += file.deletions;
  }
  return total;
}

QStringList gitDiffArgsFor(const GitCompareEndpoint &base,
                           const GitCompareEndpoint &compare) {
  QStringList args{QStringLiteral("diff")};

  const bool reversed = base.layerRank() < compare.layerRank();
  const GitCompareEndpoint &low = reversed ? base : compare;
  const GitCompareEndpoint &high = reversed ? compare : base;

  if (base.isCommitish() && compare.isCommitish()) {
    args << base.ref << compare.ref;
    return args;
  }

  if (high.kind == GitCompareEndpoint::Kind::WorkingTree) {
    if (low.kind == GitCompareEndpoint::Kind::Index) {

    } else {
      args << low.ref;
    }
  } else if (high.kind == GitCompareEndpoint::Kind::Index) {
    args << QStringLiteral("--cached");
    if (low.isCommitish() && low.ref != QLatin1String("HEAD")) {
      args << low.ref;
    }
  }

  if (!reversed) {
    args << QStringLiteral("-R");
  }
  return args;
}

QString gitComparisonDescription(const GitCompareEndpoint &base,
                                 const GitCompareEndpoint &compare) {
  return QObject::tr("Changes from %1 to %2").arg(base.label, compare.label);
}

QList<GitComparisonFile> parseComparisonFiles(const QString &numstat,
                                              const QString &nameStatus) {
  QList<GitComparisonFile> files;
  QHash<QString, int> indexByPath;

  for (const QString &line :
       numstat.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
    const QStringList parts = line.split(QLatin1Char('\t'));
    if (parts.size() < 3) {
      continue;
    }
    GitComparisonFile file;

    file.isBinary = parts[0] == QLatin1String("-");
    file.additions = file.isBinary ? 0 : parts[0].toInt();
    file.deletions = file.isBinary ? 0 : parts[1].toInt();
    if (parts.size() >= 4) {
      file.oldPath = parts[2];
      file.path = parts[3];
    } else {
      file.path = parts[2];
    }
    indexByPath.insert(file.path, files.size());
    files.append(file);
  }

  for (const QString &line :
       nameStatus.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
    const QStringList parts = line.split(QLatin1Char('\t'));
    if (parts.size() < 2 || parts[0].isEmpty()) {
      continue;
    }
    const QChar status = parts[0].at(0);
    const QString path = parts.last();
    const auto it = indexByPath.constFind(path);
    if (it != indexByPath.constEnd()) {
      files[it.value()].status = status;
      if (parts.size() >= 3) {
        files[it.value()].oldPath = parts[1];
      }
      continue;
    }

    GitComparisonFile file;
    file.status = status;
    file.path = path;
    if (parts.size() >= 3) {
      file.oldPath = parts[1];
    }
    files.append(file);
  }

  return files;
}

GitComparisonResult runGitComparison(GitIntegration *git,
                                     const GitCompareEndpoint &base,
                                     const GitCompareEndpoint &compare,
                                     int maxCommits) {
  GitComparisonResult result;
  result.base = base;
  result.compare = compare;

  if (!git || !git->isValidRepository()) {
    result.error = QObject::tr("No Git repository.");
    return result;
  }
  if (base == compare) {
    result.valid = true;
    return result;
  }

  const QStringList diffArgs = gitDiffArgsFor(base, compare);

  QStringList statArgs = diffArgs;
  statArgs.insert(1, QStringLiteral("--numstat"));
  QStringList nameArgs = diffArgs;
  nameArgs.insert(1, QStringLiteral("--name-status"));

  const QString numstat = git->executeWordDiff(statArgs);
  const QString nameStatus = git->executeWordDiff(nameArgs);
  result.files = parseComparisonFiles(numstat, nameStatus);
  result.diffText = git->executeWordDiff(diffArgs);

  if (base.isCommitish() && compare.isCommitish()) {
    const QString mergeBase = git->getMergeBase(base.ref, compare.ref);
    if (!mergeBase.isEmpty()) {
      result.mergeBase = mergeBase;
      result.mergeBaseMeaningful = true;
    }

    result.onlyInCompare = git->getCommitLogPage(
        QStringLiteral("%1..%2").arg(base.ref, compare.ref), 0, maxCommits);
    result.onlyInBase = git->getCommitLogPage(
        QStringLiteral("%1..%2").arg(compare.ref, base.ref), 0, maxCommits);
    result.diverged =
        !result.onlyInBase.isEmpty() && !result.onlyInCompare.isEmpty();
  }

  result.valid = true;
  return result;
}

QList<GitCompareEndpoint> availableCompareEndpoints(GitIntegration *git) {
  QList<GitCompareEndpoint> endpoints;
  endpoints << GitCompareEndpoint::workingTree() << GitCompareEndpoint::index()
            << GitCompareEndpoint::head();

  if (!git || !git->isValidRepository()) {
    return endpoints;
  }

  for (const GitBranchInfo &branch : git->getBranches()) {
    endpoints << (branch.isRemote
                      ? GitCompareEndpoint::remoteBranch(branch.name)
                      : GitCompareEndpoint::branch(branch.name));
  }
  for (const GitTagInfo &tag : git->getTags()) {
    endpoints << GitCompareEndpoint::tag(tag.name);
  }
  for (const GitStashEntry &stash : git->getStashList()) {
    endpoints << GitCompareEndpoint::stash(
        QStringLiteral("stash@{%1}").arg(stash.index), stash.message);
  }

  return endpoints;
}
