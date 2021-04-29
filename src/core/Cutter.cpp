#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QDir>
#include <QCoreApplication>
#include <QVector>
#include <QStringList>
#include <QStandardPaths>

#include <cassert>
#include <memory>

#include "common/TempConfig.h"
#include "common/BasicInstructionHighlighter.h"
#include "common/Configuration.h"
#include "common/AsyncTask.h"
#include "common/RizinTask.h"
#include "dialogs/RizinTaskDialog.h"
#include "common/Json.h"
#include "core/Cutter.h"
#include "Decompiler.h"

#include <rz_asm.h>
#include <rz_cmd.h>
#include <sdb.h>

Q_GLOBAL_STATIC(CutterCore, uniqueInstance)

#define RZ_JSON_KEY(name) static const QString name = QStringLiteral(#name)

namespace RJsonKey {
RZ_JSON_KEY(addr);
RZ_JSON_KEY(addrs);
RZ_JSON_KEY(addr_end);
RZ_JSON_KEY(arrow);
RZ_JSON_KEY(baddr);
RZ_JSON_KEY(bind);
RZ_JSON_KEY(blocks);
RZ_JSON_KEY(blocksize);
RZ_JSON_KEY(bytes);
RZ_JSON_KEY(calltype);
RZ_JSON_KEY(cc);
RZ_JSON_KEY(classname);
RZ_JSON_KEY(code);
RZ_JSON_KEY(comment);
RZ_JSON_KEY(comments);
RZ_JSON_KEY(cost);
RZ_JSON_KEY(data);
RZ_JSON_KEY(description);
RZ_JSON_KEY(ebbs);
RZ_JSON_KEY(edges);
RZ_JSON_KEY(enabled);
RZ_JSON_KEY(entropy);
RZ_JSON_KEY(fcn_addr);
RZ_JSON_KEY(fcn_name);
RZ_JSON_KEY(fields);
RZ_JSON_KEY(file);
RZ_JSON_KEY(flags);
RZ_JSON_KEY(flagname);
RZ_JSON_KEY(format);
RZ_JSON_KEY(from);
RZ_JSON_KEY(functions);
RZ_JSON_KEY(graph);
RZ_JSON_KEY(haddr);
RZ_JSON_KEY(hw);
RZ_JSON_KEY(in_functions);
RZ_JSON_KEY(index);
RZ_JSON_KEY(jump);
RZ_JSON_KEY(laddr);
RZ_JSON_KEY(lang);
RZ_JSON_KEY(len);
RZ_JSON_KEY(length);
RZ_JSON_KEY(license);
RZ_JSON_KEY(methods);
RZ_JSON_KEY(name);
RZ_JSON_KEY(realname);
RZ_JSON_KEY(nargs);
RZ_JSON_KEY(nbbs);
RZ_JSON_KEY(nlocals);
RZ_JSON_KEY(offset);
RZ_JSON_KEY(opcode);
RZ_JSON_KEY(opcodes);
RZ_JSON_KEY(ordinal);
RZ_JSON_KEY(libname);
RZ_JSON_KEY(outdegree);
RZ_JSON_KEY(paddr);
RZ_JSON_KEY(path);
RZ_JSON_KEY(perm);
RZ_JSON_KEY(pid);
RZ_JSON_KEY(plt);
RZ_JSON_KEY(prot);
RZ_JSON_KEY(ref);
RZ_JSON_KEY(refs);
RZ_JSON_KEY(reg);
RZ_JSON_KEY(rwx);
RZ_JSON_KEY(section);
RZ_JSON_KEY(sections);
RZ_JSON_KEY(size);
RZ_JSON_KEY(stackframe);
RZ_JSON_KEY(status);
RZ_JSON_KEY(string);
RZ_JSON_KEY(strings);
RZ_JSON_KEY(symbols);
RZ_JSON_KEY(text);
RZ_JSON_KEY(to);
RZ_JSON_KEY(trace);
RZ_JSON_KEY(type);
RZ_JSON_KEY(uid);
RZ_JSON_KEY(vaddr);
RZ_JSON_KEY(value);
RZ_JSON_KEY(vsize);
}

#undef RZ_JSON_KEY

static void updateOwnedCharPtr(char *&variable, const QString &newValue)
{
    auto data = newValue.toUtf8();
    RZ_FREE(variable)
    variable = strdup(data.data());
}

static QString fromOwnedCharPtr(char *str)
{
    QString result(str ? str : "");
    rz_mem_free(str);
    return result;
}

RzCoreLocked::RzCoreLocked(CutterCore *core) : core(core)
{
    core->coreMutex.lock();
    assert(core->coreLockDepth >= 0);
    core->coreLockDepth++;
    if (core->coreLockDepth == 1) {
        assert(core->coreBed);
        rz_cons_sleep_end(core->coreBed);
        core->coreBed = nullptr;
    }
}

RzCoreLocked::~RzCoreLocked()
{
    assert(core->coreLockDepth > 0);
    core->coreLockDepth--;
    if (core->coreLockDepth == 0) {
        core->coreBed = rz_cons_sleep_begin();
    }
    core->coreMutex.unlock();
}

RzCoreLocked::operator RzCore *() const
{
    return core->core_;
}

RzCore *RzCoreLocked::operator->() const
{
    return core->core_;
}

#define CORE_LOCK() RzCoreLocked core(this)

static void cutterREventCallback(RzEvent *, int type, void *user, void *data)
{
    auto core = reinterpret_cast<CutterCore *>(user);
    core->handleREvent(type, data);
}

CutterCore::CutterCore(QObject *parent)
    : QObject(parent)
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
      ,
      coreMutex(QMutex::Recursive)
#endif
{
}

CutterCore *CutterCore::instance()
{
    return uniqueInstance;
}

void CutterCore::initialize(bool loadPlugins)
{
    auto prefix = QDir(QCoreApplication::applicationDirPath());

#if defined(CUTTER_ENABLE_PACKAGING) && defined(Q_OS_WIN)
    auto prefixBytes = prefix.absolutePath().toUtf8();
    rz_sys_prefix(prefixBytes.constData());
#endif

    rz_cons_new(); // initialize console
    core_ = rz_core_new();
    rz_core_task_sync_begin(&core_->tasks);
    coreBed = rz_cons_sleep_begin();
    CORE_LOCK();

    rz_event_hook(core_->analysis->ev, RZ_EVENT_ALL, cutterREventCallback, this);

#if defined(APPIMAGE) || defined(MACOS_RZ_BUNDLED)
#    ifdef APPIMAGE
    // Executable is in appdir/bin
    prefix.cdUp();
    qInfo() << "Setting Rizin prefix =" << prefix.absolutePath() << " for AppImage.";
#    else // MACOS_RZ_BUNDLED
    // Executable is in Contents/MacOS, prefix is Contents/Resources/rz
    prefix.cdUp();
    prefix.cd("Resources");
    qInfo() << "Setting Rizin prefix =" << prefix.absolutePath()
            << " for macOS Application Bundle.";
    setConfig("dir.prefix", prefix.absolutePath());
#    endif

    auto pluginsDir = prefix;
    if (pluginsDir.cd("share/rizin/plugins")) {
        qInfo() << "Setting Rizin plugins dir =" << pluginsDir.absolutePath();
        setConfig("dir.plugins", pluginsDir.absolutePath());
    } else {
        qInfo() << "Rizin plugins dir under" << pluginsDir.absolutePath() << "does not exist!";
    }
#endif

    if (!loadPlugins) {
        setConfig("cfg.plugins", 0);
    }
    if (getConfigi("cfg.plugins")) {
        rz_core_loadlibs(this->core_, RZ_CORE_LOADLIBS_ALL, nullptr);
    }
    // IMPLICIT rz_bin_iobind (core_->bin, core_->io);

    // Otherwise Rizin may ask the user for input and Cutter would freeze
    setConfig("scr.interactive", false);

    // Initialize graph node highlighter
    bbHighlighter = new BasicBlockHighlighter();

    // Initialize Async tasks manager
    asyncTaskManager = new AsyncTaskManager(this);
}

CutterCore::~CutterCore()
{
    delete bbHighlighter;
    rz_cons_sleep_end(coreBed);
    rz_core_task_sync_end(&core_->tasks);
    rz_core_free(this->core_);
    rz_cons_free();
}

RzCoreLocked CutterCore::core()
{
    return RzCoreLocked(this);
}

QDir CutterCore::getCutterRCDefaultDirectory() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
}

QVector<QString> CutterCore::getCutterRCFilePaths() const
{
    QVector<QString> result;
    result.push_back(QFileInfo(QDir::home(), ".cutterrc").absoluteFilePath());
    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::AppConfigLocation);
    for (auto &location : locations) {
        result.push_back(QFileInfo(QDir(location), ".cutterrc").absoluteFilePath());
    }
    result.push_back(QFileInfo(getCutterRCDefaultDirectory(), "rc")
                             .absoluteFilePath()); // File in config editor is from this path
    return result;
}

void CutterCore::loadCutterRC()
{
    CORE_LOCK();
    const auto result = getCutterRCFilePaths();
    for (auto &cutterRCFilePath : result) {
        auto cutterRCFileInfo = QFileInfo(cutterRCFilePath);
        if (!cutterRCFileInfo.exists() || !cutterRCFileInfo.isFile()) {
            continue;
        }
        qInfo() << "Loading initialization file from " << cutterRCFilePath;
        rz_core_cmd_file(core, cutterRCFilePath.toUtf8().constData());
    }
}

void CutterCore::loadDefaultCutterRC()
{
    CORE_LOCK();
    auto cutterRCFilePath = QFileInfo(getCutterRCDefaultDirectory(), "rc").absoluteFilePath();
    const auto cutterRCFileInfo = QFileInfo(cutterRCFilePath);
    if (!cutterRCFileInfo.exists() || !cutterRCFileInfo.isFile()) {
        return;
    }
    qInfo() << "Loading initialization file from " << cutterRCFilePath;
    rz_core_cmd_file(core, cutterRCFilePath.toUtf8().constData());
}

QList<QString> CutterCore::sdbList(QString path)
{
    CORE_LOCK();
    QList<QString> list = QList<QString>();
    Sdb *root = sdb_ns_path(core->sdb, path.toUtf8().constData(), 0);
    if (root) {
        void *vsi;
        ls_iter_t *iter;
        ls_foreach(root->ns, iter, vsi)
        {
            SdbNs *nsi = (SdbNs *)vsi;
            list << nsi->name;
        }
    }
    return list;
}

using SdbListPtr = std::unique_ptr<SdbList, decltype(&ls_free)>;
static SdbListPtr makeSdbListPtr(SdbList *list)
{
    return { list, ls_free };
}

QList<QString> CutterCore::sdbListKeys(QString path)
{
    CORE_LOCK();
    QList<QString> list = QList<QString>();
    Sdb *root = sdb_ns_path(core->sdb, path.toUtf8().constData(), 0);
    if (root) {
        void *vsi;
        ls_iter_t *iter;
        SdbListPtr l = makeSdbListPtr(sdb_foreach_list(root, false));
        ls_foreach(l, iter, vsi)
        {
            SdbKv *nsi = (SdbKv *)vsi;
            list << reinterpret_cast<char *>(nsi->base.key);
        }
    }
    return list;
}

QString CutterCore::sdbGet(QString path, QString key)
{
    CORE_LOCK();
    Sdb *db = sdb_ns_path(core->sdb, path.toUtf8().constData(), 0);
    if (db) {
        const char *val = sdb_const_get(db, key.toUtf8().constData(), 0);
        if (val && *val)
            return val;
    }
    return QString();
}

bool CutterCore::sdbSet(QString path, QString key, QString val)
{
    CORE_LOCK();
    Sdb *db = sdb_ns_path(core->sdb, path.toUtf8().constData(), 1);
    if (!db)
        return false;
    return sdb_set(db, key.toUtf8().constData(), val.toUtf8().constData(), 0);
}

QString CutterCore::sanitizeStringForCommand(QString s)
{
    static const QRegularExpression regexp(";|@");
    return s.replace(regexp, QStringLiteral("_"));
}

QString CutterCore::cmd(const char *str)
{
    CORE_LOCK();

    RVA offset = core->offset;
    char *res = rz_core_cmd_str(core, str);
    QString o = fromOwnedCharPtr(res);

    if (offset != core->offset) {
        updateSeek();
    }
    return o;
}

bool CutterCore::isRedirectableDebugee()
{
    if (!currentlyDebugging || currentlyAttachedToPID != -1) {
        return false;
    }

    // We are only able to redirect locally debugged unix processes
    QJsonArray openFilesArray = cmdj("oj").array();
    ;
    for (QJsonValue value : openFilesArray) {
        QJsonObject openFile = value.toObject();
        QString URI = openFile["uri"].toString();
        if (URI.contains("ptrace") | URI.contains("mach")) {
            return true;
        }
    }
    return false;
}

bool CutterCore::isDebugTaskInProgress()
{
    if (!debugTask.isNull()) {
        return true;
    }

    return false;
}

bool CutterCore::asyncCmdEsil(const char *command, QSharedPointer<RizinCmdTask> &task)
{
    asyncCmd(command, task);

    if (task.isNull()) {
        return false;
    }

    connect(task.data(), &RizinCmdTask::finished, task.data(), [this, task]() {
        QString res = task.data()->getResult();

        if (res.contains(QStringLiteral("[ESIL] Stopped execution in an invalid instruction"))) {
            msgBox.showMessage("Stopped when attempted to run an invalid instruction. You can "
                               "disable this in Preferences");
        }
    });

    return true;
}

bool CutterCore::asyncCmd(const char *str, QSharedPointer<RizinCmdTask> &task)
{
    if (!task.isNull()) {
        return false;
    }

    CORE_LOCK();

    RVA offset = core->offset;

    task = QSharedPointer<RizinCmdTask>(new RizinCmdTask(str, true));
    connect(task.data(), &RizinCmdTask::finished, task.data(), [this, offset, task]() {
        CORE_LOCK();

        if (offset != core->offset) {
            updateSeek();
        }
    });

    return true;
}

QString CutterCore::cmdRawAt(const char *cmd, RVA address)
{
    QString res;
    RVA oldOffset = getOffset();
    seekSilent(address);

    res = cmdRaw(cmd);

    seekSilent(oldOffset);
    return res;
}

QString CutterCore::cmdRaw(const char *cmd)
{
    QString res;
    CORE_LOCK();
    rz_cons_push();

    // rz_cmd_call does not return the output of the command
    rz_cmd_call(core->rcmd, cmd);

    // we grab the output straight from rz_cons
    res = rz_cons_get_buffer();

    // cleaning up
    rz_cons_pop();
    rz_cons_echo(NULL);

    return res;
}

QJsonDocument CutterCore::cmdj(const char *str)
{
    char *res;
    {
        CORE_LOCK();
        res = rz_core_cmd_str(core, str);
    }

    QJsonDocument doc = parseJson(res, str);
    rz_mem_free(res);

    return doc;
}

QJsonDocument CutterCore::cmdjAt(const char *str, RVA address)
{
    QJsonDocument res;
    RVA oldOffset = getOffset();
    seekSilent(address);

    res = cmdj(str);

    seekSilent(oldOffset);
    return res;
}

