#include "hexandtabler.h" 
#include "ui_hexandtabler.h" 

// Essential Qt Includes
#include <QFileDialog>
#include <QFile>
#include <QMessageBox>
#include <QApplication>
#include <QSettings> 
#include <QFileInfo> 
#include <QCloseEvent>
#include <QStyle>   
#include <QPalette> 
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QDockWidget>
#include <QMenu>
#include <QAction>
#include <QKeySequence> 
#include <QHeaderView>
#include <QScrollBar>
#include <QDebug>
#include <QFont>
#include <algorithm>
#include <cctype> 
#include <QSignalBlocker> 
#include <QInputDialog> 
#include <QTextStream> 
#include <QModelIndexList> 

// Custom widget include
#include "hexeditorarea.h" 

// Constants
const char organizationName[] = "FEES"; 
const char applicationName[] = "hexandtabler"; 
const int MAX_UNDO_STATES = 50; 


// --------------------------------------------------------------------------------
// --- Helper Functions (Zoom, About, and Menu Connections) ---
// --------------------------------------------------------------------------------

void hexandtabler::createMenus() {
    // About, Open, Save, and Exit actions are connected automatically.
    
    // Zoom actions connection
    if (ui->actionZoomIn) {
        ui->actionZoomIn->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Plus)); 
        connect(ui->actionZoomIn, &QAction::triggered, this, &hexandtabler::on_actionZoomIn_triggered);
    }
    if (ui->actionZoomOut) {
        connect(ui->actionZoomOut, &QAction::triggered, this, &hexandtabler::on_actionZoomOut_triggered);
    }
    // Go To action connection
    if (ui->actionGoTo) {
        connect(ui->actionGoTo, &QAction::triggered, this, &hexandtabler::on_actionGoTo_triggered);
    }
    
    // File actions connection (RESTAURADO)
    if (ui->actionSaveAs) { 
        connect(ui->actionSaveAs, &QAction::triggered, this, &hexandtabler::on_actionSaveAs_triggered);
    }
    
    // Table actions connection (RESTAURADO)
    if (ui->actionToggleTable) { 
        connect(ui->actionToggleTable, &QAction::triggered, this, &hexandtabler::on_actionToggleTable_triggered);
    }
    if (ui->actionLoadTable) { 
        connect(ui->actionLoadTable, &QAction::triggered, this, &hexandtabler::on_actionLoadTable_triggered);
    }
    if (ui->actionSaveTable) { 
        connect(ui->actionSaveTable, &QAction::triggered, this, &hexandtabler::on_actionSaveTable_triggered);
    }
    if (ui->actionSaveTableAs) { 
        connect(ui->actionSaveTableAs, &QAction::triggered, this, &hexandtabler::on_actionSaveTableAs_triggered);
    }
    
    // New Insert Series connections (RESTAURADO)
    if (ui->actionInsertLatinUpper) {
        connect(ui->actionInsertLatinUpper, &QAction::triggered, this, &hexandtabler::on_actionInsertLatinUpper_triggered);
    }
    if (ui->actionInsertLatinLower) {
        connect(ui->actionInsertLatinLower, &QAction::triggered, this, &hexandtabler::on_actionInsertLatinLower_triggered);
    }
    if (ui->actionInsertHiragana) {
        connect(ui->actionInsertHiragana, &QAction::triggered, this, &hexandtabler::on_actionInsertHiragana_triggered);
    }
    if (ui->actionInsertKatakana) {
        connect(ui->actionInsertKatakana, &QAction::triggered, this, &hexandtabler::on_actionInsertKatakana_triggered);
    }
}


void hexandtabler::on_actionAbout_triggered() {
    // Mantiene la última modificación: Título solo "hexandtabler" y contenido solo "Author: **FEES**"
    QMessageBox::about(this, applicationName,
        tr("Author: **FEES**")
    );
}

void hexandtabler::on_actionZoomIn_triggered() {
    if (!m_hexEditorArea) return; 

    QFont currentFont = m_hexEditorArea->font();
    currentFont.setPointSize(currentFont.pointSize() + 1);
    m_hexEditorArea->setFont(currentFont);
    
    m_hexEditorArea->updateViewMetrics(); 
}

