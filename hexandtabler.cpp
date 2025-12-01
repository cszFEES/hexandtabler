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
    
    // Table actions connection
    if (ui->actionSaveTable) { 
        connect(ui->actionSaveTable, &QAction::triggered, this, &hexandtabler::on_actionSaveTable_triggered);
    }
    if (ui->actionLoadTable) { 
        connect(ui->actionLoadTable, &QAction::triggered, this, &hexandtabler::on_actionLoadTable_triggered);
    }
}


void hexandtabler::on_actionAbout_triggered() {
    QMessageBox::about(this, tr(" %1").arg(applicationName),
        tr("<h2>%1</h2>"
           "Author: **FEES**")
           .arg(applicationName)
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
    
    // --- Undo/Redo and Edit Menu Configuration ---
    undoAct = ui->actionUndo; 
    redoAct = ui->actionRedo;
    
    if (undoAct) {
        connect(undoAct, &QAction::triggered, this, &hexandtabler::on_actionUndo_triggered);
    } 
    if (redoAct) {
        // FIX: Asegurar que la acción Redo no tenga un estilo de fuente en negrita
        // Esto sobrescribe cualquier estilo predefinido por el sistema/UI para esta acción.
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
// --- FILE SLOTS ---
// --------------------------------------------------------------------------------

void hexandtabler::on_actionOpen_triggered() {
    if (!maybeSave()) return; 

    QString filePath = QFileDialog::getOpenFileName(this, tr("Open File"), "", tr("All Files (*.*)"));
    if (!filePath.isEmpty()) {
        loadFile(filePath);
    }
}

void hexandtabler::on_actionSave_triggered() {
    saveCurrentFile();
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
// --- OPTIONS AND EDIT SLOTS (Dark Mode Fix Here) ---
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
        darkPalette.setColor(QPalette::Text, Qt::white); // CLAVE: Texto blanco para ser legible
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
    // Forzamos la actualización del hex editor para usar los nuevos colores de la paleta
    if (m_hexEditorArea) {
        m_hexEditorArea->viewport()->update();
    }
}

void hexandtabler::on_actionToggleTable_triggered() {
    // The toggle is already connected to the dock widget in the constructor
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
    QString filePath = QFileDialog::getSaveFileName(this, tr("Save Conversion Table"), "", tr("Table Files (*.tbl);;All Files (*.*)"));
    if (!filePath.isEmpty()) {
        saveTableFile(filePath);
    }
}

void hexandtabler::on_actionLoadTable_triggered() {
    QString filePath = QFileDialog::getOpenFileName(this, tr("Load Conversion Table"), "", tr("Table Files (*.tbl);;All Files (*.*)"));
    if (!filePath.isEmpty()) {
        loadTableFile(filePath);
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
    bool mapChanged = false;

    // 1. Initialize newMap with DEFAULTS (printable ASCII or dot)
    // This ensures a full overwrite behavior, resetting custom entries not in the file.
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
                
                // If the value read from the file is different from what we currently have in m_charMap
                if (m_charMap[byteValue] != displayChar) { 
                    newMap[byteValue] = displayChar;
                    mapChanged = true;
                }
            }
        }
    }
    
    file.close();

    // Check if any change occurred during parsing
    if (!mapChanged) { 
        QMessageBox::information(this, tr("Load Warning"), tr("The loaded table is identical to the current one. No changes applied."));
        return true; 
    }

    // Apply the new map (defaults + file content) to the internal storage and the table widget
    for (int i = 0; i < 256; ++i) {
        m_charMap[i] = newMap[i];

        // Update the QTableWidget
        if (m_conversionTable && m_conversionTable->item(i, 1)) {
            QSignalBlocker blocker(m_conversionTable);
            m_conversionTable->item(i, 1)->setText(m_charMap[i]);
        }
    }
    
    // Notify the hex editor to redraw with the new map
    if (m_hexEditorArea) {
        m_hexEditorArea->setCharMapping(m_charMap);
    }

    QMessageBox::information(this, tr("Success"), tr("Conversion table loaded successfully and overwrote the current map."));
    return true;
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

bool hexandtabler::saveCurrentFile() {
    if (m_currentFilePath.isEmpty()) {
        QString newPath = QFileDialog::getSaveFileName(this, tr("Save File"), "", tr("All Files (*.*)"));
        if (newPath.isEmpty()) {
            return false;
        }
        m_currentFilePath = newPath;
    }
    
    QFile file(m_currentFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::critical(this, tr("Save Error"), tr("Cannot open file for writing: ") + file.errorString());
        return false;
    }

    refreshModelFromArea(); 

    qint64 bytesWritten = file.write(m_fileData); 
    file.close();

    if (bytesWritten == m_fileData.size()) { 
        setWindowTitle(QString("%1 - %2").arg(applicationName).arg(QFileInfo(m_currentFilePath).fileName()));
        m_isModified = false;
        ui->actionSave->setEnabled(false);
        prependToRecentFiles(m_currentFilePath);
        return true;
    } else {
        QMessageBox::critical(this, tr("Write Error"), tr("Failed to write all data to the file."));
        return false;
    }
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