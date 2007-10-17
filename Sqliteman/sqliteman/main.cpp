/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

// #include <iostream>
#include <signal.h>

// required for *BSD
#ifndef WIN32
#include <unistd.h>
#endif

#include <QApplication>
#include <QIcon>
#include <QDir>
#include <QLocale>
#include <QTranslator>
#include <QStyleFactory>
#include <QMessageBox>
#include <QTextStream>

#include "litemanwindow.h"
#include "preferences.h"

#define ARG_VERSION "--version"
#define ARG_HELP "--help"
#define ARG_LANG "--lang"
#define ARG_AVAILLANG "--langs"
#define ARG_VERSION_SHORT "-v"
#define ARG_HELP_SHORT "-h"
#define ARG_LANG_SHORT "-l"
#define ARG_AVAILLANG_SHORT "-la"
#define endl QString("\n")


#ifndef WIN32
void initCrashHandler();
static void defaultCrashHandler(int sig);
#endif


/*! \brief Parse the CLI user input.
Based on the Scribus code (a bit).
\author Petr Vanek <petr@scribus.info>
*/
class ArgsParser
{
	public:
		ArgsParser(int c, char ** v);
		~ArgsParser(){};
		bool parseArgs();
		QString translator();
		QString localeCode();
		const QString & fileToOpen() { return m_file; };
	private:
		int argc;
		char ** argv;
		QString m_locale;
		QMap<int,QString> m_localeList;
		void langsAvailable();
		QString m_file;
};

/*! \brief Pre-fil available translations into QMap to cooperate
with PreferencesDialog.
*/
ArgsParser::ArgsParser(int c, char ** v) :	argc(c), argv(v), m_locale(""), m_file(0)
{
	QDir d(TRANSLATION_DIR, "*.qm");
	int i = 1; // 0 is for system default
	foreach (QString f, d.entryList())
	{
		m_localeList[i] = f.remove("sqliteman_").remove(".qm");
		++i;
	}
}

//! \brief Print available translations
void ArgsParser::langsAvailable()
{
	// HACK: QTextStream simulates std::cout here. It can handle
	// QStrings without any issues. E.g. std::cout << QString::toStdString()
	// does compile problems in some Qt4 configurations.
	QTextStream cout(stdout, QIODevice::WriteOnly);
	cout << QString("Available translation:") << endl;
	foreach (QString l, m_localeList.values())
		cout << QString("  --lang ") << l << endl;
}

/*! \brief Get the right translations.
Property: 1) specified from CLI - it overrides Prefs or System
2) from preferences
3) system pre-configured
*/
QString ArgsParser::localeCode()
{
	QString ret;
	Preferences * prefs = Preferences::instance();
	if (!m_locale.isEmpty())
		ret = QLocale(m_locale).name();
	else if (prefs->GUItranslator() != 0)
		ret = m_localeList[prefs->GUItranslator()];
	else
		ret = QLocale::system().name();
	return ret.left(2);
}

//! \brief Full qualified localisation file path.
QString ArgsParser::translator()
{
	return QDir::toNativeSeparators(QString("%1/sqliteman_%2.qm").arg(TRANSLATION_DIR).arg(localeCode()));
}