void hexandtabler::on_actionZoomOut_triggered() {
    if (!m_hexEditorArea) return; 
    
    QFont currentFont = m_hexEditorArea->font();
    int newSize = currentFont.pointSize() - 1;
    if (newSize >= 6) { 
        currentFont.setPointSize(newSize);
        m_hexEditorArea->setFont(currentFont);
        
        m_hexEditorArea->updateViewMetrics(); 
    }
}


// --------------------------------------------------------------------------------
// --- START OF CLASS IMPLEMENTATIONS ---
// --------------------------------------------------------------------------------


hexandtabler::hexandtabler(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::hexandtabler)
{
    ui->setupUi(this);
    
    // Central editing area configuration
    m_hexEditorArea = new HexEditorArea(this);
    setCentralWidget(m_hexEditorArea);

    // Table widget configuration (Dock Widget)
    m_conversionTable = new QTableWidget(this); 
    setupConversionTable(); 
    
    m_tableDock = new QDockWidget(tr("Conversion Table (.TBL)"), this); 
    // Remove title bar buttons (close/float)
    m_tableDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    m_tableDock->setWidget(m_conversionTable);
    addDockWidget(Qt::RightDockWidgetArea, m_tableDock);

    setWindowTitle(applicationName); 
    m_fileData.clear(); 
    m_currentTablePath.clear(); // Initialize new member
    
    // --- Undo/Redo and Edit Menu Configuration ---
    undoAct = ui->actionUndo; 
    redoAct = ui->actionRedo;
    
    if (undoAct) {
        connect(undoAct, &QAction::triggered, this, &hexandtabler::on_actionUndo_triggered);
    } 
    if (redoAct) {
        // Fix: Ensure the Redo action does not have a bold font style (user request fix)
        QFont normalFont = redoAct->font();
        normalFont.setWeight(QFont::Normal);
        redoAct->setFont(normalFont);
        
        connect(redoAct, &QAction::triggered, this, &hexandtabler::on_actionRedo_triggered);
    } 
    
    // Connect the slot for table item changes
    connect(m_conversionTable, &QTableWidget::itemChanged, this, &hexandtabler::handleTableItemChanged);
    
    // Pass the initial map to the hex editor area
    if (m_hexEditorArea) {
        m_hexEditorArea->setCharMapping(m_charMap); 
    }

    // --- Recent Files Setup ---
    createRecentFileActions();
    loadRecentFiles();
    updateRecentFileActions();

    // Action Connections
    connect(ui->actionDarkMode, &QAction::toggled, this, &hexandtabler::on_actionDarkMode_triggered); 

    // Options Connections (Table)
    connect(ui->actionToggleTable, &QAction::toggled, m_tableDock, &QDockWidget::setVisible);
    connect(m_tableDock, &QDockWidget::visibilityChanged, ui->actionToggleTable, &QAction::setChecked);
    
    // Main connection of the editing engine with the main window
    connect(m_hexEditorArea, &HexEditorArea::dataChanged, this, &hexandtabler::handleDataEdited); 
    
    updateUndoRedoActions();
    ui->actionSave->setEnabled(false); 

    // Menu and sub-action configuration (Zoom and About, Go To, Table)
    createMenus(); 
    
    // Load saved state
    QSettings settings(organizationName, applicationName);
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
}

hexandtabler::~hexandtabler() {
    delete ui;
}

// --------------------------------------------------------------------------------
// --- FILE SLOTS AND HELPERS ---
// --------------------------------------------------------------------------------

void hexandtabler::on_actionOpen_triggered() {
    if (!maybeSave()) return; 

    QString filePath = QFileDialog::getOpenFileName(this, tr("Open File"), "", tr("All Files (*.*)"));
    if (!filePath.isEmpty()) {
        loadFile(filePath);
    }
}

void hexandtabler::on_actionSave_triggered() {
    if (m_currentFilePath.isEmpty()) {
        on_actionSaveAs_triggered(); // If no path, trigger Save As...
        return;
    }
    saveCurrentFile();
}

// DEFINICIÓN RESTAURADA
void hexandtabler::on_actionSaveAs_triggered() {
    saveFileAs();
}

