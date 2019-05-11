
#include "DisassemblerGraphView.h"
#include "common/CutterSeekable.h"
#include "core/Cutter.h"
#include "common/Colors.h"
#include "common/Configuration.h"
#include "common/CachedFontMetrics.h"
#include "common/TempConfig.h"
#include "common/SyntaxHighlighter.h"
#include "common/BasicBlockHighlighter.h"

#include <QColorDialog>
#include <QPainter>
#include <QJsonObject>
#include <QJsonArray>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QShortcut>
#include <QToolTip>
#include <QTextDocument>
#include <QTextEdit>
#include <QFileDialog>
#include <QFile>
#include <QVBoxLayout>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QClipboard>
#include <QApplication>

#include <cmath>

DisassemblerGraphView::DisassemblerGraphView(QWidget *parent)
    : GraphView(parent),
      mFontMetrics(nullptr),
      blockMenu(new DisassemblyContextMenu(this)),
      contextMenu(new QMenu(this)),
      seekable(new CutterSeekable(this))
{
    highlight_token = nullptr;
    auto *layout = new QVBoxLayout(this);
    // Signals that require a refresh all
    connect(Core(), SIGNAL(refreshAll()), this, SLOT(refreshView()));
    connect(Core(), SIGNAL(commentsChanged()), this, SLOT(refreshView()));
    connect(Core(), SIGNAL(functionRenamed(const QString &, const QString &)), this,
            SLOT(refreshView()));
    connect(Core(), SIGNAL(flagsChanged()), this, SLOT(refreshView()));
    connect(Core(), SIGNAL(varsChanged()), this, SLOT(refreshView()));
    connect(Core(), SIGNAL(instructionChanged(RVA)), this, SLOT(refreshView()));
    connect(Core(), SIGNAL(functionsChanged()), this, SLOT(refreshView()));
    connect(Core(), SIGNAL(graphOptionsChanged()), this, SLOT(refreshView()));
    connect(Core(), SIGNAL(asmOptionsChanged()), this, SLOT(refreshView()));
    connect(Core(), SIGNAL(refreshCodeViews()), this, SLOT(refreshView()));

    connect(Config(), SIGNAL(colorsUpdated()), this, SLOT(colorsUpdatedSlot()));
    connect(Config(), SIGNAL(fontsUpdated()), this, SLOT(fontsUpdatedSlot()));
    connectSeekChanged(false);

    // Space to switch to disassembly
    QShortcut *shortcut_disassembly = new QShortcut(QKeySequence(Qt::Key_Space), this);
    shortcut_disassembly->setContext(Qt::WidgetShortcut);
    connect(shortcut_disassembly, &QShortcut::activated, this, [] {
        Core()->setMemoryWidgetPriority(CutterCore::MemoryWidgetType::Disassembly);
        Core()->triggerRaisePrioritizedMemoryWidget();
    });
    // ESC for previous
    QShortcut *shortcut_escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    shortcut_escape->setContext(Qt::WidgetShortcut);
    connect(shortcut_escape, SIGNAL(activated()), seekable, SLOT(seekPrev()));

    // Zoom shortcuts
    QShortcut *shortcut_zoom_in = new QShortcut(QKeySequence(Qt::Key_Plus), this);
    shortcut_zoom_in->setContext(Qt::WidgetShortcut);
    connect(shortcut_zoom_in, &QShortcut::activated, this, std::bind(&DisassemblerGraphView::zoom, this,
                                                                     QPointF(0.5, 0.5), 1));
    QShortcut *shortcut_zoom_out = new QShortcut(QKeySequence(Qt::Key_Minus), this);
    shortcut_zoom_out->setContext(Qt::WidgetShortcut);
    connect(shortcut_zoom_out, &QShortcut::activated, this, std::bind(&DisassemblerGraphView::zoom,
                                                                      this, QPointF(0.5, 0.5), -1));
    QShortcut *shortcut_zoom_reset = new QShortcut(QKeySequence(Qt::Key_Equal), this);
    shortcut_zoom_reset->setContext(Qt::WidgetShortcut);
    connect(shortcut_zoom_reset, SIGNAL(activated()), this, SLOT(zoomReset()));

    // Branch shortcuts
    QShortcut *shortcut_take_true = new QShortcut(QKeySequence(Qt::Key_T), this);
    shortcut_take_true->setContext(Qt::WidgetShortcut);
    connect(shortcut_take_true, SIGNAL(activated()), this, SLOT(takeTrue()));
    QShortcut *shortcut_take_false = new QShortcut(QKeySequence(Qt::Key_F), this);
    shortcut_take_false->setContext(Qt::WidgetShortcut);
    connect(shortcut_take_false, SIGNAL(activated()), this, SLOT(takeFalse()));

    // Navigation shortcuts
    QShortcut *shortcut_next_instr = new QShortcut(QKeySequence(Qt::Key_J), this);
    shortcut_next_instr->setContext(Qt::WidgetShortcut);
    connect(shortcut_next_instr, SIGNAL(activated()), this, SLOT(nextInstr()));
    QShortcut *shortcut_prev_instr = new QShortcut(QKeySequence(Qt::Key_K), this);
    shortcut_prev_instr->setContext(Qt::WidgetShortcut);
    connect(shortcut_prev_instr, SIGNAL(activated()), this, SLOT(prevInstr()));
    shortcuts.append(shortcut_disassembly);
    shortcuts.append(shortcut_escape);
    shortcuts.append(shortcut_zoom_in);
    shortcuts.append(shortcut_zoom_out);
    shortcuts.append(shortcut_zoom_reset);
    shortcuts.append(shortcut_next_instr);
    shortcuts.append(shortcut_prev_instr);

    // Export Graph menu
    actionExportGraph.setText(tr("Export Graph"));
    connect(&actionExportGraph, SIGNAL(triggered(bool)), this, SLOT(on_actionExportGraph_triggered()));
    actionSyncOffset.setText(tr("Sync/unsync offset"));
    connect(&actionSyncOffset, SIGNAL(triggered(bool)), this, SLOT(toggleSync()));

    // Context menu that applies to everything
    contextMenu->addAction(&actionExportGraph);
    contextMenu->addSeparator();
    contextMenu->addAction(&actionSyncOffset);


    QAction *highlightBB = new QAction(this);
    highlightBB->setText(tr("Highlight block"));
    connect(highlightBB, &QAction::triggered, this, [this]() {
        auto bbh = Core()->getBBHighlighter();
        QColor c = QColorDialog::getColor(disassemblySelectedBackgroundColor);
        if (c.isValid()) {
            bbh->highlight(blockForAddress(seekable->getOffset())->entry, c);
        }
    });

    QAction *unhighlight = new QAction(this);
    unhighlight->setText(tr("Unhighlight block"));
    connect(unhighlight, &QAction::triggered, this, [this]() {
        auto bbh = Core()->getBBHighlighter();
        bbh->clear(blockForAddress(seekable->getOffset())->entry);
    });

    blockMenu->addAction(highlightBB);
    blockMenu->addAction(unhighlight);


    // Include all actions from generic context menu in block specific menu
    blockMenu->addSeparator();
    blockMenu->addActions(contextMenu->actions());


    initFont();
    colorsUpdatedSlot();

    connect(blockMenu, &DisassemblyContextMenu::copy, this, &DisassemblerGraphView::copySelection);

    header = new QTextEdit();
    header->setFixedHeight(30);
    header->setReadOnly(true);
    header->setLineWrapMode(QTextEdit::NoWrap);

    // Add header as widget to layout so it stretches to the layout width
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setAlignment(Qt::AlignTop);
    layout->addWidget(header);

    prepareHeader();

    highlighter = new SyntaxHighlighter(header->document());
}

