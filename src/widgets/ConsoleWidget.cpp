#include <QScrollBar>
#include <QMenu>
#include <QCompleter>
#include <QAction>
#include <QShortcut>
#include <QStringListModel>
#include <QTimer>
#include <QSettings>
#include <iostream>
#include "core/Cutter.h"
#include "ConsoleWidget.h"
#include "ui_ConsoleWidget.h"
#include "common/Helpers.h"
#include "common/SvgIconEngine.h"

#ifdef Q_OS_WIN
#include <io.h>
#include <QUuid>
#define dup2 _dup2
#define dup _dup
#define fileno _fileno
#define fdopen _fdopen
#define PIPE_SIZE 65536 // Match Linux size
#define PIPE_NAME "\\\\.\\pipe\\cutteroutput-%1"
#else
#include <unistd.h>
#define PIPE_READ  (0)
#define PIPE_WRITE (1)
#endif

static const int invalidHistoryPos = -1;

static const char *consoleWrapSettingsKey = "console.wrap";

ConsoleWidget::ConsoleWidget(MainWindow *main, QAction *action) :
    CutterDockWidget(main, action),
    ui(new Ui::ConsoleWidget),
    debugOutputEnabled(true),
    maxHistoryEntries(100),
    lastHistoryPosition(invalidHistoryPos),
    completer(nullptr),
    historyUpShortcut(nullptr),
    historyDownShortcut(nullptr)
{
    ui->setupUi(this);

    // Adjust console lineedit
    ui->inputLineEdit->setTextMargins(10, 0, 0, 0);

    setupFont();

    // Adjust text margins of consoleOutputTextEdit
    QTextDocument *console_docu = ui->outputTextEdit->document();
    console_docu->setDocumentMargin(10);

    QAction *actionClear = new QAction(tr("Clear Output"), ui->outputTextEdit);
    connect(actionClear, SIGNAL(triggered(bool)), ui->outputTextEdit, SLOT(clear()));
    actions.append(actionClear);

    actionWrapLines = new QAction(tr("Wrap Lines"), ui->outputTextEdit);
    actionWrapLines->setCheckable(true);
    setWrap(QSettings().value(consoleWrapSettingsKey, true).toBool());
    connect(actionWrapLines, &QAction::triggered, this, [this] (bool checked) {
        setWrap(checked);
    });
    actions.append(actionWrapLines);

    // Completion
    completionActive = false;
    completer = new QCompleter(&completionModel, this);
    completer->setMaxVisibleItems(20);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchStartsWith);
    ui->inputLineEdit->setCompleter(completer);

    connect(ui->inputLineEdit, &QLineEdit::textEdited, this, &ConsoleWidget::updateCompletion);
    updateCompletion();

    // Set console output context menu
    ui->outputTextEdit->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->outputTextEdit, SIGNAL(customContextMenuRequested(const QPoint &)),
            this, SLOT(showCustomContextMenu(const QPoint &)));

    // Esc clears inputLineEdit (like OmniBar)
    QShortcut *clear_shortcut = new QShortcut(QKeySequence(Qt::Key_Escape), ui->inputLineEdit);
    connect(clear_shortcut, SIGNAL(activated()), this, SLOT(clear()));
    clear_shortcut->setContext(Qt::WidgetShortcut);

    // Up and down arrows show history
    historyUpShortcut = new QShortcut(QKeySequence(Qt::Key_Up), ui->inputLineEdit);
    connect(historyUpShortcut, SIGNAL(activated()), this, SLOT(historyPrev()));
    historyUpShortcut->setContext(Qt::WidgetShortcut);

    historyDownShortcut = new QShortcut(QKeySequence(Qt::Key_Down), ui->inputLineEdit);
    connect(historyDownShortcut, SIGNAL(activated()), this, SLOT(historyNext()));
    historyDownShortcut->setContext(Qt::WidgetShortcut);

    QShortcut *completionShortcut = new QShortcut(QKeySequence(Qt::Key_Tab), ui->inputLineEdit);
    connect(completionShortcut, &QShortcut::activated, this, &ConsoleWidget::triggerCompletion);

    connect(ui->inputLineEdit, &QLineEdit::editingFinished, this, &ConsoleWidget::disableCompletion);

    connect(Config(), &Configuration::fontsUpdated, this, &ConsoleWidget::setupFont);
    connect(Config(), &Configuration::interfaceThemeChanged, this, &ConsoleWidget::setupFont);

    completer->popup()->installEventFilter(this);

    redirectOutput();
}

ConsoleWidget::~ConsoleWidget()
{
    delete completer;
}

