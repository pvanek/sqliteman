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
};


class SqlDelegateUi : public QWidget, public Ui::SqlDelegateUi
{
	Q_OBJECT
			
	public:
		SqlDelegateUi(QWidget * parent = 0);
		
		void setSqlData(const QString & data);
		QString sqlData();

	private:
		 QString m_sqlData;

	private slots:
		void nullButton_clicked(bool);
		void editButton_clicked(bool);
		void lineEdit_textEdited(const QString &);
};

#endif // LIENEDIT_H