void DisassemblerGraphView::connectSeekChanged(bool disconn)
{
    if (disconn) {
        disconnect(seekable, &CutterSeekable::seekableSeekChanged, this,
                   &DisassemblerGraphView::onSeekChanged);
    } else {
        connect(seekable, &CutterSeekable::seekableSeekChanged, this,
                &DisassemblerGraphView::onSeekChanged);
    }
}

DisassemblerGraphView::~DisassemblerGraphView()
{
    for (QShortcut *shortcut : shortcuts) {
        delete shortcut;
    }
}

void DisassemblerGraphView::toggleSync()
{
    seekable->toggleSynchronization();
    if (seekable->isSynchronized()) {
        parentWidget()->setWindowTitle(windowTitle);
    } else {
        parentWidget()->setWindowTitle(windowTitle + CutterSeekable::tr(" (unsynced)"));
    }
}

void DisassemblerGraphView::refreshView()
{
    initFont();
    loadCurrentGraph();
    viewport()->update();
    emit viewRefreshed();
}

void DisassemblerGraphView::loadCurrentGraph()
{
    TempConfig tempConfig;
    tempConfig.set("scr.color", COLOR_MODE_16M)
    .set("asm.bb.line", false)
    .set("asm.lines", false)
    .set("asm.lines.fcn", false);

    QJsonArray functions;
    RAnalFunction *fcn = Core()->functionAt(seekable->getOffset());
    if (fcn) {
        currentFcnAddr = fcn->addr;
        QJsonDocument functionsDoc = Core()->cmdj("agJ " + RAddressString(fcn->addr));
        functions = functionsDoc.array();
    }

    disassembly_blocks.clear();
    blocks.clear();

    if (highlight_token) {
        delete highlight_token;
        highlight_token = nullptr;
    }

    emptyGraph = functions.isEmpty();
    if (emptyGraph) {
        // If there's no function to print, just add a message
        if (!emptyText) {
            emptyText = new QLabel(this);
            emptyText->setText(tr("No function detected. Cannot display graph."));
            emptyText->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
            layout()->addWidget(emptyText);
            layout()->setAlignment(emptyText, Qt::AlignHCenter);
        }
        emptyText->setVisible(true);
    } else if (emptyText) {
        emptyText->setVisible(false);
    }
    // Refresh global "empty graph" variable so other widget know there is nothing to show here
    Core()->setGraphEmpty(emptyGraph);

    QJsonValue funcRef = functions.first();
    QJsonObject func = funcRef.toObject();

    windowTitle = tr("Graph");
    QString funcName = func["name"].toString().trimmed();
    if (emptyGraph) {
        windowTitle += " (Empty)";
    } else if (!funcName.isEmpty()) {
        windowTitle += " (" + funcName + ")";
    }
    if (!seekable->isSynchronized()) {
        parentWidget()->setWindowTitle(windowTitle + CutterSeekable::tr(" (unsynced)"));
    } else {
        parentWidget()->setWindowTitle(windowTitle);
    }

    RVA entry = func["offset"].toVariant().toULongLong();

    setEntry(entry);
    for (const QJsonValueRef &value : func["blocks"].toArray()) {
        QJsonObject block = value.toObject();
        RVA block_entry = block["offset"].toVariant().toULongLong();
        RVA block_size = block["size"].toVariant().toULongLong();
        RVA block_fail = block["fail"].toVariant().toULongLong();
        RVA block_jump = block["jump"].toVariant().toULongLong();

        DisassemblyBlock db;
        GraphBlock gb;
        gb.entry = block_entry;
        db.entry = block_entry;
        db.true_path = RVA_INVALID;
        db.false_path = RVA_INVALID;
        if (block_fail) {
            db.false_path = block_fail;
            gb.edges.emplace_back(block_fail);
        }
        if (block_jump) {
            if (block_fail) {
                db.true_path = block_jump;
            }
            gb.edges.emplace_back(block_jump);
        }

        QJsonObject switchOp = block["switchop"].toObject();
        if (!switchOp.isEmpty()) {
            QJsonArray caseArray = switchOp["cases"].toArray();
            for (QJsonValue caseOpValue : caseArray) {
                QJsonObject caseOp = caseOpValue.toObject();
                bool ok;
                RVA caseJump = caseOp["jump"].toVariant().toULongLong(&ok);
                if (!ok) {
                    continue;
                }
                gb.edges.emplace_back(caseJump);
            }
        }

        QJsonArray opArray = block["ops"].toArray();
        for (int opIndex = 0; opIndex < opArray.size(); opIndex++) {
            QJsonObject op = opArray[opIndex].toObject();
            Instr i;
            i.addr = op["offset"].toVariant().toULongLong();

            if (opIndex < opArray.size() - 1) {
                // get instruction size from distance to next instruction ...
                RVA nextOffset = opArray[opIndex + 1].toObject()["offset"].toVariant().toULongLong();
                i.size = nextOffset - i.addr;
            } else {
                // or to the end of the block.
                i.size = (block_entry + block_size) - i.addr;
            }

            QTextDocument textDoc;
            textDoc.setHtml(CutterCore::ansiEscapeToHtml(op["text"].toString()));

            i.plainText = textDoc.toPlainText();

            RichTextPainter::List richText = RichTextPainter::fromTextDocument(textDoc);
            //Colors::colorizeAssembly(richText, textDoc.toPlainText(), 0);

            bool cropped;
            int blockLength = Config()->getGraphBlockMaxChars() + Core()->getConfigb("asm.bytes") * 24 +
                              Core()->getConfigb("asm.emu") * 10;
            i.text = Text(RichTextPainter::cropped(richText, blockLength, "...", &cropped));
            if (cropped)
                i.fullText = richText;
            else
                i.fullText = Text();
            db.instrs.push_back(i);
        }
        disassembly_blocks[db.entry] = db;
        prepareGraphNode(gb);

        addBlock(gb);
    }

    if (!func["blocks"].toArray().isEmpty()) {
        computeGraph(entry);
    }
}

