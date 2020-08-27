#include "DecompilerWidget.h"
#include "ui_DecompilerWidget.h"
#include "menus/DecompilerContextMenu.h"

#include "common/Configuration.h"
#include "common/Helpers.h"
#include "common/TempConfig.h"
#include "common/SelectionHighlight.h"
#include "common/Decompiler.h"
#include "common/CutterSeekable.h"
#include "core/MainWindow.h"

#include <QTextEdit>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QClipboard>
#include <QObject>
#include <QTextBlockUserData>
#include <QScrollBar>
#include <QAbstractSlider>

DecompilerWidget::DecompilerWidget(MainWindow *main) :
    MemoryDockWidget(MemoryWidgetType::Decompiler, main),
    mCtxMenu(new DecompilerContextMenu(this, main)),
    ui(new Ui::DecompilerWidget),
    decompilerBusy(false),
    seekFromCursor(false),
    scrollerHorizontal(0),
    scrollerVertical(0),
    previousFunctionAddr(RVA_INVALID),
    decompiledFunctionAddr(RVA_INVALID),
    code(Decompiler::makeWarning(tr("Choose an offset and refresh to get decompiled code")),
         &r_annotated_code_free)
{
    ui->setupUi(this);
    setObjectName(main
                  ? main->getUniqueObjectName(getWidgetType())
                  : getWidgetType());
    updateWindowTitle();

    syntaxHighlighter = Config()->createSyntaxHighlighter(ui->textEdit->document());
    // Event filter to intercept double click and right click in the textbox
    ui->textEdit->viewport()->installEventFilter(this);

    setupFonts();
    colorsUpdatedSlot();

    connect(Config(), &Configuration::fontsUpdated, this, &DecompilerWidget::fontsUpdatedSlot);
    connect(Config(), &Configuration::colorsUpdated, this, &DecompilerWidget::colorsUpdatedSlot);
    connect(Core(), &CutterCore::registersChanged, this, &DecompilerWidget::highlightPC);
    connect(mCtxMenu, &DecompilerContextMenu::copy, this, &DecompilerWidget::copy);

    refreshDeferrer = createRefreshDeferrer([this]() {
        doRefresh();
    });

    auto decompilers = Core()->getDecompilers();
    QString selectedDecompilerId = Config()->getSelectedDecompiler();
    if (selectedDecompilerId.isEmpty()) {
        // If no decompiler was previously chosen. set r2ghidra as default decompiler
        selectedDecompilerId = "r2ghidra";
    }
    for (Decompiler *dec : decompilers) {
        ui->decompilerComboBox->addItem(dec->getName(), dec->getId());
        if (dec->getId() == selectedDecompilerId) {
            ui->decompilerComboBox->setCurrentIndex(ui->decompilerComboBox->count() - 1);
        }
    }
    decompilerSelectionEnabled = decompilers.size() > 1;
    ui->decompilerComboBox->setEnabled(decompilerSelectionEnabled);
    if (decompilers.isEmpty()) {
        ui->textEdit->setPlainText(tr("No Decompiler available."));
    }

    connect(ui->decompilerComboBox,
            static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &DecompilerWidget::decompilerSelected);
    connectCursorPositionChanged(true);
    connect(seekable, &CutterSeekable::seekableSeekChanged, this, &DecompilerWidget::seekChanged);
    ui->textEdit->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->textEdit, &QWidget::customContextMenuRequested,
            this, &DecompilerWidget::showDecompilerContextMenu);

    connect(Core(), &CutterCore::breakpointsChanged, this, &DecompilerWidget::updateBreakpoints);
    mCtxMenu->addSeparator();
    mCtxMenu->addAction(&syncAction);
    addActions(mCtxMenu->actions());

    ui->progressLabel->setVisible(false);
    doRefresh();

    connect(Core(), &CutterCore::refreshAll, this, &DecompilerWidget::doRefresh);
    connect(Core(), &CutterCore::functionRenamed, this, &DecompilerWidget::doRefresh);
    connect(Core(), &CutterCore::varsChanged, this, &DecompilerWidget::doRefresh);
    connect(Core(), &CutterCore::functionsChanged, this, &DecompilerWidget::doRefresh);
    connect(Core(), &CutterCore::flagsChanged, this, &DecompilerWidget::doRefresh);
    connect(Core(), &CutterCore::commentsChanged, this, &DecompilerWidget::refreshIfChanged);
    connect(Core(), &CutterCore::instructionChanged, this, &DecompilerWidget::refreshIfChanged);
    connect(Core(), &CutterCore::refreshCodeViews, this, &DecompilerWidget::doRefresh);

    // Esc to seek backward
    QAction *seekPrevAction = new QAction(this);
    seekPrevAction->setShortcut(Qt::Key_Escape);
    seekPrevAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(seekPrevAction);
    connect(seekPrevAction, &QAction::triggered, seekable, &CutterSeekable::seekPrev);
}

