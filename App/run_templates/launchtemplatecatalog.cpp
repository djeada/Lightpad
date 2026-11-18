#include "launchtemplatecatalog.h"

#include "../core/logging/logger.h"
#include "../language/languagecatalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

namespace {
QStringList stringList(const QJsonValue &value) {
  QStringList result;
  for (const QJsonValue &entry : value.toArray()) {
    const QString text = entry.toString().trimmed();
    if (!text.isEmpty()) {
      result << text;
    }
  }
  return result;
}

QByteArray readFile(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  return file.readAll();
}
} // namespace

bool LaunchTemplate::isDebuggable() const {
  return configuration.value("type").toString() != QLatin1String("command");
}

DebugConfiguration LaunchTemplate::instantiate(const QString &name) const {
  DebugConfiguration config = DebugConfiguration::fromJson(configuration);
  config.name = name;
  return config;
}

LaunchTemplateCatalog &LaunchTemplateCatalog::instance() {
  static LaunchTemplateCatalog catalog;
  return catalog;
}

QString LaunchTemplateCatalog::userTemplatesPath() {
  return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) +
         QStringLiteral("/launch_templates.json");
}

QList<LaunchTemplate> LaunchTemplateCatalog::parse(const QByteArray &json,
                                                   QStringList *categories) {
  QList<LaunchTemplate> result;
  QJsonParseError error;
  const QJsonDocument document = QJsonDocument::fromJson(json, &error);
  if (error.error != QJsonParseError::NoError || !document.isObject()) {
    return result;
  }
  const QJsonObject root = document.object();
  if (categories) {
    for (const QString &category : stringList(root.value("categories"))) {
      if (!categories->contains(category)) {
        categories->append(category);
      }
    }
  }
  for (const QJsonValue &value : root.value("templates").toArray()) {
    const QJsonObject object = value.toObject();
    LaunchTemplate entry;
    entry.id = object.value("id").toString().trimmed();
    entry.name = object.value("name").toString().trimmed();
    entry.category = object.value("category").toString().trimmed();
    entry.description = object.value("description").toString().trimmed();
    for (const QString &language : stringList(object.value("languages"))) {
      const QString normalized = LanguageCatalog::normalize(language);
      entry.languages << (normalized.isEmpty() ? language : normalized);
    }
    entry.markers = stringList(object.value("markers"));
    entry.configuration = object.value("configuration").toObject();
    entry.configuration.remove("name");
    if (entry.configuration.value("type").toString().isEmpty()) {
      continue;
    }
    if (!entry.configuration.contains("request")) {
      entry.configuration["request"] = "launch";
    }
    if (entry.name.isEmpty()) {
      entry.name = entry.id;
    }
    if (entry.category.isEmpty()) {
      entry.category = QStringLiteral("Other");
    }
    if (entry.isValid()) {
      result << entry;
    }
  }
  return result;
}

void LaunchTemplateCatalog::reload() {
  m_loaded = false;
  ensureLoaded();
}

void LaunchTemplateCatalog::ensureLoaded() const {
  if (m_loaded) {
    return;
  }
  m_loaded = true;
  m_templates.clear();
  m_categories.clear();

  const QByteArray builtIn =
      readFile(QStringLiteral(":/run_templates/launch_templates.json"));
  m_templates = parse(builtIn, &m_categories);
  if (m_templates.isEmpty()) {
    LOG_WARNING("No built-in launch templates were found");
  }

  const QString userPath = userTemplatesPath();
  if (QFileInfo::exists(userPath)) {
    const QList<LaunchTemplate> custom =
        parse(readFile(userPath), &m_categories);
    for (const LaunchTemplate &entry : custom) {
      bool replaced = false;
      for (LaunchTemplate &existing : m_templates) {
        if (existing.id == entry.id) {
          existing = entry;
          replaced = true;
          break;
        }
      }
      if (!replaced) {
        m_templates << entry;
      }
    }
    LOG_INFO(QString("Loaded %1 user launch templates from %2")
                 .arg(custom.size())
                 .arg(userPath));
  }

  for (const LaunchTemplate &entry : std::as_const(m_templates)) {
    if (!m_categories.contains(entry.category)) {
      m_categories << entry.category;
    }
  }
}

QList<LaunchTemplate> LaunchTemplateCatalog::templates() const {
  ensureLoaded();
  return m_templates;
}

LaunchTemplate LaunchTemplateCatalog::templateById(const QString &id) const {
  ensureLoaded();
  for (const LaunchTemplate &entry : m_templates) {
    if (entry.id == id) {
      return entry;
    }
  }
  return {};
}

QStringList LaunchTemplateCatalog::categories() const {
  ensureLoaded();
  return m_categories;
}

QList<LaunchTemplate>
LaunchTemplateCatalog::suggestionsFor(const QString &languageId,
                                      const QString &workspaceFolder) const {
  ensureLoaded();
  const QString language = LanguageCatalog::normalize(languageId);
  const QDir root(workspaceFolder);

  struct Ranked {
    int score;
    int order;
    LaunchTemplate entry;
  };
  QList<Ranked> ranked;
  for (int i = 0; i < m_templates.size(); ++i) {
    const LaunchTemplate &entry = m_templates.at(i);
    int score = 0;
    if (!language.isEmpty() && entry.languages.contains(language)) {
      score += 2;
    }
    if (!workspaceFolder.isEmpty()) {
      for (const QString &marker : entry.markers) {
        // Markers are file names or globs such as *.csproj.
        const bool glob = marker.contains(QLatin1Char('*')) ||
                          marker.contains(QLatin1Char('?'));
        if (glob ? !root.entryList({marker}, QDir::Files | QDir::Dirs).isEmpty()
                 : root.exists(marker)) {
          score += 3;
          break;
        }
      }
    }
    if (score > 0) {
      ranked << Ranked{score, i, entry};
    }
  }
  std::stable_sort(
      ranked.begin(), ranked.end(), [](const Ranked &a, const Ranked &b) {
        return a.score != b.score ? a.score > b.score : a.order < b.order;
      });
  QList<LaunchTemplate> result;
  for (const Ranked &entry : ranked) {
    result << entry.entry;
  }
  return result;
}
