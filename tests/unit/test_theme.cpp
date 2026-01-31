#include <QtTest/QtTest>
#include <QJsonObject>
#include "settings/theme.h"

class TestTheme : public QObject {
    Q_OBJECT

private slots:
    void testDefaultConstructor();
    void testWriteToJson();
    void testReadFromJson();
};

void TestTheme::testDefaultConstructor()
{
    Theme theme;
    
    // Verify default colors are set (modern dark theme)
    QCOMPARE(theme.backgroundColor, QColor("#0d1117"));
    QCOMPARE(theme.foregroundColor, QColor("#e6edf3"));
    QVERIFY(theme.highlightColor.isValid());
    QCOMPARE(theme.lineNumberAreaColor, QColor("#0d1117"));
}

void TestTheme::testWriteToJson()
{
    Theme theme;
    QJsonObject json;
    
    theme.write(json);
    
    // Verify all expected keys are present
    QVERIFY(json.contains("backgroundColor"));
    QVERIFY(json.contains("foregroundColor"));
    QVERIFY(json.contains("highlightColor"));
    QVERIFY(json.contains("lineNumberAreaColor"));
    QVERIFY(json.contains("keywordFormat_0"));
    QVERIFY(json.contains("keywordFormat_1"));
    QVERIFY(json.contains("keywordFormat_2"));
    QVERIFY(json.contains("searchFormat"));
    QVERIFY(json.contains("singleLineCommentFormat"));
    QVERIFY(json.contains("functionFormat"));
    QVERIFY(json.contains("quotationFormat"));
    QVERIFY(json.contains("classFormat"));
    QVERIFY(json.contains("numberFormat"));
    
    // Verify values are strings
    QVERIFY(json["backgroundColor"].isString());
    QVERIFY(json["foregroundColor"].isString());
}

void TestTheme::testReadFromJson()
{
    Theme theme;
    QJsonObject json;
    
    // Set custom values
    json["backgroundColor"] = "#ff0000";
    json["foregroundColor"] = "#00ff00";
    json["highlightColor"] = "#111111";
    json["keywordFormat_0"] = "#123456";
    json["numberFormat"] = "#654321";
    
    theme.read(json);
    
    QCOMPARE(theme.backgroundColor, QColor("#ff0000"));
    QCOMPARE(theme.foregroundColor, QColor("#00ff00"));
    QCOMPARE(theme.highlightColor, QColor("#111111"));
    QCOMPARE(theme.keywordFormat_0, QColor("#123456"));
    QCOMPARE(theme.numberFormat, QColor("#654321"));
}

QTEST_MAIN(TestTheme)
#include "test_theme.moc"
