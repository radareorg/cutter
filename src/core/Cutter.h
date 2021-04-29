#ifndef CUTTER_H
#define CUTTER_H

#include "core/CutterCommon.h"
#include "core/CutterDescriptions.h"
#include "common/BasicInstructionHighlighter.h"

#include <QMap>
#include <QMenu>
#include <QDebug>
#include <QObject>
#include <QStringList>
#include <QMessageBox>
#include <QJsonDocument>
#include <QErrorMessage>
#include <QMutex>
#include <QDir>

class AsyncTaskManager;
class BasicInstructionHighlighter;
class CutterCore;
class Decompiler;
class RizinTask;
class RizinCmdTask;
class RizinTaskDialog;

#include "common/BasicBlockHighlighter.h"
#include "common/Helpers.h"

#include <rz_project.h>

#define Core() (CutterCore::instance())

class RzCoreLocked;

class CUTTER_EXPORT CutterCore : public QObject
{
    Q_OBJECT

    friend class RzCoreLocked;
    friend class RizinTask;

public:
    explicit CutterCore(QObject *parent = nullptr);
    ~CutterCore();
    static CutterCore *instance();

    void initialize(bool loadPlugins = true);
    void loadCutterRC();
    void loadDefaultCutterRC();
    QDir getCutterRCDefaultDirectory() const;

    AsyncTaskManager *getAsyncTaskManager() { return asyncTaskManager; }

    RVA getOffset() const { return core_->offset; }

    /* Core functions (commands) */
    static QString sanitizeStringForCommand(QString s);
    /**
     * @brief send a command to Rizin
     * @param str the command you want to execute
     * @return command output
     * @note if you want to seek to an address, you should use CutterCore::seek.
     */
    QString cmd(const char *str);
    QString cmd(const QString &str) { return cmd(str.toUtf8().constData()); }
    /**
     * @brief send a command to Rizin asynchronously
     * @param str the command you want to execute
     * @param task a shared pointer that will be returned with the Rizin command task
     * @note connect to the &RizinTask::finished signal to add your own logic once
     *       the command is finished. Use task->getResult()/getResultJson() for the
     *       return value.
     *       Once you have setup connections you can start the task with task->startTask()
     *       If you want to seek to an address, you should use CutterCore::seek.
     */
    bool asyncCmd(const char *str, QSharedPointer<RizinCmdTask> &task);
    bool asyncCmd(const QString &str, QSharedPointer<RizinCmdTask> &task)
    {
        return asyncCmd(str.toUtf8().constData(), task);
    }

    /**
     * @brief Execute a Rizin command \a cmd.  By nature, the API
     * is executing raw commands, and thus ignores multiple commands and overcome command
     * injections.
     * @param cmd - a raw command to execute. Passing multiple commands (e.g "px 5; pd 7 && pdf")
     * will result in them treated as arguments to first command.
     * @return the output of the command
     */
    QString cmdRaw(const char *cmd);

    /**
     * @brief a wrapper around cmdRaw(const char *cmd,).
     */
    QString cmdRaw(const QString &cmd) { return cmdRaw(cmd.toUtf8().constData()); };

    /**
     * @brief Execute a Rizin command \a cmd at \a address. The function will preform a silent seek
     * to the address without triggering the seekChanged event nor adding new entries to the seek
     * history. By nature, the API is executing a single command without going through Rizin shell,
     * and thus ignores multiple commands and tries to overcome command injections.
     * @param cmd - a raw command to execute. If multiple commands will be passed (e.g "px 5; pd 7
     * && pdf") then only the first command will be executed.
     * @param address - an address to which Cutter will temporarily seek.
     * @return the output of the command
     */
    QString cmdRawAt(const char *cmd, RVA address);

    /**
     * @brief a wrapper around cmdRawAt(const char *cmd, RVA address).
     */
    QString cmdRawAt(const QString &str, RVA address)
    {
        return cmdRawAt(str.toUtf8().constData(), address);
    }

