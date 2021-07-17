#include "FunctionsWidget.h"
#include "ui_ListDockWidget.h"

#include "core/MainWindow.h"
#include "common/Helpers.h"
#include "common/FunctionsTask.h"
#include "common/TempConfig.h"
#include "menus/AddressableItemContextMenu.h"

#include <algorithm>
#include <QMenu>
#include <QDebug>
#include <QString>
#include <QResource>
#include <QShortcut>
#include <QJsonArray>
#include <QJsonObject>
#include <QInputDialog>
#include <QActionGroup>
#include <QBitmap>
#include <QPainter>

namespace {

static const int kMaxTooltipWidth = 400;
static const int kMaxTooltipDisasmPreviewLines = 10;
static const int kMaxTooltipHighlightsLines = 5;

}

FunctionModel::FunctionModel(QList<FunctionDescription> *functions, QSet<RVA> *importAddresses,
                             ut64 *mainAdress, bool nested, QFont default_font,
                             QFont highlight_font, QObject *parent)
    : AddressableItemModel<>(parent),
      functions(functions),
      importAddresses(importAddresses),
      mainAdress(mainAdress),
      highlightFont(highlight_font),
      defaultFont(default_font),
      nested(nested),
      currentIndex(-1),
      iconFuncImpDark(":/img/icons/function_import_dark.svg"),
      iconFuncImpLight(":/img/icons/function_import_light.svg"),
      iconFuncMainDark(":/img/icons/function_main_dark.svg"),
      iconFuncMainLight(":/img/icons/function_main_light.svg"),
      iconFuncDark(":/img/icons/function_dark.svg"),
      iconFuncLight(":/img/icons/function_light.svg")

{
    connect(Core(), &CutterCore::seekChanged, this, &FunctionModel::seekChanged);
    connect(Core(), &CutterCore::functionRenamed, this, &FunctionModel::functionRenamed);
}

QModelIndex FunctionModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!parent.isValid())
        return createIndex(row, column, (quintptr)0); // root function nodes have id = 0

    return createIndex(row, column,
                       (quintptr)(parent.row() + 1)); // sub-nodes have id = function index + 1
}

QModelIndex FunctionModel::parent(const QModelIndex &index) const
{
    if (!index.isValid() || index.column() != 0)
        return QModelIndex();

    if (index.internalId() == 0) // root function node
        return QModelIndex();
    else // sub-node
        return this->index((int)(index.internalId() - 1), 0);
}

int FunctionModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return functions->count();

    if (nested) {
        if (parent.internalId() == 0)
            return ColumnCount - 1; // sub-nodes for nested functions
        return 0;
    } else
        return 0;
}

int FunctionModel::columnCount(const QModelIndex & /*parent*/) const
{
    if (nested)
        return 1;
    else
        return ColumnCount;
}

bool FunctionModel::functionIsImport(ut64 addr) const
{
    return importAddresses->contains(addr);
}

bool FunctionModel::functionIsMain(ut64 addr) const
{
    return *mainAdress == addr;
}