DisassemblerGraphView::EdgeConfigurationMapping DisassemblerGraphView::getEdgeConfigurations()
{
    EdgeConfigurationMapping result;
    for (auto &block : blocks) {
        for (const auto &edge : block.second.edges) {
            result[ {block.first, edge.target}] = edgeConfiguration(block.second, &blocks[edge.target]);
        }
    }
    return result;
}

void DisassemblerGraphView::prepareGraphNode(GraphBlock &block)
{
    DisassemblyBlock &db = disassembly_blocks[block.entry];
    int width = 0;
    int height = 0;
    for (auto &line : db.header_text.lines) {
        int lw = 0;
        for (auto &part : line)
            lw += mFontMetrics->width(part.text);
        if (lw > width)
            width = lw;
        height += 1;
    }
    for (Instr &instr : db.instrs) {
        for (auto &line : instr.text.lines) {
            int lw = 0;
            for (auto &part : line)
                lw += mFontMetrics->width(part.text);
            if (lw > width)
                width = lw;
            height += 1;
        }
    }
    int extra = static_cast<int>(4 * charWidth + 4);
    block.width = static_cast<int>(width + extra + charWidth);
    block.height = (height * charHeight) + extra;
}

void DisassemblerGraphView::prepareHeader()
{
    QString afcf = Core()->cmd("afcf").trimmed();
    if (afcf.isEmpty()) {
        header->hide();
        return;
    }
    header->show();
    header->setPlainText(afcf);
}

