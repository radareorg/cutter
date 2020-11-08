#include "DisassemblyContextMenu.h"
#include "dialogs/preferences/PreferencesDialog.h"
#include "dialogs/EditInstructionDialog.h"
#include "dialogs/CommentsDialog.h"
#include "dialogs/FlagDialog.h"
#include "dialogs/XrefsDialog.h"
#include "dialogs/EditVariablesDialog.h"
#include "dialogs/SetToDataDialog.h"
#include "dialogs/EditFunctionDialog.h"
#include "dialogs/LinkTypeDialog.h"
#include "dialogs/EditStringDialog.h"
#include "dialogs/BreakpointsDialog.h"
#include "MainWindow.h"

#include <QtCore>
#include <QShortcut>
#include <QJsonArray>
#include <QClipboard>
#include <QApplication>
#include <QPushButton>
#include <QInputDialog>

DisassemblyContextMenu::DisassemblyContextMenu(QWidget *parent, MainWindow *mainWindow)
    :   QMenu(parent),
        offset(0),
        canCopy(false),
        mainWindow(mainWindow),
        actionEditInstruction(this),
        actionNopInstruction(this),
        actionJmpReverse(this),
        actionEditBytes(this),
        actionCopy(this),
        actionCopyAddr(this),
        actionAddComment(this),
        actionAnalyzeFunction(this),
        actionEditFunction(this),
        actionRename(this),
        actionSetFunctionVarTypes(this),
        actionXRefs(this),
        actionXRefsForVariables(this),
        actionDisplayOptions(this),
        actionDeleteComment(this),
        actionDeleteFunction(this),
        actionLinkType(this),
        actionSetBaseBinary(this),
        actionSetBaseOctal(this),
        actionSetBaseDecimal(this),
        actionSetBaseHexadecimal(this),
        actionSetBasePort(this),
        actionSetBaseIPAddr(this),
        actionSetBaseSyscall(this),
        actionSetBaseString(this),
        actionSetBits16(this),
        actionSetBits32(this),
        actionSetBits64(this),
        actionContinueUntil(this),
        actionSetPC(this),
        actionAddBreakpoint(this),
        actionAdvancedBreakpoint(this),
        actionSetToCode(this),
        actionSetAsStringAuto(this),
        actionSetAsStringRemove(this),
        actionSetAsStringAdvanced(this),
        actionSetToDataEx(this),
        actionSetToDataByte(this),
        actionSetToDataWord(this),
        actionSetToDataDword(this),
        actionSetToDataQword(this),
        showInSubmenu(this)
{
    initAction(&actionCopy, tr("Copy"), SLOT(on_actionCopy_triggered()), getCopySequence());
    addAction(&actionCopy);

    initAction(&actionCopyAddr, tr("Copy address"), SLOT(on_actionCopyAddr_triggered()),
               getCopyAddressSequence());
    addAction(&actionCopyAddr);

    initAction(&showInSubmenu, tr("Show in"), nullptr);
    addAction(&showInSubmenu);

    copySeparator = addSeparator();

    initAction(&actionAddComment, tr("Add Comment"),
               SLOT(on_actionAddComment_triggered()), getCommentSequence());
    addAction(&actionAddComment);

    initAction(&actionRename, tr("Rename or add flag"),
               SLOT(on_actionRename_triggered()), getRenameSequence());
    addAction(&actionRename);

    initAction(&actionSetFunctionVarTypes, tr("Re-type Local Variables"),
               SLOT(on_actionSetFunctionVarTypes_triggered()), getRetypeSequence());
    addAction(&actionSetFunctionVarTypes);

    initAction(&actionEditFunction, tr("Edit function"),
               SLOT(on_actionEditFunction_triggered()), getEditFunctionSequence());
    addAction(&actionEditFunction);

    initAction(&actionDeleteComment, tr("Delete comment"), SLOT(on_actionDeleteComment_triggered()));
    addAction(&actionDeleteComment);

    initAction(&actionDeleteFunction, tr("Undefine function"),
               SLOT(on_actionDeleteFunction_triggered()), getUndefineFunctionSequence());
    addAction(&actionDeleteFunction);

    initAction(&actionAnalyzeFunction, tr("Define function here"),
               SLOT(on_actionAnalyzeFunction_triggered()), getDefineNewFunctionSequence());
    addAction(&actionAnalyzeFunction);

    addSeparator();

    addSetBaseMenu();

    addSetBitsMenu();

    structureOffsetMenu = addMenu(tr("Structure offset"));
    connect(structureOffsetMenu, &QMenu::triggered,
            this, &DisassemblyContextMenu::on_actionStructureOffsetMenu_triggered);

    initAction(&actionLinkType, tr("Link Type to Address"),
               SLOT(on_actionLinkType_triggered()), getLinkTypeSequence());
    addAction(&actionLinkType);

    addSetAsMenu();

    addSeparator();

    initAction(&actionXRefs, tr("Show X-Refs"),
               SLOT(on_actionXRefs_triggered()), getXRefSequence());
    addAction(&actionXRefs);

    initAction(&actionXRefsForVariables, tr("X-Refs for local variables"),
                SLOT(on_actionXRefsForVariables_triggered()), QKeySequence({Qt::SHIFT + Qt::Key_X}));
    addAction(&actionXRefsForVariables);

    initAction(&actionDisplayOptions, tr("Show Options"),
               SLOT(on_actionDisplayOptions_triggered()), getDisplayOptionsSequence());

    addSeparator();

    addEditMenu();

    addSeparator();

    addBreakpointMenu();
    addDebugMenu();

    addSeparator();

    if (mainWindow) {
        pluginMenu = mainWindow->getContextMenuExtensions(MainWindow::ContextMenuType::Disassembly);
        pluginActionMenuAction = addMenu(pluginMenu);
    }

    addSeparator();

    connect(this, &DisassemblyContextMenu::aboutToShow,
            this, &DisassemblyContextMenu::aboutToShowSlot);
    connect(this, &DisassemblyContextMenu::aboutToHide,
            this, &DisassemblyContextMenu::aboutToHideSlot);
}

