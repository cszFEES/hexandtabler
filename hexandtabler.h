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
#include <QVector> 

// Forward declarations
class HexEditorArea;
class QTableWidget;
class QDockWidget;
class FindReplaceDialog; 
class QRadioButton; 

namespace Ui {
class hexandtabler;
}

class hexandtabler : public QMainWindow
{
    Q_OBJECT

public:
    explicit hexandtabler(QWidget *parent = nullptr);
    ~hexandtabler();
    
    // Public function required for find/replace
    QByteArray convertSearchString(const QString &input, int type) const; 

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void on_actionOpen_triggered();
    void on_actionSave_triggered();
    void on_actionSaveAs_triggered(); 
    void on_actionExit_triggered();
    void on_actionAbout_triggered();
    
    void on_actionDarkMode_triggered(bool checked); 
    
    void on_actionUndo_triggered();
    void on_actionRedo_triggered();
    
    void on_actionZoomIn_triggered();
    void on_actionZoomOut_triggered();
    
    void on_actionGoTo_triggered(); 
    
    // --- FIND/REPLACE SLOTS ---
    void on_actionFind_triggered();
    void on_actionReplace_triggered(); 
    void on_actionCopy_triggered();       // <-- AÑADIDO
    void on_actionPaste_triggered();      // <-- AÑADIDO
    
    
    // --- TABLE SLOTS (AÑADIDOS) ---
    void on_actionToggleTable_triggered();        // <-- AÑADIDO
    void on_actionLoadTable_triggered();          // <-- AÑADIDO
    void on_actionSaveTable_triggered();          // <-- AÑADIDO
    void on_actionSaveTableAs_triggered();        // <-- AÑADIDO
    void on_actionInsertLatinUpper_triggered();   // <-- AÑADIDO
    void on_actionInsertLatinLower_triggered();   // <-- AÑADIDO
    void on_actionInsertHiragana_triggered();     // <-- AÑADIDO
    void on_actionInsertKatakana_triggered();     // <-- AÑADIDO
    void on_actionInsertCyrillic_triggered();     // <-- AÑADIDO
    
    // --- DATA HANDLING SLOTS (AÑADIDOS) ---
    void handleDataEdited();                     // <-- AÑADIDO
    void handleTableItemChanged(QTableWidgetItem *item); // <-- AÑADIDO
    

private:
    Ui::hexandtabler *ui;
    HexEditorArea *m_hexEditorArea = nullptr;
    QTableWidget *m_tableWidget = nullptr;       // <-- CORRECTO: Usado en CPP como m_tableWidget
    QDockWidget *m_tableDock = nullptr;
    FindReplaceDialog *m_findReplaceDialog = nullptr; 
    
    QByteArray m_fileData;
    QString m_currentFilePath;
    QString m_currentTablePath; 
    bool m_isModified = false;
    
    // Undo/Redo
    QList<QByteArray> m_undoStack;
    QList<QByteArray> m_redoStack;

    // Table Mapping
    QString m_charMap[256]; 

    // Find/Replace Logic
    void findNext(const QByteArray &needle, bool caseSensitive, bool wrap, bool backwards);
    void replaceOne();
    void replaceAll(const QByteArray &needle, const QByteArray &replacement);
    
    void findNextRelative(const QString &searchText, bool wrap, bool backwards);
    QVector<qint8> calculateRelativeOffsets(const QString &input) const; 
    
    // Recent Files
    enum { MaxRecentFiles = 5 };
    QAction *recentFileActions[MaxRecentFiles];


    void setupConversionTable();
    
    void loadFile(const QString &filePath);
    void setCurrentFile(const QString &filePath); 
    bool saveDataToFile(const QString &filePath); 
    bool saveFileAs();                            
    bool saveCurrentFile();
    bool maybeSave();
    
    void refreshModelFromArea(); 
    void pushUndoState();
    void updateUndoRedoActions();
    
    // --- TABLE HELPERS ---
    bool saveTableFile(const QString &filePath); 
    bool loadTableFile(const QString &filePath);
    void insertSeries(const QList<QString> &series); 
    
    // Recent Files Helpers
    void createRecentFileActions();
    void loadRecentFiles();
    void updateRecentFileActions();
    void prependToRecentFiles(const QString &filePath); // <-- AÑADIDO
    
    void openRecentFile(); 

};

#endif // HEXANDTABLER_H