QVariant FunctionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    int function_index;
    bool subnode;
    bool is_dark;

    if (index.internalId() != 0) { // sub-node
        function_index = index.parent().row();
        subnode = true;
    } else { // root function node
        function_index = index.row();
        subnode = false;
    }

    const FunctionDescription &function = functions->at(function_index);

    if (function_index >= functions->count())
        return QVariant();

    switch (role) {
    case Qt::DisplayRole:
        if (nested) {
            if (subnode) {
                switch (index.row()) {
                case 0:
                    return tr("Offset: %1").arg(RAddressString(function.offset));
                case 1:
                    return tr("Size: %1").arg(RSizeString(function.linearSize));
                case 2:
                    return tr("Import: %1")
                            .arg(functionIsImport(function.offset) ? tr("true") : tr("false"));
                case 3:
                    return tr("Nargs: %1").arg(RSizeString(function.nargs));
                case 4:
                    return tr("Nbbs: %1").arg(RSizeString(function.nbbs));
                case 5:
                    return tr("Nlocals: %1").arg(RSizeString(function.nlocals));
                case 6:
                    return tr("Call type: %1").arg(function.calltype);
                case 7:
                    return tr("Edges: %1").arg(function.edges);
                case 8:
                    return tr("StackFrame: %1").arg(function.stackframe);
                case 9:
                    return tr("Comment: %1").arg(Core()->getCommentAt(function.offset));
                default:
                    return QVariant();
                }
            } else
                return function.name;
        } else {
            switch (index.column()) {
            case NameColumn:
                return function.name;
            case SizeColumn:
                return QString::number(function.linearSize);
            case ImportColumn:
                return functionIsImport(function.offset) ? tr("true") : tr("false");
            case OffsetColumn:
                return RAddressString(function.offset);
            case NargsColumn:
                return QString::number(function.nargs);
            case NlocalsColumn:
                return QString::number(function.nlocals);
            case NbbsColumn:
                return QString::number(function.nbbs);
            case CalltypeColumn:
                return function.calltype;
            case EdgesColumn:
                return QString::number(function.edges);
            case FrameColumn:
                return QString::number(function.stackframe);
            case CommentColumn:
                return Core()->getCommentAt(function.offset);
            default:
                return QVariant();
            }
        }

    case Qt::DecorationRole: {

        // Check if we aren't inside a tree view
        if (nested && subnode) {
            return QVariant();
        }

        if (index.column() == NameColumn) {
            is_dark = Config()->windowColorIsDark();

            if (functionIsImport(function.offset)) {
                if (is_dark) {
                    return iconFuncImpDark;
                }
                return iconFuncImpLight;

            } else if (functionIsMain(function.offset)) {
                if (is_dark) {
                    return iconFuncMainDark;
                }
                return iconFuncMainLight;
            }

            if (is_dark) {
                return iconFuncDark;
            }
            return iconFuncLight;
        }

        return QVariant();
    }

    case Qt::FontRole:
        if (currentIndex == function_index)
            return highlightFont;
        return defaultFont;

    case Qt::TextAlignmentRole:
        if (index.column() == 1)
            return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
        return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);

    case Qt::ToolTipRole: {

        QStringList disasmPreview =
                Core()->getDisassemblyPreview(function.offset, kMaxTooltipDisasmPreviewLines);
        const QStringList &summary = Core()->cmdList(QString("pdsf @ %1").arg(function.offset));
        const QFont &fnt = Config()->getFont();
        QFontMetrics fm { fnt };

        // elide long strings using current disasm font metrics
        QStringList highlights;
        for (const QString &s : summary) {
            highlights << fm.elidedText(s, Qt::ElideRight, kMaxTooltipWidth);
            if (highlights.length() > kMaxTooltipHighlightsLines) {
                highlights << "...";
                break;
            }
        }
        if (disasmPreview.isEmpty() && highlights.isEmpty())
            return QVariant();

        QString toolTipContent =
                QString("<html><div style=\"font-family: %1; font-size: %2pt; white-space: "
                        "nowrap;\">")
                        .arg(fnt.family())
                        .arg(qMax(6, fnt.pointSize() - 1)); // slightly decrease font size, to
                                                            // keep more text in the same box

        if (!disasmPreview.isEmpty())
            toolTipContent += tr("<div style=\"margin-bottom: 10px;\"><strong>Disassembly "
                                 "preview</strong>:<br>%1</div>")
                                      .arg(disasmPreview.join("<br>"));

        if (!highlights.isEmpty()) {
            toolTipContent += tr("<div><strong>Highlights</strong>:<br>%1</div>")
                                      .arg(highlights.join(QLatin1Char('\n'))
                                                   .toHtmlEscaped()
                                                   .replace(QLatin1Char('\n'), "<br>"));
        }
        toolTipContent += "</div></html>";
        return toolTipContent;
    }

    case Qt::ForegroundRole:
        if (functionIsImport(function.offset)) {
            return QVariant(ConfigColor("gui.imports"));
        } else if (functionIsMain(function.offset)) {
            return QVariant(ConfigColor("gui.main"));
        }

        return QVariant(this->property("color"));

    case FunctionDescriptionRole:
        return QVariant::fromValue(function);

    case IsImportRole:
        return importAddresses->contains(function.offset);

    default:
        return QVariant();
    }
}