    QJsonDocument cmdj(const char *str);
    QJsonDocument cmdj(const QString &str) { return cmdj(str.toUtf8().constData()); }
    QJsonDocument cmdjAt(const char *str, RVA address);
    QStringList cmdList(const char *str)
    {
        return cmd(str).split(QLatin1Char('\n'), CUTTER_QT_SKIP_EMPTY_PARTS);
    }
    QStringList cmdList(const QString &str) { return cmdList(str.toUtf8().constData()); }
    QString cmdTask(const QString &str);
    QJsonDocument cmdjTask(const QString &str);
    /**
     * @brief send a command to Rizin and check for ESIL errors
     * @param command the command you want to execute
     * @note If you want to seek to an address, you should use CutterCore::seek.
     */
    void cmdEsil(const char *command);
    void cmdEsil(const QString &command) { cmdEsil(command.toUtf8().constData()); }
    /**
     * @brief send a command to Rizin and check for ESIL errors
     * @param command the command you want to execute
     * @param task a shared pointer that will be returned with the Rizin command task
     * @note connect to the &RizinTask::finished signal to add your own logic once
     *       the command is finished. Use task->getResult()/getResultJson() for the
     *       return value.
     *       Once you have setup connections you can start the task with task->startTask()
     *       If you want to seek to an address, you should use CutterCore::seek.
     */
    bool asyncCmdEsil(const char *command, QSharedPointer<RizinCmdTask> &task);
    bool asyncCmdEsil(const QString &command, QSharedPointer<RizinCmdTask> &task)
    {
        return asyncCmdEsil(command.toUtf8().constData(), task);
    }
    QString getRizinVersionReadable();
    QString getVersionInformation();

    QJsonDocument parseJson(const char *res, const char *cmd = nullptr);
    QJsonDocument parseJson(const char *res, const QString &cmd = QString())
    {
        return parseJson(res, cmd.isNull() ? nullptr : cmd.toLocal8Bit().constData());
    }

    QStringList autocomplete(const QString &cmd, RzLinePromptType promptType, size_t limit = 4096);

    /* Functions methods */
    void renameFunction(const RVA offset, const QString &newName);
    void delFunction(RVA addr);
    void renameFlag(QString old_name, QString new_name);
    /**
     * @brief Renames the specified local variable in the function specified by the
     * address given.
     * @param newName Specifies the name to which the current name of the variable
     * should be renamed.
     * @param oldName Specifies the current name of the function variable.
     * @param functionAddress Specifies the exact address of the function.
     */
    void renameFunctionVariable(QString newName, QString oldName, RVA functionAddress);

    /**
     * @param addr
     * @return a function that contains addr or nullptr
     */
    RzAnalysisFunction *functionIn(ut64 addr);

    /**
     * @param addr
     * @return the function that has its entrypoint at addr or nullptr
     */
    RzAnalysisFunction *functionAt(ut64 addr);

    RVA getFunctionStart(RVA addr);
    RVA getFunctionEnd(RVA addr);
    RVA getLastFunctionInstruction(RVA addr);
    QString cmdFunctionAt(QString addr);
    QString cmdFunctionAt(RVA addr);
    QString createFunctionAt(RVA addr);
    QString createFunctionAt(RVA addr, QString name);
    QStringList getDisassemblyPreview(RVA address, int num_of_lines);

    /* Flags */
    void delFlag(RVA addr);
    void delFlag(const QString &name);
    void addFlag(RVA offset, QString name, RVA size);
    QString listFlagsAsStringAt(RVA addr);
    /**
     * @brief Get nearest flag at or before offset.
     * @param offset search position
     * @param flagOffsetOut address of returned flag
     * @return flag name
     */
    QString nearestFlag(RVA offset, RVA *flagOffsetOut);
    void triggerFlagsChanged();

