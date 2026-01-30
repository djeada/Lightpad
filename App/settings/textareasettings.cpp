#include "textareasettings.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

const int defaultTabWidth = 4;

TextAreaSettings::TextAreaSettings()
    : mainFont(QApplication::font())
    , autoIndent(true)
    , showLineNumberArea(true)
    , lineHighlighted(true)
    , matchingBracketsHighlighted(true)
    , tabWidth(defaultTabWidth){

    };

void TextAreaSettings::loadSettings(const QString& path)
{
    if (QFileInfo(path).completeSuffix() != "json") {
        qWarning("Wrong file format.");
        return;
    }

    QFile loadFile(path);

    if (!loadFile.open(QIODevice::ReadOnly)) {
        qWarning("Couldn't open save file.");
        return;
    }

    QByteArray saveData = loadFile.readAll();

    QJsonDocument loadDoc(QJsonDocument::fromJson(saveData));

    read(loadDoc.object());
}

void TextAreaSettings::saveSettings(const QString& path)
{
    if (QFileInfo(path).completeSuffix() != "json") {
        qWarning("Wrong file format.");
        return;
    }

    QFile saveFile(path);

    if (!saveFile.open(QIODevice::WriteOnly)) {
        qWarning("Couldn't open save file.");
        return;
    }

    QJsonObject jsonObject;
    write(jsonObject);

    saveFile.write(QJsonDocument(jsonObject).toJson());
}

void TextAreaSettings::read(const QJsonObject& json)
{
    QString fontFamily = QApplication::font().family();
    int fontPontSize = QApplication::font().pointSize();
    int fontWeight = QApplication::font().weight();
    bool fontItalic = false;

    if (json.contains("fontFamily") && json["fontFamily"].isString())
        fontFamily = json["fontFamily"].toString();

    if (json.contains("fontPontSize") && json["fontPontSize"].isDouble())
        fontPontSize = json["fontPontSize"].toInt();

    if (json.contains("fontWeight") && json["fontWeight"].isDouble())
        fontWeight = json["fontWeight"].toInt();

    if (json.contains("fontItalic") && json["fontItalic"].isBool())
        fontItalic = json["fontItalic"].toBool();

    mainFont = QFont(fontFamily, fontPontSize, fontWeight, fontItalic);

    if (json.contains("theme") && json["theme"].isObject())
        theme.read(json["theme"].toObject());

    if (json.contains("autoIndent") && json["autoIndent"].isBool())
        autoIndent = json["autoIndent"].toBool();

    if (json.contains("showLineNumberArea") && json["showLineNumberArea"].isBool())
        showLineNumberArea = json["showLineNumberArea"].toBool();

    if (json.contains("lineHighlighted") && json["lineHighlighted"].isBool())
        lineHighlighted = json["lineHighlighted"].toBool();

    if (json.contains("matchingBracketsHighlighted") && json["matchingBracketsHighlighted"].isBool())
        matchingBracketsHighlighted = json["matchingBracketsHighlighted"].toBool();

    if (json.contains("tabWidth") && json["tabWidth"].isDouble())
        tabWidth = json["tabWidth"].toInt();
}

void TextAreaSettings::write(QJsonObject& json)
{

    json["fontFamily"] = mainFont.family();
    json["fontPontSize"] = mainFont.pointSize();
    json["fontWeight"] = mainFont.weight();
    json["fontItalic"] = mainFont.italic();

    QJsonObject themeObject;
    theme.write(themeObject);
    json["theme"] = themeObject;

    json["autoIndent"] = autoIndent;
    json["showLineNumberArea"] = showLineNumberArea;
    json["lineHighlighted"] = lineHighlighted;
    json["matchingBracketsHighlighted"] = matchingBracketsHighlighted;
    json["tabWidth"] = tabWidth;
}