// DEFINICIÓN RESTAURADA
bool hexandtabler::saveFileAs() {
    // Show a file dialog to choose a path
    QString fileName = QFileDialog::getSaveFileName(this, 
                                                    tr("Save File As"), 
                                                    m_currentFilePath.isEmpty() ? QDir::homePath() : QFileInfo(m_currentFilePath).absoluteDir().path(), 
                                                    tr("All Files (*.*)"));
    
    if (fileName.isEmpty()) {
        return false; // User cancelled the dialog
    }
    
    // Attempt to save the data to the chosen file
    if (saveDataToFile(fileName)) {
        m_currentFilePath = fileName; // Update the current file path
        setWindowTitle(QString("%1 - %2").arg(applicationName).arg(QFileInfo(m_currentFilePath).fileName()));
        prependToRecentFiles(m_currentFilePath);
        return true;
    }
    return false;
}

// DEFINICIÓN RESTAURADA
bool hexandtabler::saveDataToFile(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("hexandtabler"),
                             tr("Cannot write file %1:\n%2.")
                             .arg(QDir::toNativeSeparators(filePath), file.errorString()));
        return false;
    }

    refreshModelFromArea(); // Ensure m_fileData is up to date

    qint64 bytesWritten = file.write(m_fileData);
    file.close();

    if (bytesWritten != m_fileData.size()) {
        QMessageBox::warning(this, tr("hexandtabler"),
                             tr("Error saving data to file: bytes written mismatch."));
        return false;
    }

    m_isModified = false;
    ui->actionSave->setEnabled(false);
    return true;
}

void hexandtabler::on_actionExit_triggered() {
    close();
}

void hexandtabler::openRecentFile() {
    QAction *action = qobject_cast<QAction *>(sender());
    if (action && maybeSave()) {
        loadFile(action->data().toString());
    }
}

// --------------------------------------------------------------------------------
// --- OPTIONS AND EDIT SLOTS ---
// --------------------------------------------------------------------------------

void hexandtabler::on_actionDarkMode_triggered(bool checked) {
    if (checked) {
        QPalette darkPalette;
        darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::WindowText, Qt::white);
        darkPalette.setColor(QPalette::Base, QColor(25, 25, 25)); 
        darkPalette.setColor(QPalette::AlternateBase, QColor(66, 66, 66)); 
        darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
        darkPalette.setColor(QPalette::ToolTipText, Qt::white);
        darkPalette.setColor(QPalette::Text, Qt::white); 
        darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::ButtonText, Qt::white);
        darkPalette.setColor(QPalette::BrightText, Qt::red);
        darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::HighlightedText, Qt::black);

        qApp->setPalette(darkPalette);
    } else {
        qApp->setPalette(QApplication::style()->standardPalette());
    }
    // Force the hex editor to update to use the new palette colors
    if (m_hexEditorArea) {
        m_hexEditorArea->viewport()->update();
    }
}

void hexandtabler::on_actionToggleTable_triggered() {
    // Toggles the visibility of the dock widget
    if (m_tableDock) {
        m_tableDock->setVisible(!m_tableDock->isVisible());
    }
}

void hexandtabler::on_actionUndo_triggered() {
    if (m_undoStack.isEmpty()) return;

    m_redoStack.append(m_fileData); 
    m_fileData = m_undoStack.takeLast(); 
    
    m_hexEditorArea->setHexData(m_fileData);
    m_hexEditorArea->updateViewMetrics();
    
    updateUndoRedoActions();
    m_isModified = true;
    ui->actionSave->setEnabled(true);
}

void hexandtabler::on_actionRedo_triggered() {
    if (m_redoStack.isEmpty()) return;

    m_undoStack.append(m_fileData);
    m_fileData = m_redoStack.takeLast();

    m_hexEditorArea->setHexData(m_fileData);
    m_hexEditorArea->updateViewMetrics();
    
    updateUndoRedoActions();
    m_isModified = true;
    ui->actionSave->setEnabled(true);
}