bool ArgsParser::parseArgs()
{
	QString arg("");
	QTextStream cout(stdout, QIODevice::WriteOnly);

	for(int i = 1; i < argc; i++)
	{
		arg = argv[i];

		if ((arg == ARG_LANG || arg == ARG_LANG_SHORT) && (++i < argc))
		{
			m_locale = argv[i];
			return true;
		}
		else if (arg == ARG_VERSION || arg == ARG_VERSION_SHORT)
		{
			cout << QString("Sqliteman ") << SQLITEMAN_VERSION << endl;
			return false;
		}
		else if (arg == ARG_HELP || arg == ARG_HELP_SHORT)
		{
			cout << endl << QString("sqliteman [options] [databasefile]") << endl;
			cout << QString("options:") << endl;
			cout << QString("  --help    -h  displays small help") << endl;
			cout << QString("  --version -v  prints version") << endl;
			cout << QString("  --lang    -l  set a GUI language. E.g. --lang cs for Czech") << endl;
			cout << QString("  --langs   -la lists available languages") << endl;
			cout << QString("  + various Qt options") << endl << endl;
			return false;
		}
		else if (arg == ARG_AVAILLANG || arg == ARG_AVAILLANG_SHORT)
		{
			langsAvailable();
			return false;
		}
		else
		{
			m_file = QFile::decodeName(argv[i]);
			if (!QFileInfo(m_file).exists())
			{
				if (m_file.left(1) == "-" || m_file.left(2) == "--")
					cout << QString("Invalid argument: ") << m_file << endl;
				else
					cout << QString("File ") << m_file << QString(" does not exist, aborting.") << endl;
				return false;
			}
			return true;
		}
	}
	return true;
}

int main(int argc, char ** argv)
{
	QApplication app(argc, argv);
#ifndef  WIN32
	initCrashHandler();
#endif
	ArgsParser cli(argc, argv);
	if (!cli.parseArgs())
		return 0;

	int style = Preferences::instance()->GUIstyle();
	if (style != 0)
	{
		QStringList sl = QStyleFactory::keys();
		sl.sort();
		QApplication::setStyle(QStyleFactory::create(sl.at(style-1)));
	}

	app.setWindowIcon(QIcon(QString(ICON_DIR) + "/sqliteman.png"));

	QTranslator translator;
	translator.load(cli.translator());
	app.installTranslator(&translator);

	LiteManWindow * wnd = new LiteManWindow(cli.fileToOpen());
	wnd->setLanguage(cli.localeCode());
	wnd->show();

	int r = app.exec();
	delete wnd;
	return r;
}

#ifndef WIN32
void initCrashHandler()
{
	typedef void (*HandlerType)(int);
	HandlerType handler	= 0;
	handler = defaultCrashHandler;
	if (!handler)
		handler = SIG_DFL;
	sigset_t mask;
	sigemptyset(&mask);
#ifdef SIGSEGV
	signal (SIGSEGV, handler);
	sigaddset(&mask, SIGSEGV);
#endif
#ifdef SIGFPE
	signal (SIGFPE, handler);
	sigaddset(&mask, SIGFPE);
#endif
#ifdef SIGILL
	signal (SIGILL, handler);
	sigaddset(&mask, SIGILL);
#endif
#ifdef SIGABRT
	signal (SIGABRT, handler);
	sigaddset(&mask, SIGABRT);
#endif
	sigprocmask(SIG_UNBLOCK, &mask, 0);
}


void defaultCrashHandler(int sig)
{
	QTextStream cout(stdout, QIODevice::WriteOnly);
	static int crashRecursionCounter = 0;
	crashRecursionCounter++;
	signal(SIGALRM, SIG_DFL);
	if (crashRecursionCounter < 2)
	{
		crashRecursionCounter++;
		QString sigMsg(QString("\nSqliteman crashes due to Signal #%1\n\n\
All database opened will be rollbacked and closed.\n\n\
Collect last steps that forced this\n\
situlation and report it as a bug, please.").arg(sig));
		cout << sigMsg << endl;
		QMessageBox::critical(0, "Sqliteman", sigMsg);
		alarm(300);
	}
	exit(255);
}
#endif

/* The following is an opening page for the source documentation. IGNORE */
/*!
\mainpage Sqliteman Source Documentation

The following pages contain an overview of the various classes, types and function that
make the Sqliteman source code. To better understand Sqliteman, this document can provide an
aid.

Maybe you can think that the documentation is weak. OK, should be better but the code
is clear in most cases.

\note This Sw is a fork of Igor Khanin's original LiteMan. But now it's totally rewritten.
You can find original copyright in the unmodified places.

How to create fresh documentation? Download the source code from SVN. Go to the
Sqliteman/devel-doc directory. Then run make. Doxygen is required, flawfinder and dot are
optional.
 */