void DisassemblerGraphView::initFont()
{
    setFont(Config()->getFont());
    QFontMetricsF metrics(font());
    baseline = int(metrics.ascent());
    charWidth = metrics.width('X');
    charHeight = static_cast<int>(metrics.height());
    charOffset = 0;
    mFontMetrics.reset(new CachedFontMetrics<qreal>(font()));
}

void DisassemblerGraphView::drawBlock(QPainter &p, GraphView::GraphBlock &block)
{
    int blockX = block.x - getViewOffset().x();
    int blockY = block.y - getViewOffset().y();

    const qreal padding = 2 * charWidth;

    p.setPen(Qt::black);
    p.setBrush(Qt::gray);
    p.setFont(Config()->getFont());
    p.drawRect(blockX, blockY, block.width, block.height);

    breakpoints = Core()->getBreakpointsAddresses();

    // Render node
    DisassemblyBlock &db = disassembly_blocks[block.entry];
    bool block_selected = false;
    bool PCInBlock = false;
    RVA selected_instruction = RVA_INVALID;

    // Figure out if the current block is selected
    RVA addr = seekable->getOffset();
    RVA PCAddr = Core()->getProgramCounterValue();
    for (const Instr &instr : db.instrs) {
        if (instr.contains(addr)) {
            block_selected = true;
            selected_instruction = instr.addr;
        }
        if (instr.contains(PCAddr)) {
            PCInBlock = true;
        }

        // TODO: L219
    }

    p.setPen(QColor(0, 0, 0, 0));
    if (db.terminal) {
        p.setBrush(retShadowColor);
    } else if (db.indirectcall) {
        p.setBrush(indirectcallShadowColor);
    } else {
        p.setBrush(QColor(0, 0, 0, 100));
    }

    p.setPen(QPen(graphNodeColor, 1));

    if (block_selected) {
        p.setBrush(disassemblySelectedBackgroundColor);
    } else {
        p.setBrush(disassemblyBackgroundColor);
    }

    // Draw basic block background
    p.drawRect(blockX, blockY,
               block.width, block.height);
    auto bb = Core()->getBBHighlighter()->getBasicBlock(block.entry);
    if (bb) {
        QColor color(bb->color);
        p.setBrush(color);
        p.drawRect(blockX, blockY,
                   block.width, block.height);
    }

    const int firstInstructionY = blockY + getInstructionOffset(db, 0).y();

    // Draw different background for selected instruction
    if (selected_instruction != RVA_INVALID) {
        int y = firstInstructionY;
        for (const Instr &instr : db.instrs) {
            if (instr.addr > selected_instruction) {
                break;
            }
            auto selected = instr.addr == selected_instruction;
            if (selected) {
                p.fillRect(QRect(static_cast<int>(blockX + charWidth), y,
                                 static_cast<int>(block.width - (10 + padding)),
                                 int(instr.text.lines.size()) * charHeight), disassemblySelectionColor);
            }
            y += int(instr.text.lines.size()) * charHeight;
        }
    }

    // Highlight selected tokens
    if (highlight_token != nullptr) {
        int y = firstInstructionY;
        qreal tokenWidth = mFontMetrics->width(highlight_token->content);

        for (const Instr &instr : db.instrs) {
            int pos = -1;

            while ((pos = instr.plainText.indexOf(highlight_token->content, pos + 1)) != -1) {
                int tokenEnd = pos + highlight_token->content.length();

                if ((pos > 0 && instr.plainText[pos - 1].isLetterOrNumber())
                        || (tokenEnd < instr.plainText.length() && instr.plainText[tokenEnd].isLetterOrNumber())) {
                    continue;
                }

                qreal widthBefore = mFontMetrics->width(instr.plainText.left(pos));
                if (charWidth * 3 + widthBefore > block.width - (10 + padding)) {
                    continue;
                }

                qreal highlightWidth = tokenWidth;
                if (charWidth * 3 + widthBefore + tokenWidth >= block.width - (10 + padding)) {
                    highlightWidth = block.width - widthBefore - (10 +  2 * padding);
                }

                QColor selectionColor = ConfigColor("wordhl");

                p.fillRect(QRectF(blockX + charWidth * 3 + widthBefore, y, highlightWidth,
                                 charHeight), selectionColor);
            }

            y += int(instr.text.lines.size()) * charHeight;
        }
    }

    // Highlight program counter
    if (PCInBlock) {
        int y = firstInstructionY;
        for (const Instr &instr : db.instrs) {
            if (instr.addr > PCAddr) {
                break;
            }
            auto PC = instr.addr == PCAddr;
            if (PC) {
                p.fillRect(QRect(static_cast<int>(blockX + charWidth), y,
                                 static_cast<int>(block.width - (10 + padding)),
                                 int(instr.text.lines.size()) * charHeight), PCSelectionColor);
            }
            y += int(instr.text.lines.size()) * charHeight;
        }
    }

    qreal render_height = viewport()->size().height();

    // Stop rendering text when it's too small
    if (charHeight * getViewScale() * p.device()->devicePixelRatioF() < 4) {
        return;
    }

    // Render node text
    auto x = blockX + padding;
    int y = blockY + getTextOffset(0).y();
    qreal lineHeightRender = charHeight;
    for (auto &line : db.header_text.lines) {
        qreal lineYRender = y;
        lineYRender *= getViewScale();
        // Check if line does NOT intersects with view area
        if (0 > lineYRender + lineHeightRender
                || render_height < lineYRender) {
            y += charHeight;
            continue;
        }

        RichTextPainter::paintRichText<qreal>(&p, x, y, block.width, charHeight, 0, line,
                                       mFontMetrics.get());
        y += charHeight;
    }

    for (const Instr &instr : db.instrs) {
        if (Core()->isBreakpoint(breakpoints, instr.addr)) {
            p.fillRect(QRect(static_cast<int>(blockX + charWidth), y,
                             static_cast<int>(block.width - (10 + padding)),
                             int(instr.text.lines.size()) * charHeight), ConfigColor("gui.breakpoint_background"));
            if (instr.addr == selected_instruction) {
                p.fillRect(QRect(static_cast<int>(blockX + charWidth), y,
                                 static_cast<int>(block.width - (10 + padding)),
                                 int(instr.text.lines.size()) * charHeight), disassemblySelectionColor);
            }
        }
        for (auto &line : instr.text.lines) {
            qreal lineYRender = y;
            lineYRender *= getViewScale();
            if (0 > lineYRender + lineHeightRender
                    || render_height < lineYRender) {
                y += charHeight;
                continue;
            }

            int rectSize = qRound(charWidth);
            if (rectSize % 2) {
                rectSize++;
            }
            // Assume charWidth <= charHeight
            // TODO: Breakpoint/Cip stuff
            QRectF bpRect(x - rectSize / 3.0, y + (charHeight - rectSize) / 2.0, rectSize, rectSize);
            Q_UNUSED(bpRect);

            RichTextPainter::paintRichText<qreal>(&p, x + charWidth, y,
                                           block.width - charWidth, charHeight, 0, line,
                                           mFontMetrics.get());
            y += charHeight;

        }
    }
}

