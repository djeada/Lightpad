#include "gitcommandlog.h"
#include <QObject>
#include <QRegularExpression>

namespace {

bool needsQuoting(const QString &argument) {
  static const QRegularExpression plain(
      QStringLiteral("^[A-Za-z0-9_@%+=:,./~^{}-]*$"));
  return argument.isEmpty() || !plain.match(argument).hasMatch();
}

const QStringList &readOnlySubcommands() {
  static const QStringList commands{
      QStringLiteral("status"),     QStringLiteral("log"),
      QStringLiteral("show"),       QStringLiteral("diff"),
      QStringLiteral("blame"),      QStringLiteral("rev-parse"),
      QStringLiteral("rev-list"),   QStringLiteral("for-each-ref"),
      QStringLiteral("merge-base"), QStringLiteral("ls-files"),
      QStringLiteral("cat-file"),   QStringLiteral("reflog"),
      QStringLiteral("describe"),   QStringLiteral("merge-tree"),
      QStringLiteral("config"),     QStringLiteral("worktree"),
      QStringLiteral("tag"),        QStringLiteral("branch")};
  return commands;
}

} // namespace

QString GitCommandRecord::commandLine() const { return formatGitCommand(args); }

QString gitCommandMirrorModeName(GitCommandMirrorMode mode) {
  switch (mode) {
  case GitCommandMirrorMode::Hidden:

    return QObject::tr("Hidden — record commands but do not show them");
  case GitCommandMirrorMode::Learn:
    return QObject::tr("Learn — show the command after each action");
  case GitCommandMirrorMode::Preview:
    return QObject::tr("Preview — show the command before it runs");
  case GitCommandMirrorMode::Expert:
    return QObject::tr("Expert — let me edit the command first");
  }
  return QString();
}

QString formatGitCommand(const QStringList &args) {
  QStringList parts{QStringLiteral("git")};
  for (const QString &argument : args) {
    if (needsQuoting(argument)) {
      QString quoted = argument;
      quoted.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
      parts << QStringLiteral("'%1'").arg(quoted);
    } else {
      parts << argument;
    }
  }
  return redactSecrets(parts.join(QLatin1Char(' ')));
}

QString redactSecrets(const QString &text) {
  QString result = text;

  static const QRegularExpression urlCredentials(
      QStringLiteral("(https?://)([^/@\\s:]+)(:[^/@\\s]*)?@"));
  result.replace(urlCredentials, QStringLiteral("\\1***@"));

  static const QRegularExpression namedSecret(
      QStringLiteral("(--?(?:password|token|secret|api[-_]?key)[= ])([^\\s]+)"),
      QRegularExpression::CaseInsensitiveOption);
  result.replace(namedSecret, QStringLiteral("\\1***"));

  static const QRegularExpression bearer(
      QStringLiteral("(Bearer\\s+)([A-Za-z0-9._~+/-]{12,}=*)"),
      QRegularExpression::CaseInsensitiveOption);
  result.replace(bearer, QStringLiteral("\\1***"));

  static const QRegularExpression ghToken(
      QStringLiteral("gh[pousr]_[A-Za-z0-9]{16,}"));
  result.replace(ghToken, QStringLiteral("***"));

  return result;
}