QVariant FunctionModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        if (nested) {
            return tr("Name");
        } else {
            switch (section) {
            case NameColumn:
                return tr("Name");
            case SizeColumn:
                return tr("Size");
            case ImportColumn:
                return tr("Imp.");
            case OffsetColumn:
                return tr("Offset");
            case NargsColumn:
                return tr("Nargs");
            case NlocalsColumn:
                return tr("Nlocals");
            case NbbsColumn:
                return tr("Nbbs");
            case CalltypeColumn:
                return tr("Call type");
            case EdgesColumn:
                return tr("Edges");
            case FrameColumn:
                return tr("StackFrame");
            case CommentColumn:
                return tr("Comment");
            default:
                return QVariant();
            }
        }
    }

    return QVariant();
}

void FunctionModel::setNested(bool nested)
{
    beginResetModel();
    this->nested = nested;
    updateCurrentIndex();
    endResetModel();
}

RVA FunctionModel::address(const QModelIndex &index) const
{
    auto function = data(index, FunctionDescriptionRole).value<FunctionDescription>();
    return function.offset;
}

QString FunctionModel::name(const QModelIndex &index) const
{
    auto function = data(index, FunctionDescriptionRole).value<FunctionDescription>();
    return function.name;
}

void FunctionModel::seekChanged(RVA)
{
    int previousIndex = currentIndex;
    if (updateCurrentIndex()) {
        if (previousIndex >= 0) {
            emit dataChanged(index(previousIndex, 0), index(previousIndex, columnCount() - 1));
        }
        if (currentIndex >= 0) {
            emit dataChanged(index(currentIndex, 0), index(currentIndex, columnCount() - 1));
        }
    }
}

bool FunctionModel::updateCurrentIndex()
{
    int index = -1;
    RVA offset = 0;

    RVA seek = Core()->getOffset();

    for (int i = 0; i < functions->count(); i++) {
        const FunctionDescription &function = functions->at(i);

        if (function.contains(seek) && function.offset >= offset) {
            offset = function.offset;
            index = i;
        }
    }

    bool changed = currentIndex != index;

    currentIndex = index;

    return changed;
}

void FunctionModel::functionRenamed(const RVA offset, const QString &new_name)
{
    for (int i = 0; i < functions->count(); i++) {
        FunctionDescription &function = (*functions)[i];
        if (function.offset == offset) {
            function.name = new_name;
            emit dataChanged(index(i, 0), index(i, columnCount() - 1));
        }
    }
}