GraphView::EdgeConfiguration DisassemblerGraphView::edgeConfiguration(GraphView::GraphBlock &from,
                                                                      GraphView::GraphBlock *to)
{
    EdgeConfiguration ec;
    DisassemblyBlock &db = disassembly_blocks[from.entry];
    if (to->entry == db.true_path) {
        ec.color = brtrueColor;
    } else if (to->entry == db.false_path) {
        ec.color = brfalseColor;
    } else {
        ec.color = jmpColor;
    }
    ec.start_arrow = false;
    ec.end_arrow = true;
    return ec;
}


RVA DisassemblerGraphView::getAddrForMouseEvent(GraphBlock &block, QPoint *point)
{
    DisassemblyBlock &db = disassembly_blocks[block.entry];

    // Remove header and margin
    int off_y = getInstructionOffset(db, 0).y();

    // Get mouse coordinate over the actual text
    int text_point_y = point->y() - off_y;
    int mouse_row = text_point_y / charHeight;

    int cur_row = static_cast<int>(db.header_text.lines.size());
    if (mouse_row < cur_row) {
        return db.entry;
    }

    Instr *instr = getInstrForMouseEvent(block, point);
    if (instr) {
        return instr->addr;
    }

    return RVA_INVALID;
}

DisassemblerGraphView::Instr *DisassemblerGraphView::getInstrForMouseEvent(
    GraphView::GraphBlock &block, QPoint *point)
{
    DisassemblyBlock &db = disassembly_blocks[block.entry];

    // Remove header and margin
    int off_y = getInstructionOffset(db, 0).y();

    // Get mouse coordinate over the actual text
    int text_point_y = point->y() - off_y;
    int mouse_row = text_point_y / charHeight;

    int cur_row = static_cast<int>(db.header_text.lines.size());

    for (Instr &instr : db.instrs) {
        if (mouse_row < cur_row + (int)instr.text.lines.size()) {
            return &instr;
        }
        cur_row += instr.text.lines.size();
    }

    return nullptr;
}

