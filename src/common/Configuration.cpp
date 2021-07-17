#include "Configuration.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFontDatabase>
#include <QFile>
#include <QApplication>

#ifdef CUTTER_ENABLE_KSYNTAXHIGHLIGHTING
#    include <KSyntaxHighlighting/repository.h>
#    include <KSyntaxHighlighting/theme.h>
#    include <KSyntaxHighlighting/definition.h>
#endif

#include "common/ColorThemeWorker.h"
#include "common/SyntaxHighlighter.h"
#include "common/ResourcePaths.h"

/* Map with names of themes associated with its color palette
 * (Dark or Light), so for dark interface themes will be shown only Dark color themes
 * and for light - only light ones.
 */
const QHash<QString, ColorFlags> Configuration::relevantThemes = {
    { "ayu", DarkFlag },     { "consonance", DarkFlag }, { "darkda", DarkFlag },
    { "onedark", DarkFlag }, { "solarized", DarkFlag },  { "zenburn", DarkFlag },
    { "cutter", LightFlag }, { "dark", LightFlag },      { "matrix", LightFlag },
    { "tango", LightFlag },  { "white", LightFlag }
};
static const QString DEFAULT_LIGHT_COLOR_THEME = "cutter";
static const QString DEFAULT_DARK_COLOR_THEME = "ayu";

const QHash<QString, QHash<ColorFlags, QColor>> Configuration::cutterOptionColors = {
    { "gui.cflow",
      { { DarkFlag, QColor(0xff, 0xff, 0xff) }, { LightFlag, QColor(0x00, 0x00, 0x00) } } },
    { "gui.dataoffset",
      { { DarkFlag, QColor(0xff, 0xff, 0xff) }, { LightFlag, QColor(0x00, 0x00, 0x00) } } },
    { "gui.imports",
      { { DarkFlag, QColor(0x32, 0x8c, 0xff) }, { LightFlag, QColor(0x32, 0x8c, 0xff) } } },
    { "gui.item_invalid",
      { { DarkFlag, QColor(0x9b, 0x9b, 0x9b) }, { LightFlag, QColor(0x9b, 0x9b, 0x9b) } } },
    { "gui.main",
      { { DarkFlag, QColor(0x21, 0xd8, 0x93) }, { LightFlag, QColor(0x00, 0x80, 0x00) } } },
    { "gui.item_unsafe",
      { { DarkFlag, QColor(0xff, 0x81, 0x7b) }, { LightFlag, QColor(0xff, 0x81, 0x7b) } } },
    { "gui.navbar.seek",
      { { DarkFlag, QColor(0xe9, 0x56, 0x56) }, { LightFlag, QColor(0xff, 0x00, 0x00) } } },
    { "gui.navbar.pc",
      { { DarkFlag, QColor(0x42, 0xee, 0xf4) }, { LightFlag, QColor(0x42, 0xee, 0xf4) } } },
    { "gui.navbar.code",
      { { DarkFlag, QColor(0x82, 0xc8, 0x6f) }, { LightFlag, QColor(0x68, 0xe5, 0x45) } } },
    { "gui.navbar.str",
      { { DarkFlag, QColor(0x6f, 0x86, 0xd8) }, { LightFlag, QColor(0x45, 0x68, 0xe5) } } },
    { "gui.navbar.sym",
      { { DarkFlag, QColor(0xdd, 0xa3, 0x68) }, { LightFlag, QColor(0xe5, 0x96, 0x45) } } },
    { "gui.navbar.empty",
      { { DarkFlag, QColor(0x64, 0x64, 0x64) }, { LightFlag, QColor(0xdc, 0xec, 0xf5) } } },
    { "gui.breakpoint_background",
      { { DarkFlag, QColor(0x8c, 0x4c, 0x4c) }, { LightFlag, QColor(0xe9, 0x8f, 0x8f) } } },
    { "gui.overview.node",
      { { DarkFlag, QColor(0x64, 0x64, 0x64) }, { LightFlag, QColor(0xf5, 0xfa, 0xff) } } },
    { "gui.tooltip.background",
      { { DarkFlag, QColor(0x2a, 0x2c, 0x2e) }, { LightFlag, QColor(0xfa, 0xfc, 0xfe) } } },
    { "gui.tooltip.foreground",
      { { DarkFlag, QColor(0xfa, 0xfc, 0xfe) }, { LightFlag, QColor(0x2a, 0x2c, 0x2e) } } },
    { "gui.border",
      { { DarkFlag, QColor(0x64, 0x64, 0x64) }, { LightFlag, QColor(0x91, 0xc8, 0xfa) } } },
    { "gui.background",
      { { DarkFlag, QColor(0x25, 0x28, 0x2b) }, { LightFlag, QColor(0xff, 0xff, 0xff) } } },
    { "gui.alt_background",
      { { DarkFlag, QColor(0x1c, 0x1f, 0x24) }, { LightFlag, QColor(0xf5, 0xfa, 0xff) } } },
    { "gui.disass_selected",
      { { DarkFlag, QColor(0x1f, 0x22, 0x28) }, { LightFlag, QColor(0xff, 0xff, 0xff) } } },
    { "lineHighlight",
      { { DarkFlag, QColor(0x15, 0x1d, 0x1d, 0x96) },
        { LightFlag, QColor(0xd2, 0xd2, 0xff, 0x96) } } },
    { "wordHighlight",
      { { DarkFlag, QColor(0x34, 0x3a, 0x47, 0xff) },
        { LightFlag, QColor(0xb3, 0x77, 0xd6, 0x3c) } } },
    { "highlightPC",
      { { DarkFlag, QColor(0x57, 0x1a, 0x07) }, { LightFlag, QColor(0xd6, 0xff, 0xd2) } } },
    { "gui.overview.fill",
      { { DarkFlag, QColor(0xff, 0xff, 0xff, 0x28) },
        { LightFlag, QColor(0xaf, 0xd9, 0xea, 0x41) } } },
    { "gui.overview.border",
      { { DarkFlag, QColor(0x63, 0xda, 0xe8, 0x32) },
        { LightFlag, QColor(0x63, 0xda, 0xe8, 0x32) } } },
    { "gui.navbar.err",
      { { DarkFlag, QColor(0x03, 0xaa, 0xf5) }, { LightFlag, QColor(0x03, 0xaa, 0xf5) } } }
};