void hexandtabler::on_actionGoTo_triggered() {
    bool ok;
    // Request the hex address to jump to. QInputDialog::getText is modal and closes itself.
    QString hexOffsetStr = QInputDialog::getText(this, tr("Go to Offset"),
                                                tr("Enter hexadecimal offset (e.g., 1A0):"), QLineEdit::Normal,
                                                "", &ok);
    
    if (ok && !hexOffsetStr.isEmpty()) {
        bool conversionOk;
        // Convert the string to a long long integer (base 16)
        quint64 offset = hexOffsetStr.toULongLong(&conversionOk, 16);
        
        if (conversionOk && m_hexEditorArea) {
            m_hexEditorArea->goToOffset(offset);
        } else {
            QMessageBox::warning(this, tr("Invalid Input"), tr("Invalid hexadecimal offset entered."));
        }
    }
}


void hexandtabler::handleDataEdited() {
    pushUndoState(); 
    m_isModified = true;
    ui->actionSave->setEnabled(true);
}

// Slot to handle table changes and update the character map
void hexandtabler::handleTableItemChanged(QTableWidgetItem *item) {
    // We are only interested in changes in the second column (index 1)
    if (item->column() != 1) {
        return; 
    }
    
    int byteValue = item->row();
    QString newChar = item->text();
    
    // The map only supports single characters for display
    QString displayChar = newChar.left(1); 

    // Update the map storage
    m_charMap[byteValue] = displayChar;
    
    // Ensure the table item itself only shows one character
    if (newChar != displayChar) {
        // Block signals to prevent recursive calls
        QSignalBlocker blocker(m_conversionTable);
        item->setText(displayChar);
    }
    
    // Notify the hex editor to redraw with the new map
    if (m_hexEditorArea) {
        m_hexEditorArea->setCharMapping(m_charMap);
    }
}

// --------------------------------------------------------------------------------
// --- TABLE SLOTS AND HELPERS (.TBL) ---
// --------------------------------------------------------------------------------

void hexandtabler::on_actionSaveTable_triggered() {
    // If a path is already saved, use it. Otherwise, trigger Save As...
    if (m_currentTablePath.isEmpty()) {
        on_actionSaveTableAs_triggered();
        return;
    }
    saveTableFile(m_currentTablePath);
}

// DEFINICIÓN RESTAURADA
void hexandtabler::on_actionSaveTableAs_triggered() {
    // The filter defaults to *.tbl and is the first option
    QString fileName = QFileDialog::getSaveFileName(this, 
                                                    tr("Save Conversion Table As"), 
                                                    QDir::homePath(), 
                                                    tr("Conversion Table Files (*.tbl);;All Files (*)"));
    
    if (fileName.isEmpty()) {
        return; // User cancelled
    }
    
    // Ensure the file has the correct extension if the user didn't specify a filter
    if (!fileName.toLower().endsWith(".tbl")) {
        fileName += ".tbl";
    }
    
    if (saveTableFile(fileName)) {
        m_currentTablePath = fileName; // Update the current table path
    }
}

void hexandtabler::on_actionLoadTable_triggered() {
    QString filePath = QFileDialog::getOpenFileName(this, tr("Load Conversion Table"), "", tr("Table Files (*.tbl);;All Files (*.*)"));
    if (!filePath.isEmpty()) {
        if (loadTableFile(filePath)) {
            m_currentTablePath = filePath; // Update the current table path on successful load
        }
    }
}

bool hexandtabler::saveTableFile(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Save Error"), tr("Cannot open file for writing: ") + file.errorString());
        return false;
    }
    
    QTextStream out(&file);

    // Save map content: XX=C format (256 entries)
    for (int i = 0; i < 256; ++i) {
        // Hex byte (00 to FF)
        QString hexByte = QString("%1").arg(i, 2, 16, QChar('0')).toUpper(); 
        
        // Character (use the stored one, default is '.')
        QString charValue = m_charMap[i].isEmpty() ? "." : m_charMap[i];

        // Write: 00=. or 41=A
        out << hexByte << "=" << charValue << "\n";
    }

    file.close();
    QMessageBox::information(this, tr("Success"), tr("Conversion table saved successfully."));
    return true;
}