QRectF DisassemblerGraphView::getInstrRect(GraphView::GraphBlock &block, RVA addr) const
{
    auto blockIt = disassembly_blocks.find(block.entry);
    if (blockIt == disassembly_blocks.end()) {
        return QRectF();
    }
    auto &db = blockIt->second;
    if (db.instrs.empty()) {
        return QRectF();
    }

    size_t sequenceAddr = db.instrs[0].addr;
    size_t firstLineWithAddr = 0;
    size_t currentLine = 0;
    for (size_t i = 0; i < db.instrs.size(); i++) {
        auto &instr = db.instrs[i];
        if (instr.addr != sequenceAddr) {
            sequenceAddr = instr.addr;
            firstLineWithAddr = currentLine;
        }
        if (instr.contains(addr)) {
            while (i < db.instrs.size() && db.instrs[i + 1].addr == sequenceAddr) {
                currentLine += db.instrs[i].text.lines.size();
                i++;
            }
            QPointF topLeft = getInstructionOffset(db, static_cast<int>(firstLineWithAddr));
            return QRectF(topLeft, QSizeF(block.width - 4 * charWidth,
                                          charHeight * int(currentLine - firstLineWithAddr + db.instrs[i].text.lines.size())));
        }
        currentLine += instr.text.lines.size();
    }
    return QRectF();
}

void DisassemblerGraphView::showInstruction(GraphView::GraphBlock &block, RVA addr)
{
    QRectF rect = getInstrRect(block, addr);
    rect.translate(block.x, block.y);
    showRectangle(QRect(rect.x(), rect.y(), rect.width(), rect.height()), true);
}

// Public Slots

void DisassemblerGraphView::colorsUpdatedSlot()
{
    disassemblyBackgroundColor = ConfigColor("gui.alt_background");
    disassemblySelectedBackgroundColor = ConfigColor("gui.disass_selected");
    mDisabledBreakpointColor = disassemblyBackgroundColor;
    graphNodeColor = ConfigColor("gui.border");
    backgroundColor = ConfigColor("gui.background");
    disassemblySelectionColor = ConfigColor("linehl");
    PCSelectionColor = ConfigColor("highlightPC");

    jmpColor = ConfigColor("graph.trufae");
    brtrueColor = ConfigColor("graph.true");
    brfalseColor = ConfigColor("graph.false");

    mCommentColor = ConfigColor("comment");
    initFont();
    refreshView();
}

void DisassemblerGraphView::fontsUpdatedSlot()
{
    initFont();
    refreshView();
}

DisassemblerGraphView::DisassemblyBlock *DisassemblerGraphView::blockForAddress(RVA addr)
{
    for (auto &blockIt : disassembly_blocks) {
        DisassemblyBlock &db = blockIt.second;
        for (const Instr &i : db.instrs) {
            if (i.addr == RVA_INVALID || i.size == RVA_INVALID) {
                continue;
            }

            if (i.contains(addr)) {
                return &db;
            }
        }
    }
    return nullptr;
}

void DisassemblerGraphView::onSeekChanged(RVA addr)
{
    blockMenu->setOffset(addr);
    DisassemblyBlock *db = blockForAddress(addr);
    bool switchFunction = false;
    if (!db) {
        // not in this function, try refreshing
        refreshView();
        db = blockForAddress(addr);
        switchFunction = true;
    }
    if (db) {
        // This is a local address! We animated to it.
        transition_dont_seek = true;
        showBlock(&blocks[db->entry], !switchFunction);
        showInstruction(blocks[db->entry], addr);
        prepareHeader();
    } else {
        header->hide();
    }
}

