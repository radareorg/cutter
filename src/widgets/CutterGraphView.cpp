#include "CutterGraphView.h"

#include "core/Cutter.h"
#include "common/Configuration.h"
#include "dialogs/MultitypeFileSaveDialog.h"
#include "TempConfig.h"

#include <cmath>

#include <QStandardPaths>

static const int KEY_ZOOM_IN = Qt::Key_Plus + Qt::ControlModifier;
static const int KEY_ZOOM_OUT = Qt::Key_Minus + Qt::ControlModifier;
static const int KEY_ZOOM_RESET = Qt::Key_Equal + Qt::ControlModifier;

CutterGraphView::CutterGraphView(QWidget *parent)
    : GraphView(parent)
    , mFontMetrics(nullptr)
{
    connect(Core(), &CutterCore::graphOptionsChanged, this, &CutterGraphView::refreshView);
    connect(Config(), &Configuration::colorsUpdated, this, &CutterGraphView::colorsUpdatedSlot);
    connect(Config(), &Configuration::fontsUpdated, this, &CutterGraphView::fontsUpdatedSlot);

    initFont();
    updateColors();
}

QPoint CutterGraphView::getTextOffset(int line) const
{
    int padding = static_cast<int>(2 * charWidth);
    return QPoint(padding, padding + line * charHeight);
}

void CutterGraphView::initFont()
{
    setFont(Config()->getFont());
    QFontMetricsF metrics(font());
    baseline = int(metrics.ascent());
    charWidth = metrics.width('X');
    charHeight = static_cast<int>(metrics.height());
    charOffset = 0;
    mFontMetrics.reset(new CachedFontMetrics<qreal>(font()));
}

void CutterGraphView::zoom(QPointF mouseRelativePos, double velocity)
{
    qreal newScale = getViewScale() * std::pow(1.25, velocity);
    setZoom(mouseRelativePos, newScale);
}

void CutterGraphView::setZoom(QPointF mouseRelativePos, double scale)
{
    mouseRelativePos.rx() *= size().width();
    mouseRelativePos.ry() *= size().height();
    mouseRelativePos /= getViewScale();

    auto globalMouse = mouseRelativePos + getViewOffset();
    mouseRelativePos *= getViewScale();
    qreal newScale = scale;
    newScale = std::max(newScale, 0.05);
    mouseRelativePos /= newScale;
    setViewScale(newScale);

    // Adjusting offset, so that zooming will be approaching to the cursor.
    setViewOffset(globalMouse.toPoint() - mouseRelativePos.toPoint());

    viewport()->update();
    emit viewZoomed();
}

void CutterGraphView::zoomIn()
{
    zoom(QPointF(0.5, 0.5), 1);
}

void CutterGraphView::zoomOut()
{
    zoom(QPointF(0.5, 0.5), -1);
}

void CutterGraphView::zoomReset()
{
    setZoom(QPointF(0.5, 0.5), 1);
}

void CutterGraphView::updateColors()
{
    disassemblyBackgroundColor = ConfigColor("gui.alt_background");
    disassemblySelectedBackgroundColor = ConfigColor("gui.disass_selected");
    mDisabledBreakpointColor = disassemblyBackgroundColor;
    graphNodeColor = ConfigColor("gui.border");
    backgroundColor = ConfigColor("gui.background");
    disassemblySelectionColor = ConfigColor("lineHighlight");
    PCSelectionColor = ConfigColor("highlightPC");

    jmpColor = ConfigColor("graph.trufae");
    brtrueColor = ConfigColor("graph.true");
    brfalseColor = ConfigColor("graph.false");

    mCommentColor = ConfigColor("comment");
}

void CutterGraphView::colorsUpdatedSlot()
{
    updateColors();
    refreshView();
}