bool ConsoleWidget::eventFilter(QObject *obj, QEvent *event)
{
    if(completer && obj == completer->popup() &&
        // disable up/down shortcuts if completer is shown
        (event->type() == QEvent::Type::Show || event->type() == QEvent::Type::Hide)) {
        bool enabled = !completer->popup()->isVisible();
        if (historyUpShortcut) {
            historyUpShortcut->setEnabled(enabled);
        }
        if (historyDownShortcut) {
            historyDownShortcut->setEnabled(enabled);
        }
    }
    return false;
}

void ConsoleWidget::setupFont()
{
    ui->outputTextEdit->setFont(Config()->getFont());
}

void ConsoleWidget::addOutput(const QString &msg)
{
    ui->outputTextEdit->appendPlainText(msg);
    scrollOutputToEnd();
}

void ConsoleWidget::addDebugOutput(const QString &msg)
{
    if (debugOutputEnabled) {
        ui->outputTextEdit->appendHtml("<font color=\"red\"> [DEBUG]:\t" + msg + "</font>");
        scrollOutputToEnd();
    }
}

void ConsoleWidget::focusInputLineEdit()
{
    ui->inputLineEdit->setFocus();
}

void ConsoleWidget::removeLastLine()
{
    ui->outputTextEdit->setFocus();
    QTextCursor cur = ui->outputTextEdit->textCursor();
    ui->outputTextEdit->moveCursor(QTextCursor::End, QTextCursor::MoveAnchor);
    ui->outputTextEdit->moveCursor(QTextCursor::StartOfLine, QTextCursor::MoveAnchor);
    ui->outputTextEdit->moveCursor(QTextCursor::End, QTextCursor::KeepAnchor);
    ui->outputTextEdit->textCursor().removeSelectedText();
    ui->outputTextEdit->textCursor().deletePreviousChar();
    ui->outputTextEdit->setTextCursor(cur);
}

void ConsoleWidget::executeCommand(const QString &command)
{
    if (!commandTask.isNull()) {
        return;
    }
    ui->inputLineEdit->setEnabled(false);

    const int originalLines = ui->outputTextEdit->blockCount();
    QTimer *timer = new QTimer(this);
    timer->setInterval(500);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, [this]() {
        ui->outputTextEdit->appendPlainText("Executing the command...");
    });

    QString cmd_line = "<br>[" + RAddressString(Core()->getOffset()) + "]> " + command + "<br>";
    RVA oldOffset = Core()->getOffset();
    commandTask = QSharedPointer<CommandTask>(new CommandTask(command, CommandTask::ColorMode::MODE_256, true));
    connect(commandTask.data(), &CommandTask::finished, this, [this, cmd_line,
          command, originalLines, oldOffset] (const QString & result) {

        if (originalLines < ui->outputTextEdit->blockCount()) {
            removeLastLine();
        }
        ui->outputTextEdit->appendHtml(cmd_line + result);
        scrollOutputToEnd();
        historyAdd(command);
        commandTask = nullptr;
        ui->inputLineEdit->setEnabled(true);
        ui->inputLineEdit->setFocus();
        if (oldOffset != Core()->getOffset()) {
            Core()->updateSeek();
        }
    });
    connect(commandTask.data(), &CommandTask::finished, timer, &QTimer::stop);

    timer->start();
    Core()->getAsyncTaskManager()->start(commandTask);
}

void ConsoleWidget::setWrap(bool wrap)
{
    QSettings().setValue(consoleWrapSettingsKey, wrap);
    actionWrapLines->setChecked(wrap);
    ui->outputTextEdit->setLineWrapMode(wrap ? QPlainTextEdit::WidgetWidth: QPlainTextEdit::NoWrap);
}

void ConsoleWidget::on_inputLineEdit_returnPressed()
{
    QString input = ui->inputLineEdit->text();
    if (input.isEmpty()) {
        return;
    }
    executeCommand(input);
    ui->inputLineEdit->clear();
}

void ConsoleWidget::on_execButton_clicked()
{
    on_inputLineEdit_returnPressed();
}

void ConsoleWidget::showCustomContextMenu(const QPoint &pt)
{
    actionWrapLines->setChecked(ui->outputTextEdit->lineWrapMode() == QPlainTextEdit::WidgetWidth);

    QMenu *menu = new QMenu(ui->outputTextEdit);
    menu->addActions(actions);
    menu->exec(ui->outputTextEdit->mapToGlobal(pt));
    menu->deleteLater();
}

void ConsoleWidget::historyNext()
{
    if (!history.isEmpty()) {
        if (lastHistoryPosition > invalidHistoryPos) {
            if (lastHistoryPosition >= history.size()) {
                lastHistoryPosition = history.size() - 1 ;
            }

            --lastHistoryPosition;

            if (lastHistoryPosition >= 0) {
                ui->inputLineEdit->setText(history.at(lastHistoryPosition));
            } else {
                ui->inputLineEdit->clear();
            }


        }
    }
}

