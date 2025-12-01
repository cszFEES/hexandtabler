#include "hexandtabler.h" 
#include "ui_hexandtabler.h" 

// Includes esenciales de Qt
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

// Include del widget personalizado
#include "hexeditorarea.h" 

// Constantes
const char organizationName[] = "FEES"; 
const char applicationName[] = "hexandtabler"; // hexandtabler en minúsculas
const int MAX_UNDO_STATES = 50; 


// --------------------------------------------------------------------------------
// --- Funciones Auxiliares (Zoom y About) ---
// --------------------------------------------------------------------------------

void hexandtabler::createMenus() {
    // Las acciones de About, Open, Save y Exit se conectan automáticamente.
    
    // Conexión de acciones de Zoom 
    if (ui->actionZoomIn) {
        ui->actionZoomIn->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Plus)); 
        connect(ui->actionZoomIn, &QAction::triggered, this, &hexandtabler::on_actionZoomIn_triggered);
    }
    if (ui->actionZoomOut) {
        connect(ui->actionZoomOut, &QAction::triggered, this, &hexandtabler::on_actionZoomOut_triggered);
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
// --- INICIO DE IMPLEMENTACIONES DE CLASE ---
// --------------------------------------------------------------------------------


hexandtabler::hexandtabler(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::hexandtabler)
{
    ui->setupUi(this);
    
    // Configuración del área de edición central
    m_hexEditorArea = new HexEditorArea(this);
    setCentralWidget(m_hexEditorArea);

    // Configuración del widget de tabla (Dock Widget)
    m_conversionTable = new QTableWidget(this); 
    setupConversionTable(); 
    
    m_tableDock = new QDockWidget(tr("Conversion Table (.TBL)"), this); 
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
        connect(redoAct, &QAction::triggered, this, &hexandtabler::on_actionRedo_triggered);
    } 

    // --- Recent Files Setup ---
    createRecentFileActions();
    loadRecentFiles();
    updateRecentFileActions();

    // Conexiones de Acciones
    connect(ui->actionDarkMode, &QAction::toggled, this, &hexandtabler::on_actionDarkMode_triggered); 

    // Conexiones de Opciones (Tabla)
    connect(ui->actionToggleTable, &QAction::toggled, m_tableDock, &QDockWidget::setVisible);
    connect(m_tableDock, &QDockWidget::visibilityChanged, ui->actionToggleTable, &QAction::setChecked);
    
    // Conexión principal del motor de edición con la ventana principal
    connect(m_hexEditorArea, &HexEditorArea::dataChanged, this, &hexandtabler::handleDataEdited); 
    
    updateUndoRedoActions();
    ui->actionSave->setEnabled(false); 

    // Configuración de menús y sub-acciones (Zoom y About)
    createMenus(); 
    
    // Carga del estado guardado
    QSettings settings(organizationName, applicationName);
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
}

hexandtabler::~hexandtabler() {
    delete ui;
}

// --------------------------------------------------------------------------------
// --- SLOTS DE ARCHIVO ---
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
// --- SLOTS DE OPCIONES Y EDICIÓN ---
// --------------------------------------------------------------------------------

void hexandtabler::on_actionDarkMode_triggered(bool checked) {
    if (checked) {
        QPalette darkPalette;
        darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::WindowText, Qt::white);
        darkPalette.setColor(QPalette::Base, QColor(25, 25, 25)); 
        darkPalette.setColor(QPalette::Text, Qt::white);
        darkPalette.setColor(QPalette::ButtonText, Qt::white);
        darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::HighlightedText, Qt::black);

        qApp->setPalette(darkPalette);
    } else {
        qApp->setPalette(QApplication::style()->standardPalette());
    }
}

void hexandtabler::on_actionToggleTable_triggered() {
    // El toggle ya está conectado al dock widget en el constructor
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

void hexandtabler::handleDataEdited() {
    pushUndoState(); 
    m_isModified = true;
    ui->actionSave->setEnabled(true);
}

// --------------------------------------------------------------------------------
// --- FUNCIONES DE LÓGICA Y ASISTENCIA ---
// --------------------------------------------------------------------------------

void hexandtabler::setupConversionTable() {
    if (!m_conversionTable) return;
    
    m_conversionTable->setRowCount(256);
    m_conversionTable->setColumnCount(3);
    
    QStringList headers;
    headers << tr("Hex") << tr("Dec") << tr("ASCII"); 
    m_conversionTable->setHorizontalHeaderLabels(headers);
    
    m_conversionTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_conversionTable->verticalHeader()->setVisible(false);
    m_conversionTable->setEditTriggers(QAbstractItemView::NoEditTriggers); 

    for (int i = 0; i < 256; ++i) {
        QTableWidgetItem *hexItem = new QTableWidgetItem(QString("%1").arg(i, 2, 16, QChar('0')).toUpper());
        hexItem->setTextAlignment(Qt::AlignCenter);
        m_conversionTable->setItem(i, 0, hexItem);

        QTableWidgetItem *decItem = new QTableWidgetItem(QString::number(i));
        decItem->setTextAlignment(Qt::AlignCenter);
        m_conversionTable->setItem(i, 1, decItem);

        char c = (char)i;
        // Solución a las 'comas': usar un punto para caracteres no imprimibles.
        bool isSafePrint = (i >= 0x20 && i <= 0x7E);
        QString asciiChar = isSafePrint ? QString(c) : QString("."); 
        QTableWidgetItem *asciiItem = new QTableWidgetItem(asciiChar);
        asciiItem->setTextAlignment(Qt::AlignCenter);
        m_conversionTable->setItem(i, 2, asciiItem);
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

// --------------------------------------------------------------------------------
// --- Implementaciones de Archivos Recientes ---
// --------------------------------------------------------------------------------

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