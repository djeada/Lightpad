#ifndef GITCOMPARISON_H
#define GITCOMPARISON_H

#include "gitintegration.h"
#include <QList>
#include <QString>
#include <QStringList>

struct GitCompareEndpoint {
  enum class Kind {
    WorkingTree,
    Index,
    Commit,
    Branch,
    RemoteBranch,
    Tag,
    Stash,
  };

  Kind kind = Kind::Commit;

  QString ref;

  QString label;

  bool isCommitish() const {
    return kind != Kind::WorkingTree && kind != Kind::Index;
  }

  int layerRank() const {
    switch (kind) {
    case Kind::WorkingTree:
      return 2;
    case Kind::Index:
      return 1;
    default:
      return 0;
    }
  }

  bool operator==(const GitCompareEndpoint &other) const {
    return kind == other.kind && ref == other.ref;
  }

  static GitCompareEndpoint workingTree();
  static GitCompareEndpoint index();
  static GitCompareEndpoint head();
  static GitCompareEndpoint commit(const QString &hash,
                                   const QString &label = QString());
  static GitCompareEndpoint branch(const QString &name);
  static GitCompareEndpoint remoteBranch(const QString &name);
  static GitCompareEndpoint tag(const QString &name);
  static GitCompareEndpoint stash(const QString &selector,
                                  const QString &message = QString());
};

struct GitComparisonFile {
  QString path;
  QString oldPath;

  QChar status = QLatin1Char('M');
  int additions = 0;
  int deletions = 0;
  bool isBinary = false;

  QString statusText() const;
};

struct GitComparisonResult {
  bool valid = false;
  QString error;

  GitCompareEndpoint base;
  GitCompareEndpoint compare;

  QString mergeBase;
  bool mergeBaseMeaningful = false;

  bool diverged = false;

  QList<GitCommitInfo> onlyInBase;
  QList<GitCommitInfo> onlyInCompare;

  QList<GitComparisonFile> files;
  QString diffText;

  int additions() const;
  int deletions() const;
  bool isEmpty() const {
    return files.isEmpty() && diffText.trimmed().isEmpty();
  }
};

QStringList gitDiffArgsFor(const GitCompareEndpoint &base,
                           const GitCompareEndpoint &compare);

QString gitComparisonDescription(const GitCompareEndpoint &base,
                                 const GitCompareEndpoint &compare);

QList<GitComparisonFile> parseComparisonFiles(const QString &numstat,
                                              const QString &nameStatus);

GitComparisonResult runGitComparison(GitIntegration *git,
                                     const GitCompareEndpoint &base,
                                     const GitCompareEndpoint &compare,
                                     int maxCommits = 200);

QList<GitCompareEndpoint> availableCompareEndpoints(GitIntegration *git);

#endif
