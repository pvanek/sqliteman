/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef SQLEDITOR_H
#define SQLEDITOR_H

#include <qwidget.h>
#include <qsyntaxhighlighter.h>

#include "ui_sqleditor.h"

class QTextDocument;
class QLabel;
class QProgressDialog;


//! \brief Support tools for SqlEditor class
namespace SqlEditorTools
{

	/*!
	\brief Simple SQL syntax highlighter.
	Taken and modified code from Scribus scripter and Qt docs.
	\author Petr Vanek <petr@scribus.info>
	*/
	class SqlHighlighter : public QSyntaxHighlighter
	{
		Q_OBJECT

	public:
		SqlHighlighter(QTextDocument *parent = 0);

	protected:
		void highlightBlock(const QString &text);

	private:
		struct HighlightingRule
		{
			QRegExp pattern;
			QTextCharFormat format;
		};
		QVector<HighlightingRule> highlightingRules;

		QRegExp commentStartExpression;
		QRegExp commentEndExpression;

		QTextCharFormat keywordFormat;
		QTextCharFormat singleLineCommentFormat;
		QTextCharFormat multiLineCommentFormat;
		QTextCharFormat quotationFormat;
	};


	/*! \brief A line numbers for the text widget
	Taken from Azevedo Filipe's TextEditor.
	http://www.monkeystudio.org/
	*/
	class Gluter : public QWidget
	{
		Q_OBJECT
		Q_PROPERTY( int digitNumbers READ digitNumbers WRITE setDigitNumbers )
		Q_PROPERTY( QColor textColor READ textColor WRITE setTextColor )
		Q_PROPERTY( QColor backgroundColor READ backgroundColor WRITE setBackgroundColor )
		//
	public:
		Gluter( QTextEdit* );
		//
		void setDigitNumbers( int );
		int digitNumbers() const;
		//
		void setTextColor( const QColor& );
		const QColor& textColor() const;
		//
		void setBackgroundColor( const QColor& );
		const QColor& backgroundColor() const;
		//
	protected:
		virtual void paintEvent( QPaintEvent* );
		//
	private:
		QTextEdit* mEdit;
		int mDigitNumbers;
		QColor mTextColor;
		QColor mBackgroundColor;
		//
	signals:	
		void digitNumbersChanged( int );
		void textColorChanged( const QColor& );
		void backgroundColorChanged( const QColor& );
		//
	public slots:
		void setDefaultProperties();
	};

};


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
		void rebuildViewTree(QString schema, QString name);

	private:
		Ui::SqlEditor ui;
		SqlEditorTools::SqlHighlighter *highlighter;
		SqlEditorTools::Gluter *mGluter;

		QString m_fileName;

		QLabel * changedLabel;
		QLabel * cursorLabel;
		QString cursorTemplate;

		//! \brief True when user cancel file opening
		bool canceled;
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

    private slots:
		void action_Run_SQL_triggered();
		void actionRun_Explain_triggered();
		void action_Open_triggered();
		void action_Save_triggered();
		void action_New_triggered();
		void actionSave_As_triggered();
		void actionCreateView_triggered();
		void sqlTextEdit_cursorPositionChanged();
		void documentChanged(bool state);
		void prefsChanged();
		void cancel();
};

#endif