GraphLayout::LayoutConfig CutterGraphView::getLayoutConfig()
{
    auto blockSpacing = Config()->getGraphBlockSpacing();
    auto edgeSpacing = Config()->getGraphEdgeSpacing();
    GraphLayout::LayoutConfig layoutConfig;
    layoutConfig.blockHorizontalSpacing = blockSpacing.x();
    layoutConfig.blockVerticalSpacing = blockSpacing.y();
    layoutConfig.edgeHorizontalSpacing = edgeSpacing.x();
    layoutConfig.edgeVerticalSpacing = edgeSpacing.y();
    return layoutConfig;
}

void CutterGraphView::fontsUpdatedSlot()
{
    initFont();
    refreshView();
}

bool CutterGraphView::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::ShortcutOverride: {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        int key = keyEvent->key() + keyEvent->modifiers();
        if (key == KEY_ZOOM_OUT || key == KEY_ZOOM_RESET
                || key == KEY_ZOOM_IN || (key == (KEY_ZOOM_IN | Qt::ShiftModifier))) {
            event->accept();
            return true;
        }
        break;
    }
    case QEvent::KeyPress: {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        int key = keyEvent->key() + keyEvent->modifiers();
        if (key == KEY_ZOOM_IN || (key == (KEY_ZOOM_IN | Qt::ShiftModifier))) {
            zoomIn();
            return true;
        } else if (key == KEY_ZOOM_OUT) {
            zoomOut();
            return true;
        } else if (key == KEY_ZOOM_RESET) {
            zoomReset();
            return true;
        }
        break;
    }
    default:
        break;
    }
    return GraphView::event(event);
}

void CutterGraphView::refreshView()
{
    initFont();
    setLayoutConfig(getLayoutConfig());
}