Configuration *Configuration::mPtr = nullptr;

/**
 * @brief All asm.* options saved as settings. Values are the default values.
 */
static const QHash<QString, QVariant> asmOptions = { { "asm.esil", false },
                                                     { "asm.pseudo", false },
                                                     { "asm.offset", true },
                                                     { "asm.xrefs", false },
                                                     { "asm.indent", false },
                                                     { "asm.describe", false },
                                                     { "asm.slow", true },
                                                     { "asm.lines", true },
                                                     { "asm.lines.fcn", true },
                                                     { "asm.flags.offset", false },
                                                     { "asm.emu", false },
                                                     { "emu.str", false },
                                                     { "asm.cmt.right", true },
                                                     { "asm.cmt.col", 35 },
                                                     { "asm.var.summary", false },
                                                     { "asm.bytes", false },
                                                     { "asm.size", false },
                                                     { "asm.bytes.space", false },
                                                     { "asm.lbytes", true },
                                                     { "asm.nbytes", 10 },
                                                     { "asm.syntax", "intel" },
                                                     { "asm.ucase", false },
                                                     { "asm.bb.line", false },
                                                     { "asm.capitalize", false },
                                                     { "asm.sub.var", true },
                                                     { "asm.sub.varonly", true },
                                                     { "asm.tabs", 8 },
                                                     { "asm.tabs.off", 5 },
                                                     { "asm.marks", false },
                                                     { "asm.refptr", false },
                                                     { "asm.flags.real", true },
                                                     { "asm.reloff", false },
                                                     { "asm.reloff.flags", false },
                                                     { "esil.breakoninvalid", true },
                                                     { "dbg.trace_continue", true },
                                                     { "graph.offset", false } };

