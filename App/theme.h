#ifndef THEME_H
#define THEME_H

#include <QColor>

class Theme
{
public:
    Theme();

private:
    QColor keywordFormat_0;
    QColor keywordFormat_1;
    QColor keywordFormat_2;
    QColor searchFormat;
    QColor singleLineCommentFormat;
    QColor functionFormat;
    QColor quotationFormat;
    QColor classFormat;
    QColor numberFormat;
};

#endif // THEME_H
