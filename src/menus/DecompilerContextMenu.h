#ifndef DECOMPILERCONTEXTMENU_H
#define DECOMPILERCONTEXTMENU_H

#include "core/Cutter.h"
#include <QMenu>
#include <QKeySequence>

#include <r_util/r_annotated_code.h>

class MainWindow;

class DecompilerContextMenu : public QMenu
{
    Q_OBJECT

public:
    DecompilerContextMenu(QWidget *parent, MainWindow *mainWindow);
    ~DecompilerContextMenu();

    bool getIsTogglingBreakpoints();
    void setAnnotationHere(RCodeAnnotation *annotation);
    RCodeAnnotation *getAnnotationHere();
    RVA getFirstOffsetInLine();

signals:
    void copy();

public slots:
    void setCurHighlightedWord(QString word);
    void setOffset(RVA newOffset);
    void setDecompiledFunctionAddress(RVA functionAddr);
    void setFirstOffsetInLine(RVA firstOffset);
    void setAvailableBreakpoints(QVector<RVA> offsetList);

private slots:
    void aboutToShowSlot();
    void aboutToHideSlot();

    void actionCopyTriggered();
    void actionCopyInstructionAddressTriggered();
    void actionCopyReferenceAddressTriggered();

    void actionAddCommentTriggered();
    void actionDeleteCommentTriggered();

    void actionRenameThingHereTriggered();
    void actionDeleteNameTriggered();

    void actionEditFunctionVariablesTriggered();

    void actionXRefsTriggered();

    void actionToggleBreakpointTriggered();
    void actionAdvancedBreakpointTriggered();

    void actionContinueUntilTriggered();
    void actionSetPCTriggered();

private:
    // Private variables
    MainWindow *mainWindow;
    QString curHighlightedWord;
    RVA offset;
    RVA decompiledFunctionAddress;
    /**
     * Lowest offset among all offsets present in the line under cursor in the decompiler widget.
     */
    RVA firstOffsetInLine;
    /**
     * When the actionToggleBreakpoint has been triggered, and it hasn't finished executing,
     * the value of this variable will be true, otherwise false
     */
    bool isTogglingBreakpoints;
    /**
     * List of the offsets of all the breakpoints (enabled and disabled) that are present in the line under cursor.
     */
    QVector<RVA> availableBreakpoints;
    /**
     * Context-related annotation for the data under cursor in the decompiler widget.
     * If such an annotation doesn't exist, its value is nullptr.
     */
    RCodeAnnotation *annotationHere;

    // Actions and menus in the context menu
    QAction actionCopy;
    QAction actionCopyInstructionAddress;
    QAction actionCopyReferenceAddress;
    QAction *copySeparator;

    QAction actionShowInSubmenu;
    QList<QAction *> showTargetMenuActions;

    QAction actionAddComment;
    QAction actionDeleteComment;

    QAction actionRenameThingHere;
    QAction actionDeleteName;

    QAction actionEditFunctionVariables;

    QAction actionXRefs;

    QMenu *breakpointMenu;
    QAction actionToggleBreakpoint;
    QAction actionAdvancedBreakpoint;

    QMenu *breakpointsInLineMenu;

    QMenu *debugMenu;
    QAction actionContinueUntil;
    QAction actionSetPC;

    // Private Functions
    /**
     * @brief Sets the shortcut context in all the actions contained
     * in the specified QMenu to Qt::WidgetWithChildrenShortcut.
     *
     * @param menu - QMenu specified
     */
    void setShortcutContextInActions(QMenu *menu);
    void setupBreakpointsInLineMenu();
    void setIsTogglingBreakpoints(bool isToggling);

    // Set actions
    void setActionCopy();

    void setActionShowInSubmenu();

    void setActionAddComment();
    void setActionDeleteComment();

    void setActionXRefs();

    void setActionRenameThingHere();
    void setActionDeleteName();

    void setActionEditFunctionVariables();

    void setActionToggleBreakpoint();
    void setActionAdvancedBreakpoint();

    void setActionContinueUntil();
    void setActionSetPC();

    // Add Menus
    void addBreakpointMenu();
    void addDebugMenu();

    /**
     * @brief Updates targeted "Show in" menu.
     *
     * Removes all actions from the existing targeted "show in" menu. If annotationHere
     * represents an item that has an address assigned to it, insert actions compatible with the
     * type of this item in the targeted "Show in" menu.
     */
    void updateTargetMenuActions();

    /**
     * @brief Check if annotationHere is a reference (function name,
     * global variable, constant variable with an address).
     *
     * @return True if annotationHere is a reference, otherwise false.
     */
    bool isReference();
    /**
     * @brief Check if annotationHere is a function variable
     * (local variable or function parameter).
     *
     * @return True if annotationHere is a function variable, otherwise false.
     */
    bool isFunctionVariable();
    /**
     * @brief Check if the function variable annotated by annotationHere is
     * present in radare2.
     *
     * @return True if the variable is present, otherwise false
     */
    bool variablePresentInR2();
};

#endif // DECOMPILERCONTEXTMENU_H