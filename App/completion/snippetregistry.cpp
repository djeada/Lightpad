#include "snippetregistry.h"
#include "../core/logging/logger.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

static const QRegularExpression s_tabstopRe(R"(\$\{(\d+)(?::([^}]*))?\})");
static const QRegularExpression s_simpleTabstopRe(R"(\$(\d+))");

QString Snippet::expandedBody() const {
  QString result = body;

  result.replace(s_tabstopRe, "\\2");

  result.replace(s_simpleTabstopRe, "");

  return result;
}

SnippetRegistry &SnippetRegistry::instance() {
  static SnippetRegistry instance;
  return instance;
}

void SnippetRegistry::registerSnippet(const QString &languageId,
                                      const Snippet &snippet) {
  QString langKey = languageId.toLower();
  m_snippets[langKey].append(snippet);
}

bool SnippetRegistry::loadFromJson(const QString &languageId,
                                   const QString &jsonPath) {
  QFile file(jsonPath);
  if (!file.open(QIODevice::ReadOnly)) {
    Logger::instance().warning(
        QString("Failed to open snippets file: %1").arg(jsonPath));
    return false;
  }

  QByteArray data = file.readAll();
  file.close();

  return loadFromJsonData(languageId, data);
}

bool SnippetRegistry::loadFromJsonData(const QString &languageId,
                                       const QByteArray &jsonData) {
  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);

  if (parseError.error != QJsonParseError::NoError) {
    Logger::instance().warning(
        QString("Failed to parse snippets JSON for '%1': %2")
            .arg(languageId)
            .arg(parseError.errorString()));
    return false;
  }

  QJsonObject root = doc.object();
  QJsonArray snippetsArray = root["snippets"].toArray();

  for (const QJsonValue &val : snippetsArray) {
    QJsonObject obj = val.toObject();
    Snippet snippet;
    snippet.prefix = obj["prefix"].toString();
    snippet.label = obj["label"].toString();
    snippet.body = obj["body"].toString();
    snippet.description = obj["description"].toString();
    snippet.languageId = languageId;

    if (!snippet.prefix.isEmpty() && !snippet.body.isEmpty()) {
      registerSnippet(languageId, snippet);
    }
  }

  Logger::instance().info(QString("Loaded %1 snippets for language '%2'")
                              .arg(snippetsArray.size())
                              .arg(languageId));

  return true;
}

QList<Snippet> SnippetRegistry::snippets(const QString &languageId) const {
  return m_snippets.value(languageId.toLower());
}

QList<Snippet>
SnippetRegistry::snippetsWithPrefix(const QString &languageId,
                                    const QString &prefix) const {
  QList<Snippet> result;
  QList<Snippet> allSnippets = snippets(languageId);

  for (const Snippet &snippet : allSnippets) {
    if (snippet.prefix.startsWith(prefix, Qt::CaseInsensitive)) {
      result.append(snippet);
    }
  }

  return result;
}

bool SnippetRegistry::hasSnippets(const QString &languageId) const {
  return m_snippets.contains(languageId.toLower()) &&
         !m_snippets.value(languageId.toLower()).isEmpty();
}

QStringList SnippetRegistry::registeredLanguages() const {
  return m_snippets.keys();
}

void SnippetRegistry::clear() {
  m_snippets.clear();
  Logger::instance().info("Cleared all snippets from registry");
}