bool hexandtabler::loadTableFile(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Load Error"), tr("Cannot open file for reading: ") + file.errorString());
        return false;
    }

    QTextStream in(&file);
    QString newMap[256];
    
    // 1. Initialize newMap with DEFAULTS (printable ASCII or dot)
    for (int i = 0; i < 256; ++i) {
        char c = (char)i;
        bool isSafePrint = (i >= 0x20 && i <= 0x7E);
        newMap[i] = isSafePrint ? QString(c) : QString(".");
    }
    
    // Parse the file line by line, overwriting defaults in newMap
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue; 

        int equalsIndex = line.indexOf('=');
        if (equalsIndex > 0) {
            QString hexStr = line.left(equalsIndex).trimmed();
            QString charStr = line.mid(equalsIndex + 1).trimmed();
            
            bool ok;
            int byteValue = hexStr.toInt(&ok, 16); 

            if (ok && byteValue >= 0 && byteValue <= 255) {
                // We only care about the first character of the string
                QString displayChar = charStr.left(1);
                if (displayChar.isEmpty()) displayChar = "."; 
                
                newMap[byteValue] = displayChar;
            }
        }
    }
    
    file.close();

    // Apply the new map (defaults + file content) to the internal storage and the table widget
    bool mapChanged = false;
    for (int i = 0; i < 256; ++i) {
        if (m_charMap[i] != newMap[i]) {
            m_charMap[i] = newMap[i];
            mapChanged = true;

            // Update the QTableWidget
            if (m_conversionTable && m_conversionTable->item(i, 1)) {
                QSignalBlocker blocker(m_conversionTable);
                m_conversionTable->item(i, 1)->setText(m_charMap[i]);
            }
        }
    }
    
    if (!mapChanged) { 
        QMessageBox::information(this, tr("Load Warning"), tr("The loaded table is identical to the current one. No changes applied."));
        return true; 
    }

    // Notify the hex editor to redraw with the new map
    if (m_hexEditorArea) {
        m_hexEditorArea->setCharMapping(m_charMap);
    }

    QMessageBox::information(this, tr("Success"), tr("Conversion table loaded successfully and overwritten the current map."));
    return true;
}

// --- SERIES INSERTION IMPLEMENTATION (RESTAURADO) ---

void hexandtabler::insertSeries(const QList<QString> &series) {
    if (!m_conversionTable || series.isEmpty()) return;

    // Get the starting row (top-left selected cell's row, or 0 if nothing selected)
    QModelIndexList selectedIndexes = m_conversionTable->selectionModel()->selectedIndexes();
    int startRow = 0;
    if (!selectedIndexes.isEmpty()) {
        // Find the index with the smallest row number (top-most selected cell)
        startRow = selectedIndexes.first().row(); 
        for (const QModelIndex &index : selectedIndexes) {
            startRow = std::min(startRow, index.row());
        }
    }
    
    // Ensure startRow is within bounds [0, 255]
    if (startRow < 0) startRow = 0;
    if (startRow >= 256) {
        QMessageBox::warning(this, tr("Insertion Error"), tr("Selected row is out of bounds for the table."));
        return;
    }

    // Block signals to prevent recursive calls from handleTableItemChanged
    QSignalBlocker blocker(m_conversionTable);
    
    // Iterate through the series and update the map and table
    for (int i = 0; i < series.size(); ++i) {
        int currentRow = startRow + i;
        if (currentRow > 255) break;

        const QString &displayChar = series[i];
        
        // 1. Update internal map
        // Ensure only the first character is stored for mapping consistency
        QString charToStore = displayChar.left(1); 
        if (charToStore.isEmpty()) charToStore = ".";
        m_charMap[currentRow] = charToStore;
        
        // 2. Update QTableWidget
        QTableWidgetItem *item = m_conversionTable->item(currentRow, 1);
        if (item) {
            item->setText(charToStore);
        }
    }
    
    // Notify the hex editor to redraw with the new map
    if (m_hexEditorArea) {
        m_hexEditorArea->setCharMapping(m_charMap);
    }
}

// DEFINICIÓN RESTAURADA
void hexandtabler::on_actionInsertLatinUpper_triggered() {
    QList<QString> series;
    for (int i = 0; i < 26; ++i) { // 'A' (65) to 'Z' (90)
        series.append(QString(QChar('A' + i)));
    }
    insertSeries(series);
}

