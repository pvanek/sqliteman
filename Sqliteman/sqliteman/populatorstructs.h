/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#ifndef POPULATORSTRUCTS_H
#define POPULATORSTRUCTS_H


namespace Populator
{
	//! Supported actions. See PopulatorDialog() for more info.
	typedef enum
	{
		T_AUTO = 0,
		T_AUTO_FROM,
		T_NUMB,
		T_TEXT,
		T_PREF,
		T_STAT,
		T_DT_NOW,
		T_DT_NOW_UNIX,
		T_DT_NOW_JULIAN,
		T_DT_RAND,
		T_DT_RAND_UNIX,
		T_DT_RAND_JULIAN,
		T_IGNORE
	} Action;
	
	//! Column description for Populator.
	typedef struct
	{
		//! Column name
		QString name;
		//! Column data type
		QString type;
		//! True if it's unique based column
		bool pk;
		//! One of supported action of populating
		int action;
		//! Regexped value of column size obtained from type. E.g. NUMBER(6)
		int size;
		//! Additional value for T_PREF and so on
		QString userValue;
	} PopColumn;

}; // namespace

#endif