DisassemblyContextMenu::~DisassemblyContextMenu()
{
}

void DisassemblyContextMenu::addSetBaseMenu()
{
    setBaseMenu = addMenu(tr("Set Immediate Base to..."));

    initAction(&actionSetBaseBinary, tr("Binary"));
    setBaseMenu->addAction(&actionSetBaseBinary);
    connect(&actionSetBaseBinary, &QAction::triggered, this, [this] { setBase("b"); });

    initAction(&actionSetBaseOctal, tr("Octal"));
    setBaseMenu->addAction(&actionSetBaseOctal);
    connect(&actionSetBaseOctal, &QAction::triggered, this, [this] { setBase("o"); });

    initAction(&actionSetBaseDecimal, tr("Decimal"));
    setBaseMenu->addAction(&actionSetBaseDecimal);
    connect(&actionSetBaseDecimal, &QAction::triggered, this, [this] { setBase("d"); });

    initAction(&actionSetBaseHexadecimal, tr("Hexadecimal"));
    setBaseMenu->addAction(&actionSetBaseHexadecimal);
    connect(&actionSetBaseHexadecimal, &QAction::triggered, this, [this] { setBase("h"); });

    initAction(&actionSetBasePort, tr("Network Port"));
    setBaseMenu->addAction(&actionSetBasePort);
    connect(&actionSetBasePort, &QAction::triggered, this, [this] { setBase("p"); });

    initAction(&actionSetBaseIPAddr, tr("IP Address"));
    setBaseMenu->addAction(&actionSetBaseIPAddr);
    connect(&actionSetBaseIPAddr, &QAction::triggered, this, [this] { setBase("i"); });

    initAction(&actionSetBaseSyscall, tr("Syscall"));
    setBaseMenu->addAction(&actionSetBaseSyscall);
    connect(&actionSetBaseSyscall, &QAction::triggered, this, [this] { setBase("S"); });

    initAction(&actionSetBaseString, tr("String"));
    setBaseMenu->addAction(&actionSetBaseString);
    connect(&actionSetBaseString, &QAction::triggered, this, [this] { setBase("s"); });
}

void DisassemblyContextMenu::addSetBitsMenu()
{
    setBitsMenu = addMenu(tr("Set current bits to..."));

    initAction(&actionSetBits16, "16");
    setBitsMenu->addAction(&actionSetBits16);
    connect(&actionSetBits16, &QAction::triggered, this, [this] { setBits(16); });

    initAction(&actionSetBits32, "32");
    setBitsMenu->addAction(&actionSetBits32);
    connect(&actionSetBits32, &QAction::triggered, this, [this] { setBits(32); });

    initAction(&actionSetBits64, "64");
    setBitsMenu->addAction(&actionSetBits64);
    connect(&actionSetBits64, &QAction::triggered, this, [this] { setBits(64); });
}


void DisassemblyContextMenu::addSetAsMenu()
{
    setAsMenu = addMenu(tr("Set as..."));

    initAction(&actionSetToCode, tr("Code"),
               SLOT(on_actionSetToCode_triggered()), getSetToCodeSequence());
    setAsMenu->addAction(&actionSetToCode);

    setAsString = setAsMenu->addMenu(tr("String..."));

    initAction(&actionSetAsStringAuto, tr("Auto-detect"),
               SLOT(on_actionSetAsString_triggered()), getSetAsStringSequence());
    initAction(&actionSetAsStringRemove, tr("Remove"),
               SLOT(on_actionSetAsStringRemove_triggered()));
    initAction(&actionSetAsStringAdvanced, tr("Advanced"),
               SLOT(on_actionSetAsStringAdvanced_triggered()), getSetAsStringAdvanced());


    setAsString->addAction(&actionSetAsStringAuto);
    setAsString->addAction(&actionSetAsStringRemove);
    setAsString->addAction(&actionSetAsStringAdvanced);

    addSetToDataMenu();
}

