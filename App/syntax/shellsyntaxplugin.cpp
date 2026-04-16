#include "shellsyntaxplugin.h"
#include <QRegularExpression>

QStringList ShellSyntaxPlugin::getPrimaryKeywords() {
  return {"if",       "then",    "else",     "elif",    "fi",
          "case",     "esac",    "for",      "while",   "until",
          "do",       "done",    "in",       "function", "select",
          "time",     "coproc",  "break",    "continue", "return"};
}

QStringList ShellSyntaxPlugin::getSecondaryKeywords() {
  return {"exit",    "shift",    "source",  ".",       "alias",
          "unalias", "export",   "readonly", "declare", "local",
          "typeset", "unset",    "set",     "shopt",   "trap",
          "eval",    "exec",     "true",    "false",   "test",
          "let",     "mapfile",  "readarray", "getopts", "wait",
          "jobs",    "fg",       "bg",      "disown",  "umask",
          "ulimit",  "bind",     "builtin", "caller",  "command",
          "compgen", "complete", "help",    "history", "logout"};
}

QStringList ShellSyntaxPlugin::getTertiaryKeywords() {
  return {"echo",    "printf",  "read",    "cd",       "pwd",
          "pushd",   "popd",    "dirs",    "ls",       "cat",
          "grep",    "egrep",   "fgrep",   "sed",      "awk",
          "find",    "xargs",   "sort",    "uniq",     "head",
          "tail",    "wc",      "cut",     "paste",    "tr",
          "expr",    "basename", "dirname", "mktemp",   "mkdir",
          "rmdir",   "rm",      "cp",      "mv",       "ln",
          "chmod",   "chown",   "touch",   "date",     "sleep",
          "curl",    "wget",    "tar",     "gzip",     "gunzip",
          "zip",     "unzip",   "ssh",     "scp",      "rsync",
          "git",     "docker",  "kubectl", "systemctl", "sudo"};
}

QVector<SyntaxRule> ShellSyntaxPlugin::syntaxRules() const {
  QVector<SyntaxRule> rules;

  auto appendRule = [&rules](const QString &pattern, const QString &name,
                             QRegularExpression::PatternOptions options =
                                 QRegularExpression::NoPatternOption) {
    SyntaxRule rule;
    rule.pattern = QRegularExpression(pattern, options);
    rule.name = name;
    rules.append(rule);
  };

  appendRule("^#!.*$", "comment");

  appendRule("(^|[\\s;|&])#.*$", "comment");

  appendRule("\\$\\((?:[^()\"'`]|\\\\.|\"(?:\\\\.|[^\"\\\\])*\"|'[^']*'|`(?:\\\\.|[^`\\\\])*`)*\\)",
             "function");

  appendRule("\\b[A-Za-z_][A-Za-z0-9_]*(?=\\s*\\(\\s*\\))", "function");
  appendRule("\\bfunction\\s+[A-Za-z_][A-Za-z0-9_]*", "function");

  appendRule("\\b[A-Za-z_][A-Za-z0-9_]*(?=\\s*(?:\\[[^\\]]+\\])?=)",
             "keyword_1");

  appendRule("\\$\\{[#?!]?[A-Za-z_][A-Za-z0-9_]*(?:\\[[^\\]]+\\])?(?:[:+?=/%#-][^}]*)?\\}",
             "keyword_1");
  appendRule("\\$[A-Za-z_][A-Za-z0-9_]*|\\$[0-9@#?$!*-]", "keyword_1");

  appendRule("\\[\\[|\\]\\]|\\b(?:-eq|-ne|-lt|-le|-gt|-ge|-nt|-ot|-ef|-z|-n|-r|-w|-x|-s|-d|-f|-e|-L|-O|-G)\\b",
             "keyword_0");

  appendRule("(?<![\\w./])-{1,2}[A-Za-z0-9][A-Za-z0-9_-]*(?:=[^\\s;|&]+)?",
             "keyword_2");

  appendRule("(?:^|[\\s;|&])(?:[0-9]?>&[0-9-]|[0-9]?>>?|[0-9]?<<?-?|&>>?|\\|&?|&&|;;|;)",
             "keyword_0");

  appendRule("\\b(?:0[xX][0-9A-Fa-f]+|[0-9]+(?:\\.[0-9]+)?)\\b", "number");
  appendRule("(^|[\\s;|&])\\.(?=\\s+)", "keyword_1");

  for (const QString &keyword : getPrimaryKeywords()) {
    appendRule("\\b" + QRegularExpression::escape(keyword) + "\\b",
               "keyword_0");
  }

  for (const QString &keyword : getSecondaryKeywords()) {
    appendRule("\\b" + QRegularExpression::escape(keyword) + "\\b",
               "keyword_1");
  }

  for (const QString &keyword : getTertiaryKeywords()) {
    appendRule("\\b" + QRegularExpression::escape(keyword) + "\\b",
               "keyword_2");
  }

  appendRule("\\$'(?:\\\\.|[^'\\\\])*'", "string");
  appendRule("\"(?:\\\\.|[^\"\\\\])*\"", "string");
  appendRule("'[^']*'", "string");
  appendRule("`(?:\\\\.|[^`\\\\])*`", "string");

  return rules;
}

QVector<MultiLineBlock> ShellSyntaxPlugin::multiLineBlocks() const {
  QVector<MultiLineBlock> blocks;

  return blocks;
}

QStringList ShellSyntaxPlugin::keywords() const {
  QStringList all;
  all << getPrimaryKeywords() << getSecondaryKeywords()
      << getTertiaryKeywords();
  return all;
}