    /* Edition functions */
    QString getInstructionBytes(RVA addr);
    QString getInstructionOpcode(RVA addr);
    void editInstruction(RVA addr, const QString &inst);
    void nopInstruction(RVA addr);
    void jmpReverse(RVA addr);
    void editBytes(RVA addr, const QString &inst);
    void editBytesEndian(RVA addr, const QString &bytes);

    /* Code/Data */
    void setToCode(RVA addr);
    enum class StringTypeFormats { None, ASCII_LATIN1, UTF8 };
    /**
     * @brief Adds string at address
     * That function calls the 'Cs' command
     * \param addr The address of the array where the string will be applied
     * \param size The size of string
     * \param type The type of string
     */
    void setAsString(RVA addr, int size = 0, StringTypeFormats type = StringTypeFormats::None);
    /**
     * @brief Removes string at address
     * That function calls the 'Cs-' command
     * \param addr The address of the array where the string will be applied
     */
    void removeString(RVA addr);
    /**
     * @brief Gets string at address
     * That function calls the 'ps' command
     * \param addr The address of the first byte of the array
     * @return string at requested address
     */
    QString getString(RVA addr);
    void setToData(RVA addr, int size, int repeat = 1);
    int sizeofDataMeta(RVA addr);

    /* Comments */
    void setComment(RVA addr, const QString &cmt);
    void delComment(RVA addr);
    QString getCommentAt(RVA addr);
    void setImmediateBase(const QString &rzBaseName, RVA offset = RVA_INVALID);
    void setCurrentBits(int bits, RVA offset = RVA_INVALID);

    /**
     * @brief Changes immediate displacement to structure offset
     * This function makes use of the "aht" command of Rizin to apply structure
     * offset to the immediate displacement used in the given instruction
     * \param structureOffset The name of struct which will be applied
     * \param offset The address of the instruction where the struct will be applied
     */
    void applyStructureOffset(const QString &structureOffset, RVA offset = RVA_INVALID);

    /* Classes */
    QList<QString> getAllAnalClasses(bool sorted);
    QList<AnalMethodDescription> getAnalClassMethods(const QString &cls);
    QList<AnalBaseClassDescription> getAnalClassBaseClasses(const QString &cls);
    QList<AnalVTableDescription> getAnalClassVTables(const QString &cls);
    void createNewClass(const QString &cls);
    void renameClass(const QString &oldName, const QString &newName);
    void deleteClass(const QString &cls);
    bool getAnalMethod(const QString &cls, const QString &meth, AnalMethodDescription *desc);
    void renameAnalMethod(const QString &className, const QString &oldMethodName,
                          const QString &newMethodName);
    void setAnalMethod(const QString &cls, const AnalMethodDescription &meth);

    /* File related methods */
    bool loadFile(QString path, ut64 baddr = 0LL, ut64 mapaddr = 0LL, int perms = RZ_PERM_R,
                  int va = 0, bool loadbin = false, const QString &forceBinPlugin = QString());
    bool tryFile(QString path, bool rw);
    bool mapFile(QString path, RVA mapaddr);
    void loadScript(const QString &scriptname);
    QJsonArray getOpenedFiles();

    /* Seek functions */
    void seek(QString thing);
    void seek(ut64 offset);
    void seekSilent(ut64 offset);
    void seekSilent(QString thing) { seekSilent(math(thing)); }
    void seekPrev();
    void seekNext();
    void updateSeek();
    /**
     * @brief Raise a memory widget showing current offset, prefer last active
     * memory widget.
     */
    void showMemoryWidget();
    /**
     * @brief Seek to \p offset and raise a memory widget showing it.
     * @param offset
     */
    void seekAndShow(ut64 offset);
    /**
     * @brief \see CutterCore::show(ut64)
     * @param thing - addressable expression
     */
    void seekAndShow(QString thing);
    RVA getOffset();
    RVA prevOpAddr(RVA startAddr, int count);
    RVA nextOpAddr(RVA startAddr, int count);

