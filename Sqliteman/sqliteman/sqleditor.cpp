/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QFileDialog>
#include <QLabel>
#include <QProgressDialog>

// Gluter guts
#include <QPainter>
#include <QScrollBar>
#include <QAbstractTextDocumentLayout>

#include "createviewdialog.h"
#include "preferences.h"
#include "sqlparser.h"
#include "sqleditor.h"
#include "sqlkeywords.h"
#include "utils.h"


SqlEditor::SqlEditor(QWidget * parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);
	Preferences * prefs = Preferences::instance();

	m_fileName = QString();
	highlighter = new SqlEditorTools::SqlHighlighter(ui.sqlTextEdit->document());
	mGluter = new SqlEditorTools::Gluter(ui.sqlTextEdit);
	ui.gridLayout->setSpacing(1);
	ui.gridLayout->addWidget(mGluter, 0, 0, 1, 1);
	ui.sqlTextEdit->setCurrentFont(prefs->sqlFont());
	ui.sqlTextEdit->setFontPointSize(prefs->sqlFontSize());

	m_fileWatcher = new QFileSystemWatcher(this);

	changedLabel = new QLabel(this);
	cursorTemplate = tr("Col: %1 Row: %2/%3");
	cursorLabel = new QLabel(this);
	statusBar()->addPermanentWidget(changedLabel);
	statusBar()->addPermanentWidget(cursorLabel);
	sqlTextEdit_cursorPositionChanged();

	ui.searchFrame->hide();

	ui.action_Run_SQL->setIcon(getIcon("runsql.png"));
	ui.actionRun_Explain->setIcon(getIcon("runexplain.png"));
	ui.action_Open->setIcon(getIcon("document-open.png"));
	ui.action_Save->setIcon(getIcon("document-save.png"));
	ui.action_New->setIcon(getIcon("document-new.png"));
	ui.actionSave_As->setIcon(getIcon("document-save-as.png"));
	ui.actionCreateView->setIcon(getIcon("view.png"));

	connect(ui.action_Run_SQL, SIGNAL(triggered()), this, SLOT(action_Run_SQL_triggered()));
	connect(ui.actionRun_Explain, SIGNAL(triggered()), this, SLOT(actionRun_Explain_triggered()));
	connect(ui.action_Open, SIGNAL(triggered()), this, SLOT(action_Open_triggered()));
	connect(ui.action_Save, SIGNAL(triggered()), this, SLOT(action_Save_triggered()));
	connect(ui.action_New, SIGNAL(triggered()), this, SLOT(action_New_triggered()));
	connect(ui.actionSave_As, SIGNAL(triggered()), this, SLOT(actionSave_As_triggered()));
	connect(ui.actionCreateView, SIGNAL(triggered()), this, SLOT(actionCreateView_triggered()));
	connect(ui.sqlTextEdit, SIGNAL(cursorPositionChanged()), this, SLOT(sqlTextEdit_cursorPositionChanged()));
	connect(ui.sqlTextEdit->document(), SIGNAL(modificationChanged(bool)), this, SLOT(documentChanged(bool)));
	connect(parent, SIGNAL(prefsChanged()), this, SLOT(prefsChanged()));

	// search
	connect(ui.actionSearch, SIGNAL(triggered()), this, SLOT(actionSearch_triggered()));
	connect(ui.searchEdit, SIGNAL(textChanged(const QString &)),
			this, SLOT(searchEdit_textChanged(const QString &)));
	connect(ui.previousToolButton, SIGNAL(clicked()), this, SLOT(findPrevious()));
	connect(ui.nextToolButton, SIGNAL(clicked()), this, SLOT(findNext()));
	connect(ui.searchEdit, SIGNAL(returnPressed()), this, SLOT(findNext()));

	// misc
	connect(m_fileWatcher, SIGNAL(fileChanged(const QString &)),
			this, SLOT(externalFileChange(const QString &)));

}

void SqlEditor::setStatusMessage(const QString & message)
{
	if (message.isNull() || message.isEmpty())
		ui.statusBar->clearMessage();
	ui.statusBar->showMessage(message);
}

QString SqlEditor::query()
{
	QTextCursor cur(ui.sqlTextEdit->textCursor());
	if (cur.hasSelection())
		return cur.selectedText();

	cur.movePosition(QTextCursor::WordLeft);

	// current "pararaph"
	SqlEditorTools::SqlParser parser(ui.sqlTextEdit->toPlainText());

	// HACK to handle semicolons at the EOL.
	// If there is a ";" at the EOL and the cursor is at the EOL
	// use the statement "before" as is it in the "std" DB tools
	int pos = cur.position();
	QTextBlock b(ui.sqlTextEdit->document()->findBlock(cur.position()));
	if (cur.atBlockEnd() && b.text().length() > 0 && b.text().at(b.text().length()-1) == ';')
		--pos;

	return parser.getStatement(pos);
}