Configuration::Configuration() : QObject(), nativePalette(qApp->palette())
{
    mPtr = this;
    if (!s.isWritable()) {
        QMessageBox::critical(
                nullptr, tr("Critical!"),
                tr("!!! Settings are not writable! Make sure you have a write access to \"%1\"")
                        .arg(s.fileName()));
    }
#ifdef CUTTER_ENABLE_KSYNTAXHIGHLIGHTING
    kSyntaxHighlightingRepository = nullptr;
#endif
}

Configuration *Configuration::instance()
{
    if (!mPtr)
        mPtr = new Configuration();
    return mPtr;
}

void Configuration::loadInitial()
{
    setInterfaceTheme(getInterfaceTheme());
    setColorTheme(getColorTheme());
    applySavedAsmOptions();

#ifdef CUTTER_ENABLE_KSYNTAXHIGHLIGHTING
    kSyntaxHighlightingRepository = new KSyntaxHighlighting::Repository();
#endif
}

QString Configuration::getRecentFolder()
{
    QString recentFolder = s.value("dir.recentFolder", QDir::homePath()).toString();

    return QDir::toNativeSeparators(recentFolder);
}

void Configuration::setRecentFolder(const QString &dir)
{
    s.setValue("dir.recentFolder", QDir::toNativeSeparators(dir));
}

/**
 * @brief Configuration::setFilesTabLastClicked
 * Set the new file dialog last clicked tab
 * @param lastClicked
 */
void Configuration::setNewFileLastClicked(int lastClicked)
{
    s.setValue("newFileLastClicked", lastClicked);
}

int Configuration::getNewFileLastClicked()
{
    return s.value("newFileLastClicked").toInt();
}

void Configuration::resetAll()
{
    // Don't reset all rizin vars, that currently breaks a bunch of stuff.
    // settingsFile.remove()+loadInitials() should reset all settings configurable using Cutter GUI.
    // Core()->cmdRaw("e-");

    Core()->setSettings();
    // Delete the file so no extra configuration is in it.
    QFile settingsFile(s.fileName());
    settingsFile.remove();
    s.clear();

    loadInitial();
    emit fontsUpdated();
}

bool Configuration::getAutoUpdateEnabled() const
{
    return s.value("autoUpdateEnabled", false).toBool();
}

void Configuration::setAutoUpdateEnabled(bool au)
{
    s.setValue("autoUpdateEnabled", au);
}

/**
 * @brief get the current Locale set in Cutter's user configuration
 * @return a QLocale object describes user's current locale
 */
QLocale Configuration::getCurrLocale() const
{
    return s.value("locale", QLocale().system()).toLocale();
}

/**
 * @brief sets Cutter's locale
 * @param l - a QLocale object describes the locate to configure
 */
void Configuration::setLocale(const QLocale &l)
{
    s.setValue("locale", l);
}

/**
 * @brief set Cutter's interface language by a given locale name
 * @param language - a string represents the name of a locale language
 * @return true on success
 */
bool Configuration::setLocaleByName(const QString &language)
{
    const auto &allLocales =
            QLocale::matchingLocales(QLocale::AnyLanguage, QLocale::AnyScript, QLocale::AnyCountry);

    for (auto &it : allLocales) {
        if (QString::compare(it.nativeLanguageName(), language, Qt::CaseInsensitive) == 0 ||
            it.name() == language) {
            setLocale(it);
            return true;
        }
    }
    return false;
}

bool Configuration::windowColorIsDark()
{
    ColorFlags currentThemeColorFlags = getCurrentTheme()->flag;
    if (currentThemeColorFlags == ColorFlags::LightFlag) {
        return false;
    } else if (currentThemeColorFlags == ColorFlags::DarkFlag) {
        return true;
    }
    return nativeWindowIsDark();
}

bool Configuration::nativeWindowIsDark()
{
    const QPalette &palette = qApp->palette();
    auto windowColor = palette.color(QPalette::Window).toRgb();
    return (windowColor.red() + windowColor.green() + windowColor.blue()) < 382;
}