    /* Math functions */
    ut64 math(const QString &expr);
    ut64 num(const QString &expr);
    QString itoa(ut64 num, int rdx = 16);

    /* Config functions */
    void setConfig(const char *k, const char *v);
    void setConfig(const QString &k, const char *v);
    void setConfig(const char *k, const QString &v);
    void setConfig(const QString &k, const QString &v) { setConfig(k.toUtf8().constData(), v); }
    void setConfig(const char *k, int v);
    void setConfig(const QString &k, int v) { setConfig(k.toUtf8().constData(), v); }
    void setConfig(const char *k, bool v);
    void setConfig(const QString &k, bool v) { setConfig(k.toUtf8().constData(), v); }
    void setConfig(const char *k, const QVariant &v);
    void setConfig(const QString &k, const QVariant &v) { setConfig(k.toUtf8().constData(), v); }
    int getConfigi(const char *k);
    int getConfigi(const QString &k) { return getConfigi(k.toUtf8().constData()); }
    ut64 getConfigut64(const char *k);
    ut64 getConfigut64(const QString &k) { return getConfigut64(k.toUtf8().constData()); }
    bool getConfigb(const char *k);
    bool getConfigb(const QString &k) { return getConfigb(k.toUtf8().constData()); }
    QString getConfig(const char *k);
    QString getConfig(const QString &k) { return getConfig(k.toUtf8().constData()); }
    QString getConfigDescription(const char *k);
    QList<QString> getColorThemes();

    /* Assembly\Hexdump related methods */
    QByteArray assemble(const QString &code);
    QString disassemble(const QByteArray &data);
    QString disassembleSingleInstruction(RVA addr);
    QList<DisassemblyLine> disassembleLines(RVA offset, int lines);

    static QByteArray hexStringToBytes(const QString &hex);
    static QString bytesToHexString(const QByteArray &bytes);
    enum class HexdumpFormats { Normal, Half, Word, Quad, Signed, Octal };
    QString hexdump(RVA offset, int size, HexdumpFormats format);
    QString getHexdumpPreview(RVA offset, int size);

    void setCPU(QString arch, QString cpu, int bits);
    void setEndianness(bool big);

    /* SDB */
    QList<QString> sdbList(QString path);
    QList<QString> sdbListKeys(QString path);
    QString sdbGet(QString path, QString key);
    bool sdbSet(QString path, QString key, QString val);

    /* Debug */
    QJsonDocument getRegistersInfo();
    QJsonDocument getRegisterValues();
    QString getRegisterName(QString registerRole);
    RVA getProgramCounterValue();
    void setRegister(QString regName, QString regValue);
    void setCurrentDebugThread(int tid);
    /**
     * @brief Attach to a given pid from a debug session
     */
    void setCurrentDebugProcess(int pid);
    /**
     * @brief Returns a list of stack address and their telescoped references
     * @param size number of bytes to scan
     * @param depth telescoping depth
     */
    QList<QJsonObject> getStack(int size = 0x100, int depth = 6);
    /**
     * @brief Recursively dereferences pointers starting at the specified address
     *        up to a given depth
     * @param addr telescoping addr
     * @param depth telescoping depth
     */
    QJsonObject getAddrRefs(RVA addr, int depth);
    /**
     * @brief return a RefDescription with a formatted ref string and configured colors
     * @param ref the "ref" JSON node from getAddrRefs
     */
    RefDescription formatRefDesc(QJsonObject ref);
    /**
     * @brief Get a list of a given process's threads
     * @param pid The pid of the process, -1 for the currently debugged process
     * @return JSON object result of dptj
     */
    QJsonDocument getProcessThreads(int pid);
    /**
     * @brief Get a list of a given process's child processes
     * @param pid The pid of the process, -1 for the currently debugged process
     * @return JSON object result of dptj
     */
    QJsonDocument getChildProcesses(int pid);
    QJsonDocument getBacktrace();
    void startDebug();
    void startEmulation();
    /**
     * @brief attach to a remote debugger
     * @param uri remote debugger uri
     * @note attachedRemote(bool) signals the result
     */
    void attachRemote(const QString &uri);
    void attachDebug(int pid);
    void stopDebug();
    void suspendDebug();
    void syncAndSeekProgramCounter();
    void continueDebug();
    void continueBackDebug();
    void continueUntilCall();
    void continueUntilSyscall();
    void continueUntilDebug(QString offset);
    void stepDebug();
    void stepOverDebug();
    void stepOutDebug();
    void stepBackDebug();

