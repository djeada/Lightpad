#include "gitprovenance.h"
#include "gitintegration.h"
#include <QObject>

QString gitProvenanceChurnSummary(const GitLineProvenance &provenance) {
  if (!provenance.valid || provenance.steps.isEmpty()) {
    return QObject::tr("No commit in the loaded history changed these lines.");
  }

  const int count = provenance.steps.size();
  const QString oldest = provenance.steps.last().commit.relativeDate;
  const QString newest = provenance.steps.first().commit.relativeDate;

  if (count == 1) {
    return QObject::tr("Changed once, %1. It has been stable since.")
        .arg(newest);
  }
  return QObject::tr(
             "Changed %1 times, most recently %2 and first %3 in the loaded "
             "history.")
      .arg(count)
      .arg(newest, oldest);
}

QString gitProvenanceCaveat(const GitLineProvenance &provenance) {
  QStringList parts;

  parts << QObject::tr(
      "Blame says which commit last touched each line as it stands now. That "
      "is not the same as who wrote the logic, and it is not the same as why.");

  if (provenance.attributionUncertain) {
    parts << (provenance.movedFromPath.isEmpty()
                  ? QObject::tr(
                        "Git's move and copy detection attributes this range "
                        "differently from plain blame, so the attribution "
                        "here is a heuristic rather than a fact.")
                  : QObject::tr(
                        "Git thinks this code moved from %1. Move detection is "
                        "a heuristic, so treat the attribution as a lead "
                        "rather than a fact.")
                        .arg(provenance.movedFromPath));
  }
  if (provenance.followedRename) {
    parts << QObject::tr(
        "The file was renamed at some point; this history follows the rename, "
        "which Git infers from content rather than records.");
  }

  return parts.join(QStringLiteral(" "));
}

GitLineProvenance buildLineProvenance(GitIntegration *git,
                                      const QString &filePath, int startLine,
                                      int endLine) {
  GitLineProvenance provenance;
  if (!git || !git->isValidRepository() || filePath.isEmpty() ||
      startLine <= 0 || endLine < startLine) {
    return provenance;
  }

  provenance.valid = true;
  provenance.filePath = filePath;
  provenance.startLine = startLine;
  provenance.endLine = endLine;

  for (const GitCommitInfo &commit :
       git->getLineHistory(filePath, startLine, endLine)) {
    GitProvenanceStep step;
    step.commit = commit;
    step.pathAtRevision = filePath;
    provenance.steps.append(step);
  }
  if (!provenance.steps.isEmpty()) {
    provenance.latest = provenance.steps.first().commit;
  }

  const QString plain = git->blameCommitForLine(filePath, startLine, false);
  const QString detected = git->blameCommitForLine(filePath, startLine, true);
  if (!plain.isEmpty() && !detected.isEmpty() && plain != detected) {
    provenance.attributionUncertain = true;
    provenance.movedFromPath = git->blameOriginalPath(filePath, startLine);
  }

  provenance.currentText = git->fileLines(filePath, startLine, endLine);
  if (!provenance.latest.hash.isEmpty()) {
    provenance.previousText = git->fileLinesAtRevision(
        filePath, provenance.latest.hash + "^", startLine, endLine);
  }

  for (const GitProvenanceStep &step : provenance.steps) {
    if (!step.pathAtRevision.isEmpty() && step.pathAtRevision != filePath) {
      provenance.followedRename = true;
      break;
    }
  }
  if (!provenance.movedFromPath.isEmpty() &&
      provenance.movedFromPath != filePath) {
    provenance.followedRename = true;
  }

  return provenance;
}