void DisassemblyContextMenu::addSetToDataMenu()
{
    setToDataMenu = setAsMenu->addMenu(tr("Data..."));

    initAction(&actionSetToDataByte, tr("Byte"));
    setToDataMenu->addAction(&actionSetToDataByte);
    connect(&actionSetToDataByte, &QAction::triggered, this, [this] { setToData(1); });

    initAction(&actionSetToDataWord, tr("Word"));
    setToDataMenu->addAction(&actionSetToDataWord);
    connect(&actionSetToDataWord, &QAction::triggered, this, [this] { setToData(2); });

    initAction(&actionSetToDataDword, tr("Dword"));
    setToDataMenu->addAction(&actionSetToDataDword);
    connect(&actionSetToDataDword, &QAction::triggered, this, [this] { setToData(4); });

    initAction(&actionSetToDataQword, tr("Qword"));
    setToDataMenu->addAction(&actionSetToDataQword);
    connect(&actionSetToDataQword, &QAction::triggered, this, [this] { setToData(8); });

    initAction(&actionSetToDataEx, "...",
               SLOT(on_actionSetToDataEx_triggered()), getSetToDataExSequence());
    setToDataMenu->addAction(&actionSetToDataEx);

    auto switchAction = new QAction(this);
    initAction(switchAction, "Switch Data",
               SLOT(on_actionSetToData_triggered()), getSetToDataSequence());
}

void DisassemblyContextMenu::addEditMenu()
{
    editMenu = addMenu(tr("Edit"));

    initAction(&actionEditInstruction, tr("Instruction"), SLOT(on_actionEditInstruction_triggered()));
    editMenu->addAction(&actionEditInstruction);

    initAction(&actionNopInstruction, tr("Nop Instruction"), SLOT(on_actionNopInstruction_triggered()));
    editMenu->addAction(&actionNopInstruction);

    initAction(&actionEditBytes, tr("Bytes"), SLOT(on_actionEditBytes_triggered()));
    editMenu->addAction(&actionEditBytes);

    initAction(&actionJmpReverse, tr("Reverse Jump"), SLOT(on_actionJmpReverse_triggered()));
    editMenu->addAction(&actionJmpReverse);
}

void DisassemblyContextMenu::addBreakpointMenu()
{
    breakpointMenu = addMenu(tr("Breakpoint"));

    initAction(&actionAddBreakpoint, tr("Add/remove breakpoint"),
               SLOT(on_actionAddBreakpoint_triggered()), getAddBPSequence());
    breakpointMenu->addAction(&actionAddBreakpoint);
    initAction(&actionAdvancedBreakpoint, tr("Advanced breakpoint"),
               SLOT(on_actionAdvancedBreakpoint_triggered()), QKeySequence(Qt::CTRL+Qt::Key_F2));
    breakpointMenu->addAction(&actionAdvancedBreakpoint);
}

void DisassemblyContextMenu::addDebugMenu()
{
    debugMenu = addMenu(tr("Debug"));

    initAction(&actionContinueUntil, tr("Continue until line"),
               SLOT(on_actionContinueUntil_triggered()));
    debugMenu->addAction(&actionContinueUntil);

    initAction(&actionSetPC, "Set PC", SLOT(on_actionSetPC_triggered()));
    debugMenu->addAction(&actionSetPC);
}

QVector<DisassemblyContextMenu::ThingUsedHere> DisassemblyContextMenu::getThingUsedHere(RVA offset)
{
    QVector<ThingUsedHere> result;
    const QJsonArray array = Core()->cmdj("anj @ " + QString::number(offset)).array();
    result.reserve(array.size());
    for (const auto &thing : array) {
        auto obj = thing.toObject();
        RVA offset = obj["offset"].toVariant().toULongLong();
        QString name;

        // If real names display is enabled, show flag's real name instead of full flag name
        if (Config()->getConfigBool("asm.flags.real") && obj.contains("realname")) {
            name = obj["realname"].toString();
        } else {
            name = obj["name"].toString();
        }

        QString typeString = obj["type"].toString();
        ThingUsedHere::Type type = ThingUsedHere::Type::Address;
        if (typeString == "var") {
            type = ThingUsedHere::Type::Var;
        } else if (typeString == "flag") {
            type = ThingUsedHere::Type::Flag;
        } else if (typeString == "function") {
            type = ThingUsedHere::Type::Function;
        } else if (typeString == "address") {
            type = ThingUsedHere::Type::Address;
        }
        result.push_back(ThingUsedHere{name, offset, type});
    }
    return result;
}