    void startTraceSession();
    void stopTraceSession();

    void addBreakpoint(const BreakpointDescription &config);
    void updateBreakpoint(int index, const BreakpointDescription &config);
    void toggleBreakpoint(RVA addr);
    void delBreakpoint(RVA addr);
    void delAllBreakpoints();
    void enableBreakpoint(RVA addr);
    void disableBreakpoint(RVA addr);
    /**
     * @brief Enable or disable breakpoint tracing.
     * @param index - breakpoint index to modify
     * @param enabled - true if tracing should be enabled
     */
    void setBreakpointTrace(int index, bool enabled);
    int breakpointIndexAt(RVA addr);
    BreakpointDescription getBreakpointAt(RVA addr);

    bool isBreakpoint(const QList<RVA> &breakpoints, RVA addr);
    QList<RVA> getBreakpointsAddresses();

    /**
     * @brief Get all breakpoinst that are belong to a functions at this address
     */
    QList<RVA> getBreakpointsInFunction(RVA funcAddr);
    QString getActiveDebugPlugin();
    QStringList getDebugPlugins();
    void setDebugPlugin(QString plugin);
    bool isDebugTaskInProgress();
    /**
     * @brief Check if we can use output/input redirection with the currently debugged process
     */
    bool isRedirectableDebugee();
    bool currentlyDebugging = false;
    bool currentlyEmulating = false;
    bool currentlyTracing = false;
    int currentlyAttachedToPID = -1;
    QString currentlyOpenFile;

    /* Decompilers */
    QList<Decompiler *> getDecompilers();
    Decompiler *getDecompilerById(const QString &id);

    /**
     * Register a new decompiler
     *
     * The decompiler must have a unique id, otherwise this method will fail.
     * The decompiler's parent will be set to this CutterCore instance, so it will automatically be
     * freed later.
     *
     * @return whether the decompiler was registered successfully
     */
    bool registerDecompiler(Decompiler *decompiler);

    RVA getOffsetJump(RVA addr);
    QJsonDocument getSignatureInfo();
    QJsonDocument getFileVersionInfo();
    QStringList getStats();
    void setGraphEmpty(bool empty);
    bool isGraphEmpty();

    void getOpcodes();
    QList<QString> opcodes;
    QList<QString> regs;
    void setSettings();

    void loadPDB(const QString &file);

    QByteArray ioRead(RVA addr, int len);

    QList<RVA> getSeekHistory();

    /* Plugins */
    QStringList getAsmPluginNames();
    QStringList getAnalPluginNames();

    /* Widgets */
    QList<RzBinPluginDescription> getRBinPluginDescriptions(const QString &type = QString());
    QList<RzIOPluginDescription> getRIOPluginDescriptions();
    QList<RzCorePluginDescription> getRCorePluginDescriptions();
    QList<RzAsmPluginDescription> getRAsmPluginDescriptions();
    QList<FunctionDescription> getAllFunctions();
    QList<ImportDescription> getAllImports();
    QList<ExportDescription> getAllExports();
    QList<SymbolDescription> getAllSymbols();
    QList<HeaderDescription> getAllHeaders();
    QList<ZignatureDescription> getAllZignatures();
    QList<CommentDescription> getAllComments(const QString &filterType);
    QList<RelocDescription> getAllRelocs();
    QList<StringDescription> getAllStrings();
    QList<FlagspaceDescription> getAllFlagspaces();
    QList<FlagDescription> getAllFlags(QString flagspace = QString());
    QList<SectionDescription> getAllSections();
    QList<SegmentDescription> getAllSegments();
    QList<EntrypointDescription> getAllEntrypoint();
    QList<BinClassDescription> getAllClassesFromBin();
    QList<BinClassDescription> getAllClassesFromFlags();
    QList<ResourcesDescription> getAllResources();
    QList<VTableDescription> getAllVTables();