QString CutterCore::cmdTask(const QString &str)
{
    RizinCmdTask task(str);
    task.startTask();
    task.joinTask();
    return task.getResult();
}

QJsonDocument CutterCore::cmdjTask(const QString &str)
{
    RizinCmdTask task(str);
    task.startTask();
    task.joinTask();
    return parseJson(task.getResultRaw(), str);
}

QJsonDocument CutterCore::parseJson(const char *res, const char *cmd)
{
    QByteArray json(res);

    if (json.isEmpty()) {
        return QJsonDocument();
    }

    QJsonParseError jsonError;
    QJsonDocument doc = QJsonDocument::fromJson(json, &jsonError);

    if (jsonError.error != QJsonParseError::NoError) {
        // don't call trimmed() before knowing that parsing failed to avoid copying huge jsons all
        // the time
        if (json.trimmed().isEmpty()) {
            return doc;
        }
        if (cmd) {
            eprintf("Failed to parse JSON for command \"%s\": %s\n", cmd,
                    jsonError.errorString().toLocal8Bit().constData());
        } else {
            eprintf("Failed to parse JSON: %s\n",
                    jsonError.errorString().toLocal8Bit().constData());
        }
        const int MAX_JSON_DUMP_SIZE = 8 * 1024;
        if (json.length() > MAX_JSON_DUMP_SIZE) {
            int originalSize = json.length();
            json.resize(MAX_JSON_DUMP_SIZE);
            eprintf("%d bytes total: %s ...\n", originalSize, json.constData());
        } else {
            eprintf("%s\n", json.constData());
        }
    }

    return doc;
}

QStringList CutterCore::autocomplete(const QString &cmd, RzLinePromptType promptType, size_t limit)
{
    RzLineBuffer buf;
    int c = snprintf(buf.data, sizeof(buf.data), "%s", cmd.toUtf8().constData());
    if (c < 0) {
        return {};
    }
    buf.index = buf.length = std::min((int)(sizeof(buf.data) - 1), c);

    RzLineCompletion completion;
    rz_line_completion_init(&completion, limit);
    rz_core_autocomplete(core(), &completion, &buf, promptType);

    QStringList r;
    r.reserve(rz_pvector_len(&completion.args));
    for (size_t i = 0; i < rz_pvector_len(&completion.args); i++) {
        r.push_back(QString::fromUtf8(
                reinterpret_cast<const char *>(rz_pvector_at(&completion.args, i))));
    }

    rz_line_completion_fini(&completion);
    return r;
}

/**
 * @brief CutterCore::loadFile
 * Load initial file. TODO Maybe use the "o" commands?
 * @param path File path
 * @param baddr Base (RzBin) address
 * @param mapaddr Map address
 * @param perms
 * @param va
 * @param loadbin Load RzBin information
 * @param forceBinPlugin
 * @return
 */
bool CutterCore::loadFile(QString path, ut64 baddr, ut64 mapaddr, int perms, int va, bool loadbin,
                          const QString &forceBinPlugin)
{
    CORE_LOCK();
    RzCoreFile *f;
    rz_config_set_i(core->config, "io.va", va);

    f = rz_core_file_open(core, path.toUtf8().constData(), perms, mapaddr);
    if (!f) {
        eprintf("rz_core_file_open failed\n");
        return false;
    }

    if (!forceBinPlugin.isNull()) {
        rz_bin_force_plugin(rz_core_get_bin(core), forceBinPlugin.toUtf8().constData());
    }

    if (loadbin && va) {
        if (!rz_core_bin_load(core, path.toUtf8().constData(), baddr)) {
            eprintf("CANNOT GET RBIN INFO\n");
        }

#if HAVE_MULTIPLE_RBIN_FILES_INSIDE_SELECT_WHICH_ONE
        if (!rz_core_file_open(core, path.toUtf8(), RZ_IO_READ | (rw ? RZ_IO_WRITE : 0, mapaddr))) {
            eprintf("Cannot open file\n");
        } else {
            // load RzBin information
            // XXX only for sub-bins
            rz_core_bin_load(core, path.toUtf8(), baddr);
            rz_bin_select_idx(core->bin, NULL, idx);
        }
#endif
    } else {
        // Not loading RzBin info coz va = false
    }

    auto iod = core->io ? core->io->desc : NULL;
    auto debug =
            core->file && iod && (core->file->fd == iod->fd) && iod->plugin && iod->plugin->isdbg;

    if (!debug && rz_flag_get(core->flags, "entry0")) {
        rz_core_cmd0(core, "s entry0");
    }

    if (perms & RZ_PERM_W) {
        rz_core_cmd0(core, "omfg+w");
    }

    fflush(stdout);
    return true;
}

bool CutterCore::tryFile(QString path, bool rw)
{
    CORE_LOCK();
    RzCoreFile *cf;
    int flags = RZ_PERM_R;
    if (rw)
        flags = RZ_PERM_RW;
    cf = rz_core_file_open(core, path.toUtf8().constData(), flags, 0LL);
    if (!cf) {
        return false;
    }

    rz_core_file_close(core, cf);

    return true;
}

/**
 * @brief Maps a file using Rizin API
 * @param path Path to file
 * @param mapaddr Map Address
 * @return bool
 */
bool CutterCore::mapFile(QString path, RVA mapaddr)
{
    CORE_LOCK();
    RVA addr = mapaddr != RVA_INVALID ? mapaddr : 0;
    RzBinInfo *info = rz_bin_get_info(Core()->core()->bin);
    if (!info) {
        return false;
    }
    if (rz_core_file_open(core, path.toUtf8().constData(), RZ_PERM_RX, addr)) {
        rz_core_bin_load(core, path.toUtf8().constData(), info->baddr);
    } else {
        return false;
    }
    return true;
}

void CutterCore::renameFunction(const RVA offset, const QString &newName)
{
    cmdRaw("afn " + newName + " " + RAddressString(offset));
    emit functionRenamed(offset, newName);
}

void CutterCore::delFunction(RVA addr)
{
    cmdRaw("af- " + RAddressString(addr));
    emit functionsChanged();
}

void CutterCore::renameFlag(QString old_name, QString new_name)
{
    cmdRaw("fr " + old_name + " " + new_name);
    emit flagsChanged();
}

void CutterCore::renameFunctionVariable(QString newName, QString oldName, RVA functionAddress)
{
    CORE_LOCK();
    RzAnalysisFunction *function = rz_analysis_get_function_at(core->analysis, functionAddress);
    RzAnalysisVar *variable =
            rz_analysis_function_get_var_byname(function, oldName.toUtf8().constData());
    if (variable) {
        rz_analysis_var_rename(variable, newName.toUtf8().constData(), true);
    }
    emit refreshCodeViews();
}

void CutterCore::delFlag(RVA addr)
{
    cmdRawAt("f-", addr);
    emit flagsChanged();
}

void CutterCore::delFlag(const QString &name)
{
    cmdRaw("f-" + name);
    emit flagsChanged();
}

QString CutterCore::getInstructionBytes(RVA addr)
{
    return cmdj("aoj @ " + RAddressString(addr))
            .array()
            .first()
            .toObject()[RJsonKey::bytes]
            .toString();
}

QString CutterCore::getInstructionOpcode(RVA addr)
{
    return cmdj("aoj @ " + RAddressString(addr))
            .array()
            .first()
            .toObject()[RJsonKey::opcode]
            .toString();
}

void CutterCore::editInstruction(RVA addr, const QString &inst)
{
    cmdRawAt(QString("wa %1").arg(inst), addr);
    emit instructionChanged(addr);
}

void CutterCore::nopInstruction(RVA addr)
{
    cmdRawAt("wao nop", addr);
    emit instructionChanged(addr);
}

void CutterCore::jmpReverse(RVA addr)
{
    cmdRawAt("wao recj", addr);
    emit instructionChanged(addr);
}

void CutterCore::editBytes(RVA addr, const QString &bytes)
{
    cmdRawAt(QString("wx %1").arg(bytes), addr);
    emit instructionChanged(addr);
}

void CutterCore::editBytesEndian(RVA addr, const QString &bytes)
{
    cmdRawAt(QString("wv %1").arg(bytes), addr);
    emit stackChanged();
}

void CutterCore::setToCode(RVA addr)
{
    cmdRawAt("Cd-", addr);
    emit instructionChanged(addr);
}

void CutterCore::setAsString(RVA addr, int size, StringTypeFormats type)
{
    if (RVA_INVALID == addr) {
        return;
    }

    QString command;

    switch (type) {
    case StringTypeFormats::None: {
        command = "Cs";
        break;
    }
    case StringTypeFormats::ASCII_LATIN1: {
        command = "Csa";
        break;
    }
    case StringTypeFormats::UTF8: {
        command = "Cs8";
        break;
    }
    default:
        return;
    }

    seekAndShow(addr);

    cmdRawAt(QString("%1 %2").arg(command).arg(size), addr);
    emit instructionChanged(addr);
}

void CutterCore::removeString(RVA addr)
{
    cmdRawAt("Cs-", addr);
    emit instructionChanged(addr);
}

QString CutterCore::getString(RVA addr)
{
    return cmdRawAt("ps", addr);
}

void CutterCore::setToData(RVA addr, int size, int repeat)
{
    if (size <= 0 || repeat <= 0) {
        return;
    }
    cmdRawAt("Cd-", addr);
    cmdRawAt(QString("Cd %1 %2").arg(size).arg(repeat), addr);
    emit instructionChanged(addr);
}

int CutterCore::sizeofDataMeta(RVA addr)
{
    bool ok;
    int size = cmdRawAt("Cd.", addr).toInt(&ok);
    return (ok ? size : 0);
}

void CutterCore::setComment(RVA addr, const QString &cmt)
{
    cmdRawAt(QString("CCu base64:%1").arg(QString(cmt.toLocal8Bit().toBase64())), addr);
    emit commentsChanged(addr);
}

void CutterCore::delComment(RVA addr)
{
    cmdRawAt("CC-", addr);
    emit commentsChanged(addr);
}

/**
 * @brief Gets the comment present at a specific address
 * @param addr The address to be checked
 * @return String containing comment
 */
QString CutterCore::getCommentAt(RVA addr)
{
    CORE_LOCK();
    return rz_meta_get_string(core->analysis, RZ_META_TYPE_COMMENT, addr);
}

void CutterCore::setImmediateBase(const QString &rzBaseName, RVA offset)
{
    if (offset == RVA_INVALID) {
        offset = getOffset();
    }

    this->cmdRawAt(QString("ahi %1").arg(rzBaseName), offset);
    emit instructionChanged(offset);
}

void CutterCore::setCurrentBits(int bits, RVA offset)
{
    if (offset == RVA_INVALID) {
        offset = getOffset();
    }

    this->cmdRawAt(QString("ahb %1").arg(bits), offset);
    emit instructionChanged(offset);
}

void CutterCore::applyStructureOffset(const QString &structureOffset, RVA offset)
{
    if (offset == RVA_INVALID) {
        offset = getOffset();
    }

    this->cmdRawAt("aht " + structureOffset, offset);
    emit instructionChanged(offset);
}

void CutterCore::seekSilent(ut64 offset)
{
    CORE_LOCK();
    if (offset == RVA_INVALID) {
        return;
    }
    rz_core_seek(core, offset, true);
}

void CutterCore::seek(ut64 offset)
{
    // Slower than using the API, but the API is not complete
    // which means we either have to duplicate code from rizin
    // here, or refactor rizin API.
    CORE_LOCK();
    if (offset == RVA_INVALID) {
        return;
    }

    RVA o_offset = core->offset;
    rz_core_seek_and_save(core, offset, true);
    if (o_offset != core->offset) {
        updateSeek();
    }
}

void CutterCore::showMemoryWidget()
{
    emit showMemoryWidgetRequested();
}

void CutterCore::seekAndShow(ut64 offset)
{
    seek(offset);
    showMemoryWidget();
}

void CutterCore::seekAndShow(QString offset)
{
    seek(offset);
    showMemoryWidget();
}

void CutterCore::seek(QString thing)
{
    cmdRaw(QString("s %1").arg(thing));
    updateSeek();
}

void CutterCore::seekPrev()
{
    CORE_LOCK();
    rz_core_seek_undo(core);
    updateSeek();
}

void CutterCore::seekNext()
{
    CORE_LOCK();
    rz_core_seek_redo(core);
    updateSeek();
}

void CutterCore::updateSeek()
{
    emit seekChanged(getOffset());
}

RVA CutterCore::prevOpAddr(RVA startAddr, int count)
{
    CORE_LOCK();
    bool ok;
    RVA offset = cmdRawAt(QString("/O %1").arg(count), startAddr).toULongLong(&ok, 16);
    return ok ? offset : startAddr - count;
}

RVA CutterCore::nextOpAddr(RVA startAddr, int count)
{
    CORE_LOCK();

    QJsonArray array =
            Core()->cmdj("pdj " + QString::number(count + 1) + "@" + QString::number(startAddr))
                    .array();
    if (array.isEmpty()) {
        return startAddr + 1;
    }

    QJsonValue instValue = array.last();
    if (!instValue.isObject()) {
        return startAddr + 1;
    }

    bool ok;
    RVA offset = instValue.toObject()[RJsonKey::offset].toVariant().toULongLong(&ok);
    if (!ok) {
        return startAddr + 1;
    }

    return offset;
}

RVA CutterCore::getOffset()
{
    return core_->offset;
}

ut64 CutterCore::math(const QString &expr)
{
    CORE_LOCK();
    return rz_num_math(core ? core->num : NULL, expr.toUtf8().constData());
}

ut64 CutterCore::num(const QString &expr)
{
    CORE_LOCK();
    return rz_num_get(core ? core->num : NULL, expr.toUtf8().constData());
}

QString CutterCore::itoa(ut64 num, int rdx)
{
    return QString::number(num, rdx);
}

void CutterCore::setConfig(const char *k, const char *v)
{
    CORE_LOCK();
    rz_config_set(core->config, k, v);
}

void CutterCore::setConfig(const QString &k, const char *v)
{
    CORE_LOCK();
    rz_config_set(core->config, k.toUtf8().constData(), v);
}

void CutterCore::setConfig(const char *k, const QString &v)
{
    CORE_LOCK();
    rz_config_set(core->config, k, v.toUtf8().constData());
}

void CutterCore::setConfig(const char *k, int v)
{
    CORE_LOCK();
    rz_config_set_i(core->config, k, static_cast<ut64>(v));
}

void CutterCore::setConfig(const char *k, bool v)
{
    CORE_LOCK();
    rz_config_set_i(core->config, k, v ? 1 : 0);
}

int CutterCore::getConfigi(const char *k)
{
    CORE_LOCK();
    return static_cast<int>(rz_config_get_i(core->config, k));
}

ut64 CutterCore::getConfigut64(const char *k)
{
    CORE_LOCK();
    return rz_config_get_i(core->config, k);
}

bool CutterCore::getConfigb(const char *k)
{
    CORE_LOCK();
    return rz_config_get_i(core->config, k) != 0;
}