void DisassemblyContextMenu::setOffset(RVA offset)
{
    this->offset = offset;
    this->actionSetFunctionVarTypes.setVisible(true);
}

void DisassemblyContextMenu::setCanCopy(bool enabled)
{
    this->canCopy = enabled;
}

void DisassemblyContextMenu::setCurHighlightedWord(const QString &text)
{
    this->curHighlightedWord = text;
}

void DisassemblyContextMenu::aboutToShowSlot()
{
    // check if set immediate base menu makes sense
    QJsonObject instObject = Core()->cmdj("aoj @ " + QString::number(
                                              offset)).array().first().toObject();
    auto keys = instObject.keys();
    bool immBase = keys.contains("val") || keys.contains("ptr");
    setBaseMenu->menuAction()->setVisible(immBase);
    setBitsMenu->menuAction()->setVisible(true);

    // Create structure offset menu if it makes sense
    QString memBaseReg; // Base register
    QVariant memDisp; // Displacement
    if (instObject.contains("opex") && instObject["opex"].toObject().contains("operands")) {
        // Loop through both the operands of the instruction
        for (const QJsonValue value: instObject["opex"].toObject()["operands"].toArray()) {
            QJsonObject operand = value.toObject();
            if (operand.contains("type") && operand["type"].toString() == "mem" &&
                    operand.contains("base") && !operand["base"].toString().contains("bp") &&
                    operand.contains("disp") && operand["disp"].toVariant().toLongLong() > 0) {

                    // The current operand is the one which has an immediate displacement
                    memBaseReg = operand["base"].toString();
                    memDisp = operand["disp"].toVariant();
                    break;

            }
        }
    }
    if (memBaseReg.isEmpty()) {
        // hide structure offset menu
        structureOffsetMenu->menuAction()->setVisible(false);
    } else {
        // show structure offset menu
        structureOffsetMenu->menuAction()->setVisible(true);
        structureOffsetMenu->clear();

        // Get the possible offsets using the "ahts" command
        // TODO: add ahtj command to radare2 and then use it here
        QStringList ret = Core()->cmdList("ahts " + memDisp.toString());
        for (const QString &val : ret) {
            if (val.isEmpty()) {
                continue;
            }
            structureOffsetMenu->addAction("[" + memBaseReg + " + " + val + "]")->setData(val);
        }
        if (structureOffsetMenu->isEmpty()) {
            // No possible offset was found so hide the menu
            structureOffsetMenu->menuAction()->setVisible(false);
        }
    }

    actionAnalyzeFunction.setVisible(true);

    // Show the option to remove a defined string only if a string is defined in this address
    QString stringDefinition = Core()->cmdRawAt("Cs.", offset);
    actionSetAsStringRemove.setVisible(!stringDefinition.isEmpty());

    QString comment = Core()->cmdRawAt("CC.", offset);

    if (comment.isNull() || comment.isEmpty()) {
        actionDeleteComment.setVisible(false);
        actionAddComment.setText(tr("Add Comment"));
    } else {
        actionDeleteComment.setVisible(true);
        actionAddComment.setText(tr("Edit Comment"));
    }

    actionCopy.setVisible(canCopy);
    copySeparator->setVisible(canCopy);

    // Handle renaming of variable, function, flag, ...
    // We must take into account cursor location to choose between current address and pointed value
    // i.e. `0x000040f3  lea rdi, [0x000199b1]` -> does the user want to add a flag at 0x40f3 or at 0x199b1?
    // and for that we will rely on |curHighlightedWord| which is the currently selected word
    RCore *core = Core()->core();
    // Now, we'll try to check if backend has any reference to our highlighted word on current line
    auto thingsUsedHere = getThingUsedHere(offset);
    ThingUsedHere *tuh = nullptr;
    ThingUsedHere defaultTuh = {};
    ut64 selection = Core()->num(curHighlightedWord);
    qDebug() << " loop -----";
    for (auto& thing : thingsUsedHere) {
        qDebug() << &thing << " " << thing.offset << " " << thing.name;
        if (thing.offset == selection || thing.name == curHighlightedWord) {
            // It's a match!
            tuh = &thing;
            break;
        }
    }
    if (!tuh) {
        // Nothing was found, we will fallback on current offset
        RAnalFunction *fcn = Core()->functionAt(offset);
        RFlagItem *flag = r_flag_get_i(core->flags, offset);
        if (fcn != nullptr) {
            defaultTuh.type = ThingUsedHere::Type::Function;
            defaultTuh.name = fcn->name;
        } else if (flag != nullptr) {
            defaultTuh.type = ThingUsedHere::Type::Flag;
            if (Config()->getConfigBool("asm.flags.real") && flag->realname) {
                defaultTuh.name = flag->realname;
            } else {
                defaultTuh.name = flag->name;
            }
        } else {
            defaultTuh.type = ThingUsedHere::Type::Address;
        }
        tuh = &defaultTuh;
    }
    qDebug() << "Type: " << (int) tuh->type;
    if (tuh->type == ThingUsedHere::Type::Address) {
        RFlagItem *flagUsedHere = r_flag_get_i(core->flags, tuh->offset);
        if (flagUsedHere) {
            doRenameAction = RENAME_FLAG;
            doRenameInfo.name = RAddressString(tuh->offset);
            doRenameInfo.addr = tuh->offset;
            actionRename.setText(tr("Rename flag %1").arg(doRenameInfo.name));
        } else {
            doRenameAction = RENAME_ADD_FLAG;
            doRenameInfo.name = RAddressString(tuh->offset);
            doRenameInfo.addr = tuh->offset;
            actionRename.setText(tr("Add flag at %1 (used here)").arg(doRenameInfo.name));
        }
    } else if (tuh->type == ThingUsedHere::Type::Function) {
        doRenameAction = RENAME_FUNCTION;
        doRenameInfo.name = tuh->name;
        doRenameInfo.addr = tuh->offset;
        actionRename.setText(tr("Rename \"%1\"").arg(doRenameInfo.name));
    } else if (tuh->type == ThingUsedHere::Type::Var) {
        // TODO
        doRenameAction = RENAME_DO_NOTHING;
        qWarning() << "NOT HANDLED";
    } else if (tuh->type == ThingUsedHere::Type::Flag) {
        // It's something else, but what?
        doRenameInfo.name = tuh->name;
        doRenameInfo.addr = tuh->offset;
        actionRename.setText(tr("Rename \"%1\" (used here)").arg(doRenameInfo.name));
    } else {
        qWarning() << "Unexpected renaming type";
        doRenameAction = RENAME_DO_NOTHING;
    }
    actionRename.setVisible(true);
    
    // Only show retype for local vars if in a function
    RAnalFunction *in_fcn = Core()->functionIn(offset);
    if (in_fcn) {
        auto vars = Core()->getVariables(offset);
        actionSetFunctionVarTypes.setVisible(!vars.empty());
        actionEditFunction.setVisible(true);
        actionEditFunction.setText(tr("Edit function \"%1\"").arg(in_fcn->name));
    } else {
        actionSetFunctionVarTypes.setVisible(false);
        actionEditFunction.setVisible(false);
    }

    // Decide to show Reverse jmp option
    showReverseJmpQuery();

    if (showInSubmenu.menu() != nullptr) {
        showInSubmenu.menu()->deleteLater();
    }
    showInSubmenu.setMenu(mainWindow->createShowInMenu(this, offset));

    // Only show debug options if we are currently debugging
    debugMenu->menuAction()->setVisible(Core()->currentlyDebugging);
    bool hasBreakpoint = Core()->breakpointIndexAt(offset) > -1;
    actionAddBreakpoint.setText(hasBreakpoint ?
                                     tr("Remove breakpoint") : tr("Add breakpoint"));
    actionAdvancedBreakpoint.setText(hasBreakpoint ?
                                     tr("Edit breakpoint") : tr("Advanced breakpoint"));
    QString progCounterName = Core()->getRegisterName("PC").toUpper();
    actionSetPC.setText("Set " + progCounterName + " here");

    if (pluginMenu) {
        pluginActionMenuAction->setVisible(!pluginMenu->isEmpty());
        for (QAction *pluginAction : pluginMenu->actions()) {
            pluginAction->setData(QVariant::fromValue(offset));
        }
    }

    bool isLocalVar = isHighlightedWordLocalVar();
    actionXRefsForVariables.setVisible(isLocalVar);
    if (isLocalVar) {
        actionXRefsForVariables.setText(tr("X-Refs for %1").arg(curHighlightedWord));
    }
}