void Configuration::loadNativeStylesheet()
{
    /* Load Qt Theme */
    QFile f(":native/native.qss");
    if (!f.exists()) {
        qWarning() << "Can't find Native theme stylesheet.";
    } else {
        f.open(QFile::ReadOnly | QFile::Text);
        QTextStream ts(&f);
        QString stylesheet = ts.readAll();
        qApp->setStyleSheet(stylesheet);
    }

    qApp->setPalette(nativePalette);
    /* Some widgets does not change its palette when QApplication changes one
     * so this loop force all widgets do this, but all widgets take palette from
     * QApplication::palette() when they are created so line above is necessary too.
     */
    for (auto widget : qApp->allWidgets()) {
        widget->setPalette(nativePalette);
    }
}

/**
 * @brief Loads the Light theme of Cutter and modify special theme colors
 */
void Configuration::loadLightStylesheet()
{
    /* Load Qt Theme */
    QFile f(":lightstyle/light.qss");
    if (!f.exists()) {
        qWarning() << "Can't find Light theme stylesheet.";
    } else {
        f.open(QFile::ReadOnly | QFile::Text);
        QTextStream ts(&f);
        QString stylesheet = ts.readAll();

        QPalette p = qApp->palette();
        p.setColor(QPalette::Text, Qt::black);
        qApp->setPalette(p);

        qApp->setStyleSheet(stylesheet);
    }
}

void Configuration::loadDarkStylesheet()
{
    /* Load Qt Theme */
    QFile f(":qdarkstyle/style.qss");
    if (!f.exists()) {
        qWarning() << "Can't find Dark theme stylesheet.";
    } else {
        f.open(QFile::ReadOnly | QFile::Text);
        QTextStream ts(&f);
        QString stylesheet = ts.readAll();
#ifdef Q_OS_MACX
        // see https://github.com/ColinDuquesnoy/QDarkStyleSheet/issues/22#issuecomment-96179529
        stylesheet += "QDockWidget::title"
                      "{"
                      "    background-color: #31363b;"
                      "    text-align: center;"
                      "    height: 12px;"
                      "}";
#endif
        QPalette p = qApp->palette();
        p.setColor(QPalette::Text, Qt::white);
        qApp->setPalette(p);
        qApp->setStyleSheet(stylesheet);
    }
}

void Configuration::loadMidnightStylesheet()
{
    /* Load Qt Theme */
    QFile f(":midnight/style.css");
    if (!f.exists()) {
        qWarning() << "Can't find Midnight theme stylesheet.";
    } else {
        f.open(QFile::ReadOnly | QFile::Text);
        QTextStream ts(&f);
        QString stylesheet = ts.readAll();

        QPalette p = qApp->palette();
        p.setColor(QPalette::Text, Qt::white);
        qApp->setPalette(p);

        qApp->setStyleSheet(stylesheet);
    }
}

const QFont Configuration::getBaseFont() const
{
    QFont font = s.value("font", QFont("Inconsolata", 11)).value<QFont>();
    return font;
}

const QFont Configuration::getFont() const
{
    QFont font = getBaseFont();
    font.setPointSizeF(font.pointSizeF() * getZoomFactor());
    return font;
}

void Configuration::setFont(const QFont &font)
{
    s.setValue("font", font);
    emit fontsUpdated();
}

void Configuration::refreshFont()
{
    emit fontsUpdated();
}

qreal Configuration::getZoomFactor() const
{
    qreal fontZoom = s.value("zoomFactor", 1.0).value<qreal>();
    return qMax(fontZoom, 0.1);
}

void Configuration::setZoomFactor(qreal zoom)
{
    s.setValue("zoomFactor", qMax(zoom, 0.1));
    emit fontsUpdated();
}

QString Configuration::getLastThemeOf(const CutterInterfaceTheme &currInterfaceTheme) const
{
    return s.value("lastThemeOf." + currInterfaceTheme.name, Config()->getColorTheme()).toString();
}