void SqlEditor::action_Run_SQL_triggered()
{
	emit showSqlResult(query());
}

void SqlEditor::actionRun_Explain_triggered()
{
	emit showSqlResult(QString("explain query plan %1").arg(query()));
}

void SqlEditor::actionCreateView_triggered()
{
	CreateViewDialog dia("", "", this);

	dia.setText(query());
	dia.exec();
	if (dia.update)
		emit rebuildViewTree(dia.schema(), dia.name());
}

void SqlEditor::showEvent(QShowEvent * event)
{
	ui.sqlTextEdit->setFocus();
}

void SqlEditor::action_Open_triggered()
{
	if (!changedConfirm())
		return;

	QString newFile = QFileDialog::getOpenFileName(this, tr("Open SQL Script"),
			QDir::currentPath(), tr("SQL file (*.sql);;All Files (*)"));
	if (newFile.isNull())
		return;
	open(newFile);
}

void SqlEditor::open(const QString &  newFile)
{
	QFile f(newFile);
	if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		QMessageBox::warning(this, tr("Open SQL Script"), tr("Cannot open file %1").arg(newFile));
		return;
	}

	canceled = false;
	int prgTmp = 0;
	progress = new QProgressDialog(tr("Opening: %1").arg(newFile), tr("Abort"), 0, f.size(), this);
	connect(progress, SIGNAL(canceled()), this, SLOT(cancel()));
	progress->setWindowModality(Qt::WindowModal);
	progress->setMinimumDuration(1000);
	ui.sqlTextEdit->clear();

	QTextStream in(&f);
	QString line;
	QStringList strList;
	while (!in.atEnd())
	{
		line = in.readLine();
		strList.append(line);
		prgTmp += line.length();
		if (!setProgress(prgTmp))
		{
			strList.clear();
			break;
		}
	}

	f.close();

	QStringList pathList = m_fileWatcher->files();
	if (pathList.count() > 0)
		m_fileWatcher->removePaths(pathList);
	m_fileName = newFile;
	m_fileWatcher->addPath(m_fileName);

	progress->setLabelText(tr("Formatting the text. Please wait."));
	ui.sqlTextEdit->append(strList.join("\n"));
	ui.sqlTextEdit->document()->setModified(false);

	// move cursor at the start of doc when it's opened
	QTextCursor cur(ui.sqlTextEdit->textCursor());
	cur.movePosition(QTextCursor::Start);
	ui.sqlTextEdit->setTextCursor(cur);

	delete progress;
	progress = 0;
}

void SqlEditor::cancel()
{
	canceled = true;
}

bool SqlEditor::setProgress(int p)
{
	if (canceled)
		return false;
	progress->setValue(p);
	qApp->processEvents();
	return true;
}

void SqlEditor::action_Save_triggered()
{
	if (m_fileName.isNull())
	{
		actionSave_As_triggered();
		return;
	}
	saveFile();
}

void SqlEditor::action_New_triggered()
{
	if (!changedConfirm())
		return;
	m_fileName = QString();
	ui.sqlTextEdit->document()->setModified(false);
	ui.sqlTextEdit->clear();
}

void SqlEditor::actionSave_As_triggered()
{
	QString newFile = QFileDialog::getSaveFileName(this, tr("Save SQL Script"),
			QDir::currentPath(), tr("SQL file (*.sql);;All Files (*)"));
	if (newFile.isNull())
		return;
	m_fileName = newFile;
	saveFile();
}

void SqlEditor::saveFile()
{
	QFile f(m_fileName);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
	{
		QMessageBox::warning(this, tr("Save SQL Script"), tr("Cannot write into file %1").arg(m_fileName));
		return;
	}
	m_fileWatcher->blockSignals(true); // switch of watching for self-excited signal
	QTextStream out(&f);
	out << ui.sqlTextEdit->toPlainText();
	f.close();
	ui.sqlTextEdit->document()->setModified(false);
	m_fileWatcher->blockSignals(false);
}

bool SqlEditor::changedConfirm()
{
	if (ui.sqlTextEdit->document()->isModified())
	{
		int ret = QMessageBox::question(this, tr("New File"),
					tr("All you changes will be lost. Are you sure?"),
					QMessageBox::Yes, QMessageBox::No);
	
		if (ret == QMessageBox::No)
			return false;
	}
	return true;
}

void SqlEditor::saveOnExit()
{
	if (!ui.sqlTextEdit->document()->isModified())
		return;
	int ret = QMessageBox::question(this, tr("Closing SQL Editor"),
				tr("Document has been changed. Do you want do save its content?"),
				QMessageBox::Yes, QMessageBox::No);
	
	if (ret == QMessageBox::No)
			return;
	if (m_fileName.isNull())
		actionSave_As_triggered();
	else
		saveFile();
}