void DisassemblyContextMenu::aboutToHideSlot()
{
    actionXRefsForVariables.setVisible(true);
}

QKeySequence DisassemblyContextMenu::getCopySequence() const
{
    return QKeySequence::Copy;
}

QKeySequence DisassemblyContextMenu::getCommentSequence() const
{
    return {Qt::Key_Semicolon};
}

QKeySequence DisassemblyContextMenu::getCopyAddressSequence() const
{
    return {Qt::CTRL | Qt::SHIFT | Qt::Key_C};
}

QKeySequence DisassemblyContextMenu::getSetToCodeSequence() const
{
    return {Qt::Key_C};
}

QKeySequence DisassemblyContextMenu::getSetAsStringSequence() const
{
    return {Qt::Key_A};
}

QKeySequence DisassemblyContextMenu::getSetAsStringAdvanced() const
{
    return {Qt::SHIFT | Qt::Key_A};
}

QKeySequence DisassemblyContextMenu::getSetToDataSequence() const
{
    return {Qt::Key_D};
}

QKeySequence DisassemblyContextMenu::getSetToDataExSequence() const
{
    return {Qt::Key_Asterisk};
}

QKeySequence DisassemblyContextMenu::getRenameSequence() const
{
    return {Qt::Key_N};
}

QKeySequence DisassemblyContextMenu::getRetypeSequence() const
{
    return {Qt::Key_Y};
}