// DEFINICIÓN RESTAURADA
void hexandtabler::on_actionInsertLatinLower_triggered() {
    QList<QString> series;
    for (int i = 0; i < 26; ++i) { // 'a' (97) to 'z' (122)
        series.append(QString(QChar('a' + i)));
    }
    insertSeries(series);
}

// DEFINICIÓN RESTAURADA
void hexandtabler::on_actionInsertHiragana_triggered() {
    QList<QString> series;
    // Hiragana basic set from U+3041 (あ) for 50 characters
    const int startCode = 0x3041; 
    const int count = 50; 
    for (int i = 0; i < count; ++i) {
        series.append(QString(QChar(startCode + i)));
    }
    insertSeries(series);
}

// DEFINICIÓN RESTAURADA
void hexandtabler::on_actionInsertKatakana_triggered() {
    QList<QString> series;
    // Katakana basic set from U+30A1 (ア) for 50 characters
    const int startCode = 0x30A1;
    const int count = 50; 
    for (int i = 0; i < count; ++i) {
        series.append(QString(QChar(startCode + i)));
    }
    insertSeries(series);
}

// --------------------------------------------------------------------------------
// --- LOGIC AND ASSISTANCE FUNCTIONS (Existing Code Below) ---
// --------------------------------------------------------------------------------

void hexandtabler::setupConversionTable() {
    if (!m_conversionTable) return;
    
    m_conversionTable->setRowCount(256);
    // Set 2 columns (Hex and Assigned)
    m_conversionTable->setColumnCount(2);
    
    QStringList headers;
    headers << tr("Hex") << tr("Assigned"); 
    m_conversionTable->setHorizontalHeaderLabels(headers);
    
    m_conversionTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_conversionTable->verticalHeader()->setVisible(false);
    
    // Allow editing on single click (selection) and key press
    m_conversionTable->setEditTriggers(QAbstractItemView::SelectedClicked | QAbstractItemView::AnyKeyPressed); 

    // Get table font and ensure it is normal weight
    QFont normalFont = m_conversionTable->font();
    normalFont.setWeight(QFont::Normal);

    for (int i = 0; i < 256; ++i) {
        QTableWidgetItem *hexItem = new QTableWidgetItem(QString("%1").arg(i, 2, 16, QChar('0')).toUpper());
        hexItem->setTextAlignment(Qt::AlignCenter);
        // Prevent editing of Hex column
        hexItem->setFlags(hexItem->flags() & ~Qt::ItemIsEditable); 
        hexItem->setFont(normalFont); // Apply normal weight
        m_conversionTable->setItem(i, 0, hexItem); // Column 0: Hex

        char c = (char)i;
        // Default: dot for non-printable characters, standard char for ASCII
        bool isSafePrint = (i >= 0x20 && i <= 0x7E);
        QString asciiChar = isSafePrint ? QString(c) : QString("."); 
        
        QTableWidgetItem *asciiItem = new QTableWidgetItem(asciiChar);
        asciiItem->setTextAlignment(Qt::AlignCenter);
        asciiItem->setFont(normalFont); // Apply normal weight
        m_conversionTable->setItem(i, 1, asciiItem); // Column 1: Assigned
        
        // Initialize the internal map with the default value
        m_charMap[i] = asciiChar;
    }
}

void hexandtabler::loadFile(const QString &filePath) { 
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, tr("Error"), tr("Cannot open file: ") + file.errorString());
        return;
    }

    m_fileData = file.readAll(); 
    file.close();
    
    m_currentFilePath = filePath;
    setWindowTitle(QString("%1 - %2").arg(applicationName).arg(QFileInfo(filePath).fileName()));
    prependToRecentFiles(filePath);
    
    m_hexEditorArea->setHexData(m_fileData);

    m_undoStack.clear(); 
    m_redoStack.clear();
    pushUndoState(); 
    
    m_isModified = false;
    ui->actionSave->setEnabled(false);
}

// DEFINICIÓN RESTAURADA
bool hexandtabler::saveCurrentFile() {
    // If we have a path, call the helper to save.
    if (!m_currentFilePath.isEmpty()) {
        return saveDataToFile(m_currentFilePath);
    }
    // If no path, trigger Save As... dialog.
    return saveFileAs();
}

