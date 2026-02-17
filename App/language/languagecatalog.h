#ifndef LANGUAGECATALOG_H
#define LANGUAGECATALOG_H

#include "../syntax/syntaxpluginregistry.h"
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>
#include <algorithm>

struct LanguageInfo {
  QString id;
  QString displayName;
  QStringList extensions;
};

class LanguageCatalog {
public:
  static QVector<LanguageInfo> builtInLanguages() {
    return {
        {"plaintext", "Normal Text", {"txt", "text", "log"}},
        {"cpp", "C++", {"cpp", "cc", "cxx", "c", "h", "hpp", "hxx"}},
        {"css", "CSS", {"css"}},
        {"go", "Go", {"go"}},
        {"html", "HTML", {"html", "htm"}},
        {"java", "Java", {"java"}},
        {"js", "JavaScript", {"js", "jsx", "mjs", "cjs"}},
        {"json", "JSON", {"json"}},
        {"make", "Make", {"mk", "makefile"}},
        {"md", "Markdown", {"md", "markdown"}},
        {"py", "Python", {"py", "pyw", "pyi"}},
        {"rust", "Rust", {"rs"}},
        {"cmake", "CMake", {"cmake", "cmakelists.txt"}},
        {"sh", "Shell", {"sh", "bash", "zsh"}},
        {"ts", "TypeScript", {"ts", "tsx"}},
        {"yaml", "YAML", {"yaml", "yml"}},
    };
  }

  static QVector<LanguageInfo> allLanguages() {
    QVector<LanguageInfo> result = builtInLanguages();
    QHash<QString, int> indexById;
    for (int i = 0; i < result.size(); ++i) {
      indexById[result[i].id] = i;
    }

    auto &registry = SyntaxPluginRegistry::instance();
    QStringList languageIds = registry.getAllLanguageIds();

    for (const QString &languageId : languageIds) {
      ISyntaxPlugin *plugin = registry.getPluginByLanguageId(languageId);
      if (!plugin) {
        continue;
      }

      LanguageInfo info;
      info.id = plugin->languageId().trimmed().toLower();
      info.displayName = plugin->languageName().trimmed();
      info.extensions = plugin->fileExtensions();
      for (QString &ext : info.extensions) {
        ext = ext.trimmed().toLower();
      }
      info.extensions.removeDuplicates();
      if (!info.id.isEmpty()) {
        if (indexById.contains(info.id)) {
          LanguageInfo &existing = result[indexById.value(info.id)];
          if (!info.displayName.isEmpty()) {
            existing.displayName = info.displayName;
          }
          for (const QString &ext : info.extensions) {
            if (!existing.extensions.contains(ext)) {
              existing.extensions.append(ext);
            }
          }
        } else {
          indexById[info.id] = result.size();
          result.append(info);
        }
      }
    }

    std::sort(result.begin(), result.end(),
              [](const LanguageInfo &a, const LanguageInfo &b) {
                return a.displayName.toLower() < b.displayName.toLower();
              });
    return result;
  }

  static QString normalize(const QString &value) {
    QString key = value.trimmed().toLower();
    if (key.isEmpty()) {
      return {};
    }

    QHash<QString, QString> aliases = aliasMap();
    if (aliases.contains(key)) {
      return aliases.value(key);
    }

    if (key.startsWith('.')) {
      QString noDot = key.mid(1);
      if (aliases.contains(noDot)) {
        return aliases.value(noDot);
      }
    } else {
      QString withDot = "." + key;
      if (aliases.contains(withDot)) {
        return aliases.value(withDot);
      }
    }

    return {};
  }

  static QString languageForExtension(const QString &extension) {
    return normalize(extension.startsWith('.') ? extension : "." + extension);
  }

  static QString displayName(const QString &languageId) {
    QString canonical = normalize(languageId);
    if (canonical.isEmpty()) {
      return {};
    }

    for (const LanguageInfo &language : builtInLanguages()) {
      if (language.id == canonical) {
        return language.displayName;
      }
    }
    return {};
  }

private:
  static void addAlias(QHash<QString, QString> &aliases, const QString &key,
                       const QString &canonicalId) {
    QString normalizedKey = key.trimmed().toLower();
    QString normalizedCanonical = canonicalId.trimmed().toLower();
    if (!normalizedKey.isEmpty() && !normalizedCanonical.isEmpty()) {
      aliases.insert(normalizedKey, normalizedCanonical);
    }
  }

  static QHash<QString, QString> aliasMap() {
    QHash<QString, QString> aliases;

    for (const LanguageInfo &language : builtInLanguages()) {
      addAlias(aliases, language.id, language.id);
      addAlias(aliases, language.displayName, language.id);
      for (const QString &ext : language.extensions) {
        addAlias(aliases, ext, language.id);
        addAlias(aliases, "." + ext, language.id);
      }
    }

    addAlias(aliases, "py", "py");
    addAlias(aliases, "python", "py");
    addAlias(aliases, "js", "js");
    addAlias(aliases, "javascript", "js");
    addAlias(aliases, "ts", "ts");
    addAlias(aliases, "typescript", "ts");
    addAlias(aliases, "cpp", "cpp");
    addAlias(aliases, "c", "c");
    addAlias(aliases, "sh", "sh");
    addAlias(aliases, "markdown", "md");
    addAlias(aliases, "md", "md");
    addAlias(aliases, "shell", "sh");
    addAlias(aliases, "bash", "sh");
    addAlias(aliases, "c++", "cpp");
    addAlias(aliases, "cxx", "cpp");
    addAlias(aliases, "yml", "yaml");
    addAlias(aliases, "go", "go");
    addAlias(aliases, "java", "java");
    addAlias(aliases, "rust", "rust");
    addAlias(aliases, "json", "json");
    addAlias(aliases, "make", "make");
    addAlias(aliases, "cmake", "cmake");
    addAlias(aliases, "html", "html");
    addAlias(aliases, "css", "css");
    addAlias(aliases, "yaml", "yaml");
    addAlias(aliases, "plain text", "plaintext");
    addAlias(aliases, "plaintext", "plaintext");
    addAlias(aliases, "text", "plaintext");
    addAlias(aliases, "normal text", "plaintext");

    return aliases;
  }
};

#endif
