#include "core/MainWindow.h"
#include "GraphWidget.h"
#include "DisassemblerGraphView.h"
#include "WidgetShortcuts.h"

GraphWidget::GraphWidget(MainWindow *main, QAction *action) :
    MemoryDockWidget(CutterCore::MemoryWidgetType::Graph, main, action)
{
    setObjectName("GraphWidget 0");
    setAllowedAreas(Qt::AllDockWidgetAreas);
    graphView = new DisassemblerGraphView(this, seekable);
    setWidget(graphView);

    // getting the name of the class is implementation defined, and cannot be
    // used reliably across different compilers.
    //QShortcut *toggle_shortcut = new QShortcut(widgetShortcuts[typeid(this).name()], main);
    QShortcut *toggle_shortcut = new QShortcut(widgetShortcuts["GraphWidget"], main);
    connect(toggle_shortcut, &QShortcut::activated, this, [ = ]() {
            toggleDockWidget(true); 
            main->updateDockActionChecked(action);
    });

    connect(this, &QDockWidget::visibilityChanged, this, [ = ](bool visibility) {
        main->toggleOverview(visibility, this);
        if (visibility) {
            Core()->setMemoryWidgetPriority(CutterCore::MemoryWidgetType::Graph);
            graphView->onSeekChanged(Core()->getOffset());
        }
    });

    connect(graphView, &DisassemblerGraphView::graphMoved, this, [ = ]() {
        main->toggleOverview(true, this);
    });
}

QWidget *GraphWidget::widgetToFocusOnRaise()
{
    return graphView;
}

void GraphWidget::closeEvent(QCloseEvent *event)
{
    CutterDockWidget::closeEvent(event);
    emit graphClosed();
}

DisassemblerGraphView *GraphWidget::getGraphView() const
{
    return graphView;
}