void SqlEditor::sqlTextEdit_cursorPositionChanged()
{
	QTextCursor cur(ui.sqlTextEdit->textCursor());
	cursorLabel->setText(cursorTemplate.arg(cur.columnNumber()+1).arg(cur.blockNumber()+1).arg(ui.sqlTextEdit->document()->blockCount()));
}

void SqlEditor::documentChanged(bool state)
{
	changedLabel->setText(state ? "*" : " ");
}

void SqlEditor::setFileName(const QString & fname)
{
	open(fname);
}

void SqlEditor::prefsChanged()
{
	Preferences * prefs = Preferences::instance();
	ui.sqlTextEdit->selectAll();
	ui.sqlTextEdit->setCurrentFont(prefs->sqlFont());
	ui.sqlTextEdit->setFontPointSize(prefs->sqlFontSize());
	QTextCursor textCursor = ui.sqlTextEdit->textCursor();
	textCursor.clearSelection();
	ui.sqlTextEdit->setTextCursor(textCursor);
	ui.sqlTextEdit->setCompletion(prefs->codeCompletion(),
								  prefs->codeCompletionLength());
	ui.sqlTextEdit->setShortcuts(prefs->useShortcuts(),
								 prefs->shortcuts());
	ui.sqlTextEdit->update();
	update();
}

void SqlEditor::actionSearch_triggered()
{
	ui.searchFrame->setVisible(ui.actionSearch->isChecked());
	if (!ui.searchFrame->isVisible())
	{
		ui.sqlTextEdit->setFocus();
		return;
	}
	ui.searchEdit->selectAll();
	ui.searchEdit->setFocus();
}

void SqlEditor::find(QString ttf, bool forward, bool backward)
{
	QTextDocument *doc(ui.sqlTextEdit->document());
	QString oldText(ui.searchEdit->text());
	QTextCursor c = ui.sqlTextEdit->textCursor();
	QTextDocument::FindFlags options;
	QPalette p = ui.searchEdit->palette();
	p.setColor(QPalette::Active, QPalette::Base, Qt::white);

	if (c.hasSelection())
		c.setPosition(forward ? c.position() : c.anchor(), QTextCursor::MoveAnchor);

	QTextCursor newCursor = c;

	if (!ttf.isEmpty())
	{
		if (backward)
			options |= QTextDocument::FindBackward;

		if (ui.caseCheckBox->isChecked())
			options |= QTextDocument::FindCaseSensitively;

		if (ui.wholeWordsCheckBox->isChecked())
			options |= QTextDocument::FindWholeWords;

		newCursor = doc->find(ttf, c, options);
		ui.wrappedLabel->hide();

		if (newCursor.isNull())
		{
			QTextCursor ac(doc);
			ac.movePosition(options & QTextDocument::FindBackward
							? QTextCursor::End : QTextCursor::Start);
			newCursor = doc->find(ttf, ac, options);
			if (newCursor.isNull())
			{
				p.setColor(QPalette::Active, QPalette::Base, QColor(255, 102, 102));
				newCursor = c;
			}
			else
				ui.wrappedLabel->show();
		}
	}

	ui.sqlTextEdit->setTextCursor(newCursor);
	ui.searchEdit->setPalette(p);
}

void SqlEditor::searchEdit_textChanged(const QString &)
{
	findNext();
}

void SqlEditor::findNext()
{
	find(ui.searchEdit->text(), true, false);
}

void SqlEditor::findPrevious()
{
	find(ui.searchEdit->text(), false, true);
}

void SqlEditor::externalFileChange(const QString & path)
{
	int b = QMessageBox::information(this, tr("Unexpected File Change"),
									 tr("Your currently edited file has been changed outside " \
									 "this application. Do you want to reload it?"),
									 QMessageBox::Yes | QMessageBox::No,
									 QMessageBox::Yes);
	if (b != QMessageBox::Yes)
		return;
	open(path);
}


/* Tools ****************************************** */


SqlEditorTools::SqlHighlighter::SqlHighlighter(QTextDocument *parent)
	: QSyntaxHighlighter(parent)
{
	HighlightingRule rule;

	keywordFormat.setForeground(Qt::darkBlue);
	keywordFormat.setFontWeight(QFont::Bold);
	// select
	QStringList keywordPatterns(sqlKeywords());

	foreach (QString pattern, keywordPatterns)
	{
		rule.pattern = QRegExp("\\b" + pattern + "\\b", Qt::CaseInsensitive);
		rule.format = keywordFormat;
		highlightingRules.append(rule);
	}

	singleLineCommentFormat.setForeground(Qt::gray);
	rule.pattern = QRegExp("--[^\n]*");
	rule.format = singleLineCommentFormat;
	highlightingRules.append(rule);

	multiLineCommentFormat.setForeground(Qt::gray);

	quotationFormat.setForeground(Qt::red);
	rule.pattern = QRegExp("\'.*\'");
	rule.pattern.setMinimal(true);
	rule.format = quotationFormat;
	highlightingRules.append(rule);

	commentStartExpression = QRegExp("/\\*");
	commentEndExpression = QRegExp("\\*/");
}