void Configuration::setInterfaceTheme(int theme)
{
    if (theme >= cutterInterfaceThemesList().size() || theme < 0) {
        theme = 0;
    }
    s.setValue("ColorPalette", theme);

    CutterInterfaceTheme interfaceTheme = cutterInterfaceThemesList()[theme];

    if (interfaceTheme.name == "Native") {
        loadNativeStylesheet();
    } else if (interfaceTheme.name == "Dark") {
        loadDarkStylesheet();
    } else if (interfaceTheme.name == "Midnight") {
        loadMidnightStylesheet();
    } else if (interfaceTheme.name == "Light") {
        loadLightStylesheet();
    } else {
        loadNativeStylesheet();
    }

    for (auto it = cutterOptionColors.cbegin(); it != cutterOptionColors.cend(); it++) {
        setColor(it.key(), it.value()[interfaceTheme.flag]);
    }

    adjustColorThemeDarkness();

    emit interfaceThemeChanged();
    emit colorsUpdated();
#ifdef CUTTER_ENABLE_KSYNTAXHIGHLIGHTING
    emit kSyntaxHighlightingThemeChanged();
#endif
}

const CutterInterfaceTheme *Configuration::getCurrentTheme()
{
    int i = getInterfaceTheme();
    if (i < 0 || i >= cutterInterfaceThemesList().size()) {
        i = 0;
        setInterfaceTheme(i);
    }
    return &cutterInterfaceThemesList()[i];
}

#ifdef CUTTER_ENABLE_KSYNTAXHIGHLIGHTING
KSyntaxHighlighting::Repository *Configuration::getKSyntaxHighlightingRepository()
{
    return kSyntaxHighlightingRepository;
}

KSyntaxHighlighting::Theme Configuration::getKSyntaxHighlightingTheme()
{
    auto repo = getKSyntaxHighlightingRepository();
    if (!repo) {
        return KSyntaxHighlighting::Theme();
    }
    return repo->defaultTheme(getCurrentTheme()->flag & DarkFlag
                                      ? KSyntaxHighlighting::Repository::DefaultTheme::DarkTheme
                                      : KSyntaxHighlighting::Repository::DefaultTheme::LightTheme);
}
#endif

QSyntaxHighlighter *Configuration::createSyntaxHighlighter(QTextDocument *document)
{
#ifdef CUTTER_ENABLE_KSYNTAXHIGHLIGHTING
    auto syntaxHighlighter = new SyntaxHighlighter(document);
    auto repo = getKSyntaxHighlightingRepository();
    if (repo) {
        syntaxHighlighter->setDefinition(repo->definitionForName("C"));
    }
    return syntaxHighlighter;
#else
    return new FallbackSyntaxHighlighter(document);
#endif
}

QString Configuration::getLogoFile()
{
    return windowColorIsDark() ? QString(":/img/cutter_white_plain.svg")
                               : QString(":/img/cutter_plain.svg");
}

/**
 * @brief Configuration::setColor sets the local Cutter configuration color
 * @param name Color Name
 * @param color The color you want to set
 */
void Configuration::setColor(const QString &name, const QColor &color)
{
    s.setValue("colors." + name, color);
}

void Configuration::setLastThemeOf(const CutterInterfaceTheme &currInterfaceTheme,
                                   const QString &theme)
{
    s.setValue("lastThemeOf." + currInterfaceTheme.name, theme);
}

const QColor Configuration::getColor(const QString &name) const
{
    if (s.contains("colors." + name)) {
        return s.value("colors." + name).value<QColor>();
    } else {
        return s.value("colors.other").value<QColor>();
    }
}

void Configuration::setColorTheme(const QString &theme)
{
    if (theme == "default") {
        Core()->cmdRaw("ecd");
        s.setValue("theme", "default");
    } else {
        Core()->cmdRaw(QStringLiteral("eco %1").arg(theme));
        s.setValue("theme", theme);
    }

    QJsonObject colorTheme = ThemeWorker().getTheme(theme).object();
    for (auto it = colorTheme.constBegin(); it != colorTheme.constEnd(); it++) {
        QJsonArray rgb = it.value().toArray();
        if (rgb.size() != 4) {
            continue;
        }
        setColor(it.key(), QColor(rgb[0].toInt(), rgb[1].toInt(), rgb[2].toInt(), rgb[3].toInt()));
    }

    emit colorsUpdated();
}