QKeySequence DisassemblyContextMenu::getXRefSequence() const
{
    return {Qt::Key_X};
}

QKeySequence DisassemblyContextMenu::getDisplayOptionsSequence() const
{
    return {}; //TODO insert correct sequence
}

QKeySequence DisassemblyContextMenu::getLinkTypeSequence() const
{
    return {Qt::Key_L};
}

QList<QKeySequence> DisassemblyContextMenu::getAddBPSequence() const
{
    return {Qt::Key_F2, Qt::CTRL + Qt::Key_B};
}

QKeySequence DisassemblyContextMenu::getDefineNewFunctionSequence() const
{
    return {Qt::Key_P};
}

QKeySequence DisassemblyContextMenu::getEditFunctionSequence() const
{
    return {Qt::SHIFT + Qt::Key_P};
}

QKeySequence DisassemblyContextMenu::getUndefineFunctionSequence() const
{
    return {Qt::Key_U};
}


void DisassemblyContextMenu::on_actionEditInstruction_triggered()
{
    if (!ioModesController.prepareForWriting()) {
        return;
    }
    EditInstructionDialog e(EDIT_TEXT, this);
    e.setWindowTitle(tr("Edit Instruction at %1").arg(RAddressString(offset)));

    QString oldInstructionOpcode = Core()->getInstructionOpcode(offset);
    QString oldInstructionBytes = Core()->getInstructionBytes(offset);

    e.setInstruction(oldInstructionOpcode);

    if (e.exec()) {
        QString userInstructionOpcode = e.getInstruction();
        if (userInstructionOpcode != oldInstructionOpcode) {
            Core()->editInstruction(offset, userInstructionOpcode);
        }
    }
}

void DisassemblyContextMenu::on_actionNopInstruction_triggered()
{
    if (!ioModesController.prepareForWriting()) {
        return;
    }
    Core()->nopInstruction(offset);
}

void DisassemblyContextMenu::showReverseJmpQuery()
{
    QString type;

    QJsonArray array = Core()->cmdj("pdj 1 @ " + RAddressString(offset)).array();
    if (array.isEmpty()) {
        return;
    }

    type = array.first().toObject()["type"].toString();
    if (type == "cjmp") {
        actionJmpReverse.setVisible(true);
    } else {
        actionJmpReverse.setVisible(false);
    }
}

void DisassemblyContextMenu::on_actionJmpReverse_triggered()
{
    if (!ioModesController.prepareForWriting()) {
        return;
    }
    Core()->jmpReverse(offset);
}

void DisassemblyContextMenu::on_actionEditBytes_triggered()
{
    if (!ioModesController.prepareForWriting()) {
        return;
    }
    EditInstructionDialog e(EDIT_BYTES, this);
    e.setWindowTitle(tr("Edit Bytes at %1").arg(RAddressString(offset)));

    QString oldBytes = Core()->getInstructionBytes(offset);
    e.setInstruction(oldBytes);

    if (e.exec()) {
        QString bytes = e.getInstruction();
        if (bytes != oldBytes) {
            Core()->editBytes(offset, bytes);
        }
    }
}

void DisassemblyContextMenu::on_actionCopy_triggered()
{
    emit copy();
}

void DisassemblyContextMenu::on_actionCopyAddr_triggered()
{
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(RAddressString(offset));
}

void DisassemblyContextMenu::on_actionAddBreakpoint_triggered()
{
    Core()->toggleBreakpoint(offset);
}

void DisassemblyContextMenu::on_actionAdvancedBreakpoint_triggered()
{
    int index = Core()->breakpointIndexAt(offset);
    if (index >= 0) {
        BreakpointsDialog::editBreakpoint(Core()->getBreakpointAt(offset), this);
    } else {
        BreakpointsDialog::createNewBreakpoint(offset, this);
    }
}

void DisassemblyContextMenu::on_actionContinueUntil_triggered()
{
    Core()->continueUntilDebug(RAddressString(offset));
}

