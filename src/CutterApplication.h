#ifndef CUTTERAPPLICATION_H
#define CUTTERAPPLICATION_H

#include <QEvent>
#include <QApplication>
#include <QList>
#include <QProxyStyle>

#include "core/MainWindow.h"

enum class AutomaticAnalysisLevel { Ask, None, AAA, AAAA };

struct CutterCommandLineOptions
{
    QStringList args;
    AutomaticAnalysisLevel analLevel = AutomaticAnalysisLevel::Ask;
    InitialOptions fileOpenOptions;
    QString pythonHome;
    bool outputRedirectionEnabled = true;
    bool enableCutterPlugins = true;
    bool enableRizinPlugins = true;
};

class CutterApplication : public QApplication
{
    Q_OBJECT

public:
    CutterApplication(int &argc, char **argv, bool test = false);
    ~CutterApplication();

    MainWindow *getMainWindow() { return mainWindow; }

    void launchNewInstance(const QStringList &args = {});

protected:
    bool event(QEvent *e);

private:
    /**
     * @brief Load and translations depending on Language settings
     * @return true on success
     */
    bool loadTranslations();
    /**
     * @brief Parse commandline options and store them in a structure.
     * @return false if options have error
     */
    bool parseCommandLineOptions();

private:
    bool m_FileAlreadyDropped;
    MainWindow *mainWindow;
    CutterCommandLineOptions clOptions;
};

/**
 * @brief CutterProxyStyle is used to force shortcuts displaying in context menu
 */
class CutterProxyStyle : public QProxyStyle
{
    Q_OBJECT
public:
    /**
     * @brief it is enough to get notification about QMenu polishing to force shortcut displaying
     */
    void polish(QWidget *widget) override;
};

#endif // CUTTERAPPLICATION_H
