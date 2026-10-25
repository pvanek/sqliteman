# Sqliteman

````{only} qthelp
```{image} images/sqliteman.png
:alt: Sqliteman
:width: 128px
:align: center
```
````

A developer's and administrator's tool for SQLite databases, built with Qt6.

Sqliteman opens SQLite files, runs SQL against them, shows and edits their
rows, and does what SQLite itself makes awkward — altering a table, for one.
It talks to SQLite through its C API, on a thread of its own, so a long
query never freezes the window. It is free software under the GPL, version
3 or later.

```{figure} images/main-window.png
:alt: The main window: the database browser, a SQL editor and its results
```

## Who it is for

```{index} motivation, purpose
```

The ideal user is a developer whose program keeps its data in SQLite, or
whoever looks after such a database: someone who writes and tunes SQL,
manages tables, views, indexes and triggers, and keeps an eye on the file's
space and statistics. Sqliteman is not a form designer, a report tool or a
spreadsheet; it is the shell's `sqlite3` with a window around it, and
everything that the shell leaves to you done for you.

## History

```{index} history, LiteMan, version 2
```

Sqliteman started in 2007, from Igor Khanin's LiteMan, as a Qt 4
application. It grew a schema browser, a SQL editor, a data grid, the
*Alter Table* dialog SQLite lacks, a populator and import, and was
packaged for most Linux distributions, Windows and macOS. Then it stood
still for years, while Qt and SQLite moved on.

Version 2 is a rewrite. It keeps what Sqliteman was for and what its
dialogs did, and takes its architecture from
[Slonisko](https://github.com/pvanek/slonisko), a PostgreSQL client by the
same author: Qt 6, the SQLite C API instead of Qt's SQL module, everything
asynchronous, and tests for nearly all of it. Two features are gone: the
*Constraint Triggers* dialog, as SQLite enforces foreign keys itself since
3.6.19, and the translations, which would have described a program that no
longer exists. They are welcome back.

## Changelog

```{index} changelog, releases, version history
```

What changed in each version is written up with its release on GitHub:
[github.com/pvanek/sqliteman/releases](https://github.com/pvanek/sqliteman/releases).
Help → *About Sqliteman* tells which version is running and which SQLite
it uses. For changes not released yet, see the
[commit history](https://github.com/pvanek/sqliteman/commits/master).

## Author and credits

```{index} author, credits, contact
```

Sqliteman is written by Petr Vanek, in Czechia, with the help of its
contributors over the years: Igor Khanin, whose LiteMan it began from;
Kamil Les for quality assurance; the translators of version 1 — Joerg
Bernau, Brice Maron, Francesco De Marchi, Kamil Les and D. Yu. Usachov;
and everyone who sent a patch or an issue. The logo and the program's icon
are by Anouk Voogt.

Bugs and ideas are best reported as
[issues on GitHub](https://github.com/pvanek/sqliteman/issues), where others
can find them too. Anything else goes to
[petr@yarpen.cz](mailto:petr@yarpen.cz). More about the author is on
[yarpen.cz](https://yarpen.cz).

```{toctree}
:caption: Using Sqliteman
:maxdepth: 2

installing
getting-started
databases
browser
objects
editor
results
data
pragmas
```

```{toctree}
:caption: Reference
:maxdepth: 2

shortcuts
configuration
```

```{toctree}
:caption: Development
:maxdepth: 2

development/building
development/architecture
development/coding-style
```