void DisassemblyContextMenu::on_actionSetPC_triggered()
{
    QString progCounterName = Core()->getRegisterName("PC");
    Core()->setRegister(progCounterName, RAddressString(offset).toUpper());
}

void DisassemblyContextMenu::on_actionAddComment_triggered()
{
    CommentsDialog::addOrEditComment(offset, this);
}

void DisassemblyContextMenu::on_actionAnalyzeFunction_triggered()
{
    bool ok;
    // Create dialog
    QString functionName = QInputDialog::getText(this, tr("New function %1").arg(RAddressString(offset)),
                            tr("Function name:"), QLineEdit::Normal, QString(), &ok);

    // If user accepted
    if (ok && !functionName.isEmpty()) {
        Core()->createFunctionAt(offset, functionName);
    }
}

void DisassemblyContextMenu::on_actionRename_triggered()
{
    bool ok;
    if (doRenameAction == RENAME_FUNCTION) {
        QString newName = QInputDialog::getText(this->mainWindow, tr("Rename function %2").arg(doRenameInfo.name),
                                            tr("Function name:"), QLineEdit::Normal, doRenameInfo.name, &ok);
        if (ok && !newName.isEmpty()) {
            Core()->renameFunction(doRenameInfo.addr, newName);
        }
    } else if (doRenameAction == RENAME_FLAG) {
        QString newName = QInputDialog::getText(this, tr("Rename flag %2").arg(doRenameInfo.name),
                                            tr("Flag name at %1:").arg(RAddressString(doRenameInfo.addr)), QLineEdit::Normal, doRenameInfo.name, &ok);
        if (ok && !newName.isEmpty()) {
            Core()->renameFlag(doRenameInfo.name, newName);
        }
    } else if (doRenameAction == RENAME_ADD_FLAG) {
        //QString newName = QInputDialog::getText(this, tr("Add a flag"),
        //                                    tr("Flag name at %1:").arg(RAddressString(doRenameInfo.addr)), QLineEdit::Normal, "", &ok);
        //if (ok && !newName.isEmpty()) {
        FlagDialog dialog(doRenameInfo.addr, this->parentWidget());
        dialog.exec();
        //}
    } else if (doRenameAction == RENAME_DO_NOTHING) {
        // Do nothing
    } else {
        qWarning() << "Unhandled renaming action: " << doRenameAction;
        assert(false);
    }
}

void DisassemblyContextMenu::on_actionSetFunctionVarTypes_triggered()
{
    RAnalFunction *fcn = Core()->functionIn(offset);

    if (!fcn) {
        QMessageBox::critical(this, tr("Re-type Local Variables"),
                              tr("You must be in a function to define variable types."));
        return;
    }

    EditVariablesDialog dialog(fcn->addr, curHighlightedWord, this);
    if (dialog.empty()) { // don't show the dialog if there are no variables
        return;
    }
    dialog.exec();
}

void DisassemblyContextMenu::on_actionXRefs_triggered()
{
    XrefsDialog dialog(mainWindow, nullptr);
    dialog.fillRefsForAddress(offset, RAddressString(offset), false);
    dialog.exec();
}

void DisassemblyContextMenu::on_actionXRefsForVariables_triggered()
{
    if (isHighlightedWordLocalVar()) {
        XrefsDialog dialog(mainWindow, nullptr);
        dialog.fillRefsForVariable(curHighlightedWord, offset);
        dialog.exec();
    }
}

void DisassemblyContextMenu::on_actionDisplayOptions_triggered()
{
    PreferencesDialog dialog(this->window());
    dialog.showSection(PreferencesDialog::Section::Disassembly);
    dialog.exec();
}

void DisassemblyContextMenu::on_actionSetToCode_triggered()
{
    Core()->setToCode(offset);
}

void DisassemblyContextMenu::on_actionSetAsString_triggered()
{
    Core()->setAsString(offset);
}

void DisassemblyContextMenu::on_actionSetAsStringRemove_triggered()
{
    Core()->removeString(offset);
}

void DisassemblyContextMenu::on_actionSetAsStringAdvanced_triggered()
{
    EditStringDialog dialog(parentWidget());
    const int predictedStrSize = Core()->getString(offset).size();
    dialog.setStringSizeValue(predictedStrSize);
    dialog.setStringStartAddress(offset);

    if(!dialog.exec())
    {
        return;
    }

    uint64_t strAddr = 0U;
    if( !dialog.getStringStartAddress(strAddr) ) {
        QMessageBox::critical(this->window(), tr("Wrong address"), tr("Can't edit string at this address"));
        return;
    }
    CutterCore::StringTypeFormats coreStringType = CutterCore::StringTypeFormats::None;

    const auto strSize = dialog.getStringSizeValue();
    const auto strType = dialog.getStringType();
    switch(strType)
    {
    case EditStringDialog::StringType::Auto:
        coreStringType = CutterCore::StringTypeFormats::None;
        break;
    case EditStringDialog::StringType::ASCII_LATIN1:
        coreStringType = CutterCore::StringTypeFormats::ASCII_LATIN1;
        break;
    case EditStringDialog::StringType::UTF8:
        coreStringType = CutterCore::StringTypeFormats::UTF8;
        break;
    };

    Core()->setAsString(strAddr, strSize, coreStringType);
}

