#include "StringsWidget.h"
#include "ui_StringsWidget.h"
#include "core/MainWindow.h"
#include "common/Helpers.h"
#include "WidgetShortcuts.h"

#include <QClipboard>
#include <QMenu>
#include <QModelIndex>
#include <QShortcut>

StringsModel::StringsModel(QList<StringDescription> *strings, QObject *parent)
    : AddressableItemModel<QAbstractListModel>(parent), strings(strings)
{
}

int StringsModel::rowCount(const QModelIndex &) const
{
    return strings->count();
}

int StringsModel::columnCount(const QModelIndex &) const
{
    return StringsModel::ColumnCount;
}

QVariant StringsModel::data(const QModelIndex &index, int role) const
{
    if (index.row() >= strings->count())
        return QVariant();

    const StringDescription &str = strings->at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case StringsModel::OffsetColumn:
            return RAddressString(str.vaddr);
        case StringsModel::StringColumn:
            return str.string;
        case StringsModel::TypeColumn:
            return str.type.toUpper();
        case StringsModel::LengthColumn:
            return QString::number(str.length);
        case StringsModel::SizeColumn:
            return QString::number(str.size);
        case StringsModel::SectionColumn:
            return str.section;
        case StringsModel::CommentColumn:
            return Core()->getCommentAt(str.vaddr);
        default:
            return QVariant();
        }
    case StringDescriptionRole:
        return QVariant::fromValue(str);
    default:
        return QVariant();
    }
}

QVariant StringsModel::headerData(int section, Qt::Orientation, int role) const
{
    switch (role) {
    case Qt::DisplayRole:
        switch (section) {
        case StringsModel::OffsetColumn:
            return tr("Address");
        case StringsModel::StringColumn:
            return tr("String");
        case StringsModel::TypeColumn:
            return tr("Type");
        case StringsModel::LengthColumn:
            return tr("Length");
        case StringsModel::SizeColumn:
            return tr("Size");
        case StringsModel::SectionColumn:
            return tr("Section");
        case StringsModel::CommentColumn:
            return tr("Comment");
        default:
            return QVariant();
        }
    default:
        return QVariant();
    }
}

RVA StringsModel::address(const QModelIndex &index) const
{
    const StringDescription &str = strings->at(index.row());
    return str.vaddr;
}

const StringDescription *StringsModel::description(const QModelIndex &index) const
{
    return &strings->at(index.row());
}

StringsProxyModel::StringsProxyModel(StringsModel *sourceModel, QObject *parent)
    : AddressableFilterProxyModel(sourceModel, parent)
{
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

void StringsProxyModel::setSelectedSection(QString section)
{
    selectedSection = section;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    invalidateFilter();
#else
    invalidateRowsFilter();
#endif
}

bool StringsProxyModel::filterAcceptsRow(int row, const QModelIndex &parent) const
{
    QModelIndex index = sourceModel()->index(row, 0, parent);
    StringDescription str =
            index.data(StringsModel::StringDescriptionRole).value<StringDescription>();
    if (selectedSection.isEmpty()) {
        return qhelpers::filterStringContains(str.string, this);
    } else {
        return selectedSection == str.section && qhelpers::filterStringContains(str.string, this);
    }
}

bool StringsProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    auto model = static_cast<StringsModel *>(sourceModel());
    auto leftStr = model->description(left);
    auto rightStr = model->description(right);

    switch (left.column()) {
    case StringsModel::OffsetColumn:
        return leftStr->vaddr < rightStr->vaddr;
    case StringsModel::StringColumn: // sort by string
        return leftStr->string < rightStr->string;
    case StringsModel::TypeColumn: // sort by type
        return leftStr->type < rightStr->type;
    case StringsModel::SizeColumn: // sort by size
        return leftStr->size < rightStr->size;
    case StringsModel::LengthColumn: // sort by length
        return leftStr->length < rightStr->length;
    case StringsModel::SectionColumn:
        return leftStr->section < rightStr->section;
    case StringsModel::CommentColumn:
        return Core()->getCommentAt(leftStr->vaddr) < Core()->getCommentAt(rightStr->vaddr);
    default:
        break;
    }

    // fallback
    return leftStr->vaddr < rightStr->vaddr;
}