void Configuration::adjustColorThemeDarkness()
{
    bool windowIsDark = windowColorIsDark();
    int windowDarkness = windowIsDark ? DarkFlag : LightFlag;
    int currentColorThemeDarkness = colorThemeDarkness(getColorTheme());

    if ((currentColorThemeDarkness & windowDarkness) == 0) {
        setColorTheme(windowIsDark ? DEFAULT_DARK_COLOR_THEME : DEFAULT_LIGHT_COLOR_THEME);
    }
}

int Configuration::colorThemeDarkness(const QString &colorTheme) const
{
    auto flags = relevantThemes.find(colorTheme);
    if (flags != relevantThemes.end()) {
        return static_cast<int>(*flags);
    }
    return DarkFlag | LightFlag;
}

void Configuration::resetToDefaultAsmOptions()
{
    for (auto it = asmOptions.cbegin(); it != asmOptions.cend(); it++) {
        setConfig(it.key(), it.value());
    }
}

void Configuration::applySavedAsmOptions()
{
    for (auto it = asmOptions.cbegin(); it != asmOptions.cend(); it++) {
        Core()->setConfig(it.key(), s.value(it.key(), it.value()));
    }
}

const QList<CutterInterfaceTheme> &Configuration::cutterInterfaceThemesList()
{
    static const QList<CutterInterfaceTheme> list = {
        { "Native", Configuration::nativeWindowIsDark() ? DarkFlag : LightFlag },
        { "Dark", DarkFlag },
        { "Midnight", DarkFlag },
        { "Light", LightFlag }
    };
    return list;
}

QVariant Configuration::getConfigVar(const QString &key)
{
    QHash<QString, QVariant>::const_iterator it = asmOptions.find(key);
    if (it != asmOptions.end()) {
        switch (it.value().type()) {
        case QVariant::Type::Bool:
            return Core()->getConfigb(key);
        case QVariant::Type::Int:
            return Core()->getConfigi(key);
        default:
            return Core()->getConfig(key);
        }
    }
    return QVariant();
}

bool Configuration::getConfigBool(const QString &key)
{
    return getConfigVar(key).toBool();
}

int Configuration::getConfigInt(const QString &key)
{
    return getConfigVar(key).toInt();
}

QString Configuration::getConfigString(const QString &key)
{
    return getConfigVar(key).toString();
}

/**
 * @brief Configuration::setConfig
 * Set Rizin configuration value (e.g. "asm.lines")
 * @param key
 * @param value
 */
void Configuration::setConfig(const QString &key, const QVariant &value)
{
    if (asmOptions.contains(key)) {
        s.setValue(key, value);
    }

    Core()->setConfig(key, value);
}

/**
 * @brief this function will gather and return available translation for Cutter
 * @return a list of all available translations
 */
QStringList Configuration::getAvailableTranslations()
{
    const auto &trDirs = Cutter::getTranslationsDirectories();

    QSet<QString> fileNamesSet;
    for (const auto &trDir : trDirs) {
        QDir dir(trDir);
        if (!dir.exists()) {
            continue;
        }
        const QStringList &currTrFileNames =
                dir.entryList(QStringList("cutter_*.qm"), QDir::Files, QDir::Name);
        for (const auto &trFile : currTrFileNames) {
            fileNamesSet << trFile;
        }
    }

    QStringList fileNames = fileNamesSet.values();
    std::sort(fileNames.begin(), fileNames.end());
    QStringList languages;
    QString currLanguageName;
    auto allLocales =
            QLocale::matchingLocales(QLocale::AnyLanguage, QLocale::AnyScript, QLocale::AnyCountry);

    for (auto i : fileNames) {
        QString localeName = i.mid(sizeof("cutter_") - 1, 2); // TODO:#2321 don't asume 2 characters
        // language code is sometimes 3 characters, and there could also be language_COUNTRY. Qt supports that.
        QLocale locale(localeName);
        if (locale.language() != QLocale::C) {
            currLanguageName = locale.nativeLanguageName();
            if (currLanguageName.isEmpty()) { // Qt doesn't have native language name for some languages
                currLanguageName = QLocale::languageToString(locale.language());
            }
            if (!currLanguageName.isEmpty()) {
                languages << currLanguageName;
            }
        }
    }
    return languages << QLatin1String("English");
}