QString gitCommandExplanation(const QStringList &args) {
  if (args.isEmpty()) {
    return QString();
  }
  const QString subcommand = args.first();

  if (subcommand == QLatin1String("status") ||
      subcommand == QLatin1String("log") ||
      subcommand == QLatin1String("show") ||
      subcommand == QLatin1String("diff") ||
      subcommand == QLatin1String("rev-parse") ||
      subcommand == QLatin1String("rev-list") ||
      subcommand == QLatin1String("for-each-ref") ||
      subcommand == QLatin1String("merge-base") ||
      subcommand == QLatin1String("blame") ||
      subcommand == QLatin1String("reflog")) {
    return QObject::tr("Reads the repository. Nothing changes.");
  }
  if (subcommand == QLatin1String("merge-tree")) {
    return QObject::tr(
        "Works out a merge in memory to find conflicts. Nothing on disk or in "
        "the index changes.");
  }
  if (subcommand == QLatin1String("add")) {
    return QObject::tr("Copies the change into the index. Your files and HEAD "
                       "are untouched.");
  }
  if (subcommand == QLatin1String("apply")) {
    return args.contains(QStringLiteral("--cached"))
               ? QObject::tr("Applies a patch to the index only.")
               : QObject::tr("Applies a patch to your files.");
  }
  if (subcommand == QLatin1String("restore")) {
    return args.contains(QStringLiteral("--staged"))
               ? QObject::tr("Resets the index back to HEAD, leaving your "
                             "files alone.")
               : QObject::tr("Rewrites your files from the index. Uncommitted "
                             "edits are lost.");
  }
  if (subcommand == QLatin1String("commit")) {
    return args.contains(QStringLiteral("--amend"))
               ? QObject::tr("Replaces the last commit with a new one, so it "
                             "gets a new hash.")
               : QObject::tr("Turns what is in the index into a commit and "
                             "moves the branch to it.");
  }
  if (subcommand == QLatin1String("reset")) {
    if (args.contains(QStringLiteral("--hard"))) {
      return QObject::tr("Moves the branch and rewrites both the index and "
                         "your files. Uncommitted work is lost.");
    }
    if (args.contains(QStringLiteral("--soft"))) {
      return QObject::tr("Moves the branch only. The index and your files "
                         "stay exactly as they are.");
    }
    return QObject::tr("Moves the branch and clears the index. Your files "
                       "stay as they are.");
  }
  if (subcommand == QLatin1String("checkout") ||
      subcommand == QLatin1String("switch")) {
    return QObject::tr("Moves HEAD and rewrites your files to match.");
  }
  if (subcommand == QLatin1String("merge")) {
    return QObject::tr("Joins another history into this branch, adding a "
                       "merge commit unless it can fast-forward.");
  }
  if (subcommand == QLatin1String("rebase")) {
    return QObject::tr("Recreates your commits on top of another commit. They "
                       "get new hashes.");
  }
  if (subcommand == QLatin1String("cherry-pick")) {
    return QObject::tr("Copies a commit's change into a new commit here.");
  }
  if (subcommand == QLatin1String("revert")) {
    return QObject::tr("Adds a commit that undoes another one. Nothing is "
                       "rewritten.");
  }
  if (subcommand == QLatin1String("fetch")) {
    return QObject::tr("Updates remote-tracking refs. Your branch and files "
                       "are untouched.");
  }
  if (subcommand == QLatin1String("pull")) {
    return QObject::tr("Fetches, then integrates. What integration means "
                       "depends on the strategy.");
  }
  if (subcommand == QLatin1String("push")) {
    return args.contains(QStringLiteral("--force"))
               ? QObject::tr("Overwrites the remote branch unconditionally.")
               : QObject::tr("Sends your commits to the remote, which only "
                             "accepts a fast-forward.");
  }
  if (subcommand == QLatin1String("stash")) {
    return QObject::tr("Moves changes between your files and the stash.");
  }
  if (subcommand == QLatin1String("branch")) {
    return args.contains(QStringLiteral("-d")) ||
                   args.contains(QStringLiteral("-D"))
               ? QObject::tr("Removes a name. The commits themselves are not "
                             "deleted straight away.")
               : QObject::tr("Creates or lists names for commits.");
  }
  if (subcommand == QLatin1String("worktree")) {
    return QObject::tr("Manages extra working directories that share this "
                       "repository's history.");
  }
  if (subcommand == QLatin1String("bisect")) {
    return QObject::tr("Walks a binary search through history, moving your "
                       "checkout to each candidate.");
  }

  return QObject::tr("Runs git %1.").arg(subcommand);
}

GitOperationRisk gitCommandRisk(const QStringList &args) {
  if (args.isEmpty()) {
    return GitOperationRisk::Safe;
  }
  const QString subcommand = args.first();

  if (subcommand == QLatin1String("reset") &&
      args.contains(QStringLiteral("--hard"))) {
    return GitOperationRisk::MayDiscardUncommitted;
  }
  if (subcommand == QLatin1String("restore") &&
      !args.contains(QStringLiteral("--staged"))) {
    return GitOperationRisk::MayDiscardUncommitted;
  }
  if (subcommand == QLatin1String("clean")) {
    return GitOperationRisk::MayDiscardUncommitted;
  }
  if (subcommand == QLatin1String("push") &&
      (args.contains(QStringLiteral("--force")) ||
       args.contains(QStringLiteral("--force-with-lease")))) {
    return GitOperationRisk::AffectsSharedHistory;
  }
  if (subcommand == QLatin1String("rebase") ||
      subcommand == QLatin1String("filter-branch") ||
      (subcommand == QLatin1String("commit") &&
       args.contains(QStringLiteral("--amend")))) {
    return GitOperationRisk::RewritesLocalHistory;
  }
  if (subcommand == QLatin1String("reset")) {
    return GitOperationRisk::RewritesLocalHistory;
  }

  return GitOperationRisk::Safe;
}

bool gitCommandIsReadOnly(const QStringList &args) {
  if (args.isEmpty()) {
    return true;
  }
  const QString subcommand = args.first();
  if (!readOnlySubcommands().contains(subcommand)) {
    return false;
  }

  if (subcommand == QLatin1String("config")) {
    return args.contains(QStringLiteral("--get")) ||
           args.contains(QStringLiteral("--list"));
  }
  if (subcommand == QLatin1String("branch")) {
    return args.size() == 1 || args.contains(QStringLiteral("--contains")) ||
           args.contains(QStringLiteral("--list")) ||
           args.contains(QStringLiteral("--all"));
  }
  if (subcommand == QLatin1String("tag")) {
    return args.size() == 1 || args.contains(QStringLiteral("--contains")) ||
           args.contains(QStringLiteral("-l"));
  }
  if (subcommand == QLatin1String("worktree")) {
    return args.contains(QStringLiteral("list"));
  }
  return true;
}