DecompilerWidget::~DecompilerWidget() = default;

QString DecompilerWidget::getWidgetType()
{
    return "Decompiler";
}

Decompiler *DecompilerWidget::getCurrentDecompiler()
{
    return Core()->getDecompilerById(ui->decompilerComboBox->currentData().toString());
}

ut64 DecompilerWidget::offsetForPosition(size_t pos)
{
    size_t closestPos = SIZE_MAX;
    ut64 closestOffset = mCtxMenu->getFirstOffsetInLine();
    void *iter;
    r_vector_foreach(&code->annotations, iter) {
        RCodeAnnotation *annotation = (RCodeAnnotation *)iter;
        if (annotation->type != R_CODE_ANNOTATION_TYPE_OFFSET || annotation->start > pos
                || annotation->end <= pos) {
            continue;
        }
        if (closestPos != SIZE_MAX && closestPos >= annotation->start) {
            continue;
        }
        closestPos = annotation->start;
        closestOffset = annotation->offset.offset;
    }
    return closestOffset;
}

size_t DecompilerWidget::positionForOffset(ut64 offset)
{
    size_t closestPos = SIZE_MAX;
    ut64 closestOffset = UT64_MAX;
    void *iter;
    r_vector_foreach(&code->annotations, iter) {
        RCodeAnnotation *annotation = (RCodeAnnotation *)iter;
        if (annotation->type != R_CODE_ANNOTATION_TYPE_OFFSET || annotation->offset.offset > offset) {
            continue;
        }
        if (closestOffset != UT64_MAX && closestOffset >= annotation->offset.offset) {
            continue;
        }
        closestPos = annotation->start;
        closestOffset = annotation->offset.offset;
    }
    return closestPos;
}

void DecompilerWidget::updateBreakpoints(RVA addr)
{
    if (!addressInRange(addr)) {
        return;
    }
    setInfoForBreakpoints();
    QTextCursor cursor = ui->textEdit->textCursor();
    cursor.select(QTextCursor::Document);
    cursor.setCharFormat(QTextCharFormat());
    cursor.setBlockFormat(QTextBlockFormat());
    ui->textEdit->setExtraSelections({});
    highlightPC();
    highlightBreakpoints();
    updateSelection();
}

void DecompilerWidget::setInfoForBreakpoints()
{
    if (mCtxMenu->getIsTogglingBreakpoints()) {
        return;
    }
    // Get the range of the line
    QTextCursor cursorForLine = ui->textEdit->textCursor();
    cursorForLine.movePosition(QTextCursor::StartOfLine);
    size_t startPos = cursorForLine.position();
    cursorForLine.movePosition(QTextCursor::EndOfLine);
    size_t endPos = cursorForLine.position();
    gatherBreakpointInfo(*code, startPos, endPos);
}

void DecompilerWidget::gatherBreakpointInfo(RAnnotatedCode &codeDecompiled, size_t startPos,
                                            size_t endPos)
{
    RVA firstOffset = RVA_MAX;
    void *iter;
    r_vector_foreach(&codeDecompiled.annotations, iter) {
        RCodeAnnotation *annotation = (RCodeAnnotation *)iter;
        if (annotation->type != R_CODE_ANNOTATION_TYPE_OFFSET) {
            continue;
        }
        if ((startPos <= annotation->start && annotation->start < endPos) || (startPos < annotation->end
                                                                              && annotation->end < endPos)) {
            firstOffset = (annotation->offset.offset < firstOffset) ? annotation->offset.offset : firstOffset;
        }
    }
    mCtxMenu->setFirstOffsetInLine(firstOffset);
    QList<RVA> functionBreakpoints = Core()->getBreakpointsInFunction(decompiledFunctionAddr);
    QVector<RVA> offsetList;
    for (RVA bpOffset : functionBreakpoints) {
        size_t pos = positionForOffset(bpOffset);
        if (startPos <= pos && pos <= endPos) {
            offsetList.push_back(bpOffset);
        }
    }
    std::sort(offsetList.begin(), offsetList.end());
    mCtxMenu->setAvailableBreakpoints(offsetList);
}