/**
 * @brief check if this is the first time Cutter's is executed on this computer
 * @return true if this is first execution; otherwise returns false.
 */
bool Configuration::isFirstExecution()
{
    // check if a variable named firstExecution existed in the configuration
    if (s.contains("firstExecution")) {
        return false;
    } else {
        s.setValue("firstExecution", false);
        return true;
    }
}

QString Configuration::getSelectedDecompiler()
{
    return s.value("selectedDecompiler").toString();
}

void Configuration::setSelectedDecompiler(const QString &id)
{
    s.setValue("selectedDecompiler", id);
}

bool Configuration::getDecompilerAutoRefreshEnabled()
{
    return s.value("decompilerAutoRefresh", true).toBool();
}

void Configuration::setDecompilerAutoRefreshEnabled(bool enabled)
{
    s.setValue("decompilerAutoRefresh", enabled);
}

void Configuration::enableDecompilerAnnotationHighlighter(bool useDecompilerHighlighter)
{
    s.setValue("decompilerAnnotationHighlighter", useDecompilerHighlighter);
    emit colorsUpdated();
}

bool Configuration::isDecompilerAnnotationHighlighterEnabled()
{
    return s.value("decompilerAnnotationHighlighter", true).value<bool>();
}

bool Configuration::getBitmapTransparentState()
{
    return s.value("bitmapGraphExportTransparency", false).value<bool>();
}

double Configuration::getBitmapExportScaleFactor()
{
    return s.value("bitmapGraphExportScale", 1.0).value<double>();
}

void Configuration::setBitmapTransparentState(bool inputValueGraph)
{
    s.setValue("bitmapGraphExportTransparency", inputValueGraph);
}

void Configuration::setBitmapExportScaleFactor(double inputValueGraph)
{
    s.setValue("bitmapGraphExportScale", inputValueGraph);
}

void Configuration::setGraphSpacing(QPoint blockSpacing, QPoint edgeSpacing)
{
    s.setValue("graph.blockSpacing", blockSpacing);
    s.setValue("graph.edgeSpacing", edgeSpacing);
}

QPoint Configuration::getGraphBlockSpacing()
{
    return s.value("graph.blockSpacing", QPoint(20, 40)).value<QPoint>();
}

QPoint Configuration::getGraphEdgeSpacing()
{
    return s.value("graph.edgeSpacing", QPoint(10, 10)).value<QPoint>();
}

void Configuration::setOutputRedirectionEnabled(bool enabled)
{
    this->outputRedirectEnabled = enabled;
}

bool Configuration::getOutputRedirectionEnabled() const
{
    return outputRedirectEnabled;
}

bool Configuration::getGraphBlockEntryOffset()
{
    return s.value("graphBlockEntryOffset", true).value<bool>();
}

void Configuration::setGraphBlockEntryOffset(bool enabled)
{
    s.setValue("graphBlockEntryOffset", enabled);
}

QStringList Configuration::getRecentFiles() const
{
    return s.value("recentFileList").toStringList();
}

void Configuration::setRecentFiles(const QStringList &list)
{
    s.setValue("recentFileList", list);
}

QStringList Configuration::getRecentProjects() const
{
    return s.value("recentProjectsList").toStringList();
}

void Configuration::setRecentProjects(const QStringList &list)
{
    s.setValue("recentProjectsList", list);
}

void Configuration::addRecentProject(QString file)
{
    QStringList files = getRecentProjects();
    files.removeAll(file);
    files.prepend(file);
    setRecentProjects(files);
}