QString CutterCore::getConfigDescription(const char *k)
{
    CORE_LOCK();
    RzConfigNode *node = rz_config_node_get(core->config, k);
    return node ? QString(node->desc) : QString("Unrecognized configuration key");
}

void CutterCore::triggerRefreshAll()
{
    emit refreshAll();
}

void CutterCore::triggerAsmOptionsChanged()
{
    emit asmOptionsChanged();
}

void CutterCore::triggerGraphOptionsChanged()
{
    emit graphOptionsChanged();
}

void CutterCore::message(const QString &msg, bool debug)
{
    if (msg.isEmpty())
        return;
    if (debug) {
        qDebug() << msg;
        emit newDebugMessage(msg);
        return;
    }
    emit newMessage(msg);
}

QString CutterCore::getConfig(const char *k)
{
    CORE_LOCK();
    return QString(rz_config_get(core->config, k));
}

void CutterCore::setConfig(const char *k, const QVariant &v)
{
    switch (v.type()) {
    case QVariant::Type::Bool:
        setConfig(k, v.toBool());
        break;
    case QVariant::Type::Int:
        setConfig(k, v.toInt());
        break;
    default:
        setConfig(k, v.toString());
        break;
    }
}

void CutterCore::setCPU(QString arch, QString cpu, int bits)
{
    if (arch != nullptr) {
        setConfig("asm.arch", arch);
    }
    if (cpu != nullptr) {
        setConfig("asm.cpu", cpu);
    }
    setConfig("asm.bits", bits);
}

void CutterCore::setEndianness(bool big)
{
    setConfig("cfg.bigendian", big);
}

QByteArray CutterCore::assemble(const QString &code)
{
    CORE_LOCK();
    RzAsmCode *ac = rz_asm_massemble(core->rasm, code.toUtf8().constData());
    QByteArray res;
    if (ac && ac->bytes) {
        res = QByteArray(reinterpret_cast<const char *>(ac->bytes), ac->len);
    }
    rz_asm_code_free(ac);
    return res;
}

QString CutterCore::disassemble(const QByteArray &data)
{
    CORE_LOCK();
    RzAsmCode *ac = rz_asm_mdisassemble(core->rasm, reinterpret_cast<const ut8 *>(data.constData()),
                                        data.length());
    QString code;
    if (ac && ac->assembly) {
        code = QString::fromUtf8(ac->assembly);
    }
    rz_asm_code_free(ac);
    return code;
}

QString CutterCore::disassembleSingleInstruction(RVA addr)
{
    return cmdRawAt("pi 1", addr).simplified();
}

RzAnalysisFunction *CutterCore::functionIn(ut64 addr)
{
    CORE_LOCK();
    RzList *fcns = rz_analysis_get_functions_in(core->analysis, addr);
    RzAnalysisFunction *fcn = !rz_list_empty(fcns)
            ? reinterpret_cast<RzAnalysisFunction *>(rz_list_first(fcns))
            : nullptr;
    rz_list_free(fcns);
    return fcn;
}

RzAnalysisFunction *CutterCore::functionAt(ut64 addr)
{
    CORE_LOCK();
    return rz_analysis_get_function_at(core->analysis, addr);
}

/**
 * @brief finds the start address of a function in a given address
 * @param addr - an address which belongs to a function
 * @returns if function exists, return its start address. Otherwise return RVA_INVALID
 */
RVA CutterCore::getFunctionStart(RVA addr)
{
    CORE_LOCK();
    RzAnalysisFunction *fcn = Core()->functionIn(addr);
    return fcn ? fcn->addr : RVA_INVALID;
}

/**
 * @brief finds the end address of a function in a given address
 * @param addr - an address which belongs to a function
 * @returns if function exists, return its end address. Otherwise return RVA_INVALID
 */
RVA CutterCore::getFunctionEnd(RVA addr)
{
    CORE_LOCK();
    RzAnalysisFunction *fcn = Core()->functionIn(addr);
    return fcn ? fcn->addr : RVA_INVALID;
}

/**
 * @brief finds the last instruction of a function in a given address
 * @param addr - an address which belongs to a function
 * @returns if function exists, return the address of its last instruction. Otherwise return
 * RVA_INVALID
 */
RVA CutterCore::getLastFunctionInstruction(RVA addr)
{
    CORE_LOCK();
    RzAnalysisFunction *fcn = Core()->functionIn(addr);
    if (!fcn) {
        return RVA_INVALID;
    }
    RzAnalysisBlock *lastBB = (RzAnalysisBlock *)rz_list_last(fcn->bbs);
    return lastBB ? rz_analysis_block_get_op_addr(lastBB, lastBB->ninstr - 1) : RVA_INVALID;
}

QString CutterCore::cmdFunctionAt(QString addr)
{
    QString ret;
    // Use cmd because cmdRaw would not work with grep
    ret = cmd(QString("fd @ %1~[0]").arg(addr));
    return ret.trimmed();
}

QString CutterCore::cmdFunctionAt(RVA addr)
{
    return cmdFunctionAt(QString::number(addr));
}

void CutterCore::cmdEsil(const char *command)
{
    // use cmd and not cmdRaw because of unexpected commands
    QString res = cmd(command);
    if (res.contains(QStringLiteral("[ESIL] Stopped execution in an invalid instruction"))) {
        msgBox.showMessage("Stopped when attempted to run an invalid instruction. You can disable "
                           "this in Preferences");
    }
}

QString CutterCore::createFunctionAt(RVA addr)
{
    QString ret = cmdRaw(QString("af %1").arg(addr));
    emit functionsChanged();
    return ret;
}

QString CutterCore::createFunctionAt(RVA addr, QString name)
{
    static const QRegularExpression regExp("[^a-zA-Z0-9_.]");
    name.remove(regExp);
    QString ret = cmdRawAt(QString("af %1").arg(name), addr);
    emit functionsChanged();
    return ret;
}

QJsonDocument CutterCore::getRegistersInfo()
{
    return cmdj("aeafj");
}

RVA CutterCore::getOffsetJump(RVA addr)
{
    bool ok;
    RVA value = cmdj("aoj @" + QString::number(addr))
                        .array()
                        .first()
                        .toObject()
                        .value(RJsonKey::jump)
                        .toVariant()
                        .toULongLong(&ok);

    if (!ok) {
        return RVA_INVALID;
    }

    return value;
}

QList<Decompiler *> CutterCore::getDecompilers()
{
    return decompilers;
}

Decompiler *CutterCore::getDecompilerById(const QString &id)
{
    for (Decompiler *dec : decompilers) {
        if (dec->getId() == id) {
            return dec;
        }
    }
    return nullptr;
}

bool CutterCore::registerDecompiler(Decompiler *decompiler)
{
    if (getDecompilerById(decompiler->getId())) {
        return false;
    }
    decompiler->setParent(this);
    decompilers.push_back(decompiler);
    return true;
}

QJsonDocument CutterCore::getFileVersionInfo()
{
    return cmdj("iVj");
}

QJsonDocument CutterCore::getSignatureInfo()
{
    return cmdj("iCj");
}

// Utility function to check if a telescoped item exists and add it with prefixes to the desc
static inline const QString appendVar(QString &dst, const QString val, const QString prepend_val,
                                      const QString append_val)
{
    if (!val.isEmpty()) {
        dst += prepend_val + val + append_val;
    }
    return val;
}

RefDescription CutterCore::formatRefDesc(QJsonObject refItem)
{
    RefDescription desc;

    // Ignore empty refs and refs that only contain addr
    if (refItem.size() <= 1) {
        return desc;
    }

    QString str = refItem["string"].toVariant().toString();
    if (!str.isEmpty()) {
        desc.ref = str;
        desc.refColor = ConfigColor("comment");
    } else {
        QString type, string;
        do {
            desc.ref += " ->";
            appendVar(desc.ref, refItem["reg"].toVariant().toString(), " @", "");
            appendVar(desc.ref, refItem["mapname"].toVariant().toString(), " (", ")");
            appendVar(desc.ref, refItem["section"].toVariant().toString(), " (", ")");
            appendVar(desc.ref, refItem["func"].toVariant().toString(), " ", "");
            type = appendVar(desc.ref, refItem["type"].toVariant().toString(), " ", "");
            appendVar(desc.ref, refItem["perms"].toVariant().toString(), " ", "");
            appendVar(desc.ref, refItem["asm"].toVariant().toString(), " \"", "\"");
            string = appendVar(desc.ref, refItem["string"].toVariant().toString(), " ", "");
            if (!string.isNull()) {
                // There is no point in adding ascii and addr info after a string
                break;
            }
            if (!refItem["value"].isNull()) {
                appendVar(desc.ref, RAddressString(refItem["value"].toVariant().toULongLong()), " ",
                          "");
            }
            refItem = refItem["ref"].toObject();
        } while (!refItem.empty());

        // Set the ref's color according to the last item type
        if (type == "ascii" || !string.isEmpty()) {
            desc.refColor = ConfigColor("comment");
        } else if (type == "program") {
            desc.refColor = ConfigColor("fname");
        } else if (type == "library") {
            desc.refColor = ConfigColor("floc");
        } else if (type == "stack") {
            desc.refColor = ConfigColor("offset");
        }
    }

    return desc;
}

QList<QJsonObject> CutterCore::getRegisterRefs(int depth)
{
    QList<QJsonObject> ret;
    if (!currentlyDebugging) {
        return ret;
    }

    QJsonObject registers = cmdj("drj").object();

    for (const QString &key : registers.keys()) {
        QJsonObject reg;
        reg["value"] = registers.value(key);
        reg["ref"] = getAddrRefs(registers.value(key).toVariant().toULongLong(), depth);
        reg["name"] = key;
        ret.append(reg);
    }

    return ret;
}

QList<QJsonObject> CutterCore::getStack(int size, int depth)
{
    QList<QJsonObject> stack;
    if (!currentlyDebugging) {
        return stack;
    }

    CORE_LOCK();
    bool ret;
    RVA addr = cmdRaw("dr SP").toULongLong(&ret, 16);
    if (!ret) {
        return stack;
    }

    int base = core->analysis->bits;
    for (int i = 0; i < size; i += base / 8) {
        if ((base == 32 && addr + i >= UT32_MAX) || (base == 16 && addr + i >= UT16_MAX)) {
            break;
        }

        stack.append(getAddrRefs(addr + i, depth));
    }

    return stack;
}

QJsonObject CutterCore::getAddrRefs(RVA addr, int depth)
{
    QJsonObject json;
    if (depth < 1 || addr == UT64_MAX) {
        return json;
    }

    CORE_LOCK();
    int bits = core->rasm->bits;
    QByteArray buf = QByteArray();
    ut64 type = rz_core_analysis_address(core, addr);

    json["addr"] = QString::number(addr);

    // Search for the section the addr is in, avoid duplication for heap/stack with type
    if (!(type & RZ_ANALYSIS_ADDR_TYPE_HEAP || type & RZ_ANALYSIS_ADDR_TYPE_STACK)) {
        // Attempt to find the address within a map
        RzDebugMap *map = rz_debug_map_get(core->dbg, addr);
        if (map && map->name && map->name[0]) {
            json["mapname"] = map->name;
        }

        RzBinSection *sect = rz_bin_get_section_at(rz_bin_cur_object(core->bin), addr, true);
        if (sect && sect->name[0]) {
            json["section"] = sect->name;
        }
    }

    // Check if the address points to a register
    RzFlagItem *fi = rz_flag_get_i(core->flags, addr);
    if (fi) {
        RzRegItem *r = rz_reg_get(core->dbg->reg, fi->name, -1);
        if (r) {
            json["reg"] = r->name;
        }
    }

    // Attempt to find the address within a function
    RzAnalysisFunction *fcn = rz_analysis_get_fcn_in(core->analysis, addr, 0);
    if (fcn) {
        json["fcn"] = fcn->name;
    }

    // Update type and permission information
    if (type != 0) {
        if (type & RZ_ANALYSIS_ADDR_TYPE_HEAP) {
            json["type"] = "heap";
        } else if (type & RZ_ANALYSIS_ADDR_TYPE_STACK) {
            json["type"] = "stack";
        } else if (type & RZ_ANALYSIS_ADDR_TYPE_PROGRAM) {
            json["type"] = "program";
        } else if (type & RZ_ANALYSIS_ADDR_TYPE_LIBRARY) {
            json["type"] = "library";
        } else if (type & RZ_ANALYSIS_ADDR_TYPE_ASCII) {
            json["type"] = "ascii";
        } else if (type & RZ_ANALYSIS_ADDR_TYPE_SEQUENCE) {
            json["type"] = "sequence";
        }

        QString perms = "";
        if (type & RZ_ANALYSIS_ADDR_TYPE_READ) {
            perms += "r";
        }
        if (type & RZ_ANALYSIS_ADDR_TYPE_WRITE) {
            perms += "w";
        }
        if (type & RZ_ANALYSIS_ADDR_TYPE_EXEC) {
            RzAsmOp op;
            buf.resize(32);
            perms += "x";
            // Instruction disassembly
            rz_io_read_at(core->io, addr, (unsigned char *)buf.data(), buf.size());
            rz_asm_set_pc(core->rasm, addr);
            rz_asm_disassemble(core->rasm, &op, (unsigned char *)buf.data(), buf.size());
            json["asm"] = rz_asm_op_get_asm(&op);
        }

        if (!perms.isEmpty()) {
            json["perms"] = perms;
        }
    }

    // Try to telescope further if depth permits it
    if ((type & RZ_ANALYSIS_ADDR_TYPE_READ)) {
        buf.resize(64);
        ut32 *n32 = (ut32 *)buf.data();
        ut64 *n64 = (ut64 *)buf.data();
        rz_io_read_at(core->io, addr, (unsigned char *)buf.data(), buf.size());
        ut64 n = (bits == 64) ? *n64 : *n32;
        // The value of the next address will serve as an indication that there's more to
        // telescope if we have reached the depth limit
        json["value"] = QString::number(n);
        if (depth && n != addr && !(type & RZ_ANALYSIS_ADDR_TYPE_EXEC)) {
            // Make sure we aren't telescoping the same address
            QJsonObject ref = getAddrRefs(n, depth - 1);
            if (!ref.empty() && !ref["type"].isNull()) {
                // If the dereference of the current pointer is an ascii character we
                // might have a string in this address
                if (ref["type"].toString().contains("ascii")) {
                    buf.resize(128);
                    rz_io_read_at(core->io, addr, (unsigned char *)buf.data(), buf.size());
                    QString strVal = QString(buf);
                    // Indicate that the string is longer than the printed value
                    if (strVal.size() == buf.size()) {
                        strVal += "...";
                    }
                    json["string"] = strVal;
                }
                json["ref"] = ref;
            }
        }
    }
    return json;
}

QJsonDocument CutterCore::getProcessThreads(int pid)
{
    if (-1 == pid) {
        // Return threads list of the currently debugged PID
        return cmdj("dptj");
    } else {
        return cmdj("dptj " + QString::number(pid));
    }
}

QJsonDocument CutterCore::getChildProcesses(int pid)
{
    // Return the currently debugged process and it's children
    if (-1 == pid) {
        return cmdj("dpj");
    }
    // Return the given pid and it's child processes
    return cmdj("dpj " + QString::number(pid));
}

