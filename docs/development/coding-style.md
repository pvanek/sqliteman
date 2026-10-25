# Coding style

```{index} coding style, clang-format
```

The [Qt coding style](https://wiki.qt.io/Qt_Coding_Style), with
`.clang-format` in the repository root as its written form:

- Four spaces, never tabs; lines up to 100 characters.
- Braces on their own line for classes, structs and function bodies,
  attached everywhere else (`if (x) {`, `} else {`).
- `*` and `&` next to the name: `Session *session`, `const QString &name`.
- A space after control keywords, none after a function name: `if (ok)`,
  `run(sql)`.
- Braces may be left out for single-statement bodies, but then the whole
  statement, `else` branch included, goes without them.
- Names: `CamelCase` types, `camelCase` functions and variables, `m_` for
  members, `SCREAMING_CASE` only for macros.

Before committing:

```sh
clang-format -i $(git ls-files '*.cpp' '*.h')
```

## Beyond the formatter

- Comments say **why**, not what the next line already says. Where the
  reason is not obvious from the code, write a sentence; where it is, write
  nothing.
- Every dialog's layout lives in a Qt Designer `.ui` file, so it can be
  changed without touching code.
- New behaviour comes with a test, and the test says what the behaviour is
  for rather than restating the implementation.
- Logic stays out of the widgets. Only `src/app` may use Qt Widgets; what
  can be tested without a display belongs in a library.
- Nothing blocks the UI thread: work on a database goes through its
  session's `sqlite::QueryRunner`, as SQL or as a task.