void CutterGraphView::wheelEvent(QWheelEvent *event)
{
    // when CTRL is pressed, we zoom in/out with mouse wheel
    if (Qt::ControlModifier == event->modifiers()) {
        const QPoint numDegrees = event->angleDelta() / 8;
        if (!numDegrees.isNull()) {
            int numSteps = numDegrees.y() / 15;

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
            QPointF relativeMousePos = event->pos();
#else
            QPointF relativeMousePos = event->position();
#endif
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

void CutterGraphView::resizeEvent(QResizeEvent *event)
{
    GraphView::resizeEvent(event);
    emit resized();
}


void CutterGraphView::mousePressEvent(QMouseEvent *event)
{
    GraphView::mousePressEvent(event);
    emit graphMoved();
}

void CutterGraphView::mouseMoveEvent(QMouseEvent *event)
{
    GraphView::mouseMoveEvent(event);
    emit graphMoved();
}

void CutterGraphView::exportGraph(QString filePath, GraphExportType type, QString graphCommand, RVA address)
{
    bool graphTransparent = Config()->getBitmapTransparentState();
    double graphScaleFactor = Config()->getBitmapExportScaleFactor();
    switch (type) {
    case GraphExportType::Png:
        this->saveAsBitmap(filePath, "png", graphScaleFactor, graphTransparent);
        break;
    case GraphExportType::Jpeg:
        this->saveAsBitmap(filePath, "jpg", graphScaleFactor, false);
        break;
    case GraphExportType::Svg:
        this->saveAsSvg(filePath);
        break;

    case GraphExportType::GVDot:
        exportR2TextGraph(filePath, graphCommand + "d", address);
        break;
    case GraphExportType::R2Json:
        exportR2TextGraph(filePath, graphCommand + "j", address);
        break;
    case GraphExportType::R2Gml:
        exportR2TextGraph(filePath, graphCommand + "g", address);
        break;
    case GraphExportType::R2SDBKeyValue:
        exportR2TextGraph(filePath, graphCommand + "k", address);
        break;

    case GraphExportType::GVJson:
        exportR2GraphvizGraph(filePath, "json", graphCommand, address);
        break;
    case GraphExportType::GVGif:
        exportR2GraphvizGraph(filePath, "gif", graphCommand, address);
        break;
    case GraphExportType::GVPng:
        exportR2GraphvizGraph(filePath, "png", graphCommand, address);
        break;
    case GraphExportType::GVJpeg:
        exportR2GraphvizGraph(filePath, "jpg", graphCommand, address);
        break;
    case GraphExportType::GVPostScript:
        exportR2GraphvizGraph(filePath, "ps", graphCommand, address);
        break;
    case GraphExportType::GVSvg:
        exportR2GraphvizGraph(filePath, "svg", graphCommand, address);
        break;
    }
}

void CutterGraphView::exportR2GraphvizGraph(QString filePath, QString type, QString graphCommand, RVA address)
{
    TempConfig tempConfig;
    tempConfig.set("graph.gv.format", type);
    qWarning() << Core()->cmdRawAt(QString("%0w \"%1\"").arg(graphCommand).arg(filePath),  address);
}

void CutterGraphView::exportR2TextGraph(QString filePath, QString graphCommand, RVA address)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Can't open file";
        return;
    }
    QTextStream fileOut(&file);
    fileOut << Core()->cmdRaw(QString("%0 0x%1").arg(graphCommand).arg(address, 0, 16));
}


Q_DECLARE_METATYPE(CutterGraphView::GraphExportType);

void CutterGraphView::showExportGraphDialog(QString defaultName, QString graphCommand, RVA address)
{
    QVector<MultitypeFileSaveDialog::TypeDescription> types = {
        {tr("PNG (*.png)"), "png", QVariant::fromValue(GraphExportType::Png)},
        {tr("JPEG (*.jpg)"), "jpg", QVariant::fromValue(GraphExportType::Jpeg)},
        {tr("SVG (*.svg)"), "svg", QVariant::fromValue(GraphExportType::Svg)}
    };

    bool r2GraphExports = !defaultName.isEmpty();
    if (r2GraphExports) {
        types.append({
            {tr("Graphviz dot (*.dot)"), "dot", QVariant::fromValue(GraphExportType::GVDot)},
            {tr("Graph Modelling Language (*.gml)"), "gml", QVariant::fromValue(GraphExportType::R2Gml)},
            {tr("R2 JSON (*.json)"), "json", QVariant::fromValue(GraphExportType::R2Json)},
            {tr("SDB key-value (*.txt)"), "txt", QVariant::fromValue(GraphExportType::R2SDBKeyValue)},
        });
        bool hasGraphviz = !QStandardPaths::findExecutable("dot").isEmpty()
                           || !QStandardPaths::findExecutable("xdot").isEmpty();
        if (hasGraphviz) {
            types.append({
                {tr("Graphviz json (*.json)"), "json", QVariant::fromValue(GraphExportType::GVJson)},
                {tr("Graphviz gif (*.gif)"), "gif", QVariant::fromValue(GraphExportType::GVGif)},
                {tr("Graphviz png (*.png)"), "png", QVariant::fromValue(GraphExportType::GVPng)},
                {tr("Graphviz jpg (*.jpg)"), "jpg", QVariant::fromValue(GraphExportType::GVJpeg)},
                {tr("Graphviz PostScript (*.ps)"), "ps", QVariant::fromValue(GraphExportType::GVPostScript)},
                {tr("Graphviz svg (*.svg)"), "svg", QVariant::fromValue(GraphExportType::GVSvg)}
            });
        }
    }

    MultitypeFileSaveDialog dialog(this, tr("Export Graph"));
    dialog.setTypes(types);
    dialog.selectFile(defaultName);
    if (!dialog.exec()) {
        return;
    }

    auto selectedType = dialog.selectedType();
    if (!selectedType.data.canConvert<GraphExportType>()) {
        qWarning() << "Bad selected type, should not happen.";
        return;
    }
    QString filePath = dialog.selectedFiles().first();
    exportGraph(filePath, selectedType.data.value<GraphExportType>(), graphCommand, address);

}