void ConsoleWidget::historyPrev()
{
    if (!history.isEmpty()) {
        if (lastHistoryPosition >= history.size() - 1) {
            lastHistoryPosition = history.size() - 2;
        }

        ui->inputLineEdit->setText(history.at(++lastHistoryPosition));
    }
}

void ConsoleWidget::triggerCompletion()
{
    if (completionActive) {
        return;
    }
    completionActive = true;
    updateCompletion();
    completer->complete();
}

void ConsoleWidget::disableCompletion()
{
    if (!completionActive) {
        return;
    }
    completionActive = false;
    updateCompletion();
    completer->popup()->hide();
}

void ConsoleWidget::updateCompletion()
{
    if (!completionActive) {
        completionModel.setStringList({});
        return;
    }

    auto current = ui->inputLineEdit->text();
    auto completions = Core()->autocomplete(current, R_LINE_PROMPT_DEFAULT);
    int lastSpace = current.lastIndexOf(' ');
    if (lastSpace >= 0) {
        current = current.left(lastSpace + 1);
        for (auto &s : completions) {
            s = current + s;
        }
    }
    completionModel.setStringList(completions);
}

void ConsoleWidget::clear()
{
    disableCompletion();
    ui->inputLineEdit->clear();

    invalidateHistoryPosition();

    // Close the potential shown completer popup
    ui->inputLineEdit->clearFocus();
    ui->inputLineEdit->setFocus();
}

void ConsoleWidget::scrollOutputToEnd()
{
    const int maxValue = ui->outputTextEdit->verticalScrollBar()->maximum();
    ui->outputTextEdit->verticalScrollBar()->setValue(maxValue);
}

void ConsoleWidget::historyAdd(const QString &input)
{
    if (history.size() + 1 > maxHistoryEntries) {
        history.removeLast();
    }

    history.prepend(input);

    invalidateHistoryPosition();
}
void ConsoleWidget::invalidateHistoryPosition()
{
    lastHistoryPosition = invalidHistoryPos;
}

void ConsoleWidget::processQueuedOutput()
{
    // Partial lines are ignored since carriage return is currently unsupported
    while (pipeSocket->canReadLine()) {
        QString output = QString(pipeSocket->readLine());

        fprintf(origStderr, "%s", output.toStdString().c_str());

        // Get the last segment that wasn't overwritten by carriage return
        output = output.trimmed();
        addOutput(output.remove(0, output.lastIndexOf('\r')));
    }
}

void ConsoleWidget::redirectOutput()
{
    // Make sure that we are running in a valid console with initialized output handles
    if(0 > fileno(stderr) && 0 > fileno(stdout)) {
        addOutput("Run cutter in a console to enable r2 output redirection into this widget.");
        return;
    }

    pipeSocket = new QLocalSocket(this);

    origStderr = fdopen(dup(fileno(stderr)), "a");
    origStdout = fdopen(dup(fileno(stdout)), "a");
#ifdef Q_OS_WIN
    QString pipeName = QString::fromLatin1(PIPE_NAME).arg(QUuid::createUuid().toString());

    SECURITY_ATTRIBUTES attributes = {sizeof(SECURITY_ATTRIBUTES), 0, false};
    hWrite = CreateNamedPipeW((wchar_t*)pipeName.utf16(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                             PIPE_TYPE_BYTE | PIPE_WAIT, 1, PIPE_SIZE, PIPE_SIZE, 0, &attributes);
 
    int writeFd = _open_osfhandle((intptr_t)hWrite, _O_WRONLY | _O_TEXT);
    dup2(writeFd, fileno(stdout));
    dup2(writeFd, fileno(stderr));

    pipeSocket->connectToServer(pipeName, QIODevice::ReadOnly);
#else
    pipe(redirectPipeFds);
    dup2(redirectPipeFds[PIPE_WRITE], fileno(stderr));
    dup2(redirectPipeFds[PIPE_WRITE], fileno(stdout));

    // Attempt to force line buffering to avoid calling processQueuedOutput
    // for partial lines
    setlinebuf(stderr);
    setlinebuf(stdout);

    // Configure the pipe to work in async mode
    fcntl(redirectPipeFds[PIPE_READ], F_SETFL, O_ASYNC | O_NONBLOCK);

    pipeSocket->setSocketDescriptor(redirectPipeFds[PIPE_READ]);
    pipeSocket->connectToServer(QIODevice::ReadOnly);
#endif

    connect(pipeSocket, SIGNAL(readyRead()), this, SLOT(processQueuedOutput()));
}
