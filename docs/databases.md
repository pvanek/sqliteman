# Databases and transactions

```{index} databases, opening a database, creating a database
```

## Opening and creating

{kbd}`Ctrl+O` opens a database (or a script; Sqliteman looks at the file to
tell which). *New Database…* in the *Databases* tab, or in the *File* menu,
makes an empty one; SQLite writes the file when the first table is created.
A database that is open already is not opened twice: it is selected in the
browser instead.

A file that cannot be written — on a read-only medium, or without the
permission — opens read-only, and says so next to its name.

*Close Database* closes the selected one, and the pages that show its
tables and objects with it. Editors stay open, without a database.

## One connection per database

```{index} connection, locking, busy timeout
```

Each open database has exactly one connection, and every page on it —
editors, table pages, dialogs — works through that connection, one
statement after another. That is how SQLite is meant to be used:

- A database is written by one connection at a time. Two connections of the
  same program would wait for each other's locks.
- `ATTACH`, temporary tables, loaded extensions and `PRAGMA` settings belong
  to a connection. With one, what an editor sets up, every other page sees.

Another *program* that has the file open can still hold a lock. A statement
then waits for it for as long as the preferences say — five seconds, to
begin with — before it fails with *database is locked*.

Nothing runs on the window's thread: a long query keeps the window
responsive, and {kbd}`Ctrl+.` in its editor stops it.

## Transactions

```{index} transactions, commit, rollback, BEGIN
```

SQLite runs each statement in a transaction of its own unless one is opened
with `BEGIN` — typed into an editor, or by the editor's *Begin* button.
After that the database is *in a transaction*: its icon in the browser
changes, *Begin* goes grey, and the *Commit* and *Rollback* buttons of
every editor on it come alive. Since the connection
is shared, so is the transaction — whatever an editor or a table page
writes is part of it until it is committed or rolled back, whichever page
does that.

Closing a database, or the program, with a transaction open asks whether to
commit it or roll it back.

The dialogs that change the schema — altering a table, editing a view or a
trigger, populating, importing — do their work inside a savepoint: when
something fails halfway, nothing of it is kept, and an open transaction
stays as it was.

## Foreign keys

```{index} foreign keys, PRAGMA foreign_keys
```

SQLite enforces `REFERENCES` only when `PRAGMA foreign_keys` is on, and it
is off by default, for compatibility. Sqliteman turns it on for each
database it opens; the preferences can leave it off instead. The
[*Maintenance*](pragmas.md) dialog checks the rows that refer to rows that
are not there.

## Attached databases

```{index} ATTACH, DETACH, attached databases
```

*Attach Database…* (on a database's context menu, or in the *Database*
menu) adds another file to the connection under a name of your choosing;
its tables are then `name.table` in SQL, and it gets a node of its own in
the browser. `ATTACH` in an editor works just as well. *Detach Database* on
that node, or `DETACH`, lets it go. The temporary database, `temp`, is shown
once something is in it.

## Extensions

```{index} extensions, load_extension
```

An SQLite extension is a shared library that adds functions, collations or
virtual tables. Since it runs inside the program, loading them is off until
the preferences allow it. Then *Load Extension…* loads one into a
database's connection, and the preferences can name extensions to load into
every database as it opens.

The browser shows a virtual table whose module is not loaded, so that its
definition can still be read, but its rows cannot be until the extension is
there.