void SnippetRegistry::initializeDefaults() {

  {
    Snippet forLoop;
    forLoop.prefix = "for";
    forLoop.label = "For Loop";
    forLoop.body =
        "for (${1:int} ${2:i} = 0; $2 < ${3:count}; $2++) {\n\t$0\n}";
    forLoop.description = "For loop with iterator";
    registerSnippet("cpp", forLoop);

    Snippet ifStmt;
    ifStmt.prefix = "if";
    ifStmt.label = "If Statement";
    ifStmt.body = "if (${1:condition}) {\n\t$0\n}";
    ifStmt.description = "If statement";
    registerSnippet("cpp", ifStmt);

    Snippet classDecl;
    classDecl.prefix = "class";
    classDecl.label = "Class Declaration";
    classDecl.body = "class ${1:ClassName} "
                     "{\npublic:\n\t${1}();\n\t~${1}();\n\nprivate:\n\t$0\n};";
    classDecl.description = "C++ class with constructor and destructor";
    registerSnippet("cpp", classDecl);

    Snippet main;
    main.prefix = "main";
    main.label = "Main Function";
    main.body = "int main(int argc, char* argv[]) {\n\t$0\n\treturn 0;\n}";
    main.description = "Main function";
    registerSnippet("cpp", main);
  }

  {
    Snippet forLoop;
    forLoop.prefix = "for";
    forLoop.label = "For Loop";
    forLoop.body = "for ${1:item} in ${2:iterable}:\n\t$0";
    forLoop.description = "For loop";
    registerSnippet("python", forLoop);
    registerSnippet("py", forLoop);

    Snippet ifStmt;
    ifStmt.prefix = "if";
    ifStmt.label = "If Statement";
    ifStmt.body = "if ${1:condition}:\n\t$0";
    ifStmt.description = "If statement";
    registerSnippet("python", ifStmt);
    registerSnippet("py", ifStmt);

    Snippet defFunc;
    defFunc.prefix = "def";
    defFunc.label = "Function Definition";
    defFunc.body =
        "def ${1:function_name}(${2:args}):\n\t${3:\"\"\"Docstring.\"\"\"}$0";
    defFunc.description = "Function definition";
    registerSnippet("python", defFunc);
    registerSnippet("py", defFunc);

    Snippet classDecl;
    classDecl.prefix = "class";
    classDecl.label = "Class Definition";
    classDecl.body =
        "class ${1:ClassName}:\n\tdef __init__(self${2:, args}):\n\t\t$0";
    classDecl.description = "Class definition";
    registerSnippet("python", classDecl);
    registerSnippet("py", classDecl);
  }

  {
    Snippet forLoop;
    forLoop.prefix = "for";
    forLoop.label = "For Loop";
    forLoop.body = "for (let ${1:i} = 0; $1 < ${2:count}; $1++) {\n\t$0\n}";
    forLoop.description = "For loop";
    registerSnippet("js", forLoop);
    registerSnippet("javascript", forLoop);

    Snippet forEach;
    forEach.prefix = "foreach";
    forEach.label = "ForEach Loop";
    forEach.body = "${1:array}.forEach((${2:item}) => {\n\t$0\n});";
    forEach.description = "ForEach loop";
    registerSnippet("js", forEach);
    registerSnippet("javascript", forEach);

    Snippet func;
    func.prefix = "function";
    func.label = "Function";
    func.body = "function ${1:name}(${2:args}) {\n\t$0\n}";
    func.description = "Function declaration";
    registerSnippet("js", func);
    registerSnippet("javascript", func);

    Snippet arrow;
    arrow.prefix = "arrow";
    arrow.label = "Arrow Function";
    arrow.body = "const ${1:name} = (${2:args}) => {\n\t$0\n};";
    arrow.description = "Arrow function";
    registerSnippet("js", arrow);
    registerSnippet("javascript", arrow);
  }

  {
    Snippet heading;
    heading.prefix = "h1";
    heading.label = "Heading 1";
    heading.body = "# ${1:Heading}\n$0";
    heading.description = "Top-level heading";
    registerSnippet("md", heading);
    registerSnippet("markdown", heading);

    Snippet heading2;
    heading2.prefix = "h2";
    heading2.label = "Heading 2";
    heading2.body = "## ${1:Heading}\n$0";
    heading2.description = "Second-level heading";
    registerSnippet("md", heading2);
    registerSnippet("markdown", heading2);

    Snippet heading3;
    heading3.prefix = "h3";
    heading3.label = "Heading 3";
    heading3.body = "### ${1:Heading}\n$0";
    heading3.description = "Third-level heading";
    registerSnippet("md", heading3);
    registerSnippet("markdown", heading3);

    Snippet link;
    link.prefix = "link";
    link.label = "Link";
    link.body = "[${1:text}](${2:url})$0";
    link.description = "Markdown link";
    registerSnippet("md", link);
    registerSnippet("markdown", link);

    Snippet image;
    image.prefix = "img";
    image.label = "Image";
    image.body = "![${1:alt text}](${2:path})$0";
    image.description = "Markdown image";
    registerSnippet("md", image);
    registerSnippet("markdown", image);

    Snippet codeBlock;
    codeBlock.prefix = "code";
    codeBlock.label = "Fenced Code Block";
    codeBlock.body = "```${1:language}\n${2:code}\n```\n$0";
    codeBlock.description = "Fenced code block with language";
    registerSnippet("md", codeBlock);
    registerSnippet("markdown", codeBlock);
    Snippet article;
    article.prefix = "article";
    article.label = "Article Document";
    article.body = "\\documentclass{article}\n\\usepackage[utf8]{inputenc}\n\n"
                   "\\title{${1:Title}}\n\\author{${2:Author}}\n"
                   "\\date{${3:\\today}}\n\n\\begin{document}\n\n"
                   "\\maketitle\n\n$0\n\n\\end{document}";
    article.description = "Article document template";
    registerSnippet("latex", article);

    Snippet report;
    report.prefix = "report";
    report.label = "Report Document";
    report.body =
        "\\documentclass{report}\n\\usepackage[utf8]{inputenc}\n\n"
        "\\title{${1:Title}}\n\\author{${2:Author}}\n"
        "\\date{${3:\\today}}\n\n\\begin{document}\n\n"
        "\\maketitle\n\\tableofcontents\n\n"
        "\\chapter{${4:Introduction}}\n$0\n\n\\end{document}";
    report.description = "Report document template";
    registerSnippet("latex", report);

    Snippet beamer;
    beamer.prefix = "beamer";
    beamer.label = "Beamer Presentation";
    beamer.body = "\\documentclass{beamer}\n\\usetheme{${1:default}}\n\n"
                  "\\title{${2:Title}}\n\\author{${3:Author}}\n"
                  "\\date{${4:\\today}}\n\n\\begin{document}\n\n"
                  "\\begin{frame}\n\\titlepage\n\\end{frame}\n\n"
                  "\\begin{frame}{${5:Slide Title}}\n$0\n"
                  "\\end{frame}\n\n\\end{document}";
    beamer.description = "Beamer presentation template";
    registerSnippet("latex", beamer);

    Snippet env;
    env.prefix = "begin";
    env.label = "Environment";
    env.body = "\\begin{${1:environment}}\n\t$0\n\\end{${1:environment}}";
    env.description = "LaTeX environment";
    registerSnippet("latex", env);

    Snippet figure;
    figure.prefix = "figure";
    figure.label = "Figure";
    figure.body =
        "\\begin{figure}[${1:htbp}]\n\t\\centering\n"
        "\t\\includegraphics[width=${2:0.8}\\textwidth]{${3:image}}\n"
        "\t\\caption{${4:Caption}}\n\t\\label{fig:${5:label}}\n\\end{figure}";
    figure.description = "Figure environment with image";
    registerSnippet("latex", figure);

    Snippet table;
    table.prefix = "table";
    table.label = "Table";
    table.body = "| ${1:Header 1} | ${2:Header 2} |\n| --- | --- |\n| ${3:Cell "
                 "1} | ${4:Cell 2} |\n$0";
    table.description = "Markdown table";
    registerSnippet("md", table);
    registerSnippet("markdown", table);

    Snippet checklist;
    checklist.prefix = "task";
    checklist.label = "Task List";
    checklist.body = "- [ ] ${1:Task 1}\n- [ ] ${2:Task 2}\n$0";
    checklist.description = "Task/checkbox list";
    registerSnippet("md", checklist);
    registerSnippet("markdown", checklist);

    Snippet frontmatter;
    frontmatter.prefix = "front";
    frontmatter.label = "YAML Frontmatter";
    frontmatter.body = "---\ntitle: ${1:Title}\ndate: ${2:2024-01-01}\ntags: "
                       "[${3:tag1, tag2}]\n---\n$0";
    frontmatter.description = "YAML frontmatter block";
    registerSnippet("md", frontmatter);
    registerSnippet("markdown", frontmatter);

    Snippet blockquote;
    blockquote.prefix = "quote";
    blockquote.label = "Blockquote";
    blockquote.body = "> ${1:Quote text}\n$0";
    blockquote.description = "Blockquote";
    registerSnippet("md", blockquote);
    registerSnippet("markdown", blockquote);

    Snippet toc;
    toc.prefix = "toc";
    toc.label = "Table of Contents";
    toc.body = "<!-- TOC -->\n\n<!-- /TOC -->\n$0";
    toc.description = "Table of contents markers";
    registerSnippet("md", toc);
    registerSnippet("markdown", toc);

    Snippet heading4;
    heading4.prefix = "h4";
    heading4.label = "Heading 4";
    heading4.body = "#### ${1:Heading}\n$0";
    heading4.description = "Fourth-level heading";
    registerSnippet("md", heading4);
    registerSnippet("markdown", heading4);

    Snippet heading5;
    heading5.prefix = "h5";
    heading5.label = "Heading 5";
    heading5.body = "##### ${1:Heading}\n$0";
    heading5.description = "Fifth-level heading";
    registerSnippet("md", heading5);
    registerSnippet("markdown", heading5);

    Snippet heading6;
    heading6.prefix = "h6";
    heading6.label = "Heading 6";
    heading6.body = "###### ${1:Heading}\n$0";
    heading6.description = "Sixth-level heading";
    registerSnippet("md", heading6);
    registerSnippet("markdown", heading6);

    Snippet hr;
    hr.prefix = "hr";
    hr.label = "Horizontal Rule";
    hr.body = "\n---\n\n$0";
    hr.description = "Horizontal rule separator";
    registerSnippet("md", hr);
    registerSnippet("markdown", hr);

    Snippet footnote;
    footnote.prefix = "footnote";
    footnote.label = "Footnote";
    footnote.body = "[^${1:ref}]\n\n[^${1:ref}]: ${2:Footnote text}\n$0";
    footnote.description = "Footnote reference and definition";
    registerSnippet("md", footnote);
    registerSnippet("markdown", footnote);

    Snippet deflist;
    deflist.prefix = "deflist";
    deflist.label = "Definition List";
    deflist.body = "${1:Term}\n: ${2:Definition}\n\n$0";
    deflist.description = "Definition list (term and definition)";
    registerSnippet("md", deflist);
    registerSnippet("markdown", deflist);

    Snippet details;
    details.prefix = "details";
    details.label = "Collapsible Section";
    details.body =
        "<details>\n<summary>${1:Click to expand}</summary>\n\n${2:Content "
        "here}\n\n</details>\n$0";
    details.description = "Collapsible/expandable section";
    registerSnippet("md", details);
    registerSnippet("markdown", details);

    Snippet admonition;
    admonition.prefix = "note";
    admonition.label = "Admonition Note";
    admonition.body = "> **${1:Note}**\n>\n> ${2:Content}\n\n$0";
    admonition.description = "Admonition/callout note block";
    registerSnippet("md", admonition);
    registerSnippet("markdown", admonition);

    Snippet warning;
    warning.prefix = "warning";
    warning.label = "Admonition Warning";
    warning.body = "> **⚠️ ${1:Warning}**\n>\n> ${2:Content}\n\n$0";
    warning.description = "Admonition/callout warning block";
    registerSnippet("md", warning);
    registerSnippet("markdown", warning);

    Snippet badge;
    badge.prefix = "badge";
    badge.label = "Badge/Shield";
    badge.body = "![${1:label}](https://img.shields.io/badge/"
                 "${2:label}-${3:message}-${4:blue})$0";
    badge.description = "Shields.io badge image";
    registerSnippet("md", badge);
    registerSnippet("markdown", badge);

    Snippet mathInline;
    mathInline.prefix = "math";
    mathInline.label = "Inline Math";
    mathInline.body = "$${1:expression}$$0";
    mathInline.description = "Inline math expression";
    registerSnippet("md", mathInline);
    registerSnippet("markdown", mathInline);

    Snippet mathBlock;
    mathBlock.prefix = "mathblock";
    mathBlock.label = "Math Block";
    mathBlock.body = "$$\n${1:expression}\n$$\n$0";
    mathBlock.description = "Display math block";
    registerSnippet("md", mathBlock);
    registerSnippet("markdown", mathBlock);

    Snippet refLink;
    refLink.prefix = "reflink";
    refLink.label = "Reference Link";
    refLink.body = "[${1:text}][${2:ref}]\n\n[${2:ref}]: ${3:url}\n$0";
    refLink.description = "Reference-style link";
    registerSnippet("md", refLink);
    registerSnippet("markdown", refLink);

    table.body = "\\begin{table}[${1:htbp}]\n\t\\centering\n"
                 "\t\\caption{${2:Caption}}\n\t\\label{tab:${3:label}}\n"
                 "\t\\begin{tabular}{${4:lcc}}\n\t\t\\hline\n"
                 "\t\t${5:Header 1} & ${6:Header 2} \\\\\\\\\n\t\t\\hline\n"
                 "\t\t$0 \\\\\\\\\n\t\t\\hline\n"
                 "\t\\end{tabular}\n\\end{table}";
    table.description = "Table environment";
    registerSnippet("latex", table);

    Snippet equation;
    equation.prefix = "equation";
    equation.label = "Equation";
    equation.body = "\\begin{equation}\n\t\\label{eq:${1:label}}\n"
                    "\t$0\n\\end{equation}";
    equation.description = "Numbered equation";
    registerSnippet("latex", equation);

    Snippet alignEnv;
    alignEnv.prefix = "align";
    alignEnv.label = "Align Environment";
    alignEnv.body = "\\begin{align}\n\t$0\n\\end{align}";
    alignEnv.description = "Aligned equations";
    registerSnippet("latex", alignEnv);

    Snippet itemize;
    itemize.prefix = "itemize";
    itemize.label = "Itemize List";
    itemize.body =
        "\\begin{itemize}\n\t\\item ${1:First item}\n\t\\item $0\n"
        "\\end{itemize}";
    itemize.description = "Bulleted list";
    registerSnippet("latex", itemize);

    Snippet enumerate;
    enumerate.prefix = "enumerate";
    enumerate.label = "Enumerate List";
    enumerate.body =
        "\\begin{enumerate}\n\t\\item ${1:First item}\n\t\\item $0\n"
        "\\end{enumerate}";
    enumerate.description = "Numbered list";
    registerSnippet("latex", enumerate);

    Snippet theorem;
    theorem.prefix = "theorem";
    theorem.label = "Theorem";
    theorem.body = "\\begin{theorem}\n\t\\label{thm:${1:label}}\n"
                   "\t$0\n\\end{theorem}";
    theorem.description = "Theorem environment";
    registerSnippet("latex", theorem);

    Snippet proof;
    proof.prefix = "proof";
    proof.label = "Proof";
    proof.body = "\\begin{proof}\n\t$0\n\\end{proof}";
    proof.description = "Proof environment";
    registerSnippet("latex", proof);

    Snippet tikz;
    tikz.prefix = "tikz";
    tikz.label = "TikZ Picture";
    tikz.body =
        "\\begin{tikzpicture}\n\t$0\n\\end{tikzpicture}";
    tikz.description = "TikZ picture environment";
    registerSnippet("latex", tikz);

    Snippet frame;
    frame.prefix = "frame";
    frame.label = "Beamer Frame";
    frame.body =
        "\\begin{frame}{${1:Frame Title}}\n\t$0\n\\end{frame}";
    frame.description = "Beamer frame (slide)";
    registerSnippet("latex", frame);

    Snippet section;
    section.prefix = "sec";
    section.label = "Section";
    section.body = "\\section{${1:Section Title}}\n\\label{sec:${2:label}}\n$0";
    section.description = "Section with label";
    registerSnippet("latex", section);

    Snippet subsection;
    subsection.prefix = "ssec";
    subsection.label = "Subsection";
    subsection.body =
        "\\subsection{${1:Subsection Title}}\n\\label{ssec:${2:label}}\n$0";
    subsection.description = "Subsection with label";
    registerSnippet("latex", subsection);

    Snippet bibSetup;
    bibSetup.prefix = "bib";
    bibSetup.label = "Bibliography Setup";
    bibSetup.body =
        "\\usepackage[backend=biber,style=${1:numeric}]{biblatex}\n"
        "\\addbibresource{${2:references}.bib}";
    bibSetup.description = "Biblatex bibliography setup";
    registerSnippet("latex", bibSetup);

    Snippet letter;
    letter.prefix = "letter";
    letter.label = "Letter Template";
    letter.body =
        "\\documentclass{letter}\n"
        "\\signature{${1:Your Name}}\n"
        "\\address{${2:Your Address}}\n"
        "\\begin{document}\n"
        "\\begin{letter}{${3:Recipient}}\n"
        "\\opening{Dear ${4:Sir or Madam},}\n"
        "${0}\n"
        "\\closing{Yours sincerely,}\n"
        "\\end{letter}\n"
        "\\end{document}";
    letter.description = "Letter document template";
    registerSnippet("latex", letter);

    Snippet minipage;
    minipage.prefix = "mini";
    minipage.label = "Minipage";
    minipage.body =
        "\\begin{minipage}{${1:0.45}\\textwidth}\n"
        "  ${0}\n"
        "\\end{minipage}";
    minipage.description = "Minipage environment";
    registerSnippet("latex", minipage);

    Snippet lstlisting;
    lstlisting.prefix = "lst";
    lstlisting.label = "Code Listing";
    lstlisting.body =
        "\\begin{lstlisting}[language=${1:Python}, "
        "caption={${2:Caption}}, label={lst:${3:label}}]\n"
        "${0}\n"
        "\\end{lstlisting}";
    lstlisting.description = "Code listing environment";
    registerSnippet("latex", lstlisting);

    Snippet href;
    href.prefix = "href";
    href.label = "Hyperlink";
    href.body = "\\href{${1:url}}{${2:text}}";
    href.description = "Hyperlink with text";
    registerSnippet("latex", href);

    Snippet latexFootnote;
    latexFootnote.prefix = "fn";
    latexFootnote.label = "Footnote";
    latexFootnote.body = "\\footnote{${1:text}}";
    latexFootnote.description = "Footnote";
    registerSnippet("latex", latexFootnote);

    Snippet multicols;
    multicols.prefix = "multicols";
    multicols.label = "Multiple Columns";
    multicols.body =
        "\\begin{multicols}{${1:2}}\n"
        "  ${0}\n"
        "\\end{multicols}";
    multicols.description = "Multiple columns environment";
    registerSnippet("latex", multicols);

    Snippet bibentry;
    bibentry.prefix = "bibentry";
    bibentry.label = "Bibliography Entry";
    bibentry.body =
        "@${1:article}{${2:key},\n"
        "  author  = {${3:Author}},\n"
        "  title   = {${4:Title}},\n"
        "  journal = {${5:Journal}},\n"
        "  year    = {${6:2024}},\n"
        "  volume  = {${7:1}},\n"
        "  pages   = {${8:1--10}}\n"
        "}";
    bibentry.description = "BibTeX bibliography entry";
    registerSnippet("latex", bibentry);

    Snippet subfile;
    subfile.prefix = "subfile";
    subfile.label = "Subfile (Multi-file)";
    subfile.body =
        "\\documentclass[${1:../main}]{subfiles}\n"
        "\\begin{document}\n"
        "${0}\n"
        "\\end{document}";
    subfile.description = "Subfile for multi-file projects";
    registerSnippet("latex", subfile);

    Snippet columns;
    columns.prefix = "cols";
    columns.label = "Beamer Columns";
    columns.body =
        "\\begin{columns}\n"
        "  \\begin{column}{${1:0.5}\\textwidth}\n"
        "    ${2}\n"
        "  \\end{column}\n"
        "  \\begin{column}{${3:0.5}\\textwidth}\n"
        "    ${0}\n"
        "  \\end{column}\n"
        "\\end{columns}";
    columns.description = "Beamer columns layout";
    registerSnippet("latex", columns);

    Snippet matrixSnip;
    matrixSnip.prefix = "matrix";
    matrixSnip.label = "Matrix";
    matrixSnip.body =
        "\\begin{${1:pmatrix}}\n"
        "  ${2:a} & ${3:b} \\\\\\\\\n"
        "  ${4:c} & ${0:d}\n"
        "\\end{${1:pmatrix}}";
    matrixSnip.description = "Matrix environment";
    registerSnippet("latex", matrixSnip);

    Snippet casesSnip;
    casesSnip.prefix = "cases";
    casesSnip.label = "Cases";
    casesSnip.body =
        "\\begin{cases}\n"
        "  ${1:expr} & \\text{if } ${2:condition} \\\\\\\\\n"
        "  ${3:expr} & \\text{otherwise}\n"
        "\\end{cases}";
    casesSnip.description = "Cases environment for piecewise functions";
    registerSnippet("latex", casesSnip);
  }

  Logger::instance().info("Initialized default snippets");
}
