// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

// Takes the manual's screenshots: drives the real main window against the
// sample database that shop.sql makes, and saves PNGs into docs/images.
// run.sh sets up everything around it; see docs/development/building.md.

#include "CompletionPopup.h"
#include "DatabaseBrowser.h"
#include "EditorPage.h"
#include "ExportDialog.h"
#include "ImportDialog.h"
#include "IndexDialog.h"
#include "MainWindow.h"
#include "MaintenanceDialog.h"
#include "ErdView.h"
#include "ObjectPage.h"
#include "PlanView.h"
#include "PopulatorDialog.h"
#include "PragmaPage.h"
#include "PreferencesDialog.h"
#include "QueryBuilderDialog.h"
#include "ResultModel.h"
#include "ResultPanel.h"
#include "ResultView.h"
#include "Session.h"
#include "Settings.h"
#include "SqlEditor.h"
#include "TableDialog.h"
#include "TableData.h"
#include "ValueViewer.h"

#include <QApplication>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QLineEdit>
#include <QPainter>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableView>
#include <QToolButton>
#include <QTest>
#include <QTreeView>

#include <cstdio>
#include <functional>

using namespace sqliteman;

namespace {

QString g_out;

[[noreturn]] void die(const QString &message)
{
    std::fprintf(stderr, "screenshots: %s\n", qPrintable(message));
    std::exit(1);
}

void waitFor(const std::function<bool()> &done, const char *what, int ms = 15000)
{
    if (!QTest::qWaitFor(done, ms))
        die(QStringLiteral("timed out waiting for %1").arg(QLatin1String(what)));
}

// Lets layouts, highlighting and repaints settle before a picture is taken.
void settle(int ms = 400)
{
    QTest::qWait(ms);
}

void save(const QPixmap &pixmap, const QString &name)
{
    const QString path = QDir(g_out).filePath(name + QStringLiteral(".png"));
    if (!pixmap.save(path))
        die(QStringLiteral("cannot write %1").arg(path));
    std::printf("%s\n", qPrintable(path));
}

void shoot(QWidget *widget, const QString &name)
{
    settle();
    save(widget->grab(), name);
}

// A dialog as it opens, taken once it is shown, then closed.
template<typename Dialog>
void shootDialog(Dialog *dialog, const QString &name, const std::function<void()> &prepare = {})
{
    dialog->show();
    if (prepare)
        prepare();
    shoot(dialog, name);
    dialog->close();
    delete dialog;
}

void runAndWait(EditorPage *page)
{
    page->run();
    waitFor([page] { return !page->isRunning(); }, "the statement");
    settle();
}

// The browser down to a database's tables, as a user would open it.
void expandTables(DatabaseBrowser *browser, Session *session)
{
    const QModelIndex main = browser->indexOf(session, QStringLiteral("main"));
    browser->view()->expand(main);
    const QModelIndex tables = browser->view()->model()->index(0, 0, main);
    browser->view()->expand(tables);
}

// A picture for the Product table's blob column: the program's own icon.
QByteArray picture()
{
    QFile file(QStringLiteral(":/icons/sqliteman-128.png"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("sqliteman"));
    QApplication::setOrganizationName(QStringLiteral("yarpen.cz"));
    if (argc != 4)
        die(QStringLiteral(
            "usage: sqliteman_screenshots <shop.sql> <work directory> <output directory>"));
    QFile script(QString::fromLocal8Bit(argv[1]));
    if (!script.open(QIODevice::ReadOnly))
        die(QStringLiteral("cannot read %1").arg(script.fileName()));
    const QString database
        = QDir(QString::fromLocal8Bit(argv[2])).filePath(QStringLiteral("shop.db"));
    g_out = QString::fromLocal8Bit(argv[3]);
    QFile::remove(database);

    MainWindow window;
    window.resize(1400, 860);
    window.show();
    DatabaseBrowser *browser = window.browser();
    Session *session = browser->openDatabase(database, true);
    waitFor([session] { return session->isOpen(); }, "the database");

    // The sample data, and a picture in a blob.
    bool done = false;
    session->runner()->run(script.readAll(), &window, [&](const sqlite::QueryOutcome &o) {
        if (!o.ok())
            die(o.error);
        done = true;
    });
    waitFor([&] { return done; }, "the sample data");
    done = false;
    sqlite::ExecuteOptions options;
    options.bindings = {{sqlite::Value::blob(picture())}};
    session->runner()->run(
        "UPDATE Product SET picture = ? WHERE id = 1", &window,
        [&](const sqlite::QueryOutcome &) { done = true; }, options);
    waitFor([&] { return done; }, "the picture");
    session->reloadSnapshot();
    waitFor(
        [session] {
            return session->snapshot()
                && session->snapshot()->find(QString(), QStringLiteral("OrderTotal"));
        },
        "the schema");
    settle();
    expandTables(browser, session);

    // The main window: the browser, an editor and its results.
    EditorPage *editor = window.currentEditor();
    editor->setSession(session);
    editor->editor()->setText(QStringLiteral("-- Orders of Praha's customers, with their totals\n"
                                             "SELECT c.name, o.id AS \"order\", o.status, t.total\n"
                                             "FROM Customer c\n"
                                             "JOIN \"Order\" o ON o.customer = c.id\n"
                                             "JOIN OrderTotal t ON t.id = o.id\n"
                                             "WHERE c.city = 'Praha'\n"
                                             "ORDER BY o.placed;\n"));
    editor->editor()->setCursorPosition(qsizetype(60));
    runAndWait(editor);
    shoot(&window, QStringLiteral("main-window"));

    // Completion.
    editor->editor()->setText(QStringLiteral("SELECT c.na\nFROM Customer c;\n"));
    editor->editor()->setCursorPosition(qsizetype(11));
    editor->editor()->setFocus();
    editor->editor()->complete(true);
    waitFor([editor] { return editor->editor()->completionPopup()->isVisible(); }, "completion");
    settle();
    {
        // The popup is a window of its own: drawn onto the editor's picture.
        QPixmap shot = editor->grab();
        QPainter painter(&shot);
        CompletionPopup *popup = editor->editor()->completionPopup();
        painter.drawPixmap(editor->mapFromGlobal(popup->mapToGlobal(QPoint(0, 0))), popup->grab());
        painter.end();
        save(shot.copy(0, 0, shot.width(), shot.height() * 2 / 3), QStringLiteral("completion"));
    }
    editor->editor()->completionPopup()->hide();

    // A query plan.
    editor->editor()->setText(QStringLiteral(
        "SELECT c.name, count(*)\nFROM Customer c\nJOIN \"Order\" o ON o.customer = c.id\n"
        "WHERE c.city = 'Praha'\nGROUP BY c.name\nORDER BY 2 DESC;\n"));
    editor->editor()->setCursorPosition(qsizetype(5));
    editor->explain();
    waitFor([editor] { return !editor->isRunning(); }, "the plan");
    shoot(editor->resultPanel(), QStringLiteral("plan"));
    editor->resultPanel()->showResults();

    // A table's rows, being edited.
    TableData *table
        = window.showObject(session, QStringLiteral("main"), QStringLiteral("Customer"))->data();
    waitFor([table] { return table->results()->model()->isEditable(); }, "the table");
    ResultModel *model = table->results()->model();
    model->setData(model->index(2, 2), QStringLiteral("cecilie@example.com"));
    table->results()->editToggle()->setChecked(true);
    table->results()->table()->setCurrentIndex(model->index(2, 2));
    shoot(&window, QStringLiteral("table-page"));
    model->discardChanges();

    // A blob, as an image, in the value editor.
    TableData *products
        = window.showObject(session, QStringLiteral("main"), QStringLiteral("Product"))->data();
    waitFor([products] { return products->results()->model()->isEditable(); }, "the products");
    {
        ValueViewer *viewer
            = products->results()->viewValue(products->results()->model()->index(0, 4));
        if (!viewer)
            die(QStringLiteral("no value viewer"));
        viewer->resize(640, 440);
        shoot(viewer, QStringLiteral("value-image"));
        viewer->close();
        ValueViewer *json
            = products->results()->viewValue(products->results()->model()->index(0, 3));
        json->resize(640, 400);
        shoot(json, QStringLiteral("value-json"));
        json->close();
    }

    // Details of a table.
    ObjectPage *details
        = window.showObject(session, QStringLiteral("main"), QStringLiteral("Order"));
    waitFor([details] { return !details->details().object.name.isEmpty(); }, "the details");
    details->tabs()->setCurrentWidget(details->columns());
    shoot(details, QStringLiteral("object-page"));
    // Its diagram: what it references, and what references it.
    details->showDiagram();
    waitFor([details] { return !details->diagram()->graph().isEmpty(); }, "the diagram");
    shoot(details, QStringLiteral("table-diagram"));

    // The database, and the diagram of all its tables.
    ObjectPage *databasePage = window.showObject(session, QStringLiteral("main"), QString());
    waitFor([databasePage] { return databasePage->properties()->rowCount() > 0; }, "the database");
    shoot(databasePage, QStringLiteral("database-page"));
    databasePage->showDiagram();
    waitFor([databasePage] { return !databasePage->diagram()->graph().isEmpty(); }, "its diagram");
    databasePage->diagram()->fitDiagram();
    shoot(databasePage, QStringLiteral("database-diagram"));

    // Pragmas.
    PragmaPage *pragmas = window.showPragmas(session, QStringLiteral("main"));
    waitFor([pragmas] { return !pragmas->value(QStringLiteral("page_size")).isEmpty(); },
            "the pragmas");
    shoot(pragmas, QStringLiteral("pragmas"));

    // The dialogs.
    shootDialog(
        new TableDialog(session, QStringLiteral("main"), QStringLiteral("Customer"), &window),
        QStringLiteral("alter-table"));
    shootDialog(new TableDialog(session, QStringLiteral("main"), &window),
                QStringLiteral("create-table"), [] { });
    {
        auto *dialog
            = new IndexDialog(session, QStringLiteral("main"), QStringLiteral("Order"), &window);
        dialog->setName(QStringLiteral("order_status"));
        dialog->useColumn(QStringLiteral("status"));
        dialog->useColumn(QStringLiteral("placed"), true);
        shootDialog(dialog, QStringLiteral("create-index"));
    }
    {
        auto *dialog = new PopulatorDialog(session, QStringLiteral("main"),
                                           QStringLiteral("Customer"), &window);
        shootDialog(dialog, QStringLiteral("populate"));
    }
    {
        auto *dialog = new QueryBuilderDialog(session->snapshot(), &window);
        dialog->setTable(QStringLiteral("main"), QStringLiteral("Customer"));
        dialog->selectColumns({QStringLiteral("name"), QStringLiteral("email")});
        dialog->addTerm(QStringLiteral("city"), catalog::QuerySpec::Relation::Equals,
                        QStringLiteral("Praha"));
        shootDialog(dialog, QStringLiteral("query-builder"));
    }
    {
        auto *dialog = new MaintenanceDialog(session, QStringLiteral("main"), &window);
        bool checked = false;
        dialog->run(QStringLiteral("PRAGMA integrity_check"), QStringLiteral("Integrity check"),
                    [&] { checked = true; });
        waitFor([&] { return checked; }, "the check");
        shootDialog(dialog, QStringLiteral("maintenance"));
    }
    {
        // A CSV file to import, next to the database.
        const QString csv
            = QDir(QString::fromLocal8Bit(argv[2])).filePath(QStringLiteral("customers.csv"));
        QFile file(csv);
        if (file.open(QIODevice::WriteOnly))
            file.write("name;email;city\nFrantišek Malý;frantisek@example.com;Plzeň\n"
                       "Gabriela Velká;;Liberec\nHana Bílá;hana@example.com;Praha\n");
        file.close();
        auto *dialog = new ImportDialog(session, QStringLiteral("main"), QStringLiteral("Customer"),
                                        &window);
        dialog->setPath(csv);
        shootDialog(dialog, QStringLiteral("import-dialog"));
    }
    {
        ExportDialog::Counts counts;
        counts.fetched = 5;
        counts.selected = 2;
        shootDialog(new ExportDialog(counts, QStringLiteral("Customer"), &window),
                    QStringLiteral("export-dialog"));
    }
    shootDialog(new PreferencesDialog(Settings::instance().preferences(), &window),
                QStringLiteral("preferences"));

    // Nothing to ask about on the way out.
    for (WorkspacePage *page : window.pages()) {
        if (auto *e = qobject_cast<EditorPage *>(page))
            e->editor()->setModified(false);
    }
    window.close();
    return 0;
}
