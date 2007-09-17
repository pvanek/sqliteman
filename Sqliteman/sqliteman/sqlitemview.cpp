/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QDataWidgetMapper>
#include <QGridLayout>
#include <QSqlQueryModel>
#include <QSqlRecord>
#include <QLineEdit>

#include "sqlitemview.h"


SqlItemView::SqlItemView(QWidget * parent)
	: QWidget(parent)
{
	setupUi(this);
	m_mapper = new QDataWidgetMapper(this);

	connect(firstButton, SIGNAL(clicked()), m_mapper, SLOT(toFirst()));
	connect(previousButton, SIGNAL(clicked()), m_mapper, SLOT(toPrevious()));
	connect(nextButton, SIGNAL(clicked()), m_mapper, SLOT(toNext()));
	connect(lastButton, SIGNAL(clicked()), m_mapper, SLOT(toLast()));
	connect(m_mapper, SIGNAL(currentIndexChanged(int)), this, SLOT(updateButtons(int)));
}

void SqlItemView::setModel(QAbstractItemModel * model)
{
	QSqlQueryModel * m = qobject_cast<QSqlQueryModel *>(model);
	if (!m)
		return;

	m_mapper->clearMapping();
	m_mapper->setModel(model);

	if (scrollWidget->widget())
	{
		delete scrollWidget->takeWidget();
		layoutWidget = 0;
	}

	QSqlRecord rec(m->record());
	layoutWidget = new QWidget(scrollWidget);
	QGridLayout *layout = new QGridLayout(layoutWidget);
	QString tmp("%1:");

	for (int i = 0; i < rec.count(); ++i)
	{
		layout->addWidget(new QLabel(tmp.arg(rec.fieldName(i)), layoutWidget), i, 0);
		QLineEdit * w = new QLineEdit(layoutWidget);
		w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		layout->addWidget(w, i, 1);
		m_mapper->addMapping(w, i);
	}
	scrollWidget->setWidget(layoutWidget);
	m_mapper->toFirst();
}

void SqlItemView::updateButtons(int row)
{
	positionLabel->setText(tr("%1 of %2").arg(m_mapper->currentIndex()+1).arg(m_mapper->model()->rowCount()));
	previousButton->setEnabled(row > 0);
	nextButton->setEnabled(row < m_mapper->model()->rowCount() - 1);
}