    /**
     * @return all loaded types
     */
    QList<TypeDescription> getAllTypes();

    /**
     * @return all loaded primitive types
     */
    QList<TypeDescription> getAllPrimitiveTypes();

    /**
     * @return all loaded unions
     */
    QList<TypeDescription> getAllUnions();

    /**
     * @return all loaded structs
     */
    QList<TypeDescription> getAllStructs();

    /**
     * @return all loaded enums
     */
    QList<TypeDescription> getAllEnums();

    /**
     * @return all loaded typedefs
     */
    QList<TypeDescription> getAllTypedefs();

    /**
     * @brief Fetching the C representation of a given Type
     * @param name - the name or the type of the given Type / Struct
     * @param category - the category of the given Type (Struct, Union, Enum, ...)
     * @return The type decleration as C output
     */
    QString getTypeAsC(QString name, QString category);

    /**
     * @brief Adds new types
     * It first uses the rz_parse_c_string() function from Rizin API to parse the
     * supplied C file (in the form of a string). If there were errors, they are displayed.
     * If there were no errors, it uses sdb_query_lines() function from Rizin API
     * to save the parsed types returned by rz_parse_c_string()
     * \param str Contains the definition of the data types
     * \return returns an empty QString if there was no error, else returns the error
     */
    QString addTypes(const char *str);
    QString addTypes(const QString &str) { return addTypes(str.toUtf8().constData()); }

    /**
     * @brief Checks if the given address is mapped to a region
     * @param addr The address to be checked
     * @return true if addr is mapped, false otherwise
     */
    bool isAddressMapped(RVA addr);

    QList<MemoryMapDescription> getMemoryMap();
    QList<SearchDescription> getAllSearch(QString searchFor, QString space, QString in);
    BlockStatistics getBlockStatistics(unsigned int blocksCount);
    QList<BreakpointDescription> getBreakpoints();
    QList<ProcessDescription> getAllProcesses();
    /**
     * @brief returns a list of reg values and their telescoped references
     * @param depth telescoping depth
     */
    QList<QJsonObject> getRegisterRefs(int depth = 6);
    QVector<RegisterRefValueDescription> getRegisterRefValues();
    QList<VariableDescription> getVariables(RVA at);
    /**
     * @brief Fetches all the writes or reads to the specified local variable 'variableName'
     * in the function in which the specified offset is a part of.
     * @param variableName Name of the local variable.
     * @param findWrites If this is true, then locations at which modification happen to the
     * specified local variable is fetched. Else, the locations at which the local is variable is
     * read is fetched.
     * @param offset An offset in the function in which the specified local variable exist.
     * @return A list of XrefDescriptions that contains details of all the writes or reads that
     * happen to the variable 'variableName'.
     */
    QList<XrefDescription> getXRefsForVariable(QString variableName, bool findWrites, RVA offset);
    QList<XrefDescription> getXRefs(RVA addr, bool to, bool whole_function,
                                    const QString &filterType = QString());

    QList<StringDescription> parseStringsJson(const QJsonDocument &doc);

    void handleREvent(int type, void *data);

    /* Signals related */
    void triggerVarsChanged();
    void triggerFunctionRenamed(const RVA offset, const QString &newName);
    void triggerRefreshAll();
    void triggerAsmOptionsChanged();
    void triggerGraphOptionsChanged();

    void message(const QString &msg, bool debug = false);

    QStringList getSectionList();

    RzCoreLocked core();