void SqlEditorTools::SqlHighlighter::highlightBlock(const QString &text)
{
	foreach (HighlightingRule rule, highlightingRules)
	{
		QRegExp expression(rule.pattern);
		int index = text.indexOf(expression);
		while (index >= 0)
		{
			int length = expression.matchedLength();
			setFormat(index, length, rule.format);
			index = text.indexOf(expression, index + length);
		}
	}
	setCurrentBlockState(0);

	int startIndex = 0;
	if (previousBlockState() != 1)
		startIndex = text.indexOf(commentStartExpression);

	while (startIndex >= 0)
	{
		int endIndex = text.indexOf(commentEndExpression, startIndex);
		int commentLength;
		if (endIndex == -1)
		{
			setCurrentBlockState(1);
			commentLength = text.length() - startIndex;
		}
		else
		{
			commentLength = endIndex - startIndex
							+ commentEndExpression.matchedLength();
		}
		setFormat(startIndex, commentLength, multiLineCommentFormat);
		startIndex = text.indexOf(commentStartExpression,
								  startIndex + commentLength);
	}
}


/* Taken from Azevedo Filipe's TextEditor.
http://www.monkeystudio.org/
*/
SqlEditorTools::Gluter::Gluter( QTextEdit* edit )
	: QWidget( edit ), mEdit( edit )
{
	setAutoFillBackground( true );
	connect( mEdit->document()->documentLayout(), SIGNAL( update( const QRectF& ) ), this, SLOT( update() ) );
	connect( mEdit->verticalScrollBar(), SIGNAL( valueChanged( int ) ), this, SLOT( update() ) );
	setDefaultProperties();
}
//
void SqlEditorTools::Gluter::paintEvent( QPaintEvent* )
{
	int contentsY = mEdit->verticalScrollBar()->value();
	qreal pageBottom = contentsY + mEdit->viewport()->height();
	int lineNumber = 1;
	const QFontMetrics fm = fontMetrics();
	const int ascent = fontMetrics().ascent() +1;
	//
	QPainter p( this );
	// need a hack to only browse the viewed block for big document
	for ( QTextBlock block = mEdit->document()->begin(); block.isValid(); block = block.next(), lineNumber++ )
	{
		QTextLayout* layout = block.layout();
		const QRectF boundingRect = layout->boundingRect();
		QPointF position = layout->position();
		if ( position.y() +boundingRect.height() < contentsY )
			continue;
		if ( position.y() > pageBottom )
			break;
		const QString txt = QString::number( lineNumber );
		p.drawText( width() -fm.width( txt ) -fm.width( "0" ), qRound( position.y() ) -contentsY +ascent, txt ); // -fm.width( "0" ) is an ampty place/indent
	}
}
// PROPERTIES
void SqlEditorTools::Gluter::setDigitNumbers( int i )
{
	if ( i == mDigitNumbers )
		return;
	mDigitNumbers = i;
	setFixedWidth( fontMetrics().width( "0" ) * ( mDigitNumbers +2 ) ); // +2 = 1 empty place before and 1 empty place after
	emit digitNumbersChanged( mDigitNumbers );
}
//
int SqlEditorTools::Gluter::digitNumbers() const
{
	return mDigitNumbers;
}
//
void SqlEditorTools::Gluter::setTextColor( const QColor& c )
{
	if ( c == mTextColor )
		return;
	mTextColor = c;
	QPalette p( palette() );
	p.setColor( foregroundRole(), mTextColor );
	setPalette( p );
	emit textColorChanged( mTextColor );
}
//
const QColor& SqlEditorTools::Gluter::textColor() const
{
	return mTextColor;
}
//
void SqlEditorTools::Gluter::setBackgroundColor( const QColor& c )
{
	if ( c == mBackgroundColor )
		return;
	mBackgroundColor = c;
	QPalette p( palette() );
	p.setColor( backgroundRole(), mBackgroundColor );
	setPalette( p );
	emit backgroundColorChanged( mBackgroundColor );
}
//
const QColor& SqlEditorTools::Gluter::backgroundColor() const
{
	return mBackgroundColor;
}
// END PROPERTIES
void SqlEditorTools::Gluter::setDefaultProperties()
{
	// Default properties
	setBackgroundColor( QColor( "#fafafa" ) );
	setTextColor( QColor( "#000000" ) );
	setDigitNumbers( 4 );
}
