# Architecture

```{index} architecture, libraries
```

Sqliteman 2 is built the way [Slonisko](https://github.com/pvanek/slonisko)
is: a few headless libraries, each with its own tests, and the GUI on top.
Everything except the widgets can be tested without a display.

| Directory | What belongs there |
|---|---|
| `src/sqlite` | The SQLite C API: connections on a thread of their own, statements, cancelling, values, the row store, quoting |
| `src/config` | Preferences and the recent files |
| `src/sql` | Lexing SQLite's SQL, keywords, statement splitting, comments, text search |
| `src/catalog` | The schema snapshot, and everything derived from a database: table definitions and altering them, editing rows, export, import, the populator, the query builder, dumps, pragmas, completion, query plans |
| `src/app` | The GUI |

Only `src/app` may depend on Qt Widgets. A query, a parser or a format
belongs in a library where a test can reach it: altering a table, writing
edited rows back, the CSV import, the export formats, the completion
matcher and the plan parser all live outside the widgets and have tests of
their own. Each library keeps its tests in its own `tests/` subdirectory.

## The SQLite layer

```{index} sqlite3 C API, threads
```

Sqliteman 1 used Qt's SQL module. Version 2 talks to SQLite directly: the
Qt driver hides what a client like this needs — a statement's error
offset, the table and column a result column comes from, the storage class
of each value, `sqlite3_interrupt()`, extensions — and it bundles a SQLite
of its own choosing.

`sqlite::Connection` owns one `sqlite3 *` and a worker thread. Everything
that touches the handle runs on that thread; the UI thread only queues work
and receives results, as queued calls, so nothing ever blocks the window.
Cancelling is `sqlite3_interrupt()`, the one call SQLite allows from
another thread.

What is queued is either SQL or a *task*. SQL is prepared and stepped
statement by statement; its rows come back in chunks of a
`sqlite::RowStore` — values kept with their storage class, `NULL` and
blobs included — so a big result shows its first rows while the rest are
still being read. A task is a function given the `sqlite3 *` on the worker
thread, for work that is more than a statement: reading the schema,
altering a table, writing edits, dumping. `sqlite::QueryRunner` puts a
queue and callbacks in front of a connection, so a page can ask for work
and be told when it is done, or be forgotten if it closes first.

An authorizer watches what each statement does, so a result can say that
it changed the schema; the session then reads the schema again.

## One connection per database

```{index} connection, Session
```

Here Sqliteman deliberately differs from Slonisko. Slonisko gives each
editor a connection of its own, because a PostgreSQL server handles many
connections, each with its own transaction, well. SQLite does not work like
that:

- A database file has one writer at a time. Two connections in the same
  program would lock each other out; a transaction opened in one editor
  would make every other editor's writes fail with *database is locked*.
- `ATTACH`, `temp` tables, loaded extensions, user functions and most
  pragmas belong to a connection. An editor that attaches a database
  expects the browser, and the other editors, to see it.

So `Session` — one open database — owns a single `sqlite::Connection` and
its `QueryRunner`, and every page on that database shares them: editors,
object pages with their rows, pragmas and dialogs. Their work is queued and runs one
item after another, and the transaction state is the session's.

## The schema

```{index} schema snapshot
```

`catalog::Snapshot` is the whole schema of a connection — every database
in it, their tables, views, indexes and triggers, with columns, keys and
foreign keys — read in one task by `catalog::loadSnapshot()`. A virtual
table whose module is not loaded is kept, with what can be known of it.
The browser, completion, the editing of rows, the diagrams and the dialogs
all read the same immutable snapshot, shared by pointer, so none of them
queries the schema on its own. `catalog::erdGraph()` makes a diagram's
tables and edges from it, and the layouts that place them are as headless
as the rest, so that the tests can check them; both come from Slonisko,
where they are built from the server's catalogue instead.

The session reads it again when a statement changed the schema, when a
transaction ends (a rollback can undo a `CREATE`), and after the tasks that
alter it; a change that comes in while one is being read makes it read
again once that is done.

## How a statement gets run

1. `sql::splitStatements()` cuts the script into statements, the way
   `sqlite3_complete()` does — trigger bodies with their semicolons
   included — and marks the `sqlite3` shell's dot commands.
2. The editor queues each statement on the session's runner, which
   prepares and steps it on the worker thread.
3. Rows arrive in chunks; the grid shows them as they come, up to the row
   limit.
4. With the columns' origin from SQLite's column metadata, and the
   snapshot, `catalog::editTarget()` decides whether the rows can be
   edited, and by which key.
5. An error comes back with SQLite's offset into the statement, which the
   editor turns into a mark in the script.

## Writing back

```{index} editing rows, alter table
```

Edited rows become parameterised `UPDATE`, `INSERT` and `DELETE`
statements (`catalog::dmlStatements()`), each finding its row by the
`rowid` or the primary key. `catalog::applyEdits()` runs them as one task
in a savepoint, and fails all of them unless each changed exactly one row.

Altering a table is `catalog::planAlterTable()`: from the table's old and
new definitions, either native `ALTER TABLE` statements or the rebuild
SQLite's documentation describes, with `legacy_alter_table` on so that
renaming the new table does not rewrite references. `catalog::alterTable()`
runs the plan as one task, with foreign keys off and checked afterwards.
The dialog shows the plan's SQL before anything runs.
