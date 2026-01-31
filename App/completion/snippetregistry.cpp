#include "snippetregistry.h"
#include "../core/logging/logger.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>

// Static regex patterns for snippet expansion (compiled once)
static const QRegularExpression s_tabstopRe(R"(\$\{(\d+)(?::([^}]*))?\})");
static const QRegularExpression s_simpleTabstopRe(R"(\$(\d+))");

QString Snippet::expandedBody() const
{
    QString result = body;
    
    // Remove tabstop markers for simple expansion
    // Full tabstop navigation would be handled by the editor
    result.replace(s_tabstopRe, "\\2");
    
    // Remove simple $N markers
    result.replace(s_simpleTabstopRe, "");
    
    return result;
}

SnippetRegistry& SnippetRegistry::instance()
{
    static SnippetRegistry instance;
    return instance;
}

void SnippetRegistry::registerSnippet(const QString& languageId, const Snippet& snippet)
{
    QString langKey = languageId.toLower();
    m_snippets[langKey].append(snippet);
}

bool SnippetRegistry::loadFromJson(const QString& languageId, const QString& jsonPath)
{
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
        Logger::instance().warning(
            QString("Failed to open snippets file: %1").arg(jsonPath)
        );
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    return loadFromJsonData(languageId, data);
}

bool SnippetRegistry::loadFromJsonData(const QString& languageId, const QByteArray& jsonData)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        Logger::instance().warning(
            QString("Failed to parse snippets JSON for '%1': %2")
                .arg(languageId)
                .arg(parseError.errorString())
        );
        return false;
    }
    
    QJsonObject root = doc.object();
    QJsonArray snippetsArray = root["snippets"].toArray();
    
    for (const QJsonValue& val : snippetsArray) {
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
    
    Logger::instance().info(
        QString("Loaded %1 snippets for language '%2'")
            .arg(snippetsArray.size())
            .arg(languageId)
    );
    
    return true;
}

QList<Snippet> SnippetRegistry::snippets(const QString& languageId) const
{
    return m_snippets.value(languageId.toLower());
}

QList<Snippet> SnippetRegistry::snippetsWithPrefix(const QString& languageId, const QString& prefix) const
{
    QList<Snippet> result;
    QList<Snippet> allSnippets = snippets(languageId);
    
    for (const Snippet& snippet : allSnippets) {
        if (snippet.prefix.startsWith(prefix, Qt::CaseInsensitive)) {
            result.append(snippet);
        }
    }
    
    return result;
}

bool SnippetRegistry::hasSnippets(const QString& languageId) const
{
    return m_snippets.contains(languageId.toLower()) && 
           !m_snippets.value(languageId.toLower()).isEmpty();
}

QStringList SnippetRegistry::registeredLanguages() const
{
    return m_snippets.keys();
}

void SnippetRegistry::clear()
{
    m_snippets.clear();
    Logger::instance().info("Cleared all snippets from registry");
}

void SnippetRegistry::initializeDefaults()
{
    // C++ snippets
    {
        Snippet forLoop;
        forLoop.prefix = "for";
        forLoop.label = "For Loop";
        forLoop.body = "for (${1:int} ${2:i} = 0; $2 < ${3:count}; $2++) {\n\t$0\n}";
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
        classDecl.body = "class ${1:ClassName} {\npublic:\n\t${1}();\n\t~${1}();\n\nprivate:\n\t$0\n};";
        classDecl.description = "C++ class with constructor and destructor";
        registerSnippet("cpp", classDecl);
        
        Snippet main;
        main.prefix = "main";
        main.label = "Main Function";
        main.body = "int main(int argc, char* argv[]) {\n\t$0\n\treturn 0;\n}";
        main.description = "Main function";
        registerSnippet("cpp", main);
    }
    
    // Python snippets
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
        defFunc.body = "def ${1:function_name}(${2:args}):\n\t${3:\"\"\"Docstring.\"\"\"}$0";
        defFunc.description = "Function definition";
        registerSnippet("python", defFunc);
        registerSnippet("py", defFunc);
        
        Snippet classDecl;
        classDecl.prefix = "class";
        classDecl.label = "Class Definition";
        classDecl.body = "class ${1:ClassName}:\n\tdef __init__(self${2:, args}):\n\t\t$0";
        classDecl.description = "Class definition";
        registerSnippet("python", classDecl);
        registerSnippet("py", classDecl);
    }
    
    // JavaScript snippets
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
    
    Logger::instance().info("Initialized default snippets");
}
