#ifndef THEME_H
#define THEME_H

#include <QColor>

class QJsonObject;

struct Theme {
    // Editor colors
    QColor backgroundColor;
    QColor foregroundColor;
    QColor highlightColor;
    QColor lineNumberAreaColor;
    
    // Syntax highlighting colors
    QColor keywordFormat_0;
    QColor keywordFormat_1;
    QColor keywordFormat_2;
    QColor searchFormat;
    QColor singleLineCommentFormat;
    QColor functionFormat;
    QColor quotationFormat;
    QColor classFormat;
    QColor numberFormat;
    
    // Modern UI design tokens
    QColor surfaceColor;        // Elevated surfaces (panels, dialogs)
    QColor surfaceAltColor;     // Alternative surface (inputs, cards)
    QColor borderColor;         // Borders and dividers
    QColor hoverColor;          // Hover state background
    QColor pressedColor;        // Pressed/active state
    QColor accentColor;         // Primary accent (buttons, links, focus)
    QColor accentSoftColor;     // Soft accent for selections
    QColor successColor;        // Success states
    QColor warningColor;        // Warning states
    QColor errorColor;          // Error states

    Theme();
    void read(const QJsonObject& json);
    void write(QJsonObject& json);
};

#endif // THEME_H
