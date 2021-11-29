#include "mainwindow.h"
#include "gui_app.h"

#include <QApplication>
#include <QCommandLineParser>

struct ReturnCode {
    int value;
};

// TODO put in header
/// Translate a string in a global context, outside of a class.
static QString gtr(
    const char *context,
    const char *sourceText,
    const char *disambiguation = nullptr,
    int n = -1)
{
    return QCoreApplication::translate(context, sourceText, disambiguation, n);
}

static bool has(std::string const& s) {
    return !s.empty();
}

static bool has(QString const& s) {
    return !s.isEmpty();
}

static QString help_text(QCommandLineParser & parser) {
    return parser.helpText();
}

[[noreturn]] static void bail_only(QString error) {
    fprintf(stderr, "%s\n", error.toUtf8().data());
    exit(1);
}

[[noreturn]] static void bail_help(QCommandLineParser & parser, QString error) {
    fprintf(stderr, "%s\n\n%s",
        error.toUtf8().data(),
        help_text(parser).toUtf8().data());
    exit(1);
}

[[noreturn]] static void help_and_exit(QCommandLineParser & parser) {
    fputs(help_text(parser).toUtf8().data(), stdout);
    exit(0);
}

struct Arguments {
    QString filename;

    /// May exit if invalid arguments, --help, or --version is passed.
    [[nodiscard]]
    static Arguments parse_or_exit(QStringList const& arguments) {
        QCommandLineParser parser;

        // Prepare the argument list.
        parser.addHelpOption();
        parser.addVersionOption();
        parser.addPositionalArgument("FILE", gtr("main", ".vgm file to open."));

        // TODO sampling rate, loop count, etc.

        // Parse the arguments.
        // May exit if invalid arguments, --help, or --version is passed.
        if (!parser.parse(arguments)) {
            bail_help(parser, QStringLiteral("%1: %2").arg(
                gtr("main", "error"),
                parser.errorText()));
        }
        if (parser.isSet(QStringLiteral("version"))) {
            // Exits the program.
            parser.showVersion();
        }
        if (parser.isSet(QStringLiteral("help"))) {
            // Prints the default help text, followed by a list of sample document names.
            help_and_exit(parser);
        }
        if (parser.isSet(QStringLiteral("help-all"))) {
            // Prints the default app+Qt help text.
            parser.process(arguments);
            // This should be unreachable, since parser should exit upon seeing
            // --help-all.
            abort();
        }

        Arguments out;
        auto positional = parser.positionalArguments();
        if (0 < positional.size()) {
            out.filename = positional[0];
        }
        if (1 < positional.size()) {
            bail_help(parser, gtr("main", "Too many command-line arguments, expected FILE"));
        }

        return out;
    }
};


int main(int argc, char *argv[]) {
    GuiApp a(argc, argv);
    QCoreApplication::setApplicationName("qvgmsplit");

    // Parse command-line arguments.
    // May exit if invalid arguments, --help, or --version is passed.
    auto arg = Arguments::parse_or_exit(QCoreApplication::arguments());

    auto w = MainWindow::new_with_path(arg.filename);
    w->show();
    return a.exec();
}