FunctionSortFilterProxyModel::FunctionSortFilterProxyModel(FunctionModel *source_model,
                                                           QObject *parent)
    : AddressableFilterProxyModel(source_model, parent)
{
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

bool FunctionSortFilterProxyModel::filterAcceptsRow(int row, const QModelIndex &parent) const
{
    QModelIndex index = sourceModel()->index(row, 0, parent);
    FunctionDescription function =
            index.data(FunctionModel::FunctionDescriptionRole).value<FunctionDescription>();

    return qhelpers::filterStringContains(function.name, this);
}

bool FunctionSortFilterProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    if (!left.isValid() || !right.isValid())
        return false;

    if (left.parent().isValid() || right.parent().isValid())
        return false;

    FunctionDescription left_function =
            left.data(FunctionModel::FunctionDescriptionRole).value<FunctionDescription>();
    FunctionDescription right_function =
            right.data(FunctionModel::FunctionDescriptionRole).value<FunctionDescription>();

    if (static_cast<FunctionModel *>(sourceModel())->isNested()) {
        return left_function.name < right_function.name;
    } else {
        switch (left.column()) {
        case FunctionModel::OffsetColumn:
            return left_function.offset < right_function.offset;
        case FunctionModel::SizeColumn:
            if (left_function.linearSize != right_function.linearSize)
                return left_function.linearSize < right_function.linearSize;
            break;
        case FunctionModel::ImportColumn: {
            bool left_is_import = left.data(FunctionModel::IsImportRole).toBool();
            bool right_is_import = right.data(FunctionModel::IsImportRole).toBool();
            if (!left_is_import && right_is_import)
                return true;
            break;
        }
        case FunctionModel::NameColumn:
            return left_function.name < right_function.name;
        case FunctionModel::NargsColumn:
            if (left_function.nargs != right_function.nargs)
                return left_function.nargs < right_function.nargs;
            break;
        case FunctionModel::NlocalsColumn:
            if (left_function.nlocals != right_function.nlocals)
                return left_function.nlocals < right_function.nlocals;
            break;
        case FunctionModel::NbbsColumn:
            if (left_function.nbbs != right_function.nbbs)
                return left_function.nbbs < right_function.nbbs;
            break;
        case FunctionModel::CalltypeColumn:
            return left_function.calltype < right_function.calltype;
        case FunctionModel::EdgesColumn:
            if (left_function.edges != right_function.edges)
                return left_function.edges < right_function.edges;
            break;
        case FunctionModel::FrameColumn:
            if (left_function.stackframe != right_function.stackframe)
                return left_function.stackframe < right_function.stackframe;
            break;
        case FunctionModel::CommentColumn:
            return Core()->getCommentAt(left_function.offset)
                    < Core()->getCommentAt(right_function.offset);
        default:
            return false;
        }

        return left_function.offset < right_function.offset;
    }
}

FunctionsWidget::FunctionsWidget(MainWindow *main)
    : ListDockWidget(main),
      actionRename(tr("Rename"), this),
      actionUndefine(tr("Undefine"), this),
      actionHorizontal(tr("Horizontal"), this),
      actionVertical(tr("Vertical"), this)
{
    setWindowTitle(tr("Functions"));
    setObjectName("FunctionsWidget");

    setTooltipStylesheet();
    connect(Config(), &Configuration::colorsUpdated, this, &FunctionsWidget::setTooltipStylesheet);

    QFontInfo font_info = ui->treeView->fontInfo();
    QFont default_font = QFont(font_info.family(), font_info.pointSize());
    QFont highlight_font = QFont(font_info.family(), font_info.pointSize(), QFont::Bold);

    functionModel = new FunctionModel(&functions, &importAddresses, &mainAdress, false,
                                      default_font, highlight_font, this);
    functionProxyModel = new FunctionSortFilterProxyModel(functionModel, this);
    setModels(functionProxyModel);
    ui->treeView->sortByColumn(FunctionModel::NameColumn, Qt::AscendingOrder);

    titleContextMenu = new QMenu(this);
    auto viewTypeGroup = new QActionGroup(titleContextMenu);
    actionHorizontal.setCheckable(true);
    actionHorizontal.setActionGroup(viewTypeGroup);
    connect(&actionHorizontal, &QAction::toggled, this,
            &FunctionsWidget::onActionHorizontalToggled);
    actionVertical.setCheckable(true);
    actionVertical.setActionGroup(viewTypeGroup);
    connect(&actionVertical, &QAction::toggled, this, &FunctionsWidget::onActionVerticalToggled);
    titleContextMenu->addActions(viewTypeGroup->actions());

    actionRename.setShortcut({ Qt::Key_N });
    actionRename.setShortcutContext(Qt::ShortcutContext::WidgetWithChildrenShortcut);
    connect(&actionRename, &QAction::triggered, this,
            &FunctionsWidget::onActionFunctionsRenameTriggered);
    connect(&actionUndefine, &QAction::triggered, this,
            &FunctionsWidget::onActionFunctionsUndefineTriggered);

    auto itemConextMenu = ui->treeView->getItemContextMenu();
    itemConextMenu->addSeparator();
    itemConextMenu->addAction(&actionRename);
    itemConextMenu->addAction(&actionUndefine);
    itemConextMenu->setWholeFunction(true);

    addActions(itemConextMenu->actions());

    // Use a custom context menu on the dock title bar
    actionHorizontal.setChecked(true);
    this->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this,
            &FunctionsWidget::showTitleContextMenu);

    connect(Core(), &CutterCore::functionsChanged, this, &FunctionsWidget::refreshTree);
    connect(Core(), &CutterCore::codeRebased, this, &FunctionsWidget::refreshTree);
    connect(Core(), &CutterCore::refreshAll, this, &FunctionsWidget::refreshTree);
    connect(Core(), &CutterCore::commentsChanged, this,
            [this]() { qhelpers::emitColumnChanged(functionModel, FunctionModel::CommentColumn); });
}

