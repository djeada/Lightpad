#include "languagekeywordsregistry.h"
#include "../core/logging/logger.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

LanguageKeywordsRegistry& LanguageKeywordsRegistry::instance()
{
    static LanguageKeywordsRegistry instance;
    return instance;
}

void LanguageKeywordsRegistry::registerLanguage(const QString& languageId, const QStringList& keywords)
{
    if (languageId.isEmpty()) {
        Logger::instance().warning("Attempted to register keywords with empty language ID");
        return;
    }
    
    m_keywords[languageId.toLower()] = keywords;
    Logger::instance().info(
        QString("Registered %1 keywords for language '%2'")
            .arg(keywords.size())
            .arg(languageId)
    );
}

bool LanguageKeywordsRegistry::loadFromJson(const QString& languageId, const QString& jsonPath)
{
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
        Logger::instance().warning(
            QString("Failed to open keywords file: %1").arg(jsonPath)
        );
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    return loadFromJsonData(languageId, data);
}

bool LanguageKeywordsRegistry::loadFromJsonData(const QString& languageId, const QByteArray& jsonData)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        Logger::instance().warning(
            QString("Failed to parse keywords JSON for '%1': %2")
                .arg(languageId)
                .arg(parseError.errorString())
        );
        return false;
    }
    
    QJsonObject root = doc.object();
    QJsonArray keywordsArray = root["keywords"].toArray();
    
    QStringList keywords;
    for (const QJsonValue& val : keywordsArray) {
        if (val.isString()) {
            keywords.append(val.toString());
        }
    }
    
    registerLanguage(languageId, keywords);
    return true;
}

QStringList LanguageKeywordsRegistry::keywords(const QString& languageId) const
{
    return m_keywords.value(languageId.toLower());
}

QStringList LanguageKeywordsRegistry::keywordsWithPrefix(const QString& languageId, const QString& prefix) const
{
    QStringList result;
    QStringList allKeywords = keywords(languageId);
    
    for (const QString& keyword : allKeywords) {
        if (keyword.startsWith(prefix, Qt::CaseInsensitive)) {
            result.append(keyword);
        }
    }
    
    return result;
}

bool LanguageKeywordsRegistry::hasLanguage(const QString& languageId) const
{
    return m_keywords.contains(languageId.toLower());
}

QStringList LanguageKeywordsRegistry::registeredLanguages() const
{
    return m_keywords.keys();
}

void LanguageKeywordsRegistry::clear()
{
    m_keywords.clear();
    Logger::instance().info("Cleared all keywords from registry");
}

void LanguageKeywordsRegistry::initializeDefaults()
{
    // Common keywords across languages
    QStringList common;
    common << "break" << "case" << "continue" << "default" << "do" << "else" 
           << "for" << "if" << "return" << "switch" << "while";
    
    // C/C++ keywords
    QStringList cpp = common;
    cpp << "auto" << "char" << "const" << "double" << "enum" << "extern" << "float"
        << "goto" << "int" << "long" << "register" << "short" << "signed" << "sizeof" 
        << "static" << "struct" << "typedef" << "union" << "unsigned" << "void" << "volatile"
        << "class" << "namespace" << "template" << "public" << "private" << "protected"
        << "virtual" << "override" << "final" << "explicit" << "inline" << "constexpr"
        << "nullptr" << "delete" << "new" << "this" << "try" << "catch" << "throw"
        << "bool" << "true" << "false"
        << "std" << "string" << "vector" << "map" << "set" << "list" << "queue" 
        << "stack" << "pair" << "cout" << "cin" << "endl" << "include" << "define" 
        << "ifdef" << "ifndef" << "endif";
    cpp.sort();
    cpp.removeDuplicates();
    registerLanguage("cpp", cpp);
    
    // Python keywords
    QStringList python;
    python << "and" << "as" << "assert" << "async" << "await" << "break" << "class" << "continue"
           << "def" << "del" << "elif" << "else" << "except" << "finally" << "for" << "from" 
           << "global" << "if" << "import" << "in" << "is" << "lambda" << "nonlocal" << "not" 
           << "or" << "pass" << "raise" << "return" << "try" << "while" << "with" << "yield"
           << "True" << "False" << "None" << "self"
           << "print" << "range" << "len" << "str" << "int" << "float" << "list" << "dict"
           << "tuple" << "set" << "open" << "file" << "read" << "write" << "append";
    python.sort();
    python.removeDuplicates();
    registerLanguage("python", python);
    registerLanguage("py", python);  // Alias
    
    // JavaScript keywords
    QStringList javascript;
    javascript << "abstract" << "arguments" << "await" << "boolean" << "break" << "byte"
               << "case" << "catch" << "char" << "class" << "const" << "continue" << "debugger" 
               << "default" << "delete" << "do" << "double" << "else" << "enum" << "eval" 
               << "export" << "extends" << "false" << "final" << "finally" << "float" << "for" 
               << "function" << "goto" << "if" << "implements" << "import" << "in" << "instanceof"
               << "int" << "interface" << "let" << "long" << "native" << "new" << "null" 
               << "package" << "private" << "protected" << "public" << "return" << "short" 
               << "static" << "super" << "switch" << "synchronized" << "this" << "throw" 
               << "throws" << "transient" << "true" << "try" << "typeof" << "var" << "void" 
               << "volatile" << "while" << "with" << "yield"
               << "console" << "log" << "document" << "window" << "alert" << "prompt" 
               << "confirm" << "getElementById" << "querySelector" << "addEventListener" 
               << "setTimeout" << "setInterval";
    javascript.sort();
    javascript.removeDuplicates();
    registerLanguage("js", javascript);
    registerLanguage("javascript", javascript);  // Alias
    
    Logger::instance().info("Initialized default language keywords");
}
