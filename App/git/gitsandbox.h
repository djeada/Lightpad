#ifndef GITSANDBOX_H
#define GITSANDBOX_H

#include "gitoperationpreview.h"
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

class GitIntegration;

struct SandboxCommit {
  QString id;
  QString shortHash;
  QString subject;
  QStringList parentIds;

  bool synthetic = false;

  QString derivedFrom;
};

struct SandboxRef {
  QString name;
  QString commitId;
  bool isHead = false;
  bool isRemote = false;
};

class GitSandbox {
public:
  void loadFrom(GitIntegration *git, int maxCommits = 60);

  bool isLoaded() const { return m_loaded; }
  const QList<SandboxCommit> &commits() const { return m_commits; }
  const QList<SandboxRef> &refs() const { return m_refs; }
  const QStringList &journal() const { return m_journal; }
  bool isModified() const { return !m_journal.isEmpty(); }

  void reset();

  QString simulateMerge(const QString &intoRef, const QString &fromRef);
  QString simulateRebase(const QString &branchRef, const QString &ontoRef);
  QString simulateReset(const QString &refName, const QString &commitId);
  QString simulateCherryPick(const QString &intoRef, const QString &commitId);
  QString simulateDeleteRef(const QString &refName);

  QStringList unreachableCommitIds() const;

  QList<GitOperationRequest> plan() const { return m_plan; }

  QStringList limitations() const;

  const SandboxCommit *commitById(const QString &id) const;
  const SandboxRef *refByName(const QString &name) const;

private:
  QString refTarget(const QString &name) const;
  void setRef(const QString &name, const QString &commitId);
  QStringList commitsOnlyIn(const QString &fromId,
                            const QString &notInId) const;
  QStringList ancestorsOf(const QString &id) const;
  QString addSynthetic(const QString &subject, const QStringList &parents,
                       const QString &derivedFrom);

  bool m_loaded = false;
  QList<SandboxCommit> m_commits;
  QList<SandboxRef> m_refs;
  QList<SandboxCommit> m_loadedCommits;
  QList<SandboxRef> m_loadedRefs;
  QStringList m_journal;
  QList<GitOperationRequest> m_plan;
  int m_syntheticCounter = 0;
};

#endif