    static QString ansiEscapeToHtml(const QString &text);
    BasicBlockHighlighter *getBBHighlighter();
    BasicInstructionHighlighter *getBIHighlighter();

    /**
     * @brief Enable or dsiable Cache mode. Cache mode is used to imagine writing to the opened file
     * without committing the changes to the disk.
     * @param enabled
     */
    void setIOCache(bool enabled);

    /**
     * @brief Check if Cache mode is enabled.
     * @return true if Cache is enabled, otherwise return false.
     */
    bool isIOCacheEnabled() const;

    /**
     * @brief Commit write cache to the file on disk.
     */
    void commitWriteCache();

    /**
     * @brief Enable or disable Write mode. When the file is opened in write mode, any changes to it
     * will be immediately committed to the file on disk, thus modify the file. This function wrap
     * Rizin function which re-open the file with the desired permissions.
     * @param enabled
     */
    void setWriteMode(bool enabled);
    /**
     * @brief Check if the file is opened in write mode.
     * @return true if write mode is enabled, otherwise return false.
     */
    bool isWriteModeEnabled();

signals:
    void refreshAll();

    void functionRenamed(const RVA offset, const QString &new_name);
    void varsChanged();
    void functionsChanged();
    void flagsChanged();
    void commentsChanged(RVA addr);
    void registersChanged();
    void instructionChanged(RVA offset);
    void breakpointsChanged(RVA offset);
    void refreshCodeViews();
    void stackChanged();
    /**
     * @brief update all the widgets that are affected by rebasing in debug mode
     */
    void codeRebased();

    void switchedThread();
    void switchedProcess();

    void classNew(const QString &cls);
    void classDeleted(const QString &cls);
    void classRenamed(const QString &oldName, const QString &newName);
    void classAttrsChanged(const QString &cls);

    /**
     * @brief end of current debug event received
     */
    void debugProcessFinished(int pid);

    void attachedRemote(bool successfully);

    void ioCacheChanged(bool newval);
    void writeModeChanged(bool newval);
    void ioModeChanged();

    /**
     * emitted when debugTask started or finished running
     */
    void debugTaskStateChanged();

    /**
     * emitted when config regarding disassembly display changes
     */
    void asmOptionsChanged();

    /**
     * emitted when config regarding graph display changes
     */
    void graphOptionsChanged();

    /**
     * @brief seekChanged is emitted each time Rizin's seek value is modified
     * @param offset
     */
    void seekChanged(RVA offset);

    void toggleDebugView();

    void newMessage(const QString &msg);
    void newDebugMessage(const QString &msg);

    void showMemoryWidgetRequested();

private:
    /**
     * Internal reference to the RzCore.
     * NEVER use this directly! Always use the CORE_LOCK(); macro and access it like core->...
     */
    RzCore *core_ = nullptr;
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    QMutex coreMutex;
#else
    QRecursiveMutex coreMutex;
#endif
    int coreLockDepth = 0;
    void *coreBed = nullptr;

    AsyncTaskManager *asyncTaskManager;
    RVA offsetPriorDebugging = RVA_INVALID;
    QErrorMessage msgBox;

    QList<Decompiler *> decompilers;

    bool emptyGraph = false;
    BasicBlockHighlighter *bbHighlighter;
    bool iocache = false;
    BasicInstructionHighlighter biHighlighter;

    QSharedPointer<RizinCmdTask> debugTask;
    RizinTaskDialog *debugTaskDialog;

    QVector<QString> getCutterRCFilePaths() const;
};

class CUTTER_EXPORT RzCoreLocked
{
    CutterCore *const core;

public:
    explicit RzCoreLocked(CutterCore *core);
    RzCoreLocked(const RzCoreLocked &) = delete;
    RzCoreLocked &operator=(const RzCoreLocked &) = delete;
    RzCoreLocked(RzCoreLocked &&);
    ~RzCoreLocked();
    operator RzCore *() const;
    RzCore *operator->() const;
};

#endif // CUTTER_H
