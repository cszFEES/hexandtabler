#ifndef HEXANDTABLER_H
#define HEXANDTABLER_H

#include <QMainWindow>
#include <QByteArray>
#include <QList>
#include <QAction>
#include <QString>
#include <QTableWidgetItem>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QScreen>
#include <QMap>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QTableWidget>
#include <QDockWidget>
#include <QLabel>
#include <QFileDialog>
#include <QDir>
#include <optional>
#include <vector>

class HexEditorArea;
class FindReplaceDialog;

namespace Ui { class hexandtabler; }

struct KnownPhrase {
    QString text;
    int length = 0;
    QMap<QChar, QList<int>> pattern;
};

class hexandtabler : public QMainWindow {
    Q_OBJECT

public:
    explicit hexandtabler(QWidget *parent = nullptr);
    ~hexandtabler();
    
    QByteArray convertSearchString(const QString &input, int type) const;

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void on_actionOpen_triggered();
    void on_actionSave_triggered();
    void on_actionSaveAs_triggered();
    void on_actionExit_triggered();
    void on_actionAbout_triggered();
    void on_actionUndo_triggered();
    void on_actionRedo_triggered();
    void on_actionCopy_triggered();
    void on_actionCopyAddress_triggered();
    void on_actionPaste_triggered();
    void on_themeChanged(int index);
    void on_actionZoomIn_triggered();
    void on_actionZoomOut_triggered();
    void on_actionGoTo_triggered();
    void on_actionFind_triggered();
    void on_actionReplace_triggered();
    void on_actionSearchRelative_triggered();
    void on_actionGuessEncoding_triggered();
    void on_actionToggleTable_triggered(bool checked);
    void on_actionLoadTable_triggered();
    void on_actionSaveTable_triggered();
    void on_actionSaveTableAs_triggered();
    void on_actionClearTable_triggered();
    void on_actionAddRange16_triggered();
    void on_actionRemoveRange16_triggered();
    void on_actionChangeEndian16_triggered(bool checked);

    void on_actionInsertLatinUpper_triggered();
    void on_actionInsertLatinLower_triggered();
    void on_actionInsertHiragana_triggered();
    void on_actionInsertKatakana_triggered();
    void on_actionInsertCyrillic_triggered();
    void on_actionInsertNumbers19_triggered();

    void onSaveFinished();
    void onFindFinished();
    void handleGuessEncodingFinished();
    void handleByteEdited(qint64 offset, quint8 oldByte, quint8 newByte);
    void handleTableItemChanged(QTableWidgetItem *item);
    void on_actionClearRecentFiles_triggered();
    void openRecentFile();
    void replaceOne();

private:
    Ui::hexandtabler *ui;

    HexEditorArea *hexArea; 
    QTableWidget *m_tableWidget;
    QDockWidget *m_tableDock;
    FindReplaceDialog *m_findReplaceDialog;

    QByteArray m_fileData;
    QString m_currentFilePath;
    QString m_currentTablePath;
    
    bool m_isDirty = false; 

    QString m_charMap[256];
    QMap<uint16_t, QString> m_charMap16;
    bool m_table16BigEndian = false;

    static const int MaxRecentFiles = 10;
    QAction *recentFileActions[MaxRecentFiles];

    QFutureWatcher<std::optional<qsizetype>> m_findWatcher;
    qsizetype m_lastRelSearchLen = 0;
    QFuture<QList<QMap<QChar, unsigned char>>> m_guessSearchFuture;

    struct ByteChange {
        qsizetype offset;
        quint8 oldByte;
        quint8 newByte;
    };

    struct EditorState {
        QList<ByteChange> changes;
        qsizetype cursorPos;
        qsizetype selectionStart; 
        qsizetype selectionEnd;   
    };

    QList<EditorState> m_undoStack;
    QList<EditorState> m_redoStack;

    void loadFile(const QString &filePath);
    void setCurrentFile(const QString &filePath);
    bool saveFileAs();
    bool saveCurrentFile();
    bool saveDataToFile(const QString &filePath);
    void updateWindowTitle();

    void findNext(const QByteArray &needle, bool caseSensitive, bool wrap, bool backwards);
    
    void findNextRelative(const QString &searchText, bool backwards);
    void findNextRelative(const QString &searchText, bool wrap, bool backwards, int tolMin = 0, int tolMax = 0);

    void replaceAll(const QByteArray &needle, const QByteArray &replacement);
    
    std::vector<int16_t> calculateRelativeOffsets(const QString &input) const;
    static std::optional<qsizetype> performRelativeSearchTask(const QByteArray data, 
                                                            const std::vector<int16_t> offsets, 
                                                            qsizetype startPos, 
                                                            bool backwards);

    void setupConversionTable();
    bool loadTableFile(const QString &filePath);
    bool saveTableFile(const QString &filePath);
    void clearCharMappingTable();
    void setupConversionTable16();
    void clearCharMappingTable16();
    void applyCharMap16ToWidget();
    void propagateCharMaps();
    void insertSeries(const QList<QString> &series);
    void refreshModelFromArea();

    QMap<QChar, QList<int>> calculatePattern(const QString &text) const;
    QList<QMap<QChar, unsigned char>> guessEncoding(const QList<KnownPhrase> &phrases, 
                                                   quint64 start, quint64 end);
    void addFoundMappingToTable(const QMap<QChar, unsigned char> &mapping);

    void createRecentFileActions();
    void loadRecentFiles();
    void updateRecentFileActions();
    void prependToRecentFiles(const QString &filePath);

    void pushUndoState();
    void pushUndoState(const EditorState &state);
    void clearUndoRedo();
    void updateUndoRedoActions();
    bool maybeSave();
    void on_actionDarkMode_triggered(bool checked);


};

#endif  