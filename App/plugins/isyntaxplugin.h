#ifndef ISYNTAXPLUGIN_H
#define ISYNTAXPLUGIN_H

#include "iplugin.h"
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QVector>

/**
 * @brief Syntax highlighting rule
 */
struct SyntaxRule {
    QRegularExpression pattern;
    QTextCharFormat format;
    QString name;  // Optional name for the rule
};

/**
 * @brief Multi-line comment/string definition
 */
struct MultiLineBlock {
    QRegularExpression startPattern;
    QRegularExpression endPattern;
    QTextCharFormat format;
};

/**
 * @brief Interface for syntax highlighting plugins
 * 
 * Plugins implementing this interface can provide syntax highlighting
 * for additional languages beyond the built-in support.
 */
class ISyntaxPlugin : public IPlugin {
public:
    virtual ~ISyntaxPlugin() = default;

    /**
     * @brief Get the language identifier
     * @return Language ID (e.g., "cpp", "python", "rust")
     */
    virtual QString languageId() const = 0;

    /**
     * @brief Get display name for the language
     * @return Human-readable language name
     */
    virtual QString languageName() const = 0;

    /**
     * @brief Get file extensions supported by this plugin
     * @return List of extensions without dots (e.g., ["cpp", "cc", "h"])
     */
    virtual QStringList fileExtensions() const = 0;

    /**
     * @brief Get syntax highlighting rules
     * @return Vector of syntax rules
     */
    virtual QVector<SyntaxRule> syntaxRules() const = 0;

    /**
     * @brief Get multi-line block definitions (comments, strings)
     * @return Vector of multi-line block definitions
     */
    virtual QVector<MultiLineBlock> multiLineBlocks() const { return {}; }

    /**
     * @brief Get keywords for autocomplete
     * @return List of language keywords
     */
    virtual QStringList keywords() const { return {}; }

    /**
     * @brief Get comment style
     * @return Pair of (single-line comment, multi-line comment start/end)
     */
    virtual QPair<QString, QPair<QString, QString>> commentStyle() const {
        return {"//", {"/*", "*/"}};
    }
};

#define ISyntaxPlugin_iid "org.lightpad.ISyntaxPlugin/1.0"
Q_DECLARE_INTERFACE(ISyntaxPlugin, ISyntaxPlugin_iid)

#endif // ISYNTAXPLUGIN_H