void DisassemblyContextMenu::on_actionSetToData_triggered()
{
    int size = Core()->sizeofDataMeta(offset);
    if (size > 8 || (size && (size & (size - 1)))) {
        return;
    }
    if (size == 0 || size == 8) {
        size = 1;
    } else {
        size *= 2;
    }
    setToData(size);
}

void DisassemblyContextMenu::on_actionSetToDataEx_triggered()
{
    SetToDataDialog dialog(offset, this->window());
    if (!dialog.exec()) {
        return;
    }
    setToData(dialog.getItemSize(), dialog.getItemCount());
}

void DisassemblyContextMenu::on_actionStructureOffsetMenu_triggered(QAction *action)
{
    Core()->applyStructureOffset(action->data().toString(), offset);
}

void DisassemblyContextMenu::on_actionLinkType_triggered()
{
    LinkTypeDialog dialog(mainWindow);
    if (!dialog.setDefaultAddress(curHighlightedWord)) {
        dialog.setDefaultAddress(RAddressString(offset));
    }
    dialog.exec();
}

void DisassemblyContextMenu::on_actionDeleteComment_triggered()
{
    Core()->delComment(offset);
}

void DisassemblyContextMenu::on_actionDeleteFunction_triggered()
{
    Core()->delFunction(offset);
}

void DisassemblyContextMenu::on_actionEditFunction_triggered()
{
    RCore *core = Core()->core();
    EditFunctionDialog dialog(mainWindow);
    RAnalFunction *fcn = r_anal_get_fcn_in(core->anal, offset, 0);

    if (fcn) {
        dialog.setWindowTitle(tr("Edit function %1").arg(fcn->name));
        dialog.setNameText(fcn->name);

        QString startAddrText = "0x" + QString::number(fcn->addr, 16);
        dialog.setStartAddrText(startAddrText);

        dialog.setStackSizeText(QString::number(fcn->stack));

        QStringList callConList = Core()->cmdRaw("afcl").split("\n");
        callConList.removeLast();
        dialog.setCallConList(callConList);
        dialog.setCallConSelected(fcn->cc);


        if (dialog.exec()) {
            QString new_name = dialog.getNameText();
            Core()->renameFunction(fcn->addr, new_name);
            QString new_start_addr = dialog.getStartAddrText();
            fcn->addr = Core()->math(new_start_addr);
            QString new_stack_size = dialog.getStackSizeText();
            fcn->stack = int(Core()->math(new_stack_size));
            Core()->cmdRaw("afc " + dialog.getCallConSelected());
            emit Core()->functionsChanged();
        }
    }
}

void DisassemblyContextMenu::setBase(QString base)
{
    Core()->setImmediateBase(base, offset);
}

void DisassemblyContextMenu::setBits(int bits)
{
    Core()->setCurrentBits(bits, offset);
}

void DisassemblyContextMenu::setToData(int size, int repeat)
{
    Core()->setToData(offset, size, repeat);
}

QAction *DisassemblyContextMenu::addAnonymousAction(QString name, const char *slot,
                                                    QKeySequence keySequence)
{
    auto action = new QAction(this);
    addAction(action);
    anonymousActions.append(action);
    initAction(action, name, slot, keySequence);
    return action;
}

void DisassemblyContextMenu::initAction(QAction *action, QString name, const char *slot)
{
    action->setParent(this);
    parentWidget()->addAction(action);
    action->setText(name);
    if (slot) {
        connect(action, SIGNAL(triggered(bool)), this, slot);
    }
}

void DisassemblyContextMenu::initAction(QAction *action, QString name,
                                        const char *slot, QKeySequence keySequence)
{
    initAction(action, name, slot);
    if (keySequence.isEmpty()) {
        return;
    }
    action->setShortcut(keySequence);
    action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
}

void DisassemblyContextMenu::initAction(QAction *action, QString name,
                                        const char *slot, QList<QKeySequence> keySequenceList)
{
    initAction(action, name, slot);
    if (keySequenceList.empty()) {
        return;
    }
    action->setShortcuts(keySequenceList);
    action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
}

bool DisassemblyContextMenu::isHighlightedWordLocalVar()
{
    QList<VariableDescription> variables = Core()->getVariables(offset);
    for (const VariableDescription &var : variables) {
        if (var.name == curHighlightedWord) {
            return true;
        }
    }
    return false;
}