QJsonDocument CutterCore::getRegisterValues()
{
    return cmdj("drj");
}

QList<VariableDescription> CutterCore::getVariables(RVA at)
{
    QList<VariableDescription> ret;
    QJsonObject varsObject = cmdj(QString("afvj @ %1").arg(at)).object();

    auto addVars = [&](VariableDescription::RefType refType, const QJsonArray &array) {
        for (const QJsonValue &varValue : array) {
            QJsonObject varObject = varValue.toObject();
            VariableDescription desc;
            desc.refType = refType;
            desc.name = varObject["name"].toString();
            desc.type = varObject["type"].toString();
            ret << desc;
        }
    };

    addVars(VariableDescription::RefType::SP, varsObject["sp"].toArray());
    addVars(VariableDescription::RefType::BP, varsObject["bp"].toArray());
    addVars(VariableDescription::RefType::Reg, varsObject["reg"].toArray());

    return ret;
}

QVector<RegisterRefValueDescription> CutterCore::getRegisterRefValues()
{
    QJsonArray registerRefArray = cmdj("drrj").array();
    QVector<RegisterRefValueDescription> result;

    for (const QJsonValue value : registerRefArray) {
        QJsonObject regRefObject = value.toObject();

        RegisterRefValueDescription desc;
        desc.name = regRefObject[RJsonKey::reg].toString();
        desc.value = regRefObject[RJsonKey::value].toString();
        desc.ref = regRefObject[RJsonKey::ref].toString();

        result.push_back(desc);
    }
    return result;
}

QString CutterCore::getRegisterName(QString registerRole)
{
    return cmdRaw("drn " + registerRole).trimmed();
}

RVA CutterCore::getProgramCounterValue()
{
    bool ok;
    if (currentlyDebugging) {
        // Use cmd because cmdRaw would not work with inner command backticked
        // TODO: Risky command due to changes in API, search for something safer
        RVA addr = cmd("dr `drn PC`").toULongLong(&ok, 16);
        if (ok) {
            return addr;
        }
    }
    return RVA_INVALID;
}

void CutterCore::setRegister(QString regName, QString regValue)
{
    cmdRaw(QString("dr %1=%2").arg(regName).arg(regValue));
    emit registersChanged();
    emit refreshCodeViews();
}

void CutterCore::setCurrentDebugThread(int tid)
{
    if (!asyncCmd("dpt=" + QString::number(tid), debugTask)) {
        return;
    }

    emit debugTaskStateChanged();
    connect(debugTask.data(), &RizinTask::finished, this, [this]() {
        debugTask.clear();
        emit registersChanged();
        emit refreshCodeViews();
        emit stackChanged();
        syncAndSeekProgramCounter();
        emit switchedThread();
        emit debugTaskStateChanged();
    });

    debugTask->startTask();
}

void CutterCore::setCurrentDebugProcess(int pid)
{
    if (!currentlyDebugging || !asyncCmd("dp=" + QString::number(pid), debugTask)) {
        return;
    }

    emit debugTaskStateChanged();
    connect(debugTask.data(), &RizinTask::finished, this, [this]() {
        debugTask.clear();
        emit registersChanged();
        emit refreshCodeViews();
        emit stackChanged();
        emit flagsChanged();
        syncAndSeekProgramCounter();
        emit switchedProcess();
        emit debugTaskStateChanged();
    });

    debugTask->startTask();
}

void CutterCore::startDebug()
{
    if (!currentlyDebugging) {
        offsetPriorDebugging = getOffset();
    }
    currentlyOpenFile = getConfig("file.path");

    if (!asyncCmd("ood", debugTask)) {
        return;
    }

    emit debugTaskStateChanged();

    connect(debugTask.data(), &RizinTask::finished, this, [this]() {
        if (debugTaskDialog) {
            delete debugTaskDialog;
        }
        debugTask.clear();

        emit registersChanged();
        if (!currentlyDebugging) {
            setConfig("asm.flags", false);
            currentlyDebugging = true;
            emit toggleDebugView();
            emit refreshCodeViews();
        }

        emit codeRebased();
        emit stackChanged();
        emit debugTaskStateChanged();
    });

    debugTaskDialog = new RizinTaskDialog(debugTask);
    debugTaskDialog->setBreakOnClose(true);
    debugTaskDialog->setAttribute(Qt::WA_DeleteOnClose);
    debugTaskDialog->setDesc(tr("Starting native debug..."));
    debugTaskDialog->show();

    debugTask->startTask();
}

void CutterCore::startEmulation()
{
    if (!currentlyDebugging) {
        offsetPriorDebugging = getOffset();
    }

    // clear registers, init esil state, stack, progcounter at current seek
    asyncCmd("aei; aeim; aeip", debugTask);

    emit debugTaskStateChanged();

    connect(debugTask.data(), &RizinTask::finished, this, [this]() {
        if (debugTaskDialog) {
            delete debugTaskDialog;
        }
        debugTask.clear();

        if (!currentlyDebugging || !currentlyEmulating) {
            // prevent register flags from appearing during debug/emul
            setConfig("asm.flags", false);
            // allows to view self-modifying code changes or other binary changes
            setConfig("io.cache", true);
            currentlyDebugging = true;
            currentlyEmulating = true;
            emit toggleDebugView();
        }

        emit registersChanged();
        emit stackChanged();
        emit codeRebased();
        emit refreshCodeViews();
        emit debugTaskStateChanged();
    });

    debugTaskDialog = new RizinTaskDialog(debugTask);
    debugTaskDialog->setBreakOnClose(true);
    debugTaskDialog->setAttribute(Qt::WA_DeleteOnClose);
    debugTaskDialog->setDesc(tr("Starting emulation..."));
    debugTaskDialog->show();

    debugTask->startTask();
}

void CutterCore::attachRemote(const QString &uri)
{
    if (!currentlyDebugging) {
        offsetPriorDebugging = getOffset();
    }

    // connect to a debugger with the given plugin
    asyncCmd("e cfg.debug=true; oodf " + uri, debugTask);
    emit debugTaskStateChanged();

    connect(debugTask.data(), &RizinTask::finished, this, [this, uri]() {
        if (debugTaskDialog) {
            delete debugTaskDialog;
        }
        debugTask.clear();
        // Check if we actually connected
        bool connected = false;
        QJsonArray openFilesArray = getOpenedFiles();
        for (QJsonValue value : openFilesArray) {
            QJsonObject openFile = value.toObject();
            QString fileUri = openFile["uri"].toString();
            if (!fileUri.compare(uri)) {
                connected = true;
            }
        }
        seekAndShow(getProgramCounterValue());
        if (!connected) {
            emit attachedRemote(false);
            emit debugTaskStateChanged();
            return;
        }

        emit registersChanged();
        if (!currentlyDebugging || !currentlyEmulating) {
            // prevent register flags from appearing during debug/emul
            setConfig("asm.flags", false);
            currentlyDebugging = true;
            emit toggleDebugView();
        }

        emit codeRebased();
        emit attachedRemote(true);
        emit debugTaskStateChanged();
    });

    debugTaskDialog = new RizinTaskDialog(debugTask);
    debugTaskDialog->setBreakOnClose(true);
    debugTaskDialog->setAttribute(Qt::WA_DeleteOnClose);
    debugTaskDialog->setDesc(tr("Connecting to: ") + uri);
    debugTaskDialog->show();

    debugTask->startTask();
}

void CutterCore::attachDebug(int pid)
{
    if (!currentlyDebugging) {
        offsetPriorDebugging = getOffset();
    }

    // attach to process with dbg plugin
    asyncCmd("e cfg.debug = true; oodf dbg://" + QString::number(pid), debugTask);
    emit debugTaskStateChanged();

    connect(debugTask.data(), &RizinTask::finished, this, [this, pid]() {
        if (debugTaskDialog) {
            delete debugTaskDialog;
        }
        debugTask.clear();

        syncAndSeekProgramCounter();
        if (!currentlyDebugging || !currentlyEmulating) {
            // prevent register flags from appearing during debug/emul
            setConfig("asm.flags", false);
            currentlyDebugging = true;
            currentlyOpenFile = getConfig("file.path");
            currentlyAttachedToPID = pid;
            emit toggleDebugView();
        }

        emit codeRebased();
        emit debugTaskStateChanged();
    });

    debugTaskDialog = new RizinTaskDialog(debugTask);
    debugTaskDialog->setBreakOnClose(true);
    debugTaskDialog->setAttribute(Qt::WA_DeleteOnClose);
    debugTaskDialog->setDesc(tr("Attaching to process (") + QString::number(pid) + ")...");
    debugTaskDialog->show();

    debugTask->startTask();
}

void CutterCore::suspendDebug()
{
    debugTask->breakTask();
}

void CutterCore::stopDebug()
{
    if (!currentlyDebugging) {
        return;
    }

    if (!debugTask.isNull()) {
        suspendDebug();
    }

    currentlyDebugging = false;
    currentlyTracing = false;
    emit debugTaskStateChanged();

    if (currentlyEmulating) {
        cmdEsil("aeim-; aei-; wcr; .ar-; aets-");
        currentlyEmulating = false;
    } else if (currentlyAttachedToPID != -1) {
        // Use cmd because cmdRaw would not work with command concatenation
        cmd(QString("dp- %1; o %2; .ar-")
                    .arg(QString::number(currentlyAttachedToPID), currentlyOpenFile));
        currentlyAttachedToPID = -1;
    } else {
        QString ptraceFiles = "";
        // close ptrace file descriptors left open
        QJsonArray openFilesArray = cmdj("oj").array();
        ;
        for (QJsonValue value : openFilesArray) {
            QJsonObject openFile = value.toObject();
            QString URI = openFile["uri"].toString();
            if (URI.contains("ptrace")) {
                ptraceFiles += "o-" + QString::number(openFile["fd"].toInt()) + ";";
            }
        }
        // Use cmd because cmdRaw would not work with command concatenation
        cmd("doc" + ptraceFiles);
    }

    syncAndSeekProgramCounter();
    setConfig("asm.flags", true);
    setConfig("io.cache", false);
    emit codeRebased();
    emit toggleDebugView();
    offsetPriorDebugging = getOffset();
    emit debugTaskStateChanged();
}

void CutterCore::syncAndSeekProgramCounter()
{
    seekAndShow(getProgramCounterValue());
    emit registersChanged();
}

void CutterCore::continueDebug()
{
    if (!currentlyDebugging) {
        return;
    }

    if (currentlyEmulating) {
        if (!asyncCmdEsil("aec", debugTask)) {
            return;
        }
    } else {
        if (!asyncCmd("dc", debugTask)) {
            return;
        }
    }

    emit debugTaskStateChanged();
    connect(debugTask.data(), &RizinTask::finished, this, [this]() {
        debugTask.clear();
        syncAndSeekProgramCounter();
        emit refreshCodeViews();
        emit debugTaskStateChanged();
    });

    debugTask->startTask();
}

void CutterCore::continueBackDebug()
{
    if (!currentlyDebugging) {
        return;
    }

    if (currentlyEmulating) {
        if (!asyncCmdEsil("aecb", debugTask)) {
            return;
        }
    } else {
        if (!asyncCmd("dcb", debugTask)) {
            return;
        }
    }
    emit debugTaskStateChanged();

    connect(debugTask.data(), &RizinTask::finished, this, [this]() {
        debugTask.clear();
        syncAndSeekProgramCounter();
        emit refreshCodeViews();
        emit debugTaskStateChanged();
    });

    debugTask->startTask();
}

void CutterCore::continueUntilDebug(QString offset)
{
    if (!currentlyDebugging) {
        return;
    }

    if (currentlyEmulating) {
        if (!asyncCmdEsil("aecu " + offset, debugTask)) {
            return;
        }
    } else {
        if (!asyncCmd("dcu " + offset, debugTask)) {
            return;
        }
    }

    emit debugTaskStateChanged();
    connect(debugTask.data(), &RizinTask::finished, this, [this]() {
        debugTask.clear();
        syncAndSeekProgramCounter();
        emit refreshCodeViews();
        emit debugTaskStateChanged();
    });

    debugTask->startTask();
}

void CutterCore::continueUntilCall()
{
    if (!currentlyDebugging) {
        return;
    }

    if (currentlyEmulating) {
        if (!asyncCmdEsil("aecc", debugTask)) {
            return;
        }
    } else {
        if (!asyncCmd("dcc", debugTask)) {
            return;
        }
    }

    emit debugTaskStateChanged();
    connect(debugTask.data(), &RizinTask::finished, this, [this]() {
        debugTask.clear();
        syncAndSeekProgramCounter();
        emit refreshCodeViews();
        emit debugTaskStateChanged();
    });

    debugTask->startTask();
}

void CutterCore::continueUntilSyscall()
{
    if (!currentlyDebugging) {
        return;
    }

    if (currentlyEmulating) {
        if (!asyncCmdEsil("aecs", debugTask)) {
            return;
        }
    } else {
        if (!asyncCmd("dcs", debugTask)) {
            return;
        }
    }

    emit debugTaskStateChanged();
    connect(debugTask.data(), &RizinTask::finished, this, [this]() {
        debugTask.clear();
        syncAndSeekProgramCounter();
        emit refreshCodeViews();
        emit debugTaskStateChanged();
    });

    debugTask->startTask();
}

void CutterCore::stepDebug()
{
    if (!currentlyDebugging) {
        return;
    }

    if (currentlyEmulating) {
        if (!asyncCmdEsil("aes", debugTask)) {
            return;
        }
    } else {
        if (!asyncCmd("ds", debugTask)) {
            return;
        }
    }

    emit debugTaskStateChanged();
    connect(debugTask.data(), &RizinTask::finished, this, [this]() {
        debugTask.clear();
        syncAndSeekProgramCounter();
        emit refreshCodeViews();
        emit debugTaskStateChanged();
    });

    debugTask->startTask();
}

void CutterCore::stepOverDebug()
{
    if (!currentlyDebugging) {
        return;
    }

    if (currentlyEmulating) {
        if (!asyncCmdEsil("aeso", debugTask)) {
            return;
        }
    } else {
        if (!asyncCmd("dso", debugTask)) {
            return;
        }
    }

    emit debugTaskStateChanged();
    connect(debugTask.data(), &RizinTask::finished, this, [this]() {
        debugTask.clear();
        syncAndSeekProgramCounter();
        emit refreshCodeViews();
        emit debugTaskStateChanged();
    });

    debugTask->startTask();
}

void CutterCore::stepOutDebug()
{
    if (!currentlyDebugging) {
        return;
    }

    emit debugTaskStateChanged();
    if (!asyncCmd("dsf", debugTask)) {
        return;
    }

    connect(debugTask.data(), &RizinTask::finished, this, [this]() {
        debugTask.clear();
        syncAndSeekProgramCounter();
        emit refreshCodeViews();
        emit debugTaskStateChanged();
    });

    debugTask->startTask();
}

void CutterCore::stepBackDebug()
{
    if (!currentlyDebugging) {
        return;
    }

    if (currentlyEmulating) {
        if (!asyncCmdEsil("aesb", debugTask)) {
            return;
        }
    } else {
        if (!asyncCmd("dsb", debugTask)) {
            return;
        }
    }
    emit debugTaskStateChanged();

    connect(debugTask.data(), &RizinTask::finished, this, [this]() {
        debugTask.clear();
        syncAndSeekProgramCounter();
        emit refreshCodeViews();
        emit debugTaskStateChanged();
    });

    debugTask->startTask();
}

