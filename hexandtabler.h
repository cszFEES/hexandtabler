#ifndef HEXANDTABLER_H
#define HEXANDTABLER_H

#include <QMainWindow>
#include <QByteArray>
#include <QList>
#include <QAction>

// Forward declarations
class HexEditorArea;
class QTableWidget;
class QDockWidget;

namespace Ui {
class hexandtabler;
}

class hexandtabler : public QMainWindow
{
    Q_OBJECT

public:
    explicit hexandtabler(QWidget *parent = nullptr);
    ~hexandtabler();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void on_actionOpen_triggered();
    void on_actionSave_triggered();
    void on_actionExit_triggered();
    void on_actionAbout_triggered();
    
    void on_actionDarkMode_triggered(bool checked);
    void on_actionToggleTable_triggered();
    
    void on_actionUndo_triggered();
    void on_actionRedo_triggered();
    
    void on_actionZoomIn_triggered();
    void on_actionZoomOut_triggered();
    
    void handleDataEdited();
    void openRecentFile();

private:
    Ui::hexandtabler *ui;
    HexEditorArea *m_hexEditorArea = nullptr;
    QTableWidget *m_conversionTable = nullptr;
    QDockWidget *m_tableDock = nullptr;

    QByteArray m_fileData;
    QString m_currentFilePath;
    bool m_isModified = false;
    
    // Undo/Redo
    QList<QByteArray> m_undoStack;
    QList<QByteArray> m_redoStack;
    QAction *undoAct = nullptr;
    QAction *redoAct = nullptr;

    // Recent Files
    enum { MaxRecentFiles = 5 };
    QAction *recentFileActions[MaxRecentFiles];


    void setupConversionTable();
    void createMenus(); // Para conectar las acciones de Zoom y About
    
    void loadFile(const QString &filePath);
    bool saveCurrentFile();
    bool maybeSave();
    
    void refreshModelFromArea(); 
    void pushUndoState();
    void updateUndoRedoActions();
    
    // Recent Files Helpers
    void createRecentFileActions();
    void loadRecentFiles();
    void updateRecentFileActions();
    void prependToRecentFiles(const QString &filePath);
};

#endif // HEXANDTABLER_H