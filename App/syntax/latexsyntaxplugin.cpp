#include "latexsyntaxplugin.h"
#include <QRegularExpression>

QStringList LatexSyntaxPlugin::getSectioningCommands() {
  return {"part",          "chapter",   "section",     "subsection",
          "subsubsection", "paragraph", "subparagraph"};
}

QStringList LatexSyntaxPlugin::getEnvironmentNames() {
  return {"document",    "figure",      "table",       "equation",
          "align",       "align*",      "enumerate",   "itemize",
          "description", "tabular",     "array",       "math",
          "displaymath", "verbatim",    "quote",       "quotation",
          "center",      "flushleft",   "flushright",  "minipage",
          "abstract",    "theorem",     "proof",       "lemma",
          "corollary",   "definition",  "example",     "remark",
          "note",        "proposition", "tikzpicture", "frame",
          "block",       "columns",     "column",      "thebibliography",
          "appendix",    "lstlisting",  "minted",      "algorithmic",
          "algorithm",   "cases",       "matrix",      "pmatrix",
          "bmatrix",     "vmatrix",     "Vmatrix",     "gather",
          "gather*",     "multline",    "multline*",   "split",
          "flalign",     "flalign*",    "eqnarray",    "eqnarray*",
          "subequations"};
}

QStringList LatexSyntaxPlugin::getTextFormattingCommands() {
  return {"textbf",   "textit",   "texttt",       "textrm",    "textsf",
          "textsc",   "textsl",   "emph",         "underline", "textcolor",
          "colorbox", "fbox",     "mbox",         "makebox",   "framebox",
          "parbox",   "footnote", "footnotetext", "marginpar"};
}

QStringList LatexSyntaxPlugin::getMathCommands() {
  return {"frac",      "sqrt",      "sum",        "prod",      "int",
          "oint",      "lim",       "infty",      "partial",   "nabla",
          "alpha",     "beta",      "gamma",      "delta",     "epsilon",
          "zeta",      "eta",       "theta",      "iota",      "kappa",
          "lambda",    "mu",        "nu",         "xi",        "pi",
          "rho",       "sigma",     "tau",        "upsilon",   "phi",
          "chi",       "psi",       "omega",      "Gamma",     "Delta",
          "Theta",     "Lambda",    "Xi",         "Pi",        "Sigma",
          "Phi",       "Psi",       "Omega",      "sin",       "cos",
          "tan",       "log",       "ln",         "exp",       "max",
          "min",       "sup",       "inf",        "det",       "dim",
          "mod",       "gcd",       "lcm",        "deg",       "hom",
          "ker",       "arg",       "left",       "right",     "big",
          "Big",       "bigg",      "Bigg",       "cdot",      "cdots",
          "ldots",     "vdots",     "ddots",      "times",     "div",
          "pm",        "mp",        "leq",        "geq",       "neq",
          "approx",    "equiv",     "sim",        "simeq",     "cong",
          "subset",    "supset",    "subseteq",   "supseteq",  "in",
          "notin",     "cup",       "cap",        "setminus",  "emptyset",
          "forall",    "exists",    "neg",        "land",      "lor",
          "implies",   "iff",       "rightarrow", "leftarrow", "Rightarrow",
          "Leftarrow", "mapsto",    "to",         "hat",       "bar",
          "vec",       "dot",       "ddot",       "tilde",     "overline",
          "underline", "overbrace", "underbrace", "mathbf",    "mathit",
          "mathrm",    "mathsf",    "mathtt",     "mathcal",   "mathbb",
          "mathfrak"};
}

QStringList LatexSyntaxPlugin::getReferenceCommands() {
  return {"label",         "ref",
          "eqref",         "pageref",
          "cite",          "citep",
          "citet",         "citealp",
          "citealt",       "citeauthor",
          "citeyear",      "nocite",
          "bibliography",  "bibliographystyle",
          "addbibresource"};
}

QStringList LatexSyntaxPlugin::getPackageCommands() {
  return {"usepackage",
          "RequirePackage",
          "documentclass",
          "input",
          "include",
          "includeonly",
          "includegraphics",
          "graphicspath",
          "newcommand",
          "renewcommand",
          "newenvironment",
          "renewenvironment",
          "DeclareMathOperator",
          "setlength",
          "addtolength",
          "setcounter",
          "addtocounter",
          "newcounter",
          "newtheorem",
          "theoremstyle",
          "pagestyle",
          "thispagestyle"};
}