QStringList CutterCore::getDebugPlugins()
{
    QStringList plugins;
    QJsonArray pluginArray = cmdj("dLj").array();

    for (const QJsonValue &value : pluginArray) {
        QJsonObject pluginObject = value.toObject();

        QString plugin = pluginObject[RJsonKey::name].toString();

        plugins << plugin;
    }
    return plugins;
}

QString CutterCore::getActiveDebugPlugin()
{
    return getConfig("dbg.backend");
}

void CutterCore::setDebugPlugin(QString plugin)
{
    setConfig("dbg.backend", plugin);
}

void CutterCore::startTraceSession()
{
    if (!currentlyDebugging || currentlyTracing) {
        return;
    }

    if (currentlyEmulating) {
        if (!asyncCmdEsil("aets+", debugTask)) {
            return;
        }
    } else {
        if (!asyncCmd("dts+", debugTask)) {
            return;
        }
    }
    emit debugTaskStateChanged();

    connect(debugTask.data(), &RizinTask::finished, this, [this]() {
        if (debugTaskDialog) {
            delete debugTaskDialog;
        }
        debugTask.clear();

        currentlyTracing = true;
        emit debugTaskStateChanged();
    });

    debugTaskDialog = new RizinTaskDialog(debugTask);
    debugTaskDialog->setBreakOnClose(true);
    debugTaskDialog->setAttribute(Qt::WA_DeleteOnClose);
    debugTaskDialog->setDesc(tr("Creating debug tracepoint..."));
    debugTaskDialog->show();

    debugTask->startTask();
}

void CutterCore::stopTraceSession()
{
    if (!currentlyDebugging || !currentlyTracing) {
        return;
    }

    if (currentlyEmulating) {
        if (!asyncCmdEsil("aets-", debugTask)) {
            return;
        }
    } else {
        if (!asyncCmd("dts-", debugTask)) {
            return;
        }
    }
    emit debugTaskStateChanged();

    connect(debugTask.data(), &RizinTask::finished, this, [this]() {
        if (debugTaskDialog) {
            delete debugTaskDialog;
        }
        debugTask.clear();

        currentlyTracing = false;
        emit debugTaskStateChanged();
    });

    debugTaskDialog = new RizinTaskDialog(debugTask);
    debugTaskDialog->setBreakOnClose(true);
    debugTaskDialog->setAttribute(Qt::WA_DeleteOnClose);
    debugTaskDialog->setDesc(tr("Stopping debug session..."));
    debugTaskDialog->show();

    debugTask->startTask();
}

void CutterCore::toggleBreakpoint(RVA addr)
{
    cmdRaw(QString("dbs %1").arg(addr));
    emit breakpointsChanged(addr);
}

void CutterCore::addBreakpoint(const BreakpointDescription &config)
{
    CORE_LOCK();
    RzBreakpointItem *breakpoint = nullptr;
    int watchpoint_prot = 0;
    if (config.hw) {
        watchpoint_prot = config.permission & ~(RZ_BP_PROT_EXEC);
    }

    auto address = config.addr;
    char *module = nullptr;
    QByteArray moduleNameData;
    if (config.type == BreakpointDescription::Named) {
        address = Core()->math(config.positionExpression);
    } else if (config.type == BreakpointDescription::Module) {
        address = 0;
        moduleNameData = config.positionExpression.toUtf8();
        module = moduleNameData.data();
    }
    breakpoint = rz_debug_bp_add(core->dbg, address, (config.hw && watchpoint_prot == 0),
                                 watchpoint_prot, watchpoint_prot, module, config.moduleDelta);
    if (!breakpoint) {
        QMessageBox::critical(nullptr, tr("Breakpoint error"), tr("Failed to create breakpoint"));
        return;
    }
    if (config.type == BreakpointDescription::Named) {
        updateOwnedCharPtr(breakpoint->expr, config.positionExpression);
    }

    if (config.hw) {
        breakpoint->size = config.size;
    }
    if (config.type == BreakpointDescription::Named) {
        updateOwnedCharPtr(breakpoint->name, config.positionExpression);
    }

    int index = std::find(core->dbg->bp->bps_idx,
                          core->dbg->bp->bps_idx + core->dbg->bp->bps_idx_count, breakpoint)
            - core->dbg->bp->bps_idx;

    breakpoint->enabled = config.enabled;
    if (config.trace) {
        setBreakpointTrace(index, config.trace);
    }
    if (!config.condition.isEmpty()) {
        updateOwnedCharPtr(breakpoint->cond, config.condition);
    }
    if (!config.command.isEmpty()) {
        updateOwnedCharPtr(breakpoint->data, config.command);
    }
    emit breakpointsChanged(breakpoint->addr);
}

void CutterCore::updateBreakpoint(int index, const BreakpointDescription &config)
{
    CORE_LOCK();
    if (auto bp = rz_bp_get_index(core->dbg->bp, index)) {
        rz_bp_del(core->dbg->bp, bp->addr);
    }
    // Delete by index currently buggy,
    // required for breakpoints with non address based position
    // rz_bp_del_index(core->dbg->bp, index);
    addBreakpoint(config);
}

void CutterCore::delBreakpoint(RVA addr)
{
    cmdRaw("db- " + RAddressString(addr));
    emit breakpointsChanged(addr);
}

void CutterCore::delAllBreakpoints()
{
    cmdRaw("db-*");
    emit refreshCodeViews();
}

void CutterCore::enableBreakpoint(RVA addr)
{
    cmdRaw("dbe " + RAddressString(addr));
    emit breakpointsChanged(addr);
}

void CutterCore::disableBreakpoint(RVA addr)
{
    cmdRaw("dbd " + RAddressString(addr));
    emit breakpointsChanged(addr);
}

void CutterCore::setBreakpointTrace(int index, bool enabled)
{
    if (enabled) {
        cmdRaw(QString("dbite %1").arg(index));
    } else {
        cmdRaw(QString("dbitd %1").arg(index));
    }
}

static BreakpointDescription breakpointDescriptionFromRizin(int index, rz_bp_item_t *bpi)
{
    BreakpointDescription bp;
    bp.addr = bpi->addr;
    bp.index = index;
    bp.size = bpi->size;
    if (bpi->expr) {
        bp.positionExpression = bpi->expr;
        bp.type = BreakpointDescription::Named;
    }
    bp.name = bpi->name;
    bp.permission = bpi->perm;
    bp.command = bpi->data;
    bp.condition = bpi->cond;
    bp.hw = bpi->hw;
    bp.trace = bpi->trace;
    bp.enabled = bpi->enabled;
    return bp;
}

int CutterCore::breakpointIndexAt(RVA addr)
{
    CORE_LOCK();
    return rz_bp_get_index_at(core->dbg->bp, addr);
}

BreakpointDescription CutterCore::getBreakpointAt(RVA addr)
{
    CORE_LOCK();
    int index = breakpointIndexAt(addr);
    auto bp = rz_bp_get_index(core->dbg->bp, index);
    if (bp) {
        return breakpointDescriptionFromRizin(index, bp);
    }
    return BreakpointDescription();
}

QList<BreakpointDescription> CutterCore::getBreakpoints()
{
    CORE_LOCK();
    QList<BreakpointDescription> ret;
    // TODO: use higher level API, don't touch rizin bps_idx directly
    for (int i = 0; i < core->dbg->bp->bps_idx_count; i++) {
        if (auto bpi = core->dbg->bp->bps_idx[i]) {
            ret.push_back(breakpointDescriptionFromRizin(i, bpi));
        }
    }

    return ret;
}

QList<RVA> CutterCore::getBreakpointsAddresses()
{
    QList<RVA> bpAddresses;
    for (const BreakpointDescription &bp : getBreakpoints()) {
        bpAddresses << bp.addr;
    }

    return bpAddresses;
}

QList<RVA> CutterCore::getBreakpointsInFunction(RVA funcAddr)
{
    QList<RVA> allBreakpoints = getBreakpointsAddresses();
    QList<RVA> functionBreakpoints;

    // Use std manipulations to take only the breakpoints that belong to this function
    std::copy_if(allBreakpoints.begin(), allBreakpoints.end(),
                 std::back_inserter(functionBreakpoints),
                 [this, funcAddr](RVA BPadd) { return getFunctionStart(BPadd) == funcAddr; });
    return functionBreakpoints;
}

bool CutterCore::isBreakpoint(const QList<RVA> &breakpoints, RVA addr)
{
    return breakpoints.contains(addr);
}

QJsonDocument CutterCore::getBacktrace()
{
    return cmdj("dbtj");
}

QList<ProcessDescription> CutterCore::getAllProcesses()
{
    QList<ProcessDescription> ret;
    QJsonArray processArray = cmdj("dplj").array();

    for (const QJsonValue &value : processArray) {
        QJsonObject procObject = value.toObject();

        ProcessDescription proc;

        proc.pid = procObject[RJsonKey::pid].toInt();
        proc.uid = procObject[RJsonKey::uid].toInt();
        proc.status = procObject[RJsonKey::status].toString();
        proc.path = procObject[RJsonKey::path].toString();

        ret << proc;
    }

    return ret;
}

QList<MemoryMapDescription> CutterCore::getMemoryMap()
{
    QList<MemoryMapDescription> ret;
    QJsonArray memoryMapArray = cmdj("dmj").array();

    for (const QJsonValue &value : memoryMapArray) {
        QJsonObject memMapObject = value.toObject();

        MemoryMapDescription memMap;

        memMap.name = memMapObject[RJsonKey::name].toString();
        memMap.fileName = memMapObject[RJsonKey::file].toString();
        memMap.addrStart = memMapObject[RJsonKey::addr].toVariant().toULongLong();
        memMap.addrEnd = memMapObject[RJsonKey::addr_end].toVariant().toULongLong();
        memMap.type = memMapObject[RJsonKey::type].toString();
        memMap.permission = memMapObject[RJsonKey::perm].toString();

        ret << memMap;
    }

    return ret;
}

QStringList CutterCore::getStats()
{
    QStringList stats;
    cmdRaw("fs functions");

    // The cmd coomand is frequently used in this function because
    // cmdRaw would not work with grep
    stats << cmd("f~?").trimmed();

    QString imps = cmd("ii~?").trimmed();
    stats << imps;

    cmdRaw("fs symbols");
    stats << cmd("f~?").trimmed();
    cmdRaw("fs strings");
    stats << cmd("f~?").trimmed();
    cmdRaw("fs relocs");
    stats << cmd("f~?").trimmed();
    cmdRaw("fs sections");
    stats << cmd("f~?").trimmed();
    cmdRaw("fs *");
    stats << cmd("f~?").trimmed();

    return stats;
}

void CutterCore::setGraphEmpty(bool empty)
{
    emptyGraph = empty;
}

bool CutterCore::isGraphEmpty()
{
    return emptyGraph;
}

void CutterCore::getOpcodes()
{
    this->opcodes = cmdList("?O");
    this->regs = cmdList("drp~[1]");
}

void CutterCore::setSettings()
{
    setConfig("scr.interactive", false);

    setConfig("hex.pairs", false);
    setConfig("asm.xrefs", false);

    setConfig("asm.tabs.once", true);
    setConfig("asm.flags.middle", 2);

    setConfig("analysis.hasnext", false);
    setConfig("asm.lines.call", false);

    // Colors
    setConfig("scr.color", COLOR_MODE_DISABLED);

    // Don't show hits
    setConfig("search.flags", false);
}

QList<RVA> CutterCore::getSeekHistory()
{
    CORE_LOCK();
    QList<RVA> ret;

    QJsonArray jsonArray = cmdj("sj").array();
    for (const QJsonValue &value : jsonArray)
        ret << value.toVariant().toULongLong();

    return ret;
}

QStringList CutterCore::getAsmPluginNames()
{
    CORE_LOCK();
    RzListIter *it;
    QStringList ret;

    RzAsmPlugin *ap;
    CutterRListForeach(core->rasm->plugins, it, RzAsmPlugin, ap) { ret << ap->name; }

    return ret;
}

QStringList CutterCore::getAnalPluginNames()
{
    CORE_LOCK();
    RzListIter *it;
    QStringList ret;

    RzAnalysisPlugin *ap;
    CutterRListForeach(core->analysis->plugins, it, RzAnalysisPlugin, ap) { ret << ap->name; }

    return ret;
}

QList<RzBinPluginDescription> CutterCore::getRBinPluginDescriptions(const QString &type)
{
    QList<RzBinPluginDescription> ret;

    QJsonObject jsonRoot = cmdj("iLj").object();
    for (const QString &key : jsonRoot.keys()) {
        if (!type.isNull() && key != type)
            continue;

        QJsonArray pluginArray = jsonRoot[key].toArray();

        for (const QJsonValue &pluginValue : pluginArray) {
            QJsonObject pluginObject = pluginValue.toObject();

            RzBinPluginDescription desc;

            desc.name = pluginObject[RJsonKey::name].toString();
            desc.description = pluginObject[RJsonKey::description].toString();
            desc.license = pluginObject[RJsonKey::license].toString();
            desc.type = key;

            ret.append(desc);
        }
    }

    return ret;
}

QList<RzIOPluginDescription> CutterCore::getRIOPluginDescriptions()
{
    QList<RzIOPluginDescription> ret;

    QJsonArray plugins = cmdj("oLj").object()["io_plugins"].toArray();
    for (const QJsonValue &pluginValue : plugins) {
        QJsonObject pluginObject = pluginValue.toObject();

        RzIOPluginDescription plugin;

        plugin.name = pluginObject["name"].toString();
        plugin.description = pluginObject["description"].toString();
        plugin.license = pluginObject["license"].toString();
        plugin.permissions = pluginObject["permissions"].toString();
        for (const auto &uri : pluginObject["uris"].toArray()) {
            plugin.uris << uri.toString();
        }

        ret << plugin;
    }

    return ret;
}

QList<RzCorePluginDescription> CutterCore::getRCorePluginDescriptions()
{
    QList<RzCorePluginDescription> ret;

    QJsonArray plugins = cmdj("Lcj").array();
    for (const QJsonValue &pluginValue : plugins) {
        QJsonObject pluginObject = pluginValue.toObject();

        RzCorePluginDescription plugin;

        plugin.name = pluginObject["Name"].toString();
        plugin.description = pluginObject["Description"].toString();

        ret << plugin;
    }

    return ret;
}

QList<RzAsmPluginDescription> CutterCore::getRAsmPluginDescriptions()
{
    CORE_LOCK();
    RzListIter *it;
    QList<RzAsmPluginDescription> ret;

    RzAsmPlugin *ap;
    CutterRListForeach(core->rasm->plugins, it, RzAsmPlugin, ap)
    {
        RzAsmPluginDescription plugin;

        plugin.name = ap->name;
        plugin.architecture = ap->arch;
        plugin.author = ap->author;
        plugin.version = ap->version;
        plugin.cpus = ap->cpus;
        plugin.description = ap->desc;
        plugin.license = ap->license;

        ret << plugin;
    }

    return ret;
}