void DecompilerWidget::refreshIfChanged(RVA addr)
{
    if (addressInRange(addr)) {
        doRefresh();
    }
}

void DecompilerWidget::doRefresh()
{
    RVA addr = seekable->getOffset();
    if (!refreshDeferrer->attemptRefresh(nullptr)) {
        return;
    }
    if (ui->decompilerComboBox->currentIndex() < 0) {
        return;
    }
    Decompiler *dec = getCurrentDecompiler();
    if (!dec) {
        return;
    }
    // Disabling decompiler selection combo box and making progress label visible ahead of decompilation.
    ui->progressLabel->setVisible(true);
    ui->decompilerComboBox->setEnabled(false);
    if (dec->isRunning()) {
        if (!decompilerBusy) {
            connect(dec, &Decompiler::finished, this, &DecompilerWidget::doRefresh);
        }
        return;
    }
    disconnect(dec, &Decompiler::finished, this, &DecompilerWidget::doRefresh);
    // Clear all selections since we just refreshed
    ui->textEdit->setExtraSelections({});
    previousFunctionAddr = decompiledFunctionAddr;
    decompiledFunctionAddr = Core()->getFunctionStart(addr);
    updateWindowTitle();
    if (decompiledFunctionAddr == RVA_INVALID) {
        // No function was found, so making the progress label invisible and enabling
        // the decompiler selection combo box as we are not waiting for any decompilation to finish.
        ui->progressLabel->setVisible(false);
        ui->decompilerComboBox->setEnabled(true);
        connectCursorPositionChanged(false);
        ui->textEdit->setPlainText(
            tr("No function found at this offset. Seek to a function or define one in order to decompile it."));
        connectCursorPositionChanged(true);
        return;
    }
    mCtxMenu->setDecompiledFunctionAddress(decompiledFunctionAddr);
    connect(dec, &Decompiler::finished, this, &DecompilerWidget::decompilationFinished);
    decompilerBusy = true;
    dec->decompileAt(addr);
}

void DecompilerWidget::refreshDecompiler()
{
    doRefresh();
    setInfoForBreakpoints();
}

QTextCursor DecompilerWidget::getCursorForAddress(RVA addr)
{
    size_t pos = positionForOffset(addr);
    if (pos == SIZE_MAX || pos == 0) {
        return QTextCursor();
    }
    QTextCursor cursor = ui->textEdit->textCursor();
    cursor.setPosition(pos);
    return cursor;
}

void DecompilerWidget::decompilationFinished(RAnnotatedCode *codeDecompiled)
{
    bool isDisplayReset = false;
    if (previousFunctionAddr == decompiledFunctionAddr) {
        scrollerHorizontal = ui->textEdit->horizontalScrollBar()->sliderPosition();
        scrollerVertical = ui->textEdit->verticalScrollBar()->sliderPosition();
        isDisplayReset = true;
    }

    ui->progressLabel->setVisible(false);
    ui->decompilerComboBox->setEnabled(decompilerSelectionEnabled);

    mCtxMenu->setAnnotationHere(nullptr);
    this->code.reset(codeDecompiled);

    Decompiler *dec = getCurrentDecompiler();
    QObject::disconnect(dec, &Decompiler::finished, this, &DecompilerWidget::decompilationFinished);
    decompilerBusy = false;

    QString codeString = QString::fromUtf8(this->code->code);
    if (codeString.isEmpty()) {
        connectCursorPositionChanged(false);
        ui->textEdit->setPlainText(tr("Cannot decompile at this address (Not a function?)"));
        connectCursorPositionChanged(true);
        lowestOffsetInCode = RVA_MAX;
        highestOffsetInCode = 0;
        return;
    } else {
        connectCursorPositionChanged(false);
        ui->textEdit->setPlainText(codeString);
        connectCursorPositionChanged(true);
        updateCursorPosition();
        highlightPC();
        highlightBreakpoints();
        lowestOffsetInCode = RVA_MAX;
        highestOffsetInCode = 0;
        void *iter;
        r_vector_foreach(&code->annotations, iter) {
            RCodeAnnotation *annotation = (RCodeAnnotation *)iter;
            if (annotation->type == R_CODE_ANNOTATION_TYPE_OFFSET) {
                if (lowestOffsetInCode > annotation->offset.offset) {
                    lowestOffsetInCode = annotation->offset.offset;
                }
                if (highestOffsetInCode < annotation->offset.offset) {
                    highestOffsetInCode = annotation->offset.offset;
                }
            }
        }
    }

    if (isDisplayReset) {
        ui->textEdit->horizontalScrollBar()->setSliderPosition(scrollerHorizontal);
        ui->textEdit->verticalScrollBar()->setSliderPosition(scrollerVertical);
    }
}

