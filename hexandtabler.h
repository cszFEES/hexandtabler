#ifndef HEXANDTABLER_H
#define HEXANDTABLER_H

#include <QMainWindow>
#include <QByteArray>
#include <QList>
#include <QAction>
#include <QString> 
#include <QTableWidgetItem> 
#include <QCloseEvent>
#include <QModelIndexList> 

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
    void on_actionSaveAs_triggered(); // Restaurado
    void on_actionExit_triggered();
    void on_actionAbout_triggered();
    
    void on_actionDarkMode_triggered(bool checked); 
    
    void on_actionUndo_triggered();
    void on_actionRedo_triggered();
    
    void on_actionZoomIn_triggered();
    void on_actionZoomOut_triggered();
    
    void on_actionGoTo_triggered(); 
    
    // --- TABLE SLOTS ---
    void on_actionToggleTable_triggered(); 
    void on_actionLoadTable_triggered();
    void on_actionSaveTable_triggered();
    void on_actionSaveTableAs_triggered(); // Restaurado

    // --- SERIES INSERTION SLOTS ---
    void on_actionInsertLatinUpper_triggered(); // Restaurado
    void on_actionInsertLatinLower_triggered(); // Restaurado
    void on_actionInsertHiragana_triggered(); // Restaurado
    void on_actionInsertKatakana_triggered(); // Restaurado

    void handleDataEdited();
    void handleTableItemChanged(QTableWidgetItem *item);
    void openRecentFile();

private:
    Ui::hexandtabler *ui;
    HexEditorArea *m_hexEditorArea = nullptr;
    QTableWidget *m_conversionTable = nullptr;
    QDockWidget *m_tableDock = nullptr;

    QByteArray m_fileData;
    QString m_currentFilePath;
    QString m_currentTablePath; // Restaurado
    bool m_isModified = false;
    
    // Undo/Redo
    QList<QByteArray> m_undoStack;
    QList<QByteArray> m_redoStack;
    QAction *undoAct = nullptr;
    QAction *redoAct = nullptr;

    // Table Mapping
    QString m_charMap[256]; 

    // Recent Files
    enum { MaxRecentFiles = 5 };
    QAction *recentFileActions[MaxRecentFiles];


    void setupConversionTable();
    void createMenus(); 
    
    void loadFile(const QString &filePath);
    bool saveDataToFile(const QString &filePath); // Restaurado
    bool saveFileAs();                            // Restaurado
    bool saveCurrentFile();
    bool maybeSave();
    
    void refreshModelFromArea(); 
    void pushUndoState();
    void updateUndoRedoActions();
    
    // --- TABLE HELPERS ---
    bool saveTableFile(const QString &filePath);
    bool loadTableFile(const QString &filePath);
    void insertSeries(const QList<QString> &series); // Restaurado
    
    // Recent Files Helpers
    void createRecentFileActions();
    void loadRecentFiles();
    void updateRecentFileActions();
    void prependToRecentFiles(const QString &filePath);
};

#endif // HEXANDTABLER_H