QList<FunctionDescription> CutterCore::getAllFunctions()
{
    CORE_LOCK();

    QList<FunctionDescription> funcList;
    funcList.reserve(rz_list_length(core->analysis->fcns));

    RzListIter *iter;
    RzAnalysisFunction *fcn;
    CutterRListForeach(core->analysis->fcns, iter, RzAnalysisFunction, fcn)
    {
        FunctionDescription function;
        function.offset = fcn->addr;
        function.linearSize = rz_analysis_function_linear_size(fcn);
        function.nargs = rz_analysis_var_count(core->analysis, fcn, 'b', 1)
                + rz_analysis_var_count(core->analysis, fcn, 'r', 1)
                + rz_analysis_var_count(core->analysis, fcn, 's', 1);
        function.nlocals = rz_analysis_var_count(core->analysis, fcn, 'b', 0)
                + rz_analysis_var_count(core->analysis, fcn, 'r', 0)
                + rz_analysis_var_count(core->analysis, fcn, 's', 0);
        function.nbbs = rz_list_length(fcn->bbs);
        function.calltype = fcn->cc ? QString::fromUtf8(fcn->cc) : QString();
        function.name = fcn->name ? QString::fromUtf8(fcn->name) : QString();
        function.edges = rz_analysis_function_count_edges(fcn, nullptr);
        function.stackframe = fcn->maxstack;
        funcList.append(function);
    }

    return funcList;
}

QList<ImportDescription> CutterCore::getAllImports()
{
    CORE_LOCK();
    QList<ImportDescription> ret;

    QJsonArray importsArray = cmdj("iij").array();

    for (const QJsonValue &value : importsArray) {
        QJsonObject importObject = value.toObject();

        ImportDescription import;

        import.plt = importObject[RJsonKey::plt].toVariant().toULongLong();
        import.ordinal = importObject[RJsonKey::ordinal].toInt();
        import.bind = importObject[RJsonKey::bind].toString();
        import.type = importObject[RJsonKey::type].toString();
        import.libname = importObject[RJsonKey::libname].toString();
        import.name = importObject[RJsonKey::name].toString();

        ret << import;
    }

    return ret;
}

QList<ExportDescription> CutterCore::getAllExports()
{
    CORE_LOCK();
    QList<ExportDescription> ret;

    QJsonArray exportsArray = cmdj("iEj").array();

    for (const QJsonValue &value : exportsArray) {
        QJsonObject exportObject = value.toObject();

        ExportDescription exp;

        exp.vaddr = exportObject[RJsonKey::vaddr].toVariant().toULongLong();
        exp.paddr = exportObject[RJsonKey::paddr].toVariant().toULongLong();
        exp.size = exportObject[RJsonKey::size].toVariant().toULongLong();
        exp.type = exportObject[RJsonKey::type].toString();
        exp.name = exportObject[RJsonKey::name].toString();
        exp.flag_name = exportObject[RJsonKey::flagname].toString();

        ret << exp;
    }

    return ret;
}

QList<SymbolDescription> CutterCore::getAllSymbols()
{
    CORE_LOCK();
    RzListIter *it;

    QList<SymbolDescription> ret;

    RzBinSymbol *bs;
    if (core && core->bin && core->bin->cur && core->bin->cur->o) {
        CutterRListForeach(core->bin->cur->o->symbols, it, RzBinSymbol, bs)
        {
            QString type = QString(bs->bind) + " " + QString(bs->type);
            SymbolDescription symbol;
            symbol.vaddr = bs->vaddr;
            symbol.name = QString(bs->name);
            symbol.bind = QString(bs->bind);
            symbol.type = QString(bs->type);
            ret << symbol;
        }

        /* list entrypoints as symbols too */
        int n = 0;
        RzBinAddr *entry;
        CutterRListForeach(core->bin->cur->o->entries, it, RzBinAddr, entry)
        {
            SymbolDescription symbol;
            symbol.vaddr = entry->vaddr;
            symbol.name = QString("entry") + QString::number(n++);
            symbol.bind.clear();
            symbol.type = "entry";
            ret << symbol;
        }
    }

    return ret;
}

QList<HeaderDescription> CutterCore::getAllHeaders()
{
    CORE_LOCK();
    QList<HeaderDescription> ret;

    QJsonArray headersArray = cmdj("ihj").array();

    for (const QJsonValue &value : headersArray) {
        QJsonObject headerObject = value.toObject();

        HeaderDescription header;

        header.vaddr = headerObject[RJsonKey::vaddr].toVariant().toULongLong();
        header.paddr = headerObject[RJsonKey::paddr].toVariant().toULongLong();
        header.value = headerObject[RJsonKey::comment].toString();
        header.name = headerObject[RJsonKey::name].toString();

        ret << header;
    }

    return ret;
}

QList<ZignatureDescription> CutterCore::getAllZignatures()
{
    CORE_LOCK();
    QList<ZignatureDescription> zignatures;

    QJsonArray zignaturesArray = cmdj("zj").array();

    for (const QJsonValue &value : zignaturesArray) {
        QJsonObject zignatureObject = value.toObject();

        ZignatureDescription zignature;

        zignature.name = zignatureObject[RJsonKey::name].toString();
        zignature.bytes = zignatureObject[RJsonKey::bytes].toString();
        zignature.offset = zignatureObject[RJsonKey::offset].toVariant().toULongLong();
        for (const QJsonValue &ref : zignatureObject[RJsonKey::refs].toArray()) {
            zignature.refs << ref.toString();
        }

        QJsonObject graphObject = zignatureObject[RJsonKey::graph].toObject();
        zignature.cc = graphObject[RJsonKey::cc].toVariant().toULongLong();
        zignature.nbbs = graphObject[RJsonKey::nbbs].toVariant().toULongLong();
        zignature.edges = graphObject[RJsonKey::edges].toVariant().toULongLong();
        zignature.ebbs = graphObject[RJsonKey::ebbs].toVariant().toULongLong();

        zignatures << zignature;
    }

    return zignatures;
}

QList<CommentDescription> CutterCore::getAllComments(const QString &filterType)
{
    CORE_LOCK();
    QList<CommentDescription> ret;

    QJsonArray commentsArray = cmdj("CCj").array();
    for (const QJsonValue &value : commentsArray) {
        QJsonObject commentObject = value.toObject();

        QString type = commentObject[RJsonKey::type].toString();
        if (type != filterType)
            continue;

        CommentDescription comment;
        comment.offset = commentObject[RJsonKey::offset].toVariant().toULongLong();
        comment.name = commentObject[RJsonKey::name].toString();

        ret << comment;
    }
    return ret;
}

QList<RelocDescription> CutterCore::getAllRelocs()
{
    CORE_LOCK();
    QList<RelocDescription> ret;

    if (core && core->bin && core->bin->cur && core->bin->cur->o) {
        auto relocs = core->bin->cur->o->relocs;
        RBIter iter;
        RzBinReloc *br;
        rz_rbtree_foreach(relocs, iter, br, RzBinReloc, vrb)
        {
            RelocDescription reloc;

            reloc.vaddr = br->vaddr;
            reloc.paddr = br->paddr;
            reloc.type = (br->additive ? "ADD_" : "SET_") + QString::number(br->type);

            if (br->import)
                reloc.name = br->import->name;
            else
                reloc.name = QString("reloc_%1").arg(QString::number(br->vaddr, 16));

            ret << reloc;
        }
    }

    return ret;
}

QList<StringDescription> CutterCore::getAllStrings()
{
    return parseStringsJson(cmdjTask("izzj"));
}

QList<StringDescription> CutterCore::parseStringsJson(const QJsonDocument &doc)
{
    QList<StringDescription> ret;

    QJsonArray stringsArray = doc.array();
    for (const QJsonValue &value : stringsArray) {
        QJsonObject stringObject = value.toObject();

        StringDescription string;

        string.string = stringObject[RJsonKey::string].toString();
        string.vaddr = stringObject[RJsonKey::vaddr].toVariant().toULongLong();
        string.type = stringObject[RJsonKey::type].toString();
        string.size = stringObject[RJsonKey::size].toVariant().toUInt();
        string.length = stringObject[RJsonKey::length].toVariant().toUInt();
        string.section = stringObject[RJsonKey::section].toString();

        ret << string;
    }

    return ret;
}

QList<FlagspaceDescription> CutterCore::getAllFlagspaces()
{
    CORE_LOCK();
    QList<FlagspaceDescription> ret;

    QJsonArray flagspacesArray = cmdj("fsj").array();
    for (const QJsonValue &value : flagspacesArray) {
        QJsonObject flagspaceObject = value.toObject();

        FlagspaceDescription flagspace;

        flagspace.name = flagspaceObject[RJsonKey::name].toString();

        ret << flagspace;
    }
    return ret;
}

QList<FlagDescription> CutterCore::getAllFlags(QString flagspace)
{
    CORE_LOCK();
    QList<FlagDescription> ret;

    if (!flagspace.isEmpty())
        cmdRaw("fs " + flagspace);
    else
        cmdRaw("fs *");

    QJsonArray flagsArray = cmdj("fj").array();
    for (const QJsonValue &value : flagsArray) {
        QJsonObject flagObject = value.toObject();

        FlagDescription flag;

        flag.offset = flagObject[RJsonKey::offset].toVariant().toULongLong();
        flag.size = flagObject[RJsonKey::size].toVariant().toULongLong();
        flag.name = flagObject[RJsonKey::name].toString();
        flag.realname = flagObject[RJsonKey::realname].toString();

        ret << flag;
    }
    return ret;
}

QList<SectionDescription> CutterCore::getAllSections()
{
    CORE_LOCK();
    QList<SectionDescription> sections;

    QJsonDocument sectionsDoc = cmdj("iSj entropy");
    QJsonObject sectionsObj = sectionsDoc.object();
    QJsonArray sectionsArray = sectionsObj[RJsonKey::sections].toArray();

    for (const QJsonValue &value : sectionsArray) {
        QJsonObject sectionObject = value.toObject();

        QString name = sectionObject[RJsonKey::name].toString();
        if (name.isEmpty())
            continue;

        SectionDescription section;
        section.name = name;
        section.vaddr = sectionObject[RJsonKey::vaddr].toVariant().toULongLong();
        section.vsize = sectionObject[RJsonKey::vsize].toVariant().toULongLong();
        section.paddr = sectionObject[RJsonKey::paddr].toVariant().toULongLong();
        section.size = sectionObject[RJsonKey::size].toVariant().toULongLong();
        section.perm = sectionObject[RJsonKey::perm].toString();
        section.entropy = sectionObject[RJsonKey::entropy].toString();

        sections << section;
    }
    return sections;
}

QStringList CutterCore::getSectionList()
{
    CORE_LOCK();
    QStringList ret;

    QJsonArray sectionsArray = cmdj("iSj").array();
    for (const QJsonValue &value : sectionsArray) {
        ret << value.toObject()[RJsonKey::name].toString();
    }
    return ret;
}

QList<SegmentDescription> CutterCore::getAllSegments()
{
    CORE_LOCK();
    QList<SegmentDescription> ret;

    QJsonArray segments = cmdj("iSSj").array();

    for (const QJsonValue &value : segments) {
        QJsonObject segmentObject = value.toObject();

        QString name = segmentObject[RJsonKey::name].toString();
        if (name.isEmpty())
            continue;

        SegmentDescription segment;
        segment.name = name;
        segment.vaddr = segmentObject[RJsonKey::vaddr].toVariant().toULongLong();
        segment.paddr = segmentObject[RJsonKey::paddr].toVariant().toULongLong();
        segment.size = segmentObject[RJsonKey::size].toVariant().toULongLong();
        segment.vsize = segmentObject[RJsonKey::vsize].toVariant().toULongLong();
        segment.perm = segmentObject[RJsonKey::perm].toString();

        ret << segment;
    }
    return ret;
}

QList<EntrypointDescription> CutterCore::getAllEntrypoint()
{
    CORE_LOCK();
    QList<EntrypointDescription> ret;

    QJsonArray entrypointsArray = cmdj("iej").array();
    for (const QJsonValue &value : entrypointsArray) {
        QJsonObject entrypointObject = value.toObject();

        EntrypointDescription entrypoint;

        entrypoint.vaddr = entrypointObject[RJsonKey::vaddr].toVariant().toULongLong();
        entrypoint.paddr = entrypointObject[RJsonKey::paddr].toVariant().toULongLong();
        entrypoint.baddr = entrypointObject[RJsonKey::baddr].toVariant().toULongLong();
        entrypoint.laddr = entrypointObject[RJsonKey::laddr].toVariant().toULongLong();
        entrypoint.haddr = entrypointObject[RJsonKey::haddr].toVariant().toULongLong();
        entrypoint.type = entrypointObject[RJsonKey::type].toString();

        ret << entrypoint;
    }
    return ret;
}

QList<BinClassDescription> CutterCore::getAllClassesFromBin()
{
    CORE_LOCK();
    QList<BinClassDescription> ret;

    QJsonArray classesArray = cmdj("icj").array();
    for (const QJsonValue &value : classesArray) {
        QJsonObject classObject = value.toObject();

        BinClassDescription cls;

        cls.name = classObject[RJsonKey::classname].toString();
        cls.addr = classObject[RJsonKey::addr].toVariant().toULongLong();
        cls.index = classObject[RJsonKey::index].toVariant().toULongLong();

        for (const QJsonValue &value2 : classObject[RJsonKey::methods].toArray()) {
            QJsonObject methObject = value2.toObject();

            BinClassMethodDescription meth;

            meth.name = methObject[RJsonKey::name].toString();
            meth.addr = methObject[RJsonKey::addr].toVariant().toULongLong();

            cls.methods << meth;
        }

        for (const QJsonValue &value2 : classObject[RJsonKey::fields].toArray()) {
            QJsonObject fieldObject = value2.toObject();

            BinClassFieldDescription field;

            field.name = fieldObject[RJsonKey::name].toString();
            field.addr = fieldObject[RJsonKey::addr].toVariant().toULongLong();

            cls.fields << field;
        }

        ret << cls;
    }
    return ret;
}

QList<BinClassDescription> CutterCore::getAllClassesFromFlags()
{
    static const QRegularExpression classFlagRegExp("^class\\.(.*)$");
    static const QRegularExpression methodFlagRegExp("^method\\.([^\\.]*)\\.(.*)$");

    CORE_LOCK();
    QList<BinClassDescription> ret;
    QMap<QString, BinClassDescription *> classesCache;

    QJsonArray flagsArray = cmdj("fj@F:classes").array();
    for (const QJsonValue &value : flagsArray) {
        QJsonObject flagObject = value.toObject();

        QString flagName = flagObject[RJsonKey::name].toString();

        QRegularExpressionMatch match = classFlagRegExp.match(flagName);
        if (match.hasMatch()) {
            QString className = match.captured(1);
            BinClassDescription *desc = nullptr;
            auto it = classesCache.find(className);
            if (it == classesCache.end()) {
                BinClassDescription cls = {};
                ret << cls;
                desc = &ret.last();
                classesCache[className] = desc;
            } else {
                desc = it.value();
            }
            desc->name = match.captured(1);
            desc->addr = flagObject[RJsonKey::offset].toVariant().toULongLong();
            desc->index = RVA_INVALID;
            continue;
        }

        match = methodFlagRegExp.match(flagName);
        if (match.hasMatch()) {
            QString className = match.captured(1);
            BinClassDescription *classDesc = nullptr;
            auto it = classesCache.find(className);
            if (it == classesCache.end()) {
                // add a new stub class, will be replaced if class flag comes after it
                BinClassDescription cls;
                cls.name = tr("Unknown (%1)").arg(className);
                cls.addr = RVA_INVALID;
                cls.index = 0;
                ret << cls;
                classDesc = &ret.last();
                classesCache[className] = classDesc;
            } else {
                classDesc = it.value();
            }

            BinClassMethodDescription meth;
            meth.name = match.captured(2);
            meth.addr = flagObject[RJsonKey::offset].toVariant().toULongLong();
            classDesc->methods << meth;
            continue;
        }
    }
    return ret;
}