void DisassemblerGraphView::zoom(QPointF mouseRelativePos, double velocity)
{
    mouseRelativePos.rx() *= size().width();
    mouseRelativePos.ry() *= size().height();
    mouseRelativePos /= getViewScale();

    auto globalMouse = mouseRelativePos + getViewOffset();
    mouseRelativePos *= getViewScale();
    qreal newScale = getViewScale() * std::pow(1.25, velocity);
    newScale = std::max(newScale, 0.05);
    mouseRelativePos /= newScale;
    setViewScale(newScale);

    // Adjusting offset, so that zooming will be approaching to the cursor.
    setViewOffset(globalMouse.toPoint() - mouseRelativePos.toPoint());

    viewport()->update();
    emit viewZoomed();
}

void DisassemblerGraphView::zoomReset()
{
    setViewScale(1.0);
    viewport()->update();
    emit viewZoomed();
}

void DisassemblerGraphView::takeTrue()
{
    DisassemblyBlock *db = blockForAddress(seekable->getOffset());
    if (!db) {
        return;
    }

    if (db->true_path != RVA_INVALID) {
        seekable->seek(db->true_path);
    } else if (!blocks[db->entry].edges.empty()) {
        seekable->seek(blocks[db->entry].edges[0].target);
    }
}

void DisassemblerGraphView::takeFalse()
{
    DisassemblyBlock *db = blockForAddress(seekable->getOffset());
    if (!db) {
        return;
    }

    if (db->false_path != RVA_INVALID) {
        seekable->seek(db->false_path);
    } else if (!blocks[db->entry].edges.empty()) {
        seekable->seek(blocks[db->entry].edges[0].target);
    }
}

void DisassemblerGraphView::seekInstruction(bool previous_instr)
{
    RVA addr = seekable->getOffset();
    DisassemblyBlock *db = blockForAddress(addr);
    if (!db) {
        return;
    }

    for (size_t i = 0; i < db->instrs.size(); i++) {
        Instr &instr = db->instrs[i];
        if (!instr.contains(addr)) {
            continue;
        }

        // Found the instruction. Check if a next one exists
        if (!previous_instr && (i < db->instrs.size() - 1)) {
            seekable->seek(db->instrs[i + 1].addr);
        } else if (previous_instr && (i > 0)) {
            while (i > 0 && db->instrs[i].addr == addr) { // jump over 0 size instructions
                i--;
            }
            seekable->seek(db->instrs[i].addr);
            break;
        }
    }
}

void DisassemblerGraphView::nextInstr()
{
    seekInstruction(false);
}

void DisassemblerGraphView::prevInstr()
{
    seekInstruction(true);
}

void DisassemblerGraphView::seekLocal(RVA addr, bool update_viewport)
{
    RVA curAddr = seekable->getOffset();
    if (addr == curAddr) {
        return;
    }
    connectSeekChanged(true);
    seekable->seek(addr);
    connectSeekChanged(false);
    if (update_viewport) {
        viewport()->update();
    }
}

void DisassemblerGraphView::copySelection()
{
    if (!highlight_token) return;

    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(highlight_token->content);
}

DisassemblerGraphView::Token *DisassemblerGraphView::getToken(Instr *instr, int x)
{
    x -= (int) (3 * charWidth); // Ignore left margin
    if (x < 0) {
        return nullptr;
    }

    int clickedCharPos = mFontMetrics->position(instr->plainText, x);
    if (clickedCharPos > instr->plainText.length()) {
        return nullptr;
    }

    static const QRegularExpression tokenRegExp(R"(\b(?<!\.)([^\s]+)\b(?!\.))");
    QRegularExpressionMatchIterator i = tokenRegExp.globalMatch(instr->plainText);

    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();

        if (match.capturedStart() <= clickedCharPos && match.capturedEnd() > clickedCharPos) {
            auto t = new Token;
            t->start = match.capturedStart();
            t->length = match.capturedLength();
            t->content = match.captured();
            t->instr = instr;

            return t;
        }
    }

    return nullptr;
}

QPoint DisassemblerGraphView::getTextOffset(int line) const
{
    int padding = static_cast<int>(2 * charWidth);
    return QPoint(padding, padding + line * charHeight);
}

QPoint DisassemblerGraphView::getInstructionOffset(const DisassemblyBlock &block, int line) const
{
    return getTextOffset(line + static_cast<int>(block.header_text.lines.size()));
}

void DisassemblerGraphView::blockClicked(GraphView::GraphBlock &block, QMouseEvent *event,
                                         QPoint pos)
{
    Instr *instr = getInstrForMouseEvent(block, &pos);
    if (!instr) {
        return;
    }

    highlight_token = getToken(instr, pos.x());

    RVA addr = instr->addr;
    seekLocal(addr);

    blockMenu->setOffset(addr);
    blockMenu->setCanCopy(highlight_token);
    if (highlight_token) {
        blockMenu->setCurHighlightedWord(highlight_token->content);
    }
    if (event->button() == Qt::RightButton) {
        event->accept();
        blockMenu->exec(event->globalPos());
    }
    viewport()->update();
}