void DecompilerWidget::setAnnotationsAtCursor(size_t pos)
{
    RCodeAnnotation *annotationAtPos = nullptr;
    void *iter;
    r_vector_foreach(&this->code->annotations, iter) {
        RCodeAnnotation *annotation = (RCodeAnnotation *)iter;
        if (annotation->type == R_CODE_ANNOTATION_TYPE_OFFSET ||
                annotation->type == R_CODE_ANNOTATION_TYPE_SYNTAX_HIGHLIGHT ||
                annotation->start > pos || annotation->end <= pos) {
            continue;
        }
        annotationAtPos = annotation;
        break;
    }
    mCtxMenu->setAnnotationHere(annotationAtPos);
}

void DecompilerWidget::decompilerSelected()
{
    Config()->setSelectedDecompiler(ui->decompilerComboBox->currentData().toString());
    doRefresh();
}

void DecompilerWidget::connectCursorPositionChanged(bool connectPositionChange)
{
    if (!connectPositionChange) {
        disconnect(ui->textEdit, &QPlainTextEdit::cursorPositionChanged, this,
                   &DecompilerWidget::cursorPositionChanged);
    } else {
        connect(ui->textEdit, &QPlainTextEdit::cursorPositionChanged, this,
                &DecompilerWidget::cursorPositionChanged);
    }
}

void DecompilerWidget::cursorPositionChanged()
{
    // Do not perform seeks along with the cursor while selecting multiple lines
    if (!ui->textEdit->textCursor().selectedText().isEmpty()) {
        return;
    }

    size_t pos = ui->textEdit->textCursor().position();
    setAnnotationsAtCursor(pos);
    setInfoForBreakpoints();

    RVA offset = offsetForPosition(pos);
    if (offset != RVA_INVALID && offset != seekable->getOffset()) {
        seekFromCursor = true;
        seekable->seek(offset);
        mCtxMenu->setOffset(offset);
        seekFromCursor = false;
    }
    updateSelection();
}

void DecompilerWidget::seekChanged()
{
    if (seekFromCursor) {
        return;
    }
    RVA fcnAddr = Core()->getFunctionStart(seekable->getOffset());
    if (fcnAddr == RVA_INVALID || fcnAddr != decompiledFunctionAddr) {
        doRefresh();
        return;
    }
    updateCursorPosition();
}

void DecompilerWidget::updateCursorPosition()
{
    RVA offset = seekable->getOffset();
    size_t pos = positionForOffset(offset);
    if (pos == SIZE_MAX) {
        return;
    }
    mCtxMenu->setOffset(offset);
    connectCursorPositionChanged(false);
    QTextCursor cursor = ui->textEdit->textCursor();
    cursor.setPosition(pos);
    ui->textEdit->setTextCursor(cursor);
    updateSelection();
    connectCursorPositionChanged(true);
}

void DecompilerWidget::setupFonts()
{
    ui->textEdit->setFont(Config()->getFont());
}

void DecompilerWidget::updateSelection()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    // Highlight the current line
    QTextCursor cursor = ui->textEdit->textCursor();
    extraSelections.append(createLineHighlightSelection(cursor));

    // Highlight all the words in the document same as the current one
    cursor.select(QTextCursor::WordUnderCursor);
    QString searchString = cursor.selectedText();
    mCtxMenu->setCurHighlightedWord(searchString);
    extraSelections.append(createSameWordsSelections(ui->textEdit, searchString));

    ui->textEdit->setExtraSelections(extraSelections);
    // Highlight PC after updating the selected line
    highlightPC();
}