bool hexandtabler::maybeSave() {
    if (m_isModified) {
        QMessageBox::StandardButton ret;
        ret = QMessageBox::warning(this, tr("Application"),
                                   tr("The document has been modified.\n"
                                      "Do you want to save your changes?"),
                                   QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        
        if (ret == QMessageBox::Save) {
            return saveCurrentFile();
        } else if (ret == QMessageBox::Cancel) {
            return false;
        }
    }
    return true;
}

void hexandtabler::refreshModelFromArea() {
    // Get the latest data from the editor widget
    m_fileData = m_hexEditorArea->hexData(); 
}

void hexandtabler::pushUndoState() {
    refreshModelFromArea(); 
    
    if (!m_undoStack.isEmpty() && m_undoStack.last() == m_fileData) {
        return; 
    }
    
    if (!m_redoStack.isEmpty()) {
        m_redoStack.clear();
    }
    
    m_undoStack.append(m_fileData);
    if (m_undoStack.size() > MAX_UNDO_STATES) {
        m_undoStack.removeFirst();
    }
    
    updateUndoRedoActions();
}

void hexandtabler::updateUndoRedoActions() {
    if (undoAct) {
        undoAct->setEnabled(!m_undoStack.isEmpty());
    }
    if (redoAct) {
        redoAct->setEnabled(!m_redoStack.isEmpty());
    }
}

void hexandtabler::closeEvent(QCloseEvent *event) {
    if (maybeSave()) {
        QSettings settings(organizationName, applicationName);
        settings.setValue("geometry", saveGeometry());
        settings.setValue("windowState", saveState());
        
        event->accept();
    } else {
        event->ignore();
    }
}

void hexandtabler::createRecentFileActions() {
    for (int i = 0; i < MaxRecentFiles; ++i) {
        recentFileActions[i] = new QAction(this);
        recentFileActions[i]->setVisible(false);
        connect(recentFileActions[i], &QAction::triggered, this, &hexandtabler::openRecentFile);
    }
}

void hexandtabler::loadRecentFiles() {
    QSettings settings(organizationName, applicationName);
    QStringList files = settings.value("recentFiles").toStringList();

    for (int i = 0; i < MaxRecentFiles; ++i) {
        if (i < files.size()) {
            recentFileActions[i]->setData(files[i]);
        } else {
            recentFileActions[i]->setData(QString());
        }
    }
}

void hexandtabler::updateRecentFileActions() {
    QSettings settings(organizationName, applicationName);
    QStringList files = settings.value("recentFiles").toStringList();
    
    int numRecentFiles = std::min(files.size(), (int)MaxRecentFiles);
    
    QAction *separator = nullptr;
    QList<QAction*> actions = ui->menuFile->actions();
    
    for (QAction *action : actions) {
        if (action->isSeparator() && ui->menuFile->actions().indexOf(action) > ui->menuFile->actions().indexOf(ui->actionSave)) { 
             separator = action;
             break;
        }
    }

    if (!separator) {
        separator = ui->menuFile->insertSeparator(ui->actionExit);
    }
    
    separator->setVisible(numRecentFiles > 0);

    for (int i = 0; i < MaxRecentFiles; ++i) {
        ui->menuFile->removeAction(recentFileActions[i]);
    }

    for (int i = 0; i < numRecentFiles; ++i) {
        QString text = QString("&%1 %2")
            .arg(i + 1)
            .arg(QFileInfo(files[i]).fileName());
        
        recentFileActions[i]->setText(text);
        recentFileActions[i]->setData(files[i]);
        recentFileActions[i]->setVisible(true);
        ui->menuFile->insertAction(separator, recentFileActions[i]);
    }
}

void hexandtabler::prependToRecentFiles(const QString &filePath) {
    QSettings settings(organizationName, applicationName);
    QStringList files = settings.value("recentFiles").toStringList();
    files.removeAll(filePath);
    files.prepend(filePath);
    while (files.size() > MaxRecentFiles)
        files.removeLast();

    settings.setValue("recentFiles", files);

    updateRecentFileActions();
}