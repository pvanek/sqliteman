// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ResultModel.h"
#include "Settings.h"

#include <QTest>

using namespace sqliteman;
using sqlite::Column;
using sqlite::Result;
using sqlite::Value;

namespace {

Result makeResult(std::initializer_list<const char *> columns,
                  std::initializer_list<std::initializer_list<Value>> rows,
                  const QString &table = {})
{
    auto data = std::make_shared<Result::Data>();
    auto names = std::make_shared<std::vector<Column>>();
    for (const char *name : columns)
        names->push_back({QString::fromUtf8(name),
                          {},
                          table.isEmpty() ? QString() : QStringLiteral("main"),
                          table,
                          QString::fromUtf8(name)});
    data->columns = names;
    data->rows.columns = int(names->size());
    for (const auto &row : rows) {
        for (const Value &v : row)
            data->rows.add(v.type(), v.data());
        ++data->rows.rows;
    }
    return Result(std::move(data));
}

catalog::EditTarget target(int columns)
{
    catalog::EditTarget t;
    t.schema = QStringLiteral("main");
    t.table = QStringLiteral("t");
    for (int c = 0; c < columns; ++c) {
        t.columns.push_back(QStringLiteral("c%1").arg(c));
        t.readOnly.push_back(false);
        t.affinity.push_back(c == 0 ? catalog::Affinity::Integer : catalog::Affinity::Text);
    }
    t.key = {0};
    return t;
}

} // namespace

class TestResultModel : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        config::Preferences p;
        p.nullText = QStringLiteral("<null>");
        p.blobText = QStringLiteral("<blob>");
        Settings::instance().setPreferences(p);
    }

    void showsNullsAndBlobsAsThePreferencesSay()
    {
        ResultModel model;
        model.setResult(makeResult(
            {"n", "b", "s"}, {{Value::integer(7), Value::blob(QByteArray(2048, 'x')), Value()}}));
        QCOMPARE(model.data(model.index(0, 2)).toString(), QStringLiteral("<null>"));
        QVERIFY(model.data(model.index(0, 1)).toString().startsWith(QLatin1String("<blob> (")));
        QVERIFY(!model.data(model.index(0, 2), ResultModel::ValueRole).isValid());
        // A blob goes to the clipboard in hex.
        QCOMPARE(model.data(model.index(0, 1), ResultModel::ValueRole).toString().size(), 4096);
        // Numbers line up on the right.
        QCOMPARE(model.data(model.index(0, 0), Qt::TextAlignmentRole).toInt() & Qt::AlignRight,
                 int(Qt::AlignRight));
    }

    void headerSaysWhereAColumnComesFrom()
    {
        // Issue #2: two columns called id, from different tables.
        ResultModel model;
        model.setResult(makeResult({"id"}, {{Value::integer(1)}}, QStringLiteral("person")));
        QVERIFY(model.headerData(0, Qt::Horizontal, Qt::ToolTipRole)
                    .toString()
                    .contains(QLatin1String("main.person.id")));
    }

    void editsAreTypedByAffinity()
    {
        ResultModel model;
        model.setResult(
            makeResult({"id", "name"}, {{Value::integer(1), Value::text(QStringLiteral("a"))}}));
        model.setEditTarget(target(2));
        QVERIFY(model.flags(model.index(0, 1)) & Qt::ItemIsEditable);
        QVERIFY(model.setData(model.index(0, 1), QStringLiteral("42")));
        QVERIFY(model.setData(model.index(0, 0), QStringLiteral("5")));
        const auto changes = model.changes();
        QCOMPARE(changes.size(), 1u);
        QCOMPARE(changes[0].key, std::vector<Value> {Value::integer(1)});
        // The text column keeps text; the integer one gets a number.
        QCOMPARE(changes[0].values[0].second, Value::integer(5));
        QCOMPARE(changes[0].values[1].second, Value::text(QStringLiteral("42")));
        // Back to what it was: no change at all.
        model.setData(model.index(0, 1), QStringLiteral("a"));
        model.setData(model.index(0, 0), QStringLiteral("1"));
        QVERIFY(!model.hasChanges());
    }

    void blobsAreEditedThroughTheValueEditor()
    {
        ResultModel model;
        model.setResult(
            makeResult({"id", "data"}, {{Value::integer(1), Value::blob(QByteArray("\x01", 1))}}));
        model.setEditTarget(target(2));
        QVERIFY(!(model.flags(model.index(0, 1)) & Qt::ItemIsEditable));
        QVERIFY(model.isWritable(model.index(0, 1)));
        QVERIFY(model.setValue(model.index(0, 1), Value::blob(QByteArray("\x02", 1))));
        QCOMPARE(model.changes()[0].values[0].second, Value::blob(QByteArray("\x02", 1)));
    }

    void addAndDeleteRows()
    {
        ResultModel model;
        model.setResult(makeResult({"id", "name"},
                                   {{Value::integer(1), Value::text(QStringLiteral("a"))},
                                    {Value::integer(2), Value::text(QStringLiteral("b"))}}));
        model.setEditTarget(target(2));
        const int added = model.addRow();
        QCOMPARE(model.data(model.index(added, 1)).toString(), QStringLiteral("DEFAULT"));
        model.setData(model.index(added, 1), QStringLiteral("c"));
        model.toggleDeleted({0});
        const auto changes = model.changes();
        QCOMPARE(changes.size(), 2u);
        QCOMPARE(changes[0].kind, catalog::RowEdit::Kind::Delete);
        QCOMPARE(changes[1].kind, catalog::RowEdit::Kind::Insert);
        QCOMPARE(changes[1].values.size(), 1u); // Only what was set; the rest default.
        model.discardChanges();
        QCOMPARE(model.rowCount(), 2);
    }

    void sortsByStorageClass()
    {
        ResultModel model;
        model.setResult(makeResult({"v"},
                                   {{Value::text(QStringLiteral("b10"))},
                                    {Value::integer(3)},
                                    {Value()},
                                    {Value::text(QStringLiteral("B2"))},
                                    {Value::real(1.5)}}));
        model.sort(0);
        QStringList order;
        for (int row = 0; row < model.rowCount(); ++row)
            order << model.data(model.index(row, 0)).toString();
        // Numbers first, then text naturally, NULLs last.
        QCOMPARE(order,
                 (QStringList {QStringLiteral("1.5"), QStringLiteral("3"), QStringLiteral("B2"),
                               QStringLiteral("b10"), QStringLiteral("<null>")}));
    }

    void copiesRowsForSpreadsheets()
    {
        ResultModel model;
        model.setResult(makeResult({"a", "b"},
                                   {{Value::integer(1), Value::text(QStringLiteral("x\ty"))},
                                    {Value::integer(2), Value()}}));
        const QString text = ResultModel::copyText(
            {model.index(0, 0), model.index(0, 1), model.index(1, 0), model.index(1, 1)});
        QCOMPARE(text, QStringLiteral("1\t\"x\ty\"\n2\t"));
    }
};

QTEST_MAIN(TestResultModel)
#include "tst_resultmodel.moc"