QVector<SyntaxRule> LatexSyntaxPlugin::syntaxRules() const {
  QVector<SyntaxRule> rules;

  for (const QString &cmd : getSectioningCommands()) {
    SyntaxRule rule;
    rule.pattern = QRegularExpression("\\\\(" + cmd + "\\*?)\\b");
    rule.name = "keyword_0";
    rules.append(rule);
  }

  for (const QString &cmd : getPackageCommands()) {
    SyntaxRule rule;
    rule.pattern = QRegularExpression("\\\\(" + cmd + ")\\b");
    rule.name = "keyword_1";
    rules.append(rule);
  }

  SyntaxRule beginEndRule;
  beginEndRule.pattern = QRegularExpression("\\\\(begin|end)\\{[^}]*\\}");
  beginEndRule.name = "keyword_0";
  rules.append(beginEndRule);

  for (const QString &cmd : getTextFormattingCommands()) {
    SyntaxRule rule;
    rule.pattern = QRegularExpression("\\\\(" + cmd + ")\\b");
    rule.name = "keyword_2";
    rules.append(rule);
  }

  for (const QString &cmd : getReferenceCommands()) {
    SyntaxRule rule;
    rule.pattern = QRegularExpression("\\\\(" + cmd + ")\\b");
    rule.name = "function";
    rules.append(rule);
  }

  SyntaxRule generalCommandRule;
  generalCommandRule.pattern = QRegularExpression("\\\\[A-Za-z@]+");
  generalCommandRule.name = "keyword_1";
  rules.append(generalCommandRule);

  SyntaxRule inlineMathRule;
  inlineMathRule.pattern =
      QRegularExpression("(?<!\\$)\\$(?!\\$)[^$\\n]+\\$(?!\\$)");
  inlineMathRule.name = "number";
  rules.append(inlineMathRule);

  SyntaxRule displayMathRule;
  displayMathRule.pattern = QRegularExpression("\\$\\$[^$]+\\$\\$");
  displayMathRule.name = "number";
  rules.append(displayMathRule);

  SyntaxRule mathDelimiterRule;
  mathDelimiterRule.pattern = QRegularExpression("\\\\[\\(\\)\\[\\]]");
  mathDelimiterRule.name = "number";
  rules.append(mathDelimiterRule);

  SyntaxRule braceRule;
  braceRule.pattern = QRegularExpression("[{}]");
  braceRule.name = "keyword_2";
  rules.append(braceRule);

  SyntaxRule numberRule;
  numberRule.pattern = QRegularExpression("\\b\\d+\\.?\\d*\\b");
  numberRule.name = "number";
  rules.append(numberRule);

  SyntaxRule stringRule;
  stringRule.pattern = QRegularExpression("\"[^\"]*\"");
  stringRule.name = "string";
  rules.append(stringRule);

  SyntaxRule bibEntryRule;
  bibEntryRule.pattern = QRegularExpression("@[A-Za-z]+\\{[^,}]*");
  bibEntryRule.name = "class";
  rules.append(bibEntryRule);

  SyntaxRule commentRule;
  commentRule.pattern = QRegularExpression("(?<!\\\\)%[^\n]*");
  commentRule.name = "comment";
  rules.append(commentRule);

  return rules;
}

QVector<MultiLineBlock> LatexSyntaxPlugin::multiLineBlocks() const {
  QVector<MultiLineBlock> blocks;

  MultiLineBlock verbatimBlock;
  verbatimBlock.startPattern = QRegularExpression("\\\\begin\\{verbatim\\}");
  verbatimBlock.endPattern = QRegularExpression("\\\\end\\{verbatim\\}");
  blocks.append(verbatimBlock);

  MultiLineBlock lstBlock;
  lstBlock.startPattern = QRegularExpression("\\\\begin\\{lstlisting\\}");
  lstBlock.endPattern = QRegularExpression("\\\\end\\{lstlisting\\}");
  blocks.append(lstBlock);

  MultiLineBlock commentBlock;
  commentBlock.startPattern = QRegularExpression("\\\\begin\\{comment\\}");
  commentBlock.endPattern = QRegularExpression("\\\\end\\{comment\\}");
  blocks.append(commentBlock);

  return blocks;
}

QStringList LatexSyntaxPlugin::keywords() const {
  QStringList all;

  for (const QString &cmd : getSectioningCommands()) {
    all << ("\\" + cmd);
    all << ("\\" + cmd + "*");
  }

  for (const QString &cmd : getPackageCommands()) {
    all << ("\\" + cmd);
  }

  for (const QString &cmd : getTextFormattingCommands()) {
    all << ("\\" + cmd);
  }

  for (const QString &cmd : getMathCommands()) {
    all << ("\\" + cmd);
  }

  for (const QString &cmd : getReferenceCommands()) {
    all << ("\\" + cmd);
  }

  for (const QString &env : getEnvironmentNames()) {
    all << ("\\begin{" + env + "}");
    all << ("\\end{" + env + "}");
  }

  all << "\\item" << "\\maketitle" << "\\tableofcontents" << "\\listoffigures"
      << "\\listoftables" << "\\newpage" << "\\clearpage" << "\\pagebreak"
      << "\\linebreak" << "\\noindent" << "\\hspace" << "\\vspace" << "\\hfill"
      << "\\vfill" << "\\centering" << "\\raggedright" << "\\raggedleft"
      << "\\tiny" << "\\scriptsize" << "\\footnotesize" << "\\small"
      << "\\normalsize" << "\\large" << "\\Large" << "\\LARGE" << "\\huge"
      << "\\Huge" << "\\caption" << "\\title" << "\\author" << "\\date"
      << "\\thanks" << "\\appendix" << "\\printbibliography";

  return all;
}
