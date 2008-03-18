/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef SQLDELEGATE_H
#define SQLDELEGATE_H

#include <QItemDelegate>
#include "ui_sqldelegateui.h"

class QStyleOptionViewItem;
class QAbstractItemModel;
class QModelIndex;


/*! \brief A special delegate for editation of database result cells.
See SqlDelegateUi for its widget. See Qt4 docs for delegate informations.
\author Petr Vanek <petr@scribus.info>
*/
class SqlDelegate : public QItemDelegate
{
	Q_OBJECT

	public:
		SqlDelegate(QObject *parent = 0);
		QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
							  const QModelIndex &index) const;

		void setEditorData(QWidget *editor, const QModelIndex &index) const;
		void setModelData(QWidget *editor, QAbstractItemModel *model,
						  const QModelIndex &index) const;

		void updateEditorGeometry(QWidget *editor,
								  const QStyleOptionViewItem &option, const QModelIndex &index) const;

	private slots:
		void editor_closeEditor();
};


/*! \brief A custom widget used for direct data editation in the DataViewer result table.
It contains line edit and some buttons for specific tasks.
LineEdit is used for direct editation as in the default item delegate.
This widget is disabled when there is stored a multiline text (\\n)
or the value is BLOB.
nullButton handles a real NULL values insertions. Text can be in 3 states:
a string, an empty string ('') and NULL. Sqlite3 makes difference between '' and NULL.
editButton opens "multi" editor (see MultiEditDialog()). User can edit multi lined
large texts, load files into BLOB, or format DateTime data.
\author Petr Vanek <petr@scribus.info>
*/
class SqlDelegateUi : public QWidget, public Ui::SqlDelegateUi
{
	Q_OBJECT

	public:
		SqlDelegateUi(QWidget * parent = 0);
		
		void setSqlData(const QVariant & data);
		QVariant sqlData();

	signals:
		void closeEditor();

	private:
		QVariant m_sqlData;

		/*! Set focus to the proper place implementation.
		Jørgen Lind (Trolltech): "We set focus on the widget after we have created it
		and populated it in the delegate... I must admit, its not our best
		design ever, but it works on rather a few places. To work around the
		behavior your seeing, just reimplement the focusInEvent."
		*/
		void focusInEvent(QFocusEvent *e);

	private slots:
		void nullButton_clicked(bool);
		void editButton_clicked(bool);
		void lineEdit_textEdited(const QString &);
};

#endif // LIENEDIT_H