QString DecompilerWidget::getWindowTitle() const
{
    RAnalFunction *fcn = Core()->functionAt(decompiledFunctionAddr);
    QString windowTitle = tr("Decompiler");
    if (fcn != NULL) {
        windowTitle += " (" + QString(fcn->name) + ")";
    } else {
        windowTitle += " (Empty)";
    }
    return windowTitle;
}

void DecompilerWidget::fontsUpdatedSlot()
{
    setupFonts();
}

void DecompilerWidget::colorsUpdatedSlot()
{
}

void DecompilerWidget::showDecompilerContextMenu(const QPoint &pt)
{
    mCtxMenu->exec(ui->textEdit->mapToGlobal(pt));
}

void DecompilerWidget::seekToReference()
{
    size_t pos = ui->textEdit->textCursor().position();
    // RVA offset = offsetForPosition(pos);
    // seekable->seekToReference(offset);
    RCodeAnnotation *annotationHere = mCtxMenu->getAnnotationHere();
    if (annotationHere && r_annotation_is_reference(annotationHere)) {
        seekable->seek(annotationHere->reference.offset);
    }
}

bool DecompilerWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonDblClick
            && (obj == ui->textEdit || obj == ui->textEdit->viewport())) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        ui->textEdit->setTextCursor(ui->textEdit->cursorForPosition(mouseEvent->pos()));
        seekToReference();
        return true;
    }
    if (event->type() == QEvent::MouseButtonPress
            && (obj == ui->textEdit || obj == ui->textEdit->viewport())) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::RightButton && !ui->textEdit->textCursor().hasSelection()) {
            ui->textEdit->setTextCursor(ui->textEdit->cursorForPosition(mouseEvent->pos()));
            return true;
        }
    }
    return MemoryDockWidget::eventFilter(obj, event);
}

void DecompilerWidget::highlightPC()
{
    RVA PCAddress = Core()->getProgramCounterValue();
    if (PCAddress == RVA_INVALID || (Core()->getFunctionStart(PCAddress) != decompiledFunctionAddr)) {
        return;
    }

    QTextCursor cursor = getCursorForAddress(PCAddress);
    if (!cursor.isNull()) {
        colorLine(createLineHighlightPC(cursor));
    }

}

void DecompilerWidget::highlightBreakpoints()
{

    QList<RVA> functionBreakpoints = Core()->getBreakpointsInFunction(decompiledFunctionAddr);
    QTextCursor cursor;
    for (RVA &bp : functionBreakpoints) {
        if (bp == RVA_INVALID) {
            continue;;
        }
        cursor = getCursorForAddress(bp);
        if (!cursor.isNull()) {
            // Use a Block formatting since these lines are not updated frequently as selections and PC
            QTextBlockFormat f;
            f.setBackground(ConfigColor("gui.breakpoint_background"));
            cursor.setBlockFormat(f);
        }
    }
}

bool DecompilerWidget::colorLine(QTextEdit::ExtraSelection extraSelection)
{
    QList<QTextEdit::ExtraSelection> extraSelections = ui->textEdit->extraSelections();
    extraSelections.append(extraSelection);
    ui->textEdit->setExtraSelections(extraSelections);
    return true;
}

void DecompilerWidget::copy()
{
    if (ui->textEdit->textCursor().hasSelection()) {
        ui->textEdit->copy();
    } else {
        QTextCursor cursor = ui->textEdit->textCursor();
        QClipboard *clipboard = QApplication::clipboard();
        cursor.select(QTextCursor::WordUnderCursor);
        if (!cursor.selectedText().isEmpty()) {
            clipboard->setText(cursor.selectedText());
        } else {
            cursor.select(QTextCursor::LineUnderCursor);
            clipboard->setText(cursor.selectedText());
        }
    }
}

bool DecompilerWidget::addressInRange(RVA addr)
{
    if (lowestOffsetInCode <= addr && addr <= highestOffsetInCode) {
        return true;
    }
    return false;
}
