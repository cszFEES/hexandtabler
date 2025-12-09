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
#include <QMap>
#include <QFuture>
#include <QFutureWatcher>

class HexEditorArea;
class QTableWidget;
class QDockWidget;
class FindReplaceDialog; 
class QRadioButton; 

namespace Ui {
class hexandtabler;
}

// Estructura para manejar frases conocidas
struct KnownPhrase {
    QString text;
    int length = 0;
    QMap<QChar, QList<int>> pattern;
};

class hexandtabler : public QMainWindow
{
    Q_OBJECT

public:
    explicit hexandtabler(QWidget *parent = nullptr);
    ~hexandtabler();
    
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
    
    void on_actionFind_triggered();
    void on_actionReplace_triggered(); 
    void on_actionCopy_triggered();       
    void on_actionPaste_triggered();
    
    void on_actionToggleTable_triggered(bool checked);
    
    void on_actionLoadTable_triggered();
    void on_actionSaveTable_triggered();
    void on_actionSaveTableAs_triggered();
    void on_actionClearTable_triggered();
    
    void on_actionInsertLatinUpper_triggered();
    void on_actionInsertLatinLower_triggered();
    void on_actionInsertHiragana_triggered();
    void on_actionInsertKatakana_triggered();
    void on_actionInsertCyrillic_triggered();
    
    void openRecentFile(); 
    void handleTableItemChanged(QTableWidgetItem *item);
    void handleDataEdited(); 

    // Slots para el Hex Guesser
    void on_actionGuessEncoding_triggered();
    void handleGuessEncodingFinished();

private:
    Ui::hexandtabler *ui;
    HexEditorArea *m_hexEditorArea = nullptr;
    QTableWidget *m_tableWidget = nullptr; 
    QDockWidget *m_tableDock = nullptr;
    QByteArray m_fileData;
    FindReplaceDialog *m_findReplaceDialog = nullptr;
    
    QString m_currentFilePath;
    QString m_currentTablePath; 
    bool m_isModified = false;

    // Hex Guesser
    QFuture<QList<QMap<QChar, quint8>>> m_guessSearchFuture; 
    QMap<QChar, QList<int>> calculatePattern(const QString &text) const;
    QList<QMap<QChar, quint8>> guessEncoding(const QList<KnownPhrase> &phrases);
    void addFoundMappingToTable(const QMap<QChar, quint8> &mapping); // <-- ÚNICA DECLARACIÓN

    void inferNeighboringMappings(const QMap<QChar, quint8> &newlyAddedMapping); // <-- FUNCIÓN NUEVA
    
    struct EditorState {
        QByteArray data;
        int cursorPos;
        int selectionStart;
        int selectionEnd;
    };
    
    QList<EditorState> m_undoStack;
    QList<EditorState> m_redoStack;

    QString m_charMap[256]; 

    void findNext(const QByteArray &needle, bool caseSensitive, bool wrap, bool backwards);
    void replaceOne();
    void replaceAll(const QByteArray &needle, const QByteArray &replacement);

    // [Líneas eliminadas que contenían la declaración duplicada addFoundMappingToTable]
    
    void findNextRelative(const QString &searchText, bool wrap, bool backwards);
    QVector<qint16> calculateRelativeOffsets(const QString &input) const; 
    
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
    
    bool saveTableFile(const QString &filePath); 
    bool loadTableFile(const QString &filePath);
    void insertSeries(const QList<QString> &series); 
    void clearCharMappingTable();
    
    void createRecentFileActions();
    void loadRecentFiles();
    void updateRecentFileActions();
    void prependToRecentFiles(const QString &filePath); 
};

#endif // HEXANDTABLER_H