QList<QString> CutterCore::getAllAnalClasses(bool sorted)
{
    CORE_LOCK();
    QList<QString> ret;

    SdbListPtr l = makeSdbListPtr(rz_analysis_class_get_all(core->analysis, sorted));
    if (!l) {
        return ret;
    }
    ret.reserve(static_cast<int>(l->length));

    SdbListIter *it;
    void *entry;
    ls_foreach(l, it, entry)
    {
        auto kv = reinterpret_cast<SdbKv *>(entry);
        ret.append(QString::fromUtf8(reinterpret_cast<const char *>(kv->base.key)));
    }

    return ret;
}

QList<AnalMethodDescription> CutterCore::getAnalClassMethods(const QString &cls)
{
    CORE_LOCK();
    QList<AnalMethodDescription> ret;

    RzVector *meths = rz_analysis_class_method_get_all(core->analysis, cls.toUtf8().constData());
    if (!meths) {
        return ret;
    }

    ret.reserve(static_cast<int>(meths->len));
    RzAnalysisMethod *meth;
    CutterRVectorForeach(meths, meth, RzAnalysisMethod)
    {
        AnalMethodDescription desc;
        desc.name = QString::fromUtf8(meth->name);
        desc.addr = meth->addr;
        desc.vtableOffset = meth->vtable_offset;
        ret.append(desc);
    }
    rz_vector_free(meths);

    return ret;
}

QList<AnalBaseClassDescription> CutterCore::getAnalClassBaseClasses(const QString &cls)
{
    CORE_LOCK();
    QList<AnalBaseClassDescription> ret;

    RzVector *bases = rz_analysis_class_base_get_all(core->analysis, cls.toUtf8().constData());
    if (!bases) {
        return ret;
    }

    ret.reserve(static_cast<int>(bases->len));
    RzAnalysisBaseClass *base;
    CutterRVectorForeach(bases, base, RzAnalysisBaseClass)
    {
        AnalBaseClassDescription desc;
        desc.id = QString::fromUtf8(base->id);
        desc.offset = base->offset;
        desc.className = QString::fromUtf8(base->class_name);
        ret.append(desc);
    }
    rz_vector_free(bases);

    return ret;
}

QList<AnalVTableDescription> CutterCore::getAnalClassVTables(const QString &cls)
{
    CORE_LOCK();
    QList<AnalVTableDescription> acVtables;

    RzVector *vtables = rz_analysis_class_vtable_get_all(core->analysis, cls.toUtf8().constData());
    if (!vtables) {
        return acVtables;
    }

    acVtables.reserve(static_cast<int>(vtables->len));
    RzAnalysisVTable *vtable;
    CutterRVectorForeach(vtables, vtable, RzAnalysisVTable)
    {
        AnalVTableDescription desc;
        desc.id = QString::fromUtf8(vtable->id);
        desc.offset = vtable->offset;
        desc.addr = vtable->addr;
        acVtables.append(desc);
    }
    rz_vector_free(vtables);

    return acVtables;
}

void CutterCore::createNewClass(const QString &cls)
{
    CORE_LOCK();
    rz_analysis_class_create(core->analysis, cls.toUtf8().constData());
}

void CutterCore::renameClass(const QString &oldName, const QString &newName)
{
    CORE_LOCK();
    rz_analysis_class_rename(core->analysis, oldName.toUtf8().constData(),
                             newName.toUtf8().constData());
}

void CutterCore::deleteClass(const QString &cls)
{
    CORE_LOCK();
    rz_analysis_class_delete(core->analysis, cls.toUtf8().constData());
}

bool CutterCore::getAnalMethod(const QString &cls, const QString &meth, AnalMethodDescription *desc)
{
    CORE_LOCK();
    RzAnalysisMethod analMeth;
    if (rz_analysis_class_method_get(core->analysis, cls.toUtf8().constData(),
                                     meth.toUtf8().constData(), &analMeth)
        != RZ_ANALYSIS_CLASS_ERR_SUCCESS) {
        return false;
    }
    desc->name = QString::fromUtf8(analMeth.name);
    desc->addr = analMeth.addr;
    desc->vtableOffset = analMeth.vtable_offset;
    rz_analysis_class_method_fini(&analMeth);
    return true;
}

void CutterCore::setAnalMethod(const QString &className, const AnalMethodDescription &meth)
{
    CORE_LOCK();
    RzAnalysisMethod analMeth;
    analMeth.name = strdup(meth.name.toUtf8().constData());
    analMeth.addr = meth.addr;
    analMeth.vtable_offset = meth.vtableOffset;
    rz_analysis_class_method_set(core->analysis, className.toUtf8().constData(), &analMeth);
    rz_analysis_class_method_fini(&analMeth);
}

void CutterCore::renameAnalMethod(const QString &className, const QString &oldMethodName,
                                  const QString &newMethodName)
{
    CORE_LOCK();
    rz_analysis_class_method_rename(core->analysis, className.toUtf8().constData(),
                                    oldMethodName.toUtf8().constData(),
                                    newMethodName.toUtf8().constData());
}

QList<ResourcesDescription> CutterCore::getAllResources()
{
    CORE_LOCK();
    QList<ResourcesDescription> resources;

    QJsonArray resourcesArray = cmdj("iRj").array();
    for (const QJsonValue &value : resourcesArray) {
        QJsonObject resourceObject = value.toObject();

        ResourcesDescription res;

        res.name = resourceObject[RJsonKey::name].toString();
        res.vaddr = resourceObject[RJsonKey::vaddr].toVariant().toULongLong();
        res.index = resourceObject[RJsonKey::index].toVariant().toULongLong();
        res.type = resourceObject[RJsonKey::type].toString();
        res.size = resourceObject[RJsonKey::size].toVariant().toULongLong();
        res.lang = resourceObject[RJsonKey::lang].toString();

        resources << res;
    }
    return resources;
}

QList<VTableDescription> CutterCore::getAllVTables()
{
    CORE_LOCK();
    QList<VTableDescription> vtables;

    QJsonArray vTablesArray = cmdj("avj").array();
    for (const QJsonValue &vTableValue : vTablesArray) {
        QJsonObject vTableObject = vTableValue.toObject();

        VTableDescription res;

        res.addr = vTableObject[RJsonKey::offset].toVariant().toULongLong();
        QJsonArray methodArray = vTableObject[RJsonKey::methods].toArray();

        for (const QJsonValue &methodValue : methodArray) {
            QJsonObject methodObject = methodValue.toObject();

            BinClassMethodDescription method;

            method.addr = methodObject[RJsonKey::offset].toVariant().toULongLong();
            method.name = methodObject[RJsonKey::name].toString();

            res.methods << method;
        }

        vtables << res;
    }
    return vtables;
}

QList<TypeDescription> CutterCore::getAllTypes()
{
    QList<TypeDescription> types;

    types.append(getAllPrimitiveTypes());
    types.append(getAllUnions());
    types.append(getAllStructs());
    types.append(getAllEnums());
    types.append(getAllTypedefs());

    return types;
}

QList<TypeDescription> CutterCore::getAllPrimitiveTypes()
{
    CORE_LOCK();
    QList<TypeDescription> primitiveTypes;

    QJsonArray typesArray = cmdj("tj").array();
    for (const QJsonValue &value : typesArray) {
        QJsonObject typeObject = value.toObject();

        TypeDescription exp;

        exp.type = typeObject[RJsonKey::type].toString();
        exp.size = typeObject[RJsonKey::size].toVariant().toULongLong();
        exp.format = typeObject[RJsonKey::format].toString();
        exp.category = tr("Primitive");
        primitiveTypes << exp;
    }

    return primitiveTypes;
}

QList<TypeDescription> CutterCore::getAllUnions()
{
    CORE_LOCK();
    QList<TypeDescription> unions;

    QJsonArray typesArray = cmdj("tuj").array();
    for (const QJsonValue value : typesArray) {
        QJsonObject typeObject = value.toObject();

        TypeDescription exp;

        exp.type = typeObject[RJsonKey::type].toString();
        exp.size = typeObject[RJsonKey::size].toVariant().toULongLong();
        exp.category = "Union";
        unions << exp;
    }

    return unions;
}

QList<TypeDescription> CutterCore::getAllStructs()
{
    CORE_LOCK();
    QList<TypeDescription> structs;

    QJsonArray typesArray = cmdj("tsj").array();
    for (const QJsonValue value : typesArray) {
        QJsonObject typeObject = value.toObject();

        TypeDescription exp;

        exp.type = typeObject[RJsonKey::type].toString();
        exp.size = typeObject[RJsonKey::size].toVariant().toULongLong();
        exp.category = "Struct";
        structs << exp;
    }

    return structs;
}

QList<TypeDescription> CutterCore::getAllEnums()
{
    CORE_LOCK();
    QList<TypeDescription> enums;

    QJsonObject typesObject = cmdj("tej").object();
    for (QString key : typesObject.keys()) {
        TypeDescription exp;
        exp.type = key;
        exp.size = 0;
        exp.category = "Enum";
        enums << exp;
    }

    return enums;
}

QList<TypeDescription> CutterCore::getAllTypedefs()
{
    CORE_LOCK();
    QList<TypeDescription> typeDefs;

    QJsonObject typesObject = cmdj("ttj").object();
    for (QString key : typesObject.keys()) {
        TypeDescription exp;
        exp.type = key;
        exp.size = 0;
        exp.category = "Typedef";
        typeDefs << exp;
    }

    return typeDefs;
}

QString CutterCore::addTypes(const char *str)
{
    CORE_LOCK();
    char *error_msg = nullptr;
    char *parsed = rz_parse_c_string(core->analysis, str, &error_msg);
    QString error;

    if (!parsed) {
        if (error_msg) {
            error = error_msg;
            rz_mem_free(error_msg);
        }
        return error;
    }

    rz_analysis_save_parsed_type(core->analysis, parsed);
    rz_mem_free(parsed);

    if (error_msg) {
        error = error_msg;
        rz_mem_free(error_msg);
    }

    return error;
}

QString CutterCore::getTypeAsC(QString name, QString category)
{
    CORE_LOCK();
    QString output = "Failed to fetch the output.";
    if (name.isEmpty() || category.isEmpty()) {
        return output;
    }
    QString typeName = sanitizeStringForCommand(name);
    if (category == "Struct") {
        output = cmdRaw(QString("tsc %1").arg(typeName));
    } else if (category == "Union") {
        output = cmdRaw(QString("tuc %1").arg(typeName));
    } else if (category == "Enum") {
        output = cmdRaw(QString("tec %1").arg(typeName));
    } else if (category == "Typedef") {
        output = cmdRaw(QString("ttc %1").arg(typeName));
    }
    return output;
}

bool CutterCore::isAddressMapped(RVA addr)
{
    // If value returned by "om. @ addr" is empty means that address is not mapped
    return !Core()->cmdRawAt(QString("om."), addr).isEmpty();
}

QList<SearchDescription> CutterCore::getAllSearch(QString searchFor, QString space, QString in)
{
    CORE_LOCK();
    QList<SearchDescription> searchRef;

    QJsonArray searchArray;
    {
        TempConfig cfg;
        cfg.set("search.in", in);
        searchArray = cmdj(QString("%1 %2").arg(space, searchFor)).array();
    }

    if (space == "/Rj") {
        for (const QJsonValue &value : searchArray) {
            QJsonObject searchObject = value.toObject();

            SearchDescription exp;

            exp.code.clear();
            for (const QJsonValue &value2 : searchObject[RJsonKey::opcodes].toArray()) {
                QJsonObject gadget = value2.toObject();
                exp.code += gadget[RJsonKey::opcode].toString() + ";  ";
            }

            exp.offset = searchObject[RJsonKey::opcodes]
                                 .toArray()
                                 .first()
                                 .toObject()[RJsonKey::offset]
                                 .toVariant()
                                 .toULongLong();
            exp.size = searchObject[RJsonKey::size].toVariant().toULongLong();

            searchRef << exp;
        }
    } else {
        for (const QJsonValue &value : searchArray) {
            QJsonObject searchObject = value.toObject();

            SearchDescription exp;

            exp.offset = searchObject[RJsonKey::offset].toVariant().toULongLong();
            exp.size = searchObject[RJsonKey::len].toVariant().toULongLong();
            exp.code = searchObject[RJsonKey::code].toString();
            exp.data = searchObject[RJsonKey::data].toString();

            searchRef << exp;
        }
    }
    return searchRef;
}

BlockStatistics CutterCore::getBlockStatistics(unsigned int blocksCount)
{
    BlockStatistics blockStats;
    if (blocksCount == 0) {
        blockStats.from = blockStats.to = blockStats.blocksize = 0;
        return blockStats;
    }

    QJsonObject statsObj;

    // User TempConfig here to set the search boundaries to all sections. This makes sure
    // that the Visual Navbar will show all the relevant addresses.
    {
        TempConfig tempConfig;
        tempConfig.set("search.in", "bin.sections");
        statsObj = cmdj("p-j " + QString::number(blocksCount)).object();
    }

    blockStats.from = statsObj[RJsonKey::from].toVariant().toULongLong();
    blockStats.to = statsObj[RJsonKey::to].toVariant().toULongLong();
    blockStats.blocksize = statsObj[RJsonKey::blocksize].toVariant().toULongLong();

    QJsonArray blocksArray = statsObj[RJsonKey::blocks].toArray();

    for (const QJsonValue &value : blocksArray) {
        QJsonObject blockObj = value.toObject();

        BlockDescription block;

        block.addr = blockObj[RJsonKey::offset].toVariant().toULongLong();
        block.size = blockObj[RJsonKey::size].toVariant().toULongLong();
        block.flags = blockObj[RJsonKey::flags].toInt(0);
        block.functions = blockObj[RJsonKey::functions].toInt(0);
        block.inFunctions = blockObj[RJsonKey::in_functions].toInt(0);
        block.comments = blockObj[RJsonKey::comments].toInt(0);
        block.symbols = blockObj[RJsonKey::symbols].toInt(0);
        block.strings = blockObj[RJsonKey::strings].toInt(0);

        block.rwx = 0;
        QString rwxStr = blockObj[RJsonKey::rwx].toString();
        if (rwxStr.length() == 3) {
            if (rwxStr[0] == 'r') {
                block.rwx |= (1 << 0);
            }
            if (rwxStr[1] == 'w') {
                block.rwx |= (1 << 1);
            }
            if (rwxStr[2] == 'x') {
                block.rwx |= (1 << 2);
            }
        }

        blockStats.blocks << block;
    }

    return blockStats;
}

QList<XrefDescription> CutterCore::getXRefsForVariable(QString variableName, bool findWrites,
                                                       RVA offset)
{
    QList<XrefDescription> xrefList = QList<XrefDescription>();
    QJsonArray xrefsArray;
    if (findWrites) {
        xrefsArray = cmdjAt("afvWj", offset).array();
    } else {
        xrefsArray = cmdjAt("afvRj", offset).array();
    }
    for (const QJsonValue &value : xrefsArray) {
        QJsonObject xrefObject = value.toObject();
        QString name = xrefObject[RJsonKey::name].toString();
        if (name == variableName) {
            QJsonArray addressArray = xrefObject[RJsonKey::addrs].toArray();
            for (const QJsonValue &address : addressArray) {
                XrefDescription xref;
                RVA addr = address.toVariant().toULongLong();
                xref.from = addr;
                xref.to = addr;
                if (findWrites) {
                    xref.from_str = RAddressString(addr);
                } else {
                    xref.to_str = RAddressString(addr);
                }
                xrefList << xref;
            }
        }
    }
    return xrefList;
}

QList<XrefDescription> CutterCore::getXRefs(RVA addr, bool to, bool whole_function,
                                            const QString &filterType)
{
    QList<XrefDescription> xrefList = QList<XrefDescription>();

    QJsonArray xrefsArray;

    if (to) {
        xrefsArray = cmdj("axtj@" + QString::number(addr)).array();
    } else {
        xrefsArray = cmdj("axfj@" + QString::number(addr)).array();
    }

    for (const QJsonValue &value : xrefsArray) {
        QJsonObject xrefObject = value.toObject();

        XrefDescription xref;

        xref.type = xrefObject[RJsonKey::type].toString();

        if (!filterType.isNull() && filterType != xref.type)
            continue;

        xref.from = xrefObject[RJsonKey::from].toVariant().toULongLong();
        if (!to) {
            xref.from_str = RAddressString(xref.from);
        } else {
            QString fcn = xrefObject[RJsonKey::fcn_name].toString();
            if (!fcn.isEmpty()) {
                RVA fcnAddr = xrefObject[RJsonKey::fcn_addr].toVariant().toULongLong();
                xref.from_str = fcn + " + 0x" + QString::number(xref.from - fcnAddr, 16);
            } else {
                xref.from_str = RAddressString(xref.from);
            }
        }

        if (!whole_function && !to && xref.from != addr) {
            continue;
        }

        if (to && !xrefObject.contains(RJsonKey::to)) {
            xref.to = addr;
        } else {
            xref.to = xrefObject[RJsonKey::to].toVariant().toULongLong();
        }
        xref.to_str = Core()->cmdRaw(QString("fd %1").arg(xref.to)).trimmed();

        xrefList << xref;
    }

    return xrefList;
}

void CutterCore::addFlag(RVA offset, QString name, RVA size)
{
    name = sanitizeStringForCommand(name);
    cmdRawAt(QString("f %1 %2").arg(name).arg(size), offset);
    emit flagsChanged();
}

/**
 * @brief Gets all the flags present at a specific address
 * @param addr The address to be checked
 * @return String containing all the flags which are comma-separated
 */
QString CutterCore::listFlagsAsStringAt(RVA addr)
{
    CORE_LOCK();
    char *flagList = rz_flag_get_liststr(core->flags, addr);
    QString result = fromOwnedCharPtr(flagList);
    return result;
}

QString CutterCore::nearestFlag(RVA offset, RVA *flagOffsetOut)
{
    auto r = cmdj(QString("fdj @") + QString::number(offset)).object();
    QString name = r.value("name").toString();
    if (flagOffsetOut) {
        auto offsetValue = r.value("offset");
        *flagOffsetOut = offsetValue.isUndefined() ? offset : offsetValue.toVariant().toULongLong();
    }
    return name;
}

void CutterCore::handleREvent(int type, void *data)
{
    switch (type) {
    case RZ_EVENT_CLASS_NEW: {
        auto ev = reinterpret_cast<RzEventClass *>(data);
        emit classNew(QString::fromUtf8(ev->name));
        break;
    }
    case RZ_EVENT_CLASS_DEL: {
        auto ev = reinterpret_cast<RzEventClass *>(data);
        emit classDeleted(QString::fromUtf8(ev->name));
        break;
    }
    case RZ_EVENT_CLASS_RENAME: {
        auto ev = reinterpret_cast<RzEventClassRename *>(data);
        emit classRenamed(QString::fromUtf8(ev->name_old), QString::fromUtf8(ev->name_new));
        break;
    }
    case RZ_EVENT_CLASS_ATTR_SET: {
        auto ev = reinterpret_cast<RzEventClassAttrSet *>(data);
        emit classAttrsChanged(QString::fromUtf8(ev->attr.class_name));
        break;
    }
    case RZ_EVENT_CLASS_ATTR_DEL: {
        auto ev = reinterpret_cast<RzEventClassAttr *>(data);
        emit classAttrsChanged(QString::fromUtf8(ev->class_name));
        break;
    }
    case RZ_EVENT_CLASS_ATTR_RENAME: {
        auto ev = reinterpret_cast<RzEventClassAttrRename *>(data);
        emit classAttrsChanged(QString::fromUtf8(ev->attr.class_name));
        break;
    }
    case RZ_EVENT_DEBUG_PROCESS_FINISHED: {
        auto ev = reinterpret_cast<RzEventDebugProcessFinished *>(data);
        emit debugProcessFinished(ev->pid);
        break;
    }
    default:
        break;
    }
}

void CutterCore::triggerFlagsChanged()
{
    emit flagsChanged();
}

void CutterCore::triggerVarsChanged()
{
    emit varsChanged();
}

void CutterCore::triggerFunctionRenamed(const RVA offset, const QString &newName)
{
    emit functionRenamed(offset, newName);
}

void CutterCore::loadPDB(const QString &file)
{
    cmdRaw("idp " + sanitizeStringForCommand(file));
}

QList<DisassemblyLine> CutterCore::disassembleLines(RVA offset, int lines)
{
    QJsonArray array = cmdj(QString("pdJ ") + QString::number(lines) + QString(" @ ")
                            + QString::number(offset))
                               .array();
    QList<DisassemblyLine> r;

    for (const QJsonValueRef &value : array) {
        QJsonObject object = value.toObject();
        DisassemblyLine line;
        line.offset = object[RJsonKey::offset].toVariant().toULongLong();
        line.text = ansiEscapeToHtml(object[RJsonKey::text].toString());
        const auto &arrow = object[RJsonKey::arrow];
        line.arrow = arrow.isNull() ? RVA_INVALID : arrow.toVariant().toULongLong();
        r << line;
    }

    return r;
}

/**
 * @brief return hexdump of <size> from an <offset> by a given formats
 * @param address - the address from which to print the hexdump
 * @param size - number of bytes to print
 * @param format - the type of hexdump (qwords, words. decimal, etc)
 */
QString CutterCore::hexdump(RVA address, int size, HexdumpFormats format)
{
    QString command = "px";
    switch (format) {
    case HexdumpFormats::Normal:
        break;
    case HexdumpFormats::Half:
        command += "h";
        break;
    case HexdumpFormats::Word:
        command += "w";
        break;
    case HexdumpFormats::Quad:
        command += "q";
        break;
    case HexdumpFormats::Signed:
        command += "d";
        break;
    case HexdumpFormats::Octal:
        command += "o";
        break;
    }

    return cmdRawAt(QString("%1 %2").arg(command).arg(size), address);
}

QByteArray CutterCore::hexStringToBytes(const QString &hex)
{
    QByteArray hexChars = hex.toUtf8();
    QByteArray bytes;
    bytes.reserve(hexChars.length() / 2);
    int size = rz_hex_str2bin(hexChars.constData(), reinterpret_cast<ut8 *>(bytes.data()));
    bytes.resize(size);
    return bytes;
}

QString CutterCore::bytesToHexString(const QByteArray &bytes)
{
    QByteArray hex;
    hex.resize(bytes.length() * 2);
    rz_hex_bin2str(reinterpret_cast<const ut8 *>(bytes.constData()), bytes.size(), hex.data());
    return QString::fromUtf8(hex);
}

void CutterCore::loadScript(const QString &scriptname)
{
    {
        CORE_LOCK();
        rz_core_cmd_file(core, scriptname.toUtf8().constData());
    }
    triggerRefreshAll();
}

QString CutterCore::getRizinVersionReadable()
{
    return QString("%1 (%2)").arg(QString::fromUtf8(RZ_VERSION),
                                  QString::fromUtf8(RZ_GITTIP).left(7));
}

QString CutterCore::getVersionInformation()
{
    int i;
    QString versionInfo;
    struct vcs_t
    {
        const char *name;
        const char *(*callback)();
    } vcs[] = {
        { "rz_analysis", &rz_analysis_version },
        { "rz_lib", &rz_lib_version },
        { "rz_egg", &rz_egg_version },
        { "rz_asm", &rz_asm_version },
        { "rz_bin", &rz_bin_version },
        { "rz_cons", &rz_cons_version },
        { "rz_flag", &rz_flag_version },
        { "rz_core", &rz_core_version },
        { "rz_crypto", &rz_crypto_version },
        { "rz_bp", &rz_bp_version },
        { "rz_debug", &rz_debug_version },
        { "rz_hash", &rz_hash_version },
        { "rz_io", &rz_io_version },
#if !USE_LIB_MAGIC
        { "rz_magic", &rz_magic_version },
#endif
        { "rz_parse", &rz_parse_version },
        { "rz_reg", &rz_reg_version },
        { "rz_sign", &rz_sign_version },
        { "rz_search", &rz_search_version },
        { "rz_syscall", &rz_syscall_version },
        { "rz_util", &rz_util_version },
        /* ... */
        { NULL, NULL }
    };
    versionInfo.append(QString("%1 rz\n").arg(getRizinVersionReadable()));
    for (i = 0; vcs[i].name; i++) {
        struct vcs_t *v = &vcs[i];
        const char *name = v->callback();
        versionInfo.append(QString("%1 %2\n").arg(name, v->name));
    }
    return versionInfo;
}

QJsonArray CutterCore::getOpenedFiles()
{
    QJsonDocument files = cmdj("oj");
    return files.array();
}

QList<QString> CutterCore::getColorThemes()
{
    QList<QString> r;
    QJsonDocument themes = cmdj("ecoj");
    for (const QJsonValue &s : themes.array()) {
        r << s.toString();
    }
    return r;
}

QString CutterCore::ansiEscapeToHtml(const QString &text)
{
    int len;
    char *html = rz_cons_html_filter(text.toUtf8().constData(), &len);
    if (!html) {
        return QString();
    }
    QString r = QString::fromUtf8(html, len);
    rz_mem_free(html);
    return r;
}

BasicBlockHighlighter *CutterCore::getBBHighlighter()
{
    return bbHighlighter;
}

BasicInstructionHighlighter *CutterCore::getBIHighlighter()
{
    return &biHighlighter;
}

void CutterCore::setIOCache(bool enabled)
{
    if (enabled) {
        // disable write mode when cache is enabled
        setWriteMode(false);
    }
    setConfig("io.cache", enabled);
    this->iocache = enabled;

    emit ioCacheChanged(enabled);
    emit ioModeChanged();
}

bool CutterCore::isIOCacheEnabled() const
{
    return iocache;
}

void CutterCore::commitWriteCache()
{
    // Temporarily disable cache mode
    TempConfig tempConfig;
    tempConfig.set("io.cache", false);
    if (!isWriteModeEnabled()) {
        cmdRaw("oo+");
        cmdRaw("wci");
        cmdRaw("oo");
    } else {
        cmdRaw("wci");
    }
}

// Enable or disable write-mode. Avoid unecessary changes if not need.
void CutterCore::setWriteMode(bool enabled)
{
    bool writeModeState = isWriteModeEnabled();

    if (writeModeState == enabled && !this->iocache) {
        // New mode is the same as current and IO Cache is disabled. Do nothing.
        return;
    }

    // Change from read-only to write-mode
    if (enabled && !writeModeState) {
        cmdRaw("oo+");
        // Change from write-mode to read-only
    } else {
        cmdRaw("oo");
    }
    // Disable cache mode because we specifically set write or
    // read-only modes.
    setIOCache(false);
    writeModeChanged(enabled);
    emit ioModeChanged();
}

bool CutterCore::isWriteModeEnabled()
{
    using namespace std;
    QJsonArray ans = cmdj("oj").array();
    return find_if(begin(ans), end(ans),
                   [](const QJsonValue &v) { return v.toObject().value("raised").toBool(); })
            ->toObject()
            .value("writable")
            .toBool();
}

/**
 * @brief get a compact disassembly preview for tooltips
 * @param address - the address from which to print the disassembly
 * @param num_of_lines - number of instructions to print
 */
QStringList CutterCore::getDisassemblyPreview(RVA address, int num_of_lines)
{
    QList<DisassemblyLine> disassemblyLines;
    {
        // temporarily simplify the disasm output to get it colorful and simple to read
        TempConfig tempConfig;
        tempConfig.set("scr.color", COLOR_MODE_16M)
                .set("asm.lines", false)
                .set("asm.var", false)
                .set("asm.comments", false)
                .set("asm.bytes", false)
                .set("asm.lines.fcn", false)
                .set("asm.lines.out", false)
                .set("asm.lines.bb", false)
                .set("asm.bb.line", false);

        disassemblyLines = disassembleLines(address, num_of_lines + 1);
    }
    QStringList disasmPreview;
    for (const DisassemblyLine &line : disassemblyLines) {
        disasmPreview << line.text;
        if (disasmPreview.length() >= num_of_lines) {
            disasmPreview << "...";
            break;
        }
    }
    if (!disasmPreview.isEmpty()) {
        return disasmPreview;
    } else {
        return QStringList();
    }
}

/**
 * @brief get a compact hexdump preview for tooltips
 * @param address - the address from which to print the hexdump
 * @param size - number of bytes to print
 */
QString CutterCore::getHexdumpPreview(RVA address, int size)
{
    // temporarily simplify the disasm output to get it colorful and simple to read
    TempConfig tempConfig;
    tempConfig.set("scr.color", COLOR_MODE_16M)
            .set("asm.offset", true)
            .set("hex.header", false)
            .set("hex.cols", 16);
    return ansiEscapeToHtml(hexdump(address, size, HexdumpFormats::Normal))
            .replace(QLatin1Char('\n'), "<br>");
}

QByteArray CutterCore::ioRead(RVA addr, int len)
{
    CORE_LOCK();

    QByteArray array;

    if (len <= 0)
        return array;

    /* Zero-copy */
    array.resize(len);
    if (!rz_io_read_at(core->io, addr, (uint8_t *)array.data(), len)) {
        qWarning() << "Can't read data" << addr << len;
        array.fill(0xff);
    }

    return array;
}