StringsWidget::StringsWidget(MainWindow *main)
    : CutterDockWidget(main), ui(new Ui::StringsWidget), tree(new CutterTreeWidget(this))
{
    ui->setupUi(this);
    ui->quickFilterView->setLabelText(tr("Section:"));

    // Add Status Bar footer
    tree->addStatusBar(ui->verticalLayout);

    qhelpers::setVerticalScrollMode(ui->stringsTreeView);

    // Shift-F12 to toggle strings window
    QShortcut *toggle_shortcut = new QShortcut(widgetShortcuts["StringsWidget"], main);
    connect(toggle_shortcut, &QShortcut::activated, this, [=]() { toggleDockWidget(true); });

    connect(ui->actionCopy_String, &QAction::triggered, this, &StringsWidget::on_actionCopy);

    ui->actionFilter->setShortcut(QKeySequence::Find);

    ui->stringsTreeView->setContextMenuPolicy(Qt::CustomContextMenu);

    model = new StringsModel(&strings, this);
    proxyModel = new StringsProxyModel(model, this);
    ui->stringsTreeView->setMainWindow(main);
    ui->stringsTreeView->setModel(proxyModel);
    ui->stringsTreeView->sortByColumn(-1, Qt::AscendingOrder);

    //
    auto menu = ui->stringsTreeView->getItemContextMenu();
    menu->addAction(ui->actionCopy_String);

    connect(ui->quickFilterView, &ComboQuickFilterView::filterTextChanged, proxyModel,
            &QSortFilterProxyModel::setFilterWildcard);

    connect(ui->quickFilterView, &ComboQuickFilterView::filterTextChanged, this,
            [this] { tree->showItemsNumber(proxyModel->rowCount()); });

    QShortcut *searchShortcut = new QShortcut(QKeySequence::Find, this);
    connect(searchShortcut, &QShortcut::activated, ui->quickFilterView,
            &ComboQuickFilterView::showFilter);
    searchShortcut->setContext(Qt::WidgetWithChildrenShortcut);

    QShortcut *clearShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(clearShortcut, &QShortcut::activated, this, [this]() {
        ui->quickFilterView->clearFilter();
        ui->stringsTreeView->setFocus();
    });
    clearShortcut->setContext(Qt::WidgetWithChildrenShortcut);

    connect(Core(), &CutterCore::refreshAll, this, &StringsWidget::refreshStrings);
    connect(Core(), &CutterCore::codeRebased, this, &StringsWidget::refreshStrings);
    connect(Core(), &CutterCore::commentsChanged, this,
            [this]() { qhelpers::emitColumnChanged(model, StringsModel::CommentColumn); });

    connect(ui->quickFilterView->comboBox(), &QComboBox::currentTextChanged, this, [this]() {
        proxyModel->setSelectedSection(ui->quickFilterView->comboBox()->currentData().toString());
        tree->showItemsNumber(proxyModel->rowCount());
    });

    auto header = ui->stringsTreeView->header();
    header->setSectionResizeMode(QHeaderView::ResizeMode::Interactive);
    header->resizeSections(QHeaderView::ResizeMode::Stretch);
}

StringsWidget::~StringsWidget() {}

void StringsWidget::refreshStrings()
{
    if (task) {
        task->wait();
    }

    task = QSharedPointer<StringsTask>(new StringsTask());
    connect(task.data(), &StringsTask::stringSearchFinished, this,
            &StringsWidget::stringSearchFinished);
    Core()->getAsyncTaskManager()->start(task);

    refreshSectionCombo();
}

void StringsWidget::refreshSectionCombo()
{
    QComboBox *combo = ui->quickFilterView->comboBox();

    combo->clear();
    combo->addItem(tr("(all)"));

    for (const QString &section : Core()->getSectionList()) {
        combo->addItem(section, section);
    }

    proxyModel->setSelectedSection(QString());
}

void StringsWidget::stringSearchFinished(const QList<StringDescription> &strings)
{
    model->beginResetModel();
    this->strings = strings;
    model->endResetModel();

    tree->showItemsNumber(proxyModel->rowCount());

    task.clear();
}

void StringsWidget::on_actionCopy()
{
    QModelIndex current_item = ui->stringsTreeView->currentIndex();
    int row = current_item.row();

    QModelIndex index;

    index = ui->stringsTreeView->model()->index(row, 1);

    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(index.data().toString());
}
