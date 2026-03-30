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
  }

  Logger::instance().info("Initialized default snippets");
}