void DisassemblerGraphView::blockDoubleClicked(GraphView::GraphBlock &block, QMouseEvent *event,
                                               QPoint pos)
{
    Q_UNUSED(event);

    RVA instr = getAddrForMouseEvent(block, &pos);
    if (instr == RVA_INVALID) {
        return;
    }
    QList<XrefDescription> refs = Core()->getXRefs(instr, false, false);
    if (refs.length()) {
        seekable->seek(refs.at(0).to);
    }
    if (refs.length() > 1) {
        qWarning() << "Too many references here. Weird behaviour expected.";
    }
}

void DisassemblerGraphView::blockHelpEvent(GraphView::GraphBlock &block, QHelpEvent *event,
                                           QPoint pos)
{
    Instr *instr = getInstrForMouseEvent(block, &pos);
    if (!instr || instr->fullText.lines.empty()) {
        QToolTip::hideText();
        event->ignore();
        return;
    }

    QToolTip::showText(event->globalPos(), instr->fullText.ToQString());
}

bool DisassemblerGraphView::helpEvent(QHelpEvent *event)
{
    if (!GraphView::helpEvent(event)) {
        QToolTip::hideText();
        event->ignore();
    }

    return true;
}

void DisassemblerGraphView::blockTransitionedTo(GraphView::GraphBlock *to)
{
    if (transition_dont_seek) {
        transition_dont_seek = false;
        return;
    }
    seekLocal(to->entry);
}


void DisassemblerGraphView::on_actionExportGraph_triggered()
{
    QStringList filters;
    filters.append(tr("Graphiz dot (*.dot)"));
    if (!QStandardPaths::findExecutable("dot").isEmpty()
            || !QStandardPaths::findExecutable("xdot").isEmpty()) {
        filters.append(tr("GIF (*.gif)"));
        filters.append(tr("PNG (*.png)"));
        filters.append(tr("JPEG (*.jpg)"));
        filters.append(tr("PostScript (*.ps)"));
        filters.append(tr("SVG (*.svg)"));
        filters.append(tr("JSON (*.json)"));
    }

    QFileDialog dialog(this, tr("Export Graph"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.selectFile("graph");
    dialog.setDefaultSuffix("dot");
    if (!dialog.exec())
        return;
    int startIdx = dialog.selectedNameFilter().lastIndexOf("*.") + 2;
    int count = dialog.selectedNameFilter().length() - startIdx - 1;
    QString format = dialog.selectedNameFilter().mid(startIdx, count);
    QString fileName = dialog.selectedFiles()[0];
    if (format != "dot") {
        TempConfig tempConfig;
        tempConfig.set("graph.gv.format", format);
        qWarning() << Core()->cmd(QString("agfw \"%1\" @ $FB").arg(fileName));
        return;
    }
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Can't open file";
        return;
    }
    QTextStream fileOut(&file);
    fileOut << Core()->cmd("agfd $FB");
}

void DisassemblerGraphView::mousePressEvent(QMouseEvent *event)
{
    GraphView::mousePressEvent(event);
    if (!event->isAccepted() && event->button() == Qt::RightButton) {
        contextMenu->exec(event->globalPos());
        event->accept();
    }
    emit graphMoved();
}

void DisassemblerGraphView::mouseMoveEvent(QMouseEvent *event)
{
    GraphView::mouseMoveEvent(event);
    emit graphMoved();
}

void DisassemblerGraphView::wheelEvent(QWheelEvent *event)
{
    // when CTRL is pressed, we zoom in/out with mouse wheel
    if (Qt::ControlModifier == event->modifiers()) {
        const QPoint numDegrees = event->angleDelta() / 8;
        if (!numDegrees.isNull()) {
            int numSteps = numDegrees.y() / 15;

            QPointF relativeMousePos = event->pos();
            relativeMousePos.rx() /= size().width();
            relativeMousePos.ry() /= size().height();

            zoom(relativeMousePos, numSteps);
        }
        event->accept();
    } else {
        // use mouse wheel for scrolling when CTRL is not pressed
        GraphView::wheelEvent(event);
    }
    emit graphMoved();
}

void DisassemblerGraphView::resizeEvent(QResizeEvent *event)
{
    GraphView::resizeEvent(event);
    emit resized();
}

void DisassemblerGraphView::paintEvent(QPaintEvent *event)
{
    // DisassemblerGraphView is always dirty
    setCacheDirty();
    GraphView::paintEvent(event);
}

bool DisassemblerGraphView::Instr::contains(ut64 addr) const
{
    return this->addr <= addr && (addr - this->addr) < size;
}
