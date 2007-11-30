/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#ifndef SQLKEYWORDS_H
#define SQLKEYWORDS_H

class QStringList;


//! \brief Sqlite SQL dialect keywords
QStringList sqlKeywords();

bool isKeyword(const QString & w);

#endif
