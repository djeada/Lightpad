#ifndef TEXTTRANSFORMS_H
#define TEXTTRANSFORMS_H

#include <QString>
#include <QStringList>

/**
 * @brief Stateless text transformation utilities
 * 
 * Pure functions for transforming text content - sorting, case changes, etc.
 */
namespace TextTransforms {

/**
 * @brief Sort lines alphabetically (case-insensitive, ascending)
 * @param text Text with lines to sort
 * @return Sorted text
 */
QString sortLinesAscending(const QString& text);

/**
 * @brief Sort lines alphabetically (case-insensitive, descending)
 * @param text Text with lines to sort
 * @return Sorted text
 */
QString sortLinesDescending(const QString& text);

/**
 * @brief Convert text to uppercase
 * @param text Input text
 * @return Uppercase text
 */
QString toUppercase(const QString& text);

/**
 * @brief Convert text to lowercase
 * @param text Input text
 * @return Lowercase text
 */
QString toLowercase(const QString& text);

/**
 * @brief Convert text to title case (capitalize first letter of each word)
 * @param text Input text
 * @return Title case text
 */
QString toTitleCase(const QString& text);

/**
 * @brief Remove duplicate lines (preserves order, keeps first occurrence)
 * @param text Text with lines
 * @return Text with duplicates removed
 */
QString removeDuplicateLines(const QString& text);

/**
 * @brief Reverse the order of lines
 * @param text Text with lines
 * @return Text with lines reversed
 */
QString reverseLines(const QString& text);

/**
 * @brief Trim whitespace from each line
 * @param text Text with lines
 * @return Text with trimmed lines
 */
QString trimLines(const QString& text);

} // namespace TextTransforms

#endif // TEXTTRANSFORMS_H
