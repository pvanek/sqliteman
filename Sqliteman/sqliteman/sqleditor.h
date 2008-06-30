/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef SQLEDITOR_H
#define SQLEDITOR_H

#include <QWidget>
#include <QFileSystemWatcher>

#include "ui_sqleditor.h"
#include "sqlparser/tosqlparse.h"

class QTextDocument;
class QLabel;
class QProgressDialog;



/*!
\brief Execute Query dialog. Simple SQL editor.
It allows simple editing capabilities for user. There is a simple
syntax highlighting (see SqlHighlighter class).
\author Petr Vanek <petr@scribus.info>
 */
class SqlEditor : public QMainWindow
{
	Q_OBJECT

	public:
		SqlEditor(QWidget * parent = 0);
		~SqlEditor(){};

		void saveOnExit();

		void setFileName(const QString & fname);
		QString fileName() { return m_fileName; };

		void setStatusMessage(const QString & message = 0);

   	signals:
		/*! \brief This signal is emitted when user clicks on the one
		of "run" actions. It's handled in main window later.
		\param command current SQL statement in the editor.
		*/
		void showSqlResult(QString command);
		//! \brief It's emitted when the script is started
		void sqlScriptStart();
		/*! \brief Emitted on demand in the script.
		Line is appended to the script output. */
		void showSqlScriptResult(QString line);

		/*! \brief Request for complete object tree refresh.
		It's used in "Run as Script" */
		void buildTree();
		/*! \brief Rebuild part of the tree */
		void rebuildViewTree(QString schema, QString name);

	private:
		Ui::SqlEditor ui;

		QString m_fileName;
		QFileSystemWatcher * m_fileWatcher;

		QLabel * changedLabel;
		QLabel * cursorLabel;
		QString cursorTemplate;

		//! \brief True when user cancel file opening
		bool canceled;
		bool m_scriptCancelled;
		//! \brief Handle long files (prevent app "freezing")
		QProgressDialog * progress;
		/*! \brief A helper method for progress.
		It check the canceled flag. If user cancel the file opening
		it will stop it. */
		bool setProgress(int p);

		void showEvent(QShowEvent * event);
		bool changedConfirm();
		void saveFile();
		void open(const QString & newFile);

		/*! \brief Get requested SQL statement from editor.
		\TODO: Implement a clever sql selecting like TOra/Toad etc.
		*/
		QString query();
		//! \brief From TOra
		QString prepareExec(toSQLParse::tokenizer &tokens, int line, int pos);

		void find(QString ttf, bool forward/*, bool backward*/);

		//! Reset the QFileSystemWatcher for new name.
		void setFileWatcher(const QString & newFileName);

	private slots:
		void action_Run_SQL_triggered();
		void actionRun_Explain_triggered();
		void actionRun_as_Script_triggered();
		void action_Open_triggered();
		void action_Save_triggered();
		void action_New_triggered();
		void actionSave_As_triggered();
		void actionCreateView_triggered();
		void sqlTextEdit_cursorPositionChanged(int,int);
		void documentChanged(bool state);
		void cancel();
		// searching
		void actionSearch_triggered();
		void searchEdit_textChanged(const QString & text);
		void findPrevious();
		void findNext();
		//! \brief Watch file for changes from external apps
		void externalFileChange(const QString & path);
		//
		void scriptCancelled();
};

#endif
