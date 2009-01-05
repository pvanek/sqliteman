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
#include <QTextEdit>

#include "sqlitemview.h"


SqlItemView::SqlItemView(QWidget * parent)
	: QWidget(parent),
	m_column(0),
	m_count(0)
{
	setupUi(this);
	m_mapper = new QDataWidgetMapper(this);

	connect(firstButton, SIGNAL(clicked()),
			m_mapper, SLOT(toFirst()));
	connect(previousButton, SIGNAL(clicked()),
			m_mapper, SLOT(toPrevious()));
	connect(nextButton, SIGNAL(clicked()),
			m_mapper, SLOT(toNext()));
	connect(lastButton, SIGNAL(clicked()),
			m_mapper, SLOT(toLast()));
	connect(m_mapper, SIGNAL(currentIndexChanged(int)),
			this, SLOT(updateButtons(int)));
	connect(qApp, SIGNAL(focusChanged(QWidget*,QWidget*)),
			 this, SLOT(aApp_focusChanged(QWidget*,QWidget*)));
}

void SqlItemView::setModel(QAbstractItemModel * model)
{
	QSqlQueryModel * m = qobject_cast<QSqlQueryModel *>(model);
	if (!m)
		return;

	m_mapper->clearMapping();
	m_mapper->setModel(model);
	m_mapper->setSubmitPolicy(QDataWidgetMapper::ManualSubmit);

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
		QTextEdit * w = new QTextEdit(layoutWidget);
		w->setReadOnly(true); // TODO: make it transaction reliable
		w->setAcceptRichText(false);
		w->setMaximumHeight(50);
		w->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
		layout->addWidget(w, i, 1);
		m_mapper->addMapping(w, i);
	}
	scrollWidget->setWidget(layoutWidget);

	// bug #143 - crash - just to be sure if it'll be focused on 1st column
	m_column = 0;

	positionLabel->setText(tr("%1 of %2").arg(0).arg(0));
	m_mapper->toFirst();
	m_count = rec.count();
}

QAbstractItemModel * SqlItemView::model()
{
	return m_mapper->model();
}

void SqlItemView::updateButtons(int row)
{
	positionLabel->setText(tr("%1 of %2").arg(m_mapper->currentIndex()+1).arg(m_mapper->model()->rowCount()));

	previousButton->setEnabled(row > 0);
	firstButton->setEnabled(row > 0);
	nextButton->setEnabled(row < m_mapper->model()->rowCount() - 1);
	lastButton->setEnabled(row < m_mapper->model()->rowCount() - 1);

	QWidget * w = m_mapper->mappedWidgetAt(m_column);
	if (w)
		w->setFocus(Qt::OtherFocusReason);
}

void SqlItemView::setCurrentIndex(int row, int column)
{
	m_column = column;
	m_mapper->setCurrentIndex(row); // calls signal updateButtons
}

int SqlItemView::currentIndex()
{
	return m_mapper->currentIndex();
}

int SqlItemView::currentColumn()
{
	return m_column;
}

void SqlItemView::aApp_focusChanged(QWidget* old, QWidget* now)
{
	for (int i = 0; i < m_count; ++i)
	{
		if (m_mapper->mappedWidgetAt(i) == now)
		{
			m_column = i;
			emit indexChanged();
			break;
		}
	}
}
