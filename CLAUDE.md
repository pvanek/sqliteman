# Working on Sqliteman

A tool for SQLite databases in Qt 6 Widgets, talking to SQLite through its
C API on a worker thread per connection. GPLv3. Its architecture follows
Slonisko (a PostgreSQL client by the same author): when in doubt, do what
Slonisko does, except where `docs/development/architecture.md` says why
Sqliteman differs.

## Documentation

The manual is in `docs/`, written in Markdown for Sphinx (MyST). Anything
worth explaining to a user belongs there rather than in the README, which
keeps only what someone needs before cloning. `docs/development/` holds the
build instructions, the architecture overview and the coding style, so this
file stays short. A change to what the user sees updates the manual, and
`tools/screenshots/run.sh` retakes its pictures.

## Coding style

The [Qt coding style](https://wiki.qt.io/Qt_Coding_Style), as written down in
`.clang-format`. Run it over anything you touch:

```sh
clang-format -i $(git ls-files '*.cpp' '*.h')
```

- Four spaces, never tabs; lines up to 100 characters.
- Braces on their own line for classes, structs and function bodies,
  attached everywhere else (`if (x) {`, `} else {`).
- `*` and `&` next to the name: `Session *session`, `const QString &name`.
- `m_` for members, `CamelCase` types, `camelCase` functions and variables.
- Single-statement bodies may drop their braces; then no branch of that
  statement has them either.
- `QT_NO_CAST_FROM_ASCII` and `QT_NO_KEYWORDS` are on: `QStringLiteral`,
  `QLatin1String`, `Q_EMIT`, `Q_SIGNALS`, `Q_SLOTS`.

## What goes where

| Directory     | What belongs there                                             |
|---------------|----------------------------------------------------------------|
| `src/sqlite`  | The SQLite C API: connections, statements, tasks, cancelling, values, the row store |
| `src/config`  | Preferences and recent files                                   |
| `src/sql`     | Lexing, keywords, statement splitting                          |
| `src/catalog` | The schema snapshot and everything derived from a database     |
| `src/app`     | The GUI                                                        |

Only `src/app` may depend on Qt Widgets. Keep logic out of the widgets: a
query, a parser or a format belongs in a library where a test can reach it
without a display. Each library keeps its tests in its own `tests/`.

One open database is one `Session` with one connection, shared by every
page on it. Work on it goes through the session's `sqlite::QueryRunner`,
as SQL or as a task (a function run with the `sqlite3 *` on the worker
thread); nothing touches the handle from the UI thread. Work that must
succeed or fail as a whole runs in a savepoint.

## House rules

- Every dialog's layout lives in a Qt Designer `.ui` file; the class only
  wires up behaviour.
- Comments explain why, not what. Write one wherever the reason is not
  obvious from the code, and none where it is.
- New behaviour comes with a test. Tests make their databases in a
  temporary directory; widget tests use `sqliteman_add_gui_test()`.
- Run `ctest --test-dir build` before saying anything is done, and say what
  actually happened, failures included.
- Do not commit unless asked to.
