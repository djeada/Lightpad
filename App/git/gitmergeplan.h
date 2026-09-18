#ifndef GITMERGEPLAN_H
#define GITMERGEPLAN_H

#include "gitintegration.h"
#include <QList>
#include <QString>
#include <QStringList>

enum class GitMergeRelation {
  Unknown,

  Unrelated,

  AlreadyUpToDate,

  FastForward,

  Diverged,
};

struct GitMergeSource {
  QString ref;
  QString displayName;
  QString remoteName;
  bool isRemote = false;
  bool isCurrent = false;
  GitRefSummary summary;

  QString whereLabel() const;
};

struct GitMergeChoice {
  QString name;
  QList<GitMergeSource> copies;

  bool hasLocal() const;
  bool hasRemote() const;
  bool isSplit() const { return copies.size() > 1; }

  bool copiesDisagree() const;

  const GitMergeSource *preferred() const;
};

struct GitMergePlan {
  bool valid = false;

  QString sourceRef;
  QString targetRef;
  GitRefSummary source;
  GitRefSummary target;

  GitMergeRelation relation = GitMergeRelation::Unknown;

  QString mergeBase;
  QString mergeBaseSubject;

  int incomingCommits = 0;
  int localOnlyCommits = 0;

  QStringList changedFiles;
  QStringList likelyConflicts;
  QList<GitCommitInfo> incomingLog;

  bool workingTreeDirty = false;
  bool operationInProgress = false;

  QStringList blockers;

  bool canStart() const;

  QString headline() const;

  QString outcomeSentence() const;

  QString conflictSentence() const;
};

QList<GitMergeChoice> gitMergeChoices(GitIntegration *git,
                                      const QString &targetRef = QString());

GitMergePlan buildGitMergePlan(GitIntegration *git, const QString &sourceRef,
                               const QString &targetRef = QString());

QString gitMergeRelationName(GitMergeRelation relation);

QString gitMergePreferenceExplanation(GitMergeOptions::Preference preference,
                                      const QString &oursName,
                                      const QString &theirsName);

#endif