FunctionsWidget::~FunctionsWidget() {}

void FunctionsWidget::refreshTree()
{
    if (task) {
        task->wait();
    }

    task = QSharedPointer<FunctionsTask>(new FunctionsTask());
    connect(task.data(), &FunctionsTask::fetchFinished, this,
            [this](const QList<FunctionDescription> &functions) {
                functionModel->beginResetModel();

                this->functions = functions;

                importAddresses.clear();
                for (const ImportDescription &import : Core()->getAllImports()) {
                    importAddresses.insert(import.plt);
                }

                mainAdress = (ut64)Core()->cmdj("iMj").object()["vaddr"].toInt();

                functionModel->updateCurrentIndex();
                functionModel->endResetModel();

                // resize offset and size columns
                qhelpers::adjustColumns(ui->treeView, 3, 0);
            });
    Core()->getAsyncTaskManager()->start(task);
}

void FunctionsWidget::changeSizePolicy(QSizePolicy::Policy hor, QSizePolicy::Policy ver)
{
    ui->dockWidgetContents->setSizePolicy(hor, ver);
}

void FunctionsWidget::onActionFunctionsRenameTriggered()
{
    // Get selected item in functions tree view
    FunctionDescription function = ui->treeView->selectionModel()
                                           ->currentIndex()
                                           .data(FunctionModel::FunctionDescriptionRole)
                                           .value<FunctionDescription>();

    bool ok;
    // Create dialog
    QString newName =
            QInputDialog::getText(this, tr("Rename function %1").arg(function.name),
                                  tr("Function name:"), QLineEdit::Normal, function.name, &ok);
    // If user accepted
    if (ok && !newName.isEmpty()) {
        // Rename function in rizin core
        Core()->renameFunction(function.offset, newName);

        // Seek to new renamed function
        Core()->seekAndShow(function.offset);
    }
}

void FunctionsWidget::onActionFunctionsUndefineTriggered()
{
    const auto selection = ui->treeView->selectionModel()->selection().indexes();
    std::vector<RVA> offsets;
    offsets.reserve(selection.size());
    for (const auto &index : selection) {
        offsets.push_back(functionProxyModel->address(index));
    }
    for (RVA offset : offsets) {
        Core()->delFunction(offset);
    }
}

void FunctionsWidget::showTitleContextMenu(const QPoint &pt)
{
    titleContextMenu->exec(this->mapToGlobal(pt));
}

void FunctionsWidget::onActionHorizontalToggled(bool enable)
{
    if (enable) {
        functionModel->setNested(false);
        ui->treeView->setIndentation(8);
    }
}

void FunctionsWidget::onActionVerticalToggled(bool enable)
{
    if (enable) {
        functionModel->setNested(true);
        ui->treeView->setIndentation(20);
    }
}

/**
 * @brief a SLOT to set the stylesheet for a tooltip
 */
void FunctionsWidget::setTooltipStylesheet()
{
    setStyleSheet(QString("QToolTip { border-width: 1px; max-width: %1px;"
                          "opacity: 230; background-color: %2;"
                          "color: %3; border-color: %3;}")
                          .arg(kMaxTooltipWidth)
                          .arg(Config()->getColor("gui.tooltip.background").name())
                          .arg(Config()->getColor("gui.tooltip.foreground").name()));
}
