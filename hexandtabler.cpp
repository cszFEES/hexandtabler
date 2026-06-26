#include "hexandtabler.h" 
#include "ui_hexandtabler.h" 

#include <QFileDialog>
#include <QFile>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>
#include <QSettings> 
#include <QSet>
#include <QTimer>
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
#include <QStyleHints>
#include <QGuiApplication>
#include <QScreen>
#include <algorithm>
#include <cctype> 
#include <QSignalBlocker> 
#include <QInputDialog> 
#include <QTextStream> 
#include <QModelIndexList> 
#include <QFormLayout> 
#include <QLabel> 
#include <QLineEdit> 
#include <QPushButton> 
#include <QHBoxLayout> 
#include <QVBoxLayout> 
#include <QCheckBox> 
#include <QRadioButton> 
#include <QDialog> 
#include <QDir> 
#include <climits> 
#include <QtConcurrent/QtConcurrent>
#include <QListWidget>
#include <QListWidgetItem>
#include <QDialogButtonBox>

#include "hexeditorarea.h" 

const char organizationName[] = "FEES"; 
const char applicationName[] = "hexandtabler"; 
const int MAX_UNDO_STATES = 50; 
const int MIN_CHARS_FOR_RELATIVE_SEARCH = 3; 
const qint16 WILD_CARD_OFFSET = SHRT_MIN; 

class FindReplaceDialog : public QDialog
{
    Q_OBJECT

public:
    enum SearchType {
        HexSearch,
        CharSearch,
        RelativeSearch,
        Relative16LESearch,
        Relative16BESearch
    };

    FindReplaceDialog(QWidget *parent = nullptr);
    
    QString findText() const { return findLineEdit->text(); }
    QString replaceText() const { return replaceLineEdit->text(); }
    bool isCaseSensitive() const { return caseSensitiveCheckBox->isChecked(); }
    bool isWrapped() const { return wrapCheckBox->isChecked(); }
    bool isReplaceMode() const { return replaceLineEdit->isVisible(); }
    
    SearchType searchType() const { 
        if (hexRadioButton->isChecked()) return HexSearch;
        if (relativeRadioButton->isChecked()) return RelativeSearch;
        if (relative16LERadioButton->isChecked()) return Relative16LESearch;
        if (relative16BERadioButton->isChecked()) return Relative16BESearch;
        return CharSearch; 
    } 
    
    void setFindMode() { 
        setWindowTitle(tr("Find")); 
        replaceLabel->hide(); 
        replaceLineEdit->hide(); 
        replaceButton->hide(); 
        replaceAllButton->hide();
        adjustSize();
    }

    void setReplaceMode() { 
        setWindowTitle(tr("Find and Replace"));
        replaceLabel->show(); 
        replaceLineEdit->show(); 
        replaceButton->show(); 
        replaceAllButton->show();
        adjustSize();
    }

signals:
    void findNextClicked(bool backwards);
    void replaceClicked();
    void replaceAllClicked();
    
private slots:
    void onFindNext();
    
private:
    QLineEdit *findLineEdit;
    QLineEdit *replaceLineEdit;
    QCheckBox *caseSensitiveCheckBox;
    QCheckBox *wrapCheckBox;
    QCheckBox *backwardsCheckBox;
    
    QRadioButton *hexRadioButton; 
    QRadioButton *charRadioButton; 
    QRadioButton *relativeRadioButton;
    QRadioButton *relative16LERadioButton;
    QRadioButton *relative16BERadioButton;
    
    QLabel *replaceLabel;
    QPushButton *findNextButton;
    QPushButton *replaceButton;
    QPushButton *replaceAllButton;

};

FindReplaceDialog::FindReplaceDialog(QWidget *parent)
    : QDialog(parent)
{
    findLineEdit = new QLineEdit;
    replaceLineEdit = new QLineEdit;
    
    QLabel *findLabel = new QLabel(tr("&Find:"));
    findLabel->setBuddy(findLineEdit);
    replaceLabel = new QLabel(tr("&Replace:"));
    replaceLabel->setBuddy(replaceLineEdit);

    QFormLayout *formLayout = new QFormLayout; 
    formLayout->addRow(findLabel, findLineEdit);
    formLayout->addRow(replaceLabel, replaceLineEdit);

    hexRadioButton = new QRadioButton(tr("Hexadecimal (FF 1A)"));
    charRadioButton = new QRadioButton(tr("Character (Table)"));
    relativeRadioButton = new QRadioButton(tr("Relative 8-bit")); 
    relative16LERadioButton = new QRadioButton(tr("Relative 16-bit LE"));
    relative16BERadioButton = new QRadioButton(tr("Relative 16-bit BE"));
    hexRadioButton->setChecked(true); 

    QHBoxLayout *typeLayout = new QHBoxLayout;
    typeLayout->addWidget(new QLabel(tr("Search Type:")));
    typeLayout->addWidget(hexRadioButton);
    typeLayout->addWidget(charRadioButton);
    typeLayout->addWidget(relativeRadioButton);
    typeLayout->addWidget(relative16LERadioButton);
    typeLayout->addWidget(relative16BERadioButton);

    caseSensitiveCheckBox = new QCheckBox(tr("Case sensitive"));
    wrapCheckBox = new QCheckBox(tr("Wrap around"));
    backwardsCheckBox = new QCheckBox(tr("Search backwards"));

    QHBoxLayout *optionsRow = new QHBoxLayout;
    optionsRow->addWidget(caseSensitiveCheckBox);
    optionsRow->addWidget(wrapCheckBox);
    optionsRow->addWidget(backwardsCheckBox);
    optionsRow->addStretch();
    
    findNextButton = new QPushButton(tr("Find Next"));
    findNextButton->setDefault(true);
    replaceButton = new QPushButton(tr("Replace"));
    replaceAllButton = new QPushButton(tr("Replace All"));
    QPushButton *closeButton = new QPushButton(tr("Close"));

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(findNextButton);
    buttonLayout->addWidget(replaceButton);
    buttonLayout->addWidget(replaceAllButton);
    buttonLayout->addWidget(closeButton);
    
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addLayout(formLayout);
    mainLayout->addLayout(typeLayout); 
    mainLayout->addLayout(optionsRow);
    mainLayout->addLayout(buttonLayout);
    setLayout(mainLayout);
    
    connect(findNextButton, &QPushButton::clicked, this, &FindReplaceDialog::onFindNext);
    connect(replaceButton, &QPushButton::clicked, this, &FindReplaceDialog::replaceClicked);
    connect(replaceAllButton, &QPushButton::clicked, this, &FindReplaceDialog::replaceAllClicked);
    connect(closeButton, &QPushButton::clicked, this, &FindReplaceDialog::close);
    
    connect(findLineEdit, &QLineEdit::textEdited, [this](){
        backwardsCheckBox->setChecked(false);
    });

    setFindMode();
    setMinimumWidth(500);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
}


void FindReplaceDialog::onFindNext() {
    emit findNextClicked(backwardsCheckBox->isChecked());
}

QByteArray hexandtabler::convertSearchString(const QString &input, int type) const {
    if (type == FindReplaceDialog::HexSearch) { 
        QString hexInput = input;
        hexInput.remove(' '); 
        
        if (hexInput.length() % 2 != 0) {
            return QByteArray();
        }
        return QByteArray::fromHex(hexInput.toUtf8());
    } 
    
    if (type == FindReplaceDialog::CharSearch) { 
        QByteArray result;
        QString searchString = input;
        
        for (const QChar &ch : searchString) {
            bool found = false;
            for (int byteValue = 0; byteValue < 256; ++byteValue) {
                if (!m_charMap[byteValue].isEmpty() && m_charMap[byteValue].at(0) == ch) { 
                    result.append((char)byteValue);
                    found = true;
                    break; 
                }
            }
            if (!found) {
                return QByteArray(); 
            }
        }
        return result;
    }
    
    return QByteArray();
}

hexandtabler::hexandtabler(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::hexandtabler)
{
    ui->setupUi(this);
    statusBar()->hide();
    setWindowIcon(QIcon(":/icon.png"));
    
    m_tableWidget = new QTableWidget(this); 
    
    m_tableDock = new QDockWidget(this);
    m_tableDock->setTitleBarWidget(new QWidget()); // elimina el título completamente
    m_tableDock->setFeatures(QDockWidget::DockWidgetMovable); 
    m_tableDock->setWidget(m_tableWidget);
    addDockWidget(Qt::RightDockWidgetArea, m_tableDock);
    setupConversionTable();

    connect(ui->actionDarkMode, &QAction::triggered, this, &hexandtabler::on_actionDarkMode_triggered);
    
    if (ui->hexEdit) {
        QWidget *placeholder = ui->hexEdit;
        QVBoxLayout *layout = qobject_cast<QVBoxLayout*>(placeholder->parentWidget()->layout());
        
        if (layout) {
            hexArea = new HexEditorArea(this);
            hexArea->setHexData(m_fileData);
            
            int index = layout->indexOf(placeholder);
            if (index != -1) {
                layout->insertWidget(index, hexArea);
            } else {
                layout->addWidget(hexArea);
            }
            
            delete placeholder;
            ui->hexEdit = nullptr; 
        } else {
            hexArea = nullptr; 
        }
    } else {
        hexArea = nullptr;
    }
    
    m_findReplaceDialog = new FindReplaceDialog(this); 
    
    if (hexArea) {
     connect(hexArea, &HexEditorArea::byteEdited, this, &hexandtabler::handleByteEdited);
     connect(hexArea, &HexEditorArea::dataChanged, this, [this]() {
         pushUndoState();
         m_isDirty = true;
         updateWindowTitle();
     });
}
    
    if (m_tableWidget) {
        connect(m_tableWidget, &QTableWidget::itemChanged, this, &hexandtabler::handleTableItemChanged);
    }

    if (m_findReplaceDialog) {
        connect(m_findReplaceDialog, &FindReplaceDialog::findNextClicked, this, [this](bool backwards) {
            
            if (m_findReplaceDialog->searchType() == FindReplaceDialog::RelativeSearch) {
                this->findNextRelative(m_findReplaceDialog->findText(), m_findReplaceDialog->isWrapped(), backwards);
                return;
            }
            
            if (m_findReplaceDialog->searchType() == FindReplaceDialog::Relative16LESearch) {
                this->findNextRelative16(m_findReplaceDialog->findText(), m_findReplaceDialog->isWrapped(), backwards, false);
                return;
            }
            
            if (m_findReplaceDialog->searchType() == FindReplaceDialog::Relative16BESearch) {
                this->findNextRelative16(m_findReplaceDialog->findText(), m_findReplaceDialog->isWrapped(), backwards, true);
                return;
            }
            
            QByteArray needle = this->convertSearchString(m_findReplaceDialog->findText(), m_findReplaceDialog->searchType());
            
            if (needle.isEmpty()) {
                QMessageBox::warning(m_findReplaceDialog, tr("Input Error"), 
                    tr("Invalid search pattern or character not found in map for the selected mode. "
                       "Relative search requires a minimum of %1 characters.").arg(MIN_CHARS_FOR_RELATIVE_SEARCH));
                return;
            }
            this->findNext(needle, m_findReplaceDialog->isCaseSensitive(), m_findReplaceDialog->isWrapped(), backwards);
        });

        connect(m_findReplaceDialog, &FindReplaceDialog::replaceAllClicked, this, [this]() {
            QByteArray needle = this->convertSearchString(m_findReplaceDialog->findText(), m_findReplaceDialog->searchType());
            QByteArray replacement = this->convertSearchString(m_findReplaceDialog->replaceText(), m_findReplaceDialog->searchType());
            if (needle.isEmpty()) {
                QMessageBox::warning(m_findReplaceDialog, tr("Replace Error"), tr("Invalid search pattern."));
                return;
            }
            this->replaceAll(needle, replacement);
        });
        
        connect(m_findReplaceDialog, &FindReplaceDialog::replaceClicked, this, &hexandtabler::replaceOne); 
    }

    on_actionDarkMode_triggered(ui->actionDarkMode->isChecked());
    
    if (hexArea) {
        propagateCharMaps(); 
    } 
    
    connect(ui->actionToggleTable, &QAction::toggled, m_tableDock, &QDockWidget::setVisible);
    connect(m_tableDock, &QDockWidget::visibilityChanged, ui->actionToggleTable, &QAction::setChecked);
    
    connect(&m_findWatcher, &QFutureWatcher<std::optional<qsizetype>>::finished,
            this, &hexandtabler::onFindFinished);

    createRecentFileActions();
    loadRecentFiles();
    updateUndoRedoActions();
    
    setWindowTitle(QString("%1 - %2").arg(applicationName).arg(tr("No File")));
}

hexandtabler::~hexandtabler()
{
    delete ui;
}

void hexandtabler::closeEvent(QCloseEvent *event)
{
    if (maybeSave()) {
        event->accept();
    } else {
        event->ignore();
    }
}

void hexandtabler::on_actionOpen_triggered() {
    if (!maybeSave()) return; 
    
    QString filePath = QFileDialog::getOpenFileName(this, tr("Open File"), "", tr("All Files (*.*)"));
    if (!filePath.isEmpty()) {
        loadFile(filePath);
    }
}

void hexandtabler::on_actionSave_triggered() {
    if (m_currentFilePath.isEmpty()) {
        on_actionSaveAs_triggered(); 
        return;
    }
    saveCurrentFile();
}

void hexandtabler::on_actionSaveAs_triggered() {
    saveFileAs();
}

bool hexandtabler::saveFileAs() {
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save File As"), m_currentFilePath.isEmpty() ? QDir::homePath() : QFileInfo(m_currentFilePath).absoluteDir().path(), tr("All Files (*.*)"));
    if (fileName.isEmpty()) {
        return false;
    }

    if (saveDataToFile(fileName)) {
        m_currentFilePath = fileName;
        setWindowTitle(QString("%1 - %2").arg(applicationName).arg(QFileInfo(m_currentFilePath).fileName()));
        prependToRecentFiles(m_currentFilePath); 
        return true;
    }
    return false;
}

bool hexandtabler::saveCurrentFile() {
    if (m_currentFilePath.isEmpty()) {
        return saveFileAs();
    }
    
    if (saveDataToFile(m_currentFilePath)) {
        setWindowTitle(QString("%1 - %2").arg(applicationName).arg(QFileInfo(m_currentFilePath).fileName()));
        return true;
    }
    return false;
}

bool hexandtabler::saveDataToFile(const QString &filePath) {
    if (hexArea) {
        m_fileData = hexArea->hexData();
    } else {
        QMessageBox::critical(this, tr("Error"), tr("Editor area is not initialized. Cannot save data."));
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, tr("Error"), tr("Could not write file %1:\n%2.").arg(filePath).arg(file.errorString()));
        return false;
    }

    if (file.write(m_fileData) == -1) {
        QMessageBox::critical(this, tr("Error"), tr("Could not write all data to file %1:\n%2.").arg(filePath).arg(file.errorString()));
        file.close();
        return false;
    }

    file.close();
    m_isDirty = false;
    updateUndoRedoActions();
    return true;
}

void hexandtabler::loadFile(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        statusBar()->showMessage(tr("Error opening file"), 3000);
        return;
    }

    QByteArray data = file.readAll();
    m_fileData = data;
    hexArea->setHexData(data);
    
    setCurrentFile(filePath);
    prependToRecentFiles(filePath);
    clearUndoRedo();
    m_isDirty = false;
    updateWindowTitle();
    
    statusBar()->showMessage(tr("File loaded: %1 bytes").arg(data.size()), 3000);
}

void hexandtabler::on_actionOpenNewWindow_triggered() {
    QString filePath = QFileDialog::getOpenFileName(this, tr("Open File"), "", tr("All Files (*.*)"));
    if (filePath.isEmpty()) return;

    hexandtabler *newWindow = new hexandtabler();
    newWindow->loadFile(filePath);
    newWindow->show();
}

void hexandtabler::setCurrentFile(const QString &filePath) {
    m_currentFilePath = filePath;
    updateWindowTitle();
}

bool hexandtabler::maybeSave()
{
    if (!m_isDirty)
        return true;

    const QMessageBox::StandardButton ret
        = QMessageBox::warning(this, applicationName,
                             tr("The document has been modified.\n"
                                "Do you want to save your changes?"),
                             QMessageBox::Save | QMessageBox::Discard
                             | QMessageBox::Cancel);
    switch (ret) {
    case QMessageBox::Save:
        return saveCurrentFile();
    case QMessageBox::Discard:
        return true;
    case QMessageBox::Cancel:
    default:
        return false;
    }
}

void hexandtabler::on_actionExit_triggered() {
    close();
}

void hexandtabler::on_actionAbout_triggered() {
    QMessageBox::about(this, tr("hexandtabler"), 
                       "<p>Author: FEES</p>");
}

void hexandtabler::on_actionDarkMode_triggered(bool checked) {
    QPalette p;
    if (checked) {
        p.setColor(QPalette::Window, QColor(53, 53, 53));
        p.setColor(QPalette::WindowText, Qt::white);
        p.setColor(QPalette::Base, QColor(25, 25, 25));
        p.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
        p.setColor(QPalette::ToolTipBase, Qt::white);
        p.setColor(QPalette::ToolTipText, Qt::white);
        p.setColor(QPalette::Text, Qt::white);
        p.setColor(QPalette::Button, QColor(53, 53, 53));
        p.setColor(QPalette::ButtonText, Qt::white);
        p.setColor(QPalette::BrightText, Qt::red);
        p.setColor(QPalette::Link, QColor(42, 130, 218));
        p.setColor(QPalette::Highlight, QColor(42, 130, 218));
        p.setColor(QPalette::HighlightedText, Qt::black);
    } else {
        p.setColor(QPalette::Window, QColor(240, 240, 240));
        p.setColor(QPalette::WindowText, Qt::black);
        p.setColor(QPalette::Base, Qt::white);
        p.setColor(QPalette::AlternateBase, QColor(233, 231, 227));
        p.setColor(QPalette::ToolTipBase, Qt::black);
        p.setColor(QPalette::ToolTipText, Qt::black);
        p.setColor(QPalette::Text, Qt::black);
        p.setColor(QPalette::Button, QColor(240, 240, 240));
        p.setColor(QPalette::ButtonText, Qt::black);
        p.setColor(QPalette::BrightText, Qt::red);
        p.setColor(QPalette::Link, QColor(0, 0, 255));
        p.setColor(QPalette::Highlight, QColor(0, 100, 205));
        p.setColor(QPalette::HighlightedText, Qt::white);
    }

    QApplication::setPalette(p);
    for (QWidget *widget : QApplication::allWidgets()) {
        widget->setPalette(p);
        widget->update();
    }
}

void hexandtabler::on_actionZoomIn_triggered() {
    if (hexArea) hexArea->setFont(QFont(hexArea->font().family(), hexArea->font().pointSize() + 1));
}

void hexandtabler::on_actionZoomOut_triggered() {
    if (hexArea) hexArea->setFont(QFont(hexArea->font().family(), std::max(8, hexArea->font().pointSize() - 1)));
}

void hexandtabler::on_actionGoTo_triggered() {
    if (!hexArea) return;

    static QString lastOffset = "0";

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Go To Offset"));

    QLabel *label = new QLabel(tr("Enter offset in hexadecimal:"), &dialog);
    QLineEdit *lineEdit = new QLineEdit(&dialog);
    lineEdit->setText(lastOffset);
    lineEdit->selectAll(); 

    QPushButton *okBtn = new QPushButton(tr("OK"), &dialog);
    QPushButton *cancelBtn = new QPushButton(tr("Cancel"), &dialog);
    okBtn->setDefault(true);

    QHBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->addWidget(label);
    mainLayout->addWidget(lineEdit);
    mainLayout->addLayout(btnLayout);

    connect(okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(lineEdit, &QLineEdit::returnPressed, &dialog, &QDialog::accept);

    if (dialog.exec() != QDialog::Accepted) return;

    QString text = lineEdit->text().trimmed();
    if (text.isEmpty()) return;

    bool hexOk;
    quint64 offset = text.toULongLong(&hexOk, 16);

    if (hexOk) {
        lastOffset = text; 
        hexArea->goToOffset(offset);
    } else {
        QMessageBox::warning(this, tr("Invalid Input"), tr("The input is not a valid hexadecimal number."));
    }
}

void hexandtabler::pushUndoState() {
    if (!hexArea) return;

    QByteArray newData = hexArea->hexData();

    EditorState state;
    state.cursorPos      = hexArea->cursorPosition();
    state.selectionStart = hexArea->selectionStart();
    state.selectionEnd   = hexArea->selectionEnd();

    int minLen = qMin(m_fileData.size(), newData.size());
    for (int i = 0; i < minLen; ++i) {
        if ((quint8)m_fileData[i] != (quint8)newData[i]) {
            state.changes.append(ByteChange(i, (quint8)m_fileData[i], (quint8)newData[i]));
        }
    }
    for (int i = minLen; i < newData.size(); ++i) {
        state.changes.append(ByteChange(i, 0x00, (quint8)newData[i]));
    }

    if (state.changes.isEmpty()) return;

    m_undoStack.append(state);
    if (m_undoStack.size() > MAX_UNDO_STATES)
        m_undoStack.removeFirst();

    m_redoStack.clear();
    m_fileData = newData;
    updateUndoRedoActions();
}

void hexandtabler::clearUndoRedo() {
    m_undoStack.clear();
    m_redoStack.clear();
    updateUndoRedoActions();
}

void hexandtabler::updateUndoRedoActions() {
    if (ui->actionUndo) ui->actionUndo->setEnabled(!m_undoStack.isEmpty());
    if (ui->actionRedo) ui->actionRedo->setEnabled(!m_redoStack.isEmpty());
}

void hexandtabler::updateWindowTitle() {
    QString title = "hexandtabler";
    if (!m_currentFilePath.isEmpty()) {
        title += " - " + QFileInfo(m_currentFilePath).fileName();
    }
    if (m_isDirty) title += "*";
    setWindowTitle(title);
}

void hexandtabler::onSaveFinished() {
    m_isDirty = false;
    updateWindowTitle();
}

void hexandtabler::on_actionUndo_triggered() {
    if (!hexArea || m_undoStack.isEmpty()) return;

    EditorState state = m_undoStack.takeLast();
    m_redoStack.append(state);

    QByteArray data = hexArea->hexData();
    for (const ByteChange &c : state.changes) {
        if (c.offset < data.size())
            data[(int)c.offset] = (char)c.oldByte;
    }

    m_fileData = data;
    hexArea->setHexData(m_fileData);
    hexArea->setCursorPosition(state.cursorPos);
    hexArea->setSelection(state.selectionStart, state.selectionEnd);

    m_isDirty = true;
    updateUndoRedoActions();
}

void hexandtabler::on_actionRedo_triggered() {
    if (!hexArea || m_redoStack.isEmpty()) return;

    EditorState state = m_redoStack.takeLast();
    m_undoStack.append(state);

    QByteArray data = hexArea->hexData();
    for (const ByteChange &c : state.changes) {
        if (c.offset < data.size())
            data[(int)c.offset] = (char)c.newByte;
    }

    m_fileData = data;
    hexArea->setHexData(m_fileData);
    hexArea->setCursorPosition(state.cursorPos);
    hexArea->setSelection(state.selectionStart, state.selectionEnd);

    m_isDirty = true;
    updateUndoRedoActions();
}

std::vector<int16_t> hexandtabler::calculateRelativeOffsets(const QString &input) const {
    std::vector<int16_t> offsets;
    if (input.isEmpty()) return offsets;

    int base = input[0].unicode();
    for (int i = 0; i < input.length(); ++i) {
        offsets.push_back(static_cast<int16_t>(input[i].unicode() - base));
    }
    return offsets;
}

std::optional<qsizetype> hexandtabler::performRelativeSearchTask(const QByteArray data, 
                                                                const std::vector<int16_t> offsets, 
                                                                qsizetype startPos, 
                                                                bool backwards) {
    if (data.isEmpty() || offsets.empty()) return std::nullopt;

    auto checkMatch = [&](qsizetype pos) {
        if (pos + (qsizetype)offsets.size() > data.size()) return false;
        int baseValue = static_cast<uint8_t>(data[pos]);
        for (size_t j = 1; j < offsets.size(); ++j) {
            if (static_cast<uint8_t>(data[pos + j]) - baseValue != offsets[j]) return false;
        }
        return true;
    };

    if (backwards) {
        qsizetype start = std::min<qsizetype>(startPos, data.size() - offsets.size());
        for (qsizetype i = start; i >= 0; --i) {
            if (checkMatch(i)) return i;
        }
    } else {
        for (qsizetype i = startPos; i <= data.size() - (qsizetype)offsets.size(); ++i) {
            if (checkMatch(i)) return i;
        }
    }
    return std::nullopt;
}

void hexandtabler::on_actionSearchRelative_triggered() {
    if (m_findReplaceDialog) {
        m_findReplaceDialog->show();
        m_findReplaceDialog->raise();
        m_findReplaceDialog->activateWindow();
    }
}

void hexandtabler::findNextRelative(const QString &searchText, bool wrap, bool backwards, int tolMin, int tolMax) {
    if (!hexArea || searchText.isEmpty()) return;

    QByteArray dataCopy = hexArea->hexData();
    if (dataCopy.isEmpty()) return;

    std::vector<int16_t> offsets = calculateRelativeOffsets(searchText);
    if (offsets.empty()) return;

    m_lastRelSearchLen = static_cast<qsizetype>(offsets.size());

    if (m_findWatcher.isRunning()) {
        m_findWatcher.cancel();
        m_findWatcher.waitForFinished();
    }

    qsizetype startByte = hexArea->cursorPosition() / 2;

    m_findWatcher.setFuture(QtConcurrent::run(
        [dataCopy, offsets, startByte, backwards, wrap]() -> std::optional<qsizetype> {
            qsizetype dataSize = dataCopy.size();
            qsizetype patLen = static_cast<qsizetype>(offsets.size());
            if (patLen == 0 || dataSize < patLen) return std::nullopt;

            auto checkMatch = [&](qsizetype pos) -> bool {
                if (pos < 0 || pos + patLen > dataSize) return false;
                int baseValue = static_cast<uint8_t>(dataCopy[pos]);
                for (qsizetype j = 1; j < patLen; ++j) {
                    if (static_cast<uint8_t>(dataCopy[pos + j]) - baseValue != offsets[j])
                        return false;
                }
                return true;
            };

            if (!backwards) {
                qsizetype from = startByte + 1;
                for (qsizetype i = from; i <= dataSize - patLen; ++i) {
                    if (checkMatch(i)) return i;
                }
                if (wrap) {
                    for (qsizetype i = 0; i < from && i <= dataSize - patLen; ++i) {
                        if (checkMatch(i)) return i;
                    }
                }
            } else {
                qsizetype from = startByte - 1;
                for (qsizetype i = from; i >= 0; --i) {
                    if (checkMatch(i)) return i;
                }
                if (wrap) {
                    for (qsizetype i = dataSize - patLen; i > from; --i) {
                        if (checkMatch(i)) return i;
                    }
                }
            }
            return std::nullopt;
        }
    ));
}

void hexandtabler::findNextRelative(const QString &searchText, bool backwards) {
    findNextRelative(searchText, true, backwards, 0, 0);
}

void hexandtabler::findNextRelative16(const QString &searchText, bool wrap, bool backwards, bool bigEndian) {
    if (!hexArea || searchText.isEmpty()) return;

    QByteArray dataCopy = hexArea->hexData();
    if (dataCopy.isEmpty()) return;

    // Build 16-bit offsets from unicode codepoints of the input text
    std::vector<int32_t> offsets16;
    offsets16.reserve(searchText.length());
    int base16 = searchText[0].unicode();
    for (int i = 0; i < searchText.length(); ++i) {
        offsets16.push_back(searchText[i].unicode() - base16);
    }

    if (offsets16.empty()) return;

    m_lastRelSearchLen = static_cast<qsizetype>(offsets16.size()) * 2; // 2 bytes per 16-bit value

    if (m_findWatcher.isRunning()) {
        m_findWatcher.cancel();
        m_findWatcher.waitForFinished();
    }

    qsizetype startByte = hexArea->cursorPosition() / 2;

    m_findWatcher.setFuture(QtConcurrent::run(
        [dataCopy, offsets16, startByte, backwards, wrap, bigEndian]() -> std::optional<qsizetype> {
            qsizetype dataSize = dataCopy.size();
            qsizetype patLen = static_cast<qsizetype>(offsets16.size());
            qsizetype bytePatLen = patLen * 2;
            if (patLen == 0 || dataSize < bytePatLen) return std::nullopt;

            auto readU16 = [&](qsizetype pos) -> uint16_t {
                uint8_t b0 = static_cast<uint8_t>(dataCopy[pos]);
                uint8_t b1 = static_cast<uint8_t>(dataCopy[pos + 1]);
                return bigEndian ? (uint16_t)((b0 << 8) | b1)
                                 : (uint16_t)((b1 << 8) | b0);
            };

            auto checkMatch = [&](qsizetype pos) -> bool {
                if (pos < 0 || pos + bytePatLen > dataSize) return false;
                int32_t baseVal = readU16(pos);
                for (qsizetype j = 1; j < patLen; ++j) {
                    int32_t val = readU16(pos + j * 2);
                    if (val - baseVal != offsets16[j]) return false;
                }
                return true;
            };

            // Align to 2-byte boundary relative to startByte
            qsizetype alignedStart = startByte & ~(qsizetype)1;

            if (!backwards) {
                qsizetype from = alignedStart + 2;
                for (qsizetype i = from; i <= dataSize - bytePatLen; i += 2) {
                    if (checkMatch(i)) return i;
                }
                if (wrap) {
                    for (qsizetype i = 0; i < from && i <= dataSize - bytePatLen; i += 2) {
                        if (checkMatch(i)) return i;
                    }
                }
            } else {
                qsizetype from = alignedStart - 2;
                for (qsizetype i = from; i >= 0; i -= 2) {
                    if (checkMatch(i)) return i;
                }
                if (wrap) {
                    qsizetype maxPos = ((dataSize - bytePatLen) / 2) * 2;
                    for (qsizetype i = maxPos; i > from; i -= 2) {
                        if (checkMatch(i)) return i;
                    }
                }
            }
            return std::nullopt;
        }
    ));
}

void hexandtabler::onFindFinished() {
    unsetCursor();
    auto result = m_findWatcher.result();
    if (result) {
        qsizetype bytePos = *result;
        hexArea->goToOffset(bytePos);
        hexArea->setSelection(bytePos * 2, (bytePos + m_lastRelSearchLen) * 2);
    } else {
        QMessageBox::information(this, tr("Relative Search"), tr("Pattern not found."));
    }
}

void hexandtabler::on_actionFind_triggered() {
    if (!m_findReplaceDialog) return;
    m_findReplaceDialog->setFindMode();
    m_findReplaceDialog->show();
    m_findReplaceDialog->activateWindow();
}

void hexandtabler::on_actionReplace_triggered() {
    if (!m_findReplaceDialog) return;
    m_findReplaceDialog->setReplaceMode();
    m_findReplaceDialog->show();
    m_findReplaceDialog->activateWindow();
}

void hexandtabler::findNext(const QByteArray &needle, bool caseSensitive, bool wrap, bool backwards) {
    if (!hexArea || needle.isEmpty()) return;
    
    QByteArray data = hexArea->hexData();
    qint64 dataSize = data.size();
    qint64 needleSize = needle.size();
    
    qint64 currentBytePos = hexArea->cursorPosition() / 2; 

    QByteArray searchData = caseSensitive ? data : data.toLower();
    QByteArray searchNeedle = caseSensitive ? needle : needle.toLower();

    qint64 foundPos = -1;
    
    if (!backwards) {
        qint64 searchStart;
        
        if (hexArea->selectionEnd() != -1) { 
            searchStart = hexArea->selectionEnd() / 2; 
        } 
        else if (currentBytePos < dataSize && searchData.mid(currentBytePos, needleSize) == searchNeedle) {
            searchStart = currentBytePos + 1;
        } else {
            searchStart = currentBytePos;
        }

        searchStart = std::min(searchStart, dataSize); 

        foundPos = searchData.indexOf(searchNeedle, searchStart);
        
        if (foundPos == -1 && wrap) {
            foundPos = searchData.indexOf(searchNeedle, 0);
            
            if (foundPos != -1 && foundPos >= searchStart) {
                foundPos = -1; 
            }
        }
        
    } else {
        qint64 searchEnd;
        
        if (hexArea->selectionStart() != -1) { 
            searchEnd = hexArea->selectionStart() / 2 - 1; 
        }
        else {
             searchEnd = currentBytePos - 1;
             
             if (currentBytePos >= needleSize && searchData.mid(currentBytePos - needleSize, needleSize) == searchNeedle) {
                searchEnd = currentBytePos - needleSize - 1;
             }
        }
        
        searchEnd = std::max((qint64)0, searchEnd); 

        foundPos = searchData.lastIndexOf(searchNeedle, searchEnd);
        
        if (foundPos == -1 && wrap) {
            foundPos = searchData.lastIndexOf(searchNeedle, dataSize - 1);
            
            if (foundPos != -1 && foundPos <= searchEnd) {
                foundPos = -1;
            }
        }
    }

    if (foundPos != -1) {
        hexArea->goToOffset(foundPos);
        hexArea->setSelection(foundPos * 2, (foundPos + needleSize) * 2);
    } else {
        QMessageBox::information(this, tr("Find Result"), tr("Search pattern not found."));
    }
}

void hexandtabler::replaceOne() {
    if (!hexArea || !m_findReplaceDialog) return;
    
    if (m_findReplaceDialog->searchType() == FindReplaceDialog::RelativeSearch ||
        m_findReplaceDialog->searchType() == FindReplaceDialog::Relative16LESearch ||
        m_findReplaceDialog->searchType() == FindReplaceDialog::Relative16BESearch) {
        QMessageBox::warning(this, tr("Replace Error"), tr("Replace function is not available for Relative Search mode."));
        return;
    }

    QByteArray needle = convertSearchString(m_findReplaceDialog->findText(), m_findReplaceDialog->searchType());
    QByteArray replacement = convertSearchString(m_findReplaceDialog->replaceText(), m_findReplaceDialog->searchType());

    if (needle.isEmpty()) {
        QMessageBox::warning(this, tr("Replace Error"), tr("Invalid search pattern."));
        return;
    }
    
    bool caseSensitive = m_findReplaceDialog->isCaseSensitive();
    bool wrap = m_findReplaceDialog->isWrapped();

    qint64 currentBytePos = (hexArea->selectionStart() != -1) 
                            ? (hexArea->selectionStart() / 2) 
                            : (hexArea->cursorPosition() / 2); 
    
    QByteArray data = hexArea->hexData();

    QByteArray currentDataCheck = caseSensitive ? data.mid(currentBytePos, needle.size()) : data.mid(currentBytePos, needle.size()).toLower();
    QByteArray searchNeedle = caseSensitive ? needle : needle.toLower();

    bool replaced = false;
    if (currentDataCheck == searchNeedle) {
        data.replace(currentBytePos, needle.size(), replacement);
        hexArea->setHexData(data);
        pushUndoState();
        
        hexArea->goToOffset(currentBytePos + replacement.size());
        hexArea->setSelection(-1, -1); 
        
        replaced = true;
    }

    qint64 searchStart = replaced ? currentBytePos + replacement.size() : currentBytePos;
    if (!replaced) {
        if (currentDataCheck == searchNeedle) {
             searchStart = currentBytePos + 1;
        } else {
             searchStart = hexArea->cursorPosition() / 2;
        }
    }
    
    QByteArray searchData = caseSensitive ? hexArea->hexData() : hexArea->hexData().toLower();
    qint64 foundPos = searchData.indexOf(searchNeedle, searchStart);
    
    if (foundPos != -1) {
        hexArea->goToOffset(foundPos);
        hexArea->setSelection(foundPos * 2, (foundPos + needle.size()) * 2);
    } else if (wrap) {
        foundPos = searchData.indexOf(searchNeedle, 0);
        if (foundPos != -1 && foundPos < searchStart) {
             hexArea->goToOffset(foundPos);
             hexArea->setSelection(foundPos * 2, (foundPos + needle.size()) * 2);
        } else {
            QMessageBox::information(this, tr("Replace Result"), tr("No more matches found."));
        }
    } else {
        QMessageBox::information(this, tr("Replace Result"), tr("No more matches found."));
    }
}

void hexandtabler::replaceAll(const QByteArray &needle, const QByteArray &replacement) {
    if (!hexArea || needle.isEmpty()) return;
    
    if (m_findReplaceDialog->searchType() == FindReplaceDialog::RelativeSearch ||
        m_findReplaceDialog->searchType() == FindReplaceDialog::Relative16LESearch ||
        m_findReplaceDialog->searchType() == FindReplaceDialog::Relative16BESearch) {
        QMessageBox::warning(this, tr("Replace Error"), tr("Replace All function is not available for Relative Search mode."));
        return;
    }

    QByteArray data = hexArea->hexData();
    QByteArray searchNeedle = m_findReplaceDialog->isCaseSensitive() ? needle : needle.toLower();
    
    if (!m_findReplaceDialog->isCaseSensitive()) {
        int count = 0;
        QByteArray temp_data = data;
        
        QByteArray lower_data = data.toLower(); 
        qint64 pos = lower_data.indexOf(searchNeedle, 0);
        
        qint64 offset = 0; 
        
        while (pos != -1) {
            temp_data.replace(pos + offset, needle.size(), replacement);
            
            offset += (replacement.size() - needle.size()); 
            
            pos = lower_data.indexOf(searchNeedle, pos + replacement.size() + offset);
            
            count++;
        }
        
        data = temp_data;
        
        if (count > 0) {
            QMessageBox::information(this, tr("Replace All"), tr("Replaced %n occurrence(s).", "", count));
        } else {
            QMessageBox::information(this, tr("Replace All"), tr("No occurrences found."));
        }
        
    } else {
        int count = data.count(needle);
        
        if (count > 0) {
            data.replace(needle, replacement);
            QMessageBox::information(this, tr("Replace All"), tr("Replaced %n occurrence(s).", "", count));
        } else {
            QMessageBox::information(this, tr("Replace All"), tr("No occurrences found."));
        }
    }

    hexArea->setHexData(data);
    pushUndoState();
    hexArea->goToOffset(0); 
    hexArea->setSelection(-1, -1);
}

void hexandtabler::on_actionCopy_triggered()
{
    if (hexArea) {
        hexArea->copySelection();
    }
}

void hexandtabler::on_actionCopyAddress_triggered()
{
    if (!hexArea) return;
    qint64 byteOffset = hexArea->cursorPosition() / 2;
    QString hex = QString("%1").arg(byteOffset, 8, 16, QChar('0')).toUpper();
    QApplication::clipboard()->setText(hex);
}

void hexandtabler::on_actionPaste_triggered()
{
    if (hexArea) {
        hexArea->pasteFromClipboard();
    }
}

void hexandtabler::propagateCharMaps() {
    if (!hexArea) return;
    hexArea->setCharMapping(m_charMap);
    hexArea->setCharMapping16(m_charMap16, m_table16BigEndian);
}

void hexandtabler::setupConversionTable16() {
    // 16-bit entries are appended as extra rows to m_tableWidget (rows >= 256).
    // No extra column needed; the Hex column shows LE or BE depending on m_table16BigEndian.
}

void hexandtabler::applyCharMap16ToWidget() {
    if (!m_tableWidget) return;
    QSignalBlocker blocker(m_tableWidget);

    // Remove all 16-bit rows (row >= 256) and rebuild
    m_tableWidget->setRowCount(256);

    for (auto it = m_charMap16.constBegin(); it != m_charMap16.constEnd(); ++it) {
        uint16_t key = it.key();  // key is always stored as LE internally
        QString ch   = it.value();
        uint8_t lo = (uint8_t)(key & 0xFF);
        uint8_t hi = (uint8_t)(key >> 8);

        int row = m_tableWidget->rowCount();
        m_tableWidget->insertRow(row);

        // Col 0: hex display — LE or BE depending on endian flag; stores key in UserRole
        QString hexStr = m_table16BigEndian
            ? QString("%1%2").arg(hi, 2, 16, QChar('0')).arg(lo, 2, 16, QChar('0')).toUpper()
            : QString("%1%2").arg(lo, 2, 16, QChar('0')).arg(hi, 2, 16, QChar('0')).toUpper();
        QTableWidgetItem *hexItem = new QTableWidgetItem(hexStr);
        hexItem->setData(Qt::UserRole, (uint)key);
        hexItem->setFlags(hexItem->flags() & ~Qt::ItemIsEditable);
        hexItem->setBackground(QColor(60, 80, 120, 80));
        m_tableWidget->setItem(row, 0, hexItem);

        // Col 1: assigned character (editable); stores key in UserRole+1
        QTableWidgetItem *charItem = new QTableWidgetItem(ch);
        charItem->setData(Qt::UserRole + 1, (uint)key);
        charItem->setBackground(QColor(60, 80, 120, 80));
        m_tableWidget->setItem(row, 1, charItem);
    }
}

void hexandtabler::clearCharMappingTable16() {
    if (!m_tableWidget) return;
    QSignalBlocker blocker(m_tableWidget);
    m_charMap16.clear();
    // Remove all rows beyond the 256 8-bit rows
    m_tableWidget->setRowCount(256);
    propagateCharMaps();
}

void hexandtabler::on_actionChangeEndian16_triggered(bool checked) {
    m_table16BigEndian = checked;
    applyCharMap16ToWidget();
    propagateCharMaps();
}

void hexandtabler::on_actionAddRange16_triggered() {
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Add 16-bit Range"));

    // ── Instrucción ──────────────────────────────────────────────────────────
    QLabel *helpLabel = new QLabel(
        tr("<b>Enter bytes as they appear in the file.</b><br>"
           "Example LE: first byte <tt>A0</tt>, second byte <tt>01</tt> → write <tt>A001</tt>.<br>"
           "Use <i>Vary first byte</i> when only the first byte of the pair changes<br>"
           "(e.g. <tt>A001</tt> – <tt>A401</tt> → <tt>A001 A101 A201 A301 A401</tt>)."),
        &dlg);
    helpLabel->setWordWrap(true);

    // ── Campos ───────────────────────────────────────────────────────────────
    QFormLayout *form = new QFormLayout;

    QLineEdit *startEdit = new QLineEdit("A001");
    QLineEdit *endEdit   = new QLineEdit;
    endEdit->setPlaceholderText(tr("can be empty (single entry)"));

    QCheckBox *bigEndianCheck   = new QCheckBox(tr("Big Endian"));
    bigEndianCheck->setChecked(false);

    QCheckBox *varyFirstByteCheck = new QCheckBox(
        tr("Vary first byte only (second byte stays fixed)"));
    varyFirstByteCheck->setChecked(false);

    form->addRow(tr("Start (hex, 4 digits):"), startEdit);
    form->addRow(tr("End   (hex, 4 digits):"), endEdit);
    form->addRow(bigEndianCheck);
    form->addRow(varyFirstByteCheck);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dlg);
    mainLayout->addWidget(helpLabel);
    mainLayout->addLayout(form);
    mainLayout->addWidget(bb);

    QTimer::singleShot(0, startEdit, [startEdit](){
        startEdit->selectAll();
        startEdit->setFocus();
    });

    if (dlg.exec() != QDialog::Accepted) return;

    // ── Parseo ───────────────────────────────────────────────────────────────
    // Los 4 dígitos hex se interpretan como los dos bytes en el orden del archivo:
    //   startEdit = "XXYY"  →  primer_byte=0xXX, segundo_byte=0xYY
    bool ok1;
    uint startVal = startEdit->text().trimmed().toUInt(&ok1, 16);
    QString endText = endEdit->text().trimmed();
    bool ok2 = true;
    uint endVal = endText.isEmpty() ? startVal : endText.toUInt(&ok2, 16);

    if (!ok1 || !ok2 || endVal > 0xFFFF) {
        QMessageBox::warning(this, tr("Add Range"),
            tr("Invalid input. Use 4-digit hex values (0000–FFFF)."));
        return;
    }

    bool bigEndian     = bigEndianCheck->isChecked();
    bool varyFirstByte = varyFirstByteCheck->isChecked();

    // ── Función auxiliar: convierte par de bytes (en orden de archivo)
    //    a la clave interna (siempre LE: byte0 en bits bajos).
    //    byte0 = primer byte en el archivo, byte1 = segundo byte en el archivo.
    auto makeKey = [](uint8_t byte0, uint8_t byte1) -> uint16_t {
        // La clave interna almacena el PRIMER byte del archivo en los bits BAJOS.
        return (uint16_t)((uint16_t)byte1 << 8 | byte0);
    };

    QSignalBlocker bl(m_tableWidget);

    if (varyFirstByte) {
        // Modo "solo varía el primer byte":
        //   startVal = 0xXXYY → primer_byte_inicio = 0xXX, segundo_byte_fijo = 0xYY
        //   endVal   = 0xZZYY (el segundo byte debe coincidir con el de start)
        uint8_t byte0_start = (uint8_t)((startVal >> 8) & 0xFF);
        uint8_t byte0_end   = (uint8_t)((endVal   >> 8) & 0xFF);
        uint8_t byte1_fixed = (uint8_t)(startVal & 0xFF);

        if ((endVal & 0xFF) != byte1_fixed) {
            QMessageBox::warning(this, tr("Add Range"),
                tr("In 'Vary first byte' mode, the second byte of Start and End must match.\n"
                   "Example: Start=A001, End=A401 (both end in 01)."));
            return;
        }
        if (byte0_start > byte0_end) {
            QMessageBox::warning(this, tr("Add Range"),
                tr("Start first byte must be ≤ End first byte."));
            return;
        }

        for (uint8_t b0 = byte0_start; b0 <= byte0_end; ++b0) {
            uint16_t key = makeKey(b0, byte1_fixed);
            if (!m_charMap16.contains(key))
                m_charMap16[key] = ".";
        }
    } else {
        // Modo normal: recorre el rango numerico de pares de bytes.
        // El valor numérico del par se incrementa tratando los dos bytes
        // como un número de 16 bits donde el PRIMER byte es el más significativo
        // (es decir, "A001" = 0xA001 numéricamente).
        // El endian solo afecta cómo se interpreta el par en el archivo al hacer
        // matching; la clave interna siempre tiene byte0_archivo en bits bajos.
        if (startVal > endVal) {
            QMessageBox::warning(this, tr("Add Range"),
                tr("Start must be ≤ End."));
            return;
        }

        for (uint v = startVal; v <= endVal; ++v) {
            uint8_t byte0, byte1; // bytes en orden de archivo
            if (bigEndian) {
                // En BE el primer byte en archivo es el más significativo del valor
                byte0 = (uint8_t)((v >> 8) & 0xFF);
                byte1 = (uint8_t)(v & 0xFF);
            } else {
                // En LE el primer byte en archivo es el menos significativo del valor
                // El usuario escribe los bytes en orden de archivo, así que
                // "A001" significa byte0=0xA0, byte1=0x01 en el archivo.
                byte0 = (uint8_t)((v >> 8) & 0xFF);
                byte1 = (uint8_t)(v & 0xFF);
            }
            uint16_t key = makeKey(byte0, byte1);
            if (!m_charMap16.contains(key))
                m_charMap16[key] = ".";
        }
    }

    applyCharMap16ToWidget();
    propagateCharMaps();
}

void hexandtabler::on_actionRemoveRange16_triggered() {
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Remove 16-bit Range"));

    QLabel *helpLabel = new QLabel(
        tr("<b>Enter bytes as they appear in the file.</b><br>"
           "Use <i>Vary first byte</i> to remove entries where only the first byte changes."),
        &dlg);
    helpLabel->setWordWrap(true);

    QFormLayout *form = new QFormLayout;

    QLineEdit *startEdit = new QLineEdit("A001");
    QLineEdit *endEdit   = new QLineEdit("A401");
    QCheckBox *bigEndianCheck   = new QCheckBox(tr("Big Endian"));
    bigEndianCheck->setChecked(false);
    QCheckBox *varyFirstByteCheck = new QCheckBox(
        tr("Vary first byte only (second byte stays fixed)"));
    varyFirstByteCheck->setChecked(false);

    form->addRow(tr("Start (hex, 4 digits):"), startEdit);
    form->addRow(tr("End   (hex, 4 digits):"), endEdit);
    form->addRow(bigEndianCheck);
    form->addRow(varyFirstByteCheck);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dlg);
    mainLayout->addWidget(helpLabel);
    mainLayout->addLayout(form);
    mainLayout->addWidget(bb);

    QTimer::singleShot(0, startEdit, [startEdit](){
        startEdit->selectAll();
        startEdit->setFocus();
    });

    if (dlg.exec() != QDialog::Accepted) return;

    bool ok1, ok2;
    uint startVal = startEdit->text().trimmed().toUInt(&ok1, 16);
    uint endVal   = endEdit->text().trimmed().toUInt(&ok2, 16);

    if (!ok1 || !ok2 || endVal > 0xFFFF) {
        QMessageBox::warning(this, tr("Remove Range"),
            tr("Invalid input. Use 4-digit hex values (0000–FFFF)."));
        return;
    }

    bool varyFirstByte = varyFirstByteCheck->isChecked();

    // Misma convención que AddRange16: byte0 = primer byte en archivo (bits bajos de key)
    auto makeKey = [](uint8_t byte0, uint8_t byte1) -> uint16_t {
        return (uint16_t)((uint16_t)byte1 << 8 | byte0);
    };

    QSignalBlocker bl(m_tableWidget);

    if (varyFirstByte) {
        uint8_t byte0_start = (uint8_t)((startVal >> 8) & 0xFF);
        uint8_t byte0_end   = (uint8_t)((endVal   >> 8) & 0xFF);
        uint8_t byte1_fixed = (uint8_t)(startVal & 0xFF);

        if ((endVal & 0xFF) != byte1_fixed) {
            QMessageBox::warning(this, tr("Remove Range"),
                tr("In 'Vary first byte' mode, the second byte of Start and End must match."));
            return;
        }
        if (byte0_start > byte0_end) {
            QMessageBox::warning(this, tr("Remove Range"),
                tr("Start first byte must be ≤ End first byte."));
            return;
        }

        for (uint8_t b0 = byte0_start; b0 <= byte0_end; ++b0)
            m_charMap16.remove(makeKey(b0, byte1_fixed));

    } else {
        if (startVal > endVal) {
            QMessageBox::warning(this, tr("Remove Range"), tr("Start must be ≤ End."));
            return;
        }
        for (uint v = startVal; v <= endVal; ++v) {
            uint8_t byte0 = (uint8_t)((v >> 8) & 0xFF);
            uint8_t byte1 = (uint8_t)(v & 0xFF);
            m_charMap16.remove(makeKey(byte0, byte1));
        }
    }

    applyCharMap16ToWidget();
    propagateCharMaps();
}

void hexandtabler::setupConversionTable() {
    if (!m_tableWidget) return;
    
    m_tableWidget->setRowCount(256); 
    m_tableWidget->setColumnCount(2); 
    m_tableWidget->setHorizontalHeaderLabels({tr("Hex"), tr("Assigned")});
    m_tableWidget->setHorizontalHeaderLabels({tr("Hex"), tr("Assigned")});
    m_tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tableWidget->horizontalHeader()->setHighlightSections(false);
    m_tableWidget->verticalHeader()->setVisible(false);
    m_tableWidget->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::AnyKeyPressed);
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setFont(QFont("Monospace", 10)); 

    for (int i = 0; i < 256; ++i) {
        QTableWidgetItem *hexItem = new QTableWidgetItem(QString("%1").arg(i, 2, 16, QChar('0')).toUpper());
        hexItem->setFlags(hexItem->flags() & ~Qt::ItemIsEditable);
        m_tableWidget->setItem(i, 0, hexItem);
        
        QString defaultChar;
        QChar c = QChar(i);
        
        if (!c.isPrint()) { 
            defaultChar = ".";
        } else {
            defaultChar = QString(c);
        }
        
        QTableWidgetItem *charItem = new QTableWidgetItem(defaultChar);
        m_tableWidget->setItem(i, 1, charItem);
        
        m_charMap[i] = defaultChar;
    }
}

void hexandtabler::on_actionToggleTable_triggered(bool checked) {
    if (m_tableDock) {
        m_tableDock->setVisible(checked);
    }
}

void hexandtabler::on_actionLoadTable_triggered() {
    QString fileName = QFileDialog::getOpenFileName(this, tr("Load Conversion Table"), m_currentTablePath.isEmpty() ? QDir::homePath() : QFileInfo(m_currentTablePath).absoluteDir().path(), tr("Table Files (*.tbl);;All Files (*.*)"));
    if (fileName.isEmpty()) {
        return;
    }

    if (loadTableFile(fileName)) {
        m_currentTablePath = fileName;
    } else {
        QMessageBox::critical(this, tr("Error"), tr("Failed to load conversion table from %1.").arg(QFileInfo(fileName).fileName()));
    }
}

void hexandtabler::on_actionSaveTable_triggered() {
    if (m_currentTablePath.isEmpty()) {
        on_actionSaveTableAs_triggered();
    } else {
        saveTableFile(m_currentTablePath);
    }
}

void hexandtabler::on_actionSaveTableAs_triggered() {
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save Conversion Table As"), m_currentTablePath.isEmpty() ? QDir::homePath() : QFileInfo(m_currentTablePath).absoluteDir().path(), tr("Table Files (*.tbl);;All Files (*.*)"));
    if (fileName.isEmpty()) {
        return; 
    }
    
    if (!fileName.toLower().endsWith(".tbl")) {
        fileName += ".tbl";
    }

    if (saveTableFile(fileName)) {
        m_currentTablePath = fileName;
    }
}

bool hexandtabler::saveTableFile(const QString &filePath) {
    if (!m_tableWidget) return false;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Error"), tr("Could not write table file %1:\n%2.").arg(filePath).arg(file.errorString()));
        return false;
    }

    QTextStream out(&file);

    for (int i = 0; i < 256; ++i) {
        out << QString("%1=%2\n").arg(i, 2, 16, QChar('0')).toUpper().arg(m_charMap[i]);
    }

    for (auto it = m_charMap16.constBegin(); it != m_charMap16.constEnd(); ++it) {
        uint16_t key = it.key();
        uint8_t lo = (uint8_t)(key & 0xFF);
        uint8_t hi = (uint8_t)(key >> 8);
        out << QString("%1%2=%3\n")
               .arg(lo, 2, 16, QChar('0')).toUpper()
               .arg(hi, 2, 16, QChar('0')).toUpper()
               .arg(it.value());
    }

    file.close();
    return true;
}

bool hexandtabler::loadTableFile(const QString &filePath) {
    if (!m_tableWidget) return false;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream in(&file);
    
    QString newMap8[256];
    for (int i = 0; i < 256; ++i) newMap8[i] = m_charMap[i];

    QMap<uint16_t, QString> newMap16 = m_charMap16;
    
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.trimmed().isEmpty() || line.trimmed().startsWith("#")) continue;

        int sepIndex = line.indexOf('=');
        if (sepIndex == -1) continue;

        QString hexCode = line.left(sepIndex).trimmed();
        QString charStr = line.mid(sepIndex + 1);

        while (charStr.endsWith('\n') || charStr.endsWith('\r'))
            charStr.chop(1);

        QString displayChar = charStr.left(1);
        if (displayChar.isEmpty()) displayChar = ".";

        bool ok;
        if (hexCode.length() == 4) {

            uint8_t b0 = (uint8_t)(hexCode.left(2).toUInt(&ok, 16));
            if (!ok) continue;
            uint8_t b1 = (uint8_t)(hexCode.right(2).toUInt(&ok, 16));
            if (!ok) continue;
            uint16_t key = (uint16_t)((uint16_t)b1 << 8 | b0);
            newMap16[key] = displayChar;
        } else if (hexCode.length() <= 2) {

            int byteValue = hexCode.toInt(&ok, 16);
            if (ok && byteValue >= 0 && byteValue <= 255) {
                newMap8[byteValue] = displayChar;
            }
        }

    }
    file.close();

    QSignalBlocker blocker8(m_tableWidget);
    for (int i = 0; i < 256; ++i) {
        m_charMap[i] = newMap8[i];
        QTableWidgetItem *item = m_tableWidget->item(i, 1);
        if (item) item->setText(m_charMap[i]);
    }

    m_charMap16 = newMap16;
    applyCharMap16ToWidget();

    propagateCharMaps();
    return true;
}

void hexandtabler::clearCharMappingTable() {
    if (!m_tableWidget) return; 

    QSignalBlocker blocker(m_tableWidget); 
    
    for (int i = 0; i < 256; ++i) {
        m_charMap[i] = "."; 
        
        QTableWidgetItem *item = m_tableWidget->item(i, 1); 
        if (item) {
            item->setText(".");
        }
    }
    
    if (hexArea) { 
        propagateCharMaps(); 
    }
    m_isDirty = true;
}

void hexandtabler::on_actionClearTable_triggered() {
    clearCharMappingTable();
    clearCharMappingTable16();
}

void hexandtabler::insertSeries(const QList<QString> &series) {
    if (!m_tableWidget || series.isEmpty()) return;

    QModelIndexList selectedIndexes = m_tableWidget->selectionModel()->selectedIndexes();
    int startRow = 0;
    if (!selectedIndexes.isEmpty()) {
        startRow = selectedIndexes.at(0).row();
        for (const QModelIndex &index : selectedIndexes) {
            if (index.row() < startRow)
                startRow = index.row();
        }
    }

    QSignalBlocker blocker(m_tableWidget);

    for (int i = 0; i < series.size(); ++i) {
        int currentRow = startRow + i;
        if (currentRow < 0) continue;

        QString character = series.at(i).left(1);
        if (character.isEmpty()) character = ".";

        QTableWidgetItem *item = m_tableWidget->item(currentRow, 1);
        if (!item) continue;
        item->setText(character);

        if (currentRow < 256) {
            // 8-bit entry
            m_charMap[currentRow] = character;
        } else {
            // 16-bit entry: key stored in UserRole+1 of col 1
            bool ok;
            uint16_t key = (uint16_t)item->data(Qt::UserRole + 1).toUInt(&ok);
            if (ok)
                m_charMap16[key] = character;
        }
    }

    if (hexArea)
        propagateCharMaps();
}

void hexandtabler::on_actionInsertLatinUpper_triggered() {
    QList<QString> series;
    for (int i = 0; i < 26; ++i) {
        series.append(QString(QChar('A' + i)));
    }
    insertSeries(series);
}

void hexandtabler::on_actionInsertLatinLower_triggered() {
    QList<QString> series;
    for (int i = 0; i < 26; ++i) {
        series.append(QString(QChar('a' + i)));
    }
    insertSeries(series);
}

void hexandtabler::on_actionInsertHiragana_triggered()
{
    QStringList hiraganaBasic = {
        "あ", "い", "う", "え", "お",
        "か", "き", "く", "け", "こ",
        "さ", "し", "す", "せ", "そ",
        "た", "ち", "つ", "て", "と",
        "な", "に", "ぬ", "ね", "の",
        "は", "ひ", "ふ", "へ", "ほ",
        "ま", "み", "む", "め", "も",
        "や", "ゆ", "よ",
        "ら", "り", "る", "れ", "ろ",
        "わ", "を", "ん"
    };
    insertSeries(hiraganaBasic);
}

void hexandtabler::on_actionInsertKatakana_triggered()
{
    QStringList katakanaBasic = {
        "ア", "イ", "ウ", "エ", "オ",
        "カ", "キ", "ク", "ケ", "コ",
        "サ", "シ", "ス", "セ", "ソ",
        "タ", "チ", "ツ", "テ", "ト",
        "ナ", "ニ", "ヌ", "ネ", "ノ",
        "ハ", "ヒ", "フ", "ヘ", "ホ",
        "マ", "ミ", "ム", "メ", "モ",
        "ヤ", "ユ", "ヨ",
        "ラ", "リ", "ル", "レ", "ロ",
        "ワ", "ヲ", "ン"
    };
    insertSeries(katakanaBasic);
}

void hexandtabler::on_actionInsertCyrillic_triggered() {
    QList<QString> series;
    int startCode = 0x0410; 
    int endCode = 0x042F;   
    
    for (int i = startCode; i <= endCode; ++i) {
        series.append(QString(QChar(i)));
    }
    
    insertSeries(series);
}

void hexandtabler::on_actionInsertNumbers19_triggered() {
    QList<QString> series;
    for (int i = 1; i <= 9; ++i) {
        series.append(QString::number(i));
    }
    insertSeries(series);
}

void hexandtabler::handleByteEdited(qint64 offset, quint8 oldByte, quint8 newByte) {
    if (offset < m_fileData.size()) {
        m_fileData[(int)offset] = (char)newByte;
    }

    EditorState state;
    state.cursorPos      = hexArea->cursorPosition();
    state.selectionStart = hexArea->selectionStart();
    state.selectionEnd   = hexArea->selectionEnd();
    state.changes.append(ByteChange(offset, oldByte, newByte));

    m_undoStack.append(state);
    if (m_undoStack.size() > MAX_UNDO_STATES)
        m_undoStack.removeFirst();
    m_redoStack.clear();

    m_isDirty = true;
    updateUndoRedoActions();
}

void hexandtabler::handleTableItemChanged(QTableWidgetItem *item) {
    if (!m_tableWidget || !item || item->column() != 1) {
        return;
    }

    int row = item->row();
    QString newChar = item->text();
    if (newChar.isEmpty()) newChar = ".";
    QString finalChar = newChar.left(1);

    QSignalBlocker blocker(m_tableWidget);
    if (item->text() != finalChar)
        item->setText(finalChar);

    if (row < 256) {
        // 8-bit entry
        m_charMap[row] = finalChar;
    } else {
        // 16-bit entry: key stored in UserRole+1
        bool ok;
        uint16_t key = (uint16_t)item->data(Qt::UserRole + 1).toUInt(&ok);
        if (ok)
            m_charMap16[key] = finalChar;
    }

    if (hexArea) {
        propagateCharMaps();
    }
}

void hexandtabler::openRecentFile() {
    QAction *action = qobject_cast<QAction *>(sender());
    if (action) {
        if (maybeSave())
            loadFile(action->data().toString());
    }
}

void hexandtabler::on_actionClearRecentFiles_triggered() {
    QSettings settings(organizationName, applicationName);
    settings.remove("recentFiles");
    updateRecentFileActions();
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
    
    QStringList existingFiles;
    for (const QString &filePath : std::as_const(files)) {
        if (QFile::exists(filePath)) {
            existingFiles.append(filePath);
        }
    }

    if (existingFiles != files) {
        settings.setValue("recentFiles", existingFiles);
    }

    updateRecentFileActions();
}

void hexandtabler::updateRecentFileActions() {
    QSettings settings(organizationName, applicationName);
    QStringList files = settings.value("recentFiles").toStringList();

    int numRecentFiles = std::min<int>(static_cast<int>(files.size()), static_cast<int>(MaxRecentFiles));

    // Crear el separador fijo la primera vez
    if (!m_recentSeparator) {
        m_recentSeparator = ui->menuFile->insertSeparator(ui->actionExit);
    }

    m_recentSeparator->setVisible(numRecentFiles > 0);

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
        ui->menuFile->insertAction(m_recentSeparator, recentFileActions[i]);
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

void hexandtabler::refreshModelFromArea() {
}

QMap<QChar, QList<int>> hexandtabler::calculatePattern(const QString &text) const {
    QMap<QChar, QList<int>> pattern;
    for (int i = 0; i < text.length(); ++i) {
        QChar c = text.at(i);
        if (c.isLetterOrNumber()) {
            pattern[c].append(i);
        }
    }
    return pattern;
}

QList<QMap<QChar, quint8>> hexandtabler::guessEncoding(const QList<KnownPhrase> &phrases,
                                                     quint64 startOffset, 
                                                     quint64 endOffset) {

    if (m_fileData.isEmpty()) return QList<QMap<QChar, quint8>>();

    QList<QMap<QChar, quint8>> possibleMappings;
    const QByteArray data = m_fileData; 
    

    qint64 dataSize = data.size(); 
    qint64 start = (qint64)startOffset;
    qint64 end = (qint64)endOffset;
    

    start = std::max((qint64)0, start);
    end = std::min(dataSize - 1, end); 
    
    if (start > end) return QList<QMap<QChar, quint8>>();
    

    auto checkMatchAndMap = [&](qint64 i, const KnownPhrase &phrase) -> QMap<QChar, quint8> {
        QMap<QChar, quint8> currentMapping;
        bool potentialMatch = true;
        
        for (auto it = phrase.pattern.constBegin(); it != phrase.pattern.constEnd(); ++it) {
            QChar character = it.key();
            const QList<int> &positions = it.value();
            

            quint8 byteValue = (quint8)data.at(i + positions.first());

            for (int pos : positions) {
                if ((quint8)data.at(i + pos) != byteValue) {
                    potentialMatch = false;
                    break; 
                }
            }
            
            if (!potentialMatch) break;

            if (currentMapping.contains(character) && currentMapping.value(character) != byteValue) {
                potentialMatch = false;
                break;
            }
            

            bool byteConflict = false;
            for (quint8 existingByte : currentMapping.values()) {
                if (existingByte == byteValue && currentMapping.key(existingByte) != character) {
                    byteConflict = true; 
                    break;
                }
            }
            if (byteConflict) {
                 potentialMatch = false;
                 break;
            }

            currentMapping.insert(character, byteValue);
        }
        
        if (potentialMatch && !currentMapping.isEmpty()) {
            return currentMapping;
        }
        return QMap<QChar, quint8>();
    };

    for (const KnownPhrase &phrase : phrases) {
        int phraseLength = phrase.length;
        
        if (end - start + 1 < phraseLength) continue;

        qint64 max_i = end - phraseLength + 1;

        max_i = std::min(max_i, dataSize - phraseLength); 

        for (qint64 i = start; i <= max_i; ++i) {
            QMap<QChar, quint8> mapping = checkMatchAndMap(i, phrase);
            if (!mapping.isEmpty()) {
                if (!possibleMappings.contains(mapping)) {
                    possibleMappings.append(mapping);
                }
            }
        }
    }
    
    return possibleMappings;
}

void hexandtabler::addFoundMappingToTable(const QMap<QChar, quint8> &mapping) {
    if (!m_tableWidget) return;

    QSignalBlocker blocker(m_tableWidget);
    

    for (int i = 0; i < 256; ++i) {
        if (mapping.values().contains(i)) {
             m_charMap[i] = ".";
             QTableWidgetItem *item = m_tableWidget->item(i, 1);
             if (item) item->setText(".");
        }
    }

    for (auto it = mapping.constBegin(); it != mapping.constEnd(); ++it) {
        QChar character = it.key();
        quint8 byteValue = it.value();
        
        m_charMap[byteValue] = QString(character);
        QTableWidgetItem *item = m_tableWidget->item(byteValue, 1);
        if (item) {
            item->setText(QString(character));
        }
    }

    if (hexArea) {
        propagateCharMaps();
    }
}

void hexandtabler::on_actionGuessEncoding_triggered() {
    
    if (m_fileData.isEmpty()) {
        QMessageBox::warning(this, tr("Encoding Guess"), tr("Please load a file first."));
        return;
    }

    if (m_guessSearchFuture.isRunning()) {
        QMessageBox::information(this, tr("Encoding Guess"), tr("A search is already running. Please wait."));
        return;
    }

    QDialog configDialog(this);
    configDialog.setWindowTitle(tr("Relativity due position"));

    QVBoxLayout *mainLayout = new QVBoxLayout(&configDialog);
    QTextEdit *phrasesEdit = new QTextEdit;
    phrasesEdit->setPlaceholderText(tr("Enter known phrases \nto find hex coincidences\n(one phrase per line, minimum %1 chars each):").arg(3));
    
    qint64 fileSize = m_fileData.size();
    QString maxOffsetHex = QString::number(fileSize > 0 ? fileSize - 1 : 0, 16).toUpper(); 
    QLineEdit *startOffsetEdit = new QLineEdit("0");
    QLineEdit *endOffsetEdit = new QLineEdit(maxOffsetHex);
    
    QFormLayout *formLayout = new QFormLayout;
    formLayout->addRow(tr("Known Phrases:"), phrasesEdit);
    formLayout->addRow(tr("Start Offset (Hex):"), startOffsetEdit);
    formLayout->addRow(tr("End Offset (Hex, inclusive):"), endOffsetEdit);

    mainLayout->addLayout(formLayout);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, &configDialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &configDialog, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    if (configDialog.exec() != QDialog::Accepted) {
        return;
    }

    QString input = phrasesEdit->toPlainText();
    bool startOk, endOk;
    quint64 startOffset = startOffsetEdit->text().toULongLong(&startOk, 16);
    quint64 endOffset = endOffsetEdit->text().toULongLong(&endOk, 16);
    
    if (!startOk || !endOk || startOffset > endOffset || endOffset >= (quint64)fileSize) {
        QMessageBox::warning(this, tr("Encoding Guess"), 
                             tr("Invalid start/end offsets. Please use valid hex values where Start <= End < FileSize (0x%1).")
                             .arg(QString::number(fileSize, 16).toUpper()));
        return;
    }
    
    QStringList rawPhrases = input.split('\n', Qt::SkipEmptyParts);
    
    QList<KnownPhrase> searchPhrases;

    for (const QString &text : rawPhrases) {
        QString cleanText = text.trimmed(); 
        if (cleanText.isEmpty() || cleanText.length() < 3) continue;

        KnownPhrase kp;
        kp.text = cleanText;
        kp.length = cleanText.length();
        kp.pattern = calculatePattern(cleanText);
        searchPhrases.append(kp);
    }

    if (searchPhrases.isEmpty()) {
        QMessageBox::warning(this, tr("Encoding Guess"), tr("Please enter valid known phrases (minimum %1 characters each).").arg(3));
        return;
    }
    
    m_guessSearchFuture = QtConcurrent::run([this, searchPhrases, startOffset, endOffset]() {
    return this->guessEncoding(searchPhrases, startOffset, endOffset);
});

    QFutureWatcher<QList<QMap<QChar, quint8>>> *watcher = new QFutureWatcher<QList<QMap<QChar, quint8>>>(this);
    connect(watcher, &QFutureWatcher<QList<QMap<QChar, quint8>>>::finished, this, &hexandtabler::handleGuessEncodingFinished);
    connect(watcher, &QFutureWatcher<QList<QMap<QChar, quint8>>>::finished, watcher, &QObject::deleteLater); 
    watcher->setFuture(m_guessSearchFuture);

    QMessageBox::information(this, tr("Encoding Guess"), tr("Search started in the background. The selection window will appear when the results are ready."));
}

void hexandtabler::handleGuessEncodingFinished() {
    QList<QMap<QChar, quint8>> results = m_guessSearchFuture.result();
    if (results.isEmpty()) {
        QMessageBox::information(this, tr("Encoding Guess Result"),
            tr("No patterns matching the known phrases were found. This searches by the distance of the characters in the phrases, instead of the distance according their alphabet. Try phrases with more character coincidences."));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Select Encoding Map"));

    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);
    QLabel *label = new QLabel(
        tr("Found %n potential encoding(s). Select one to apply, or use 'Apply All'.", "", results.size()));
    mainLayout->addWidget(label);

    QListWidget *listWidget = new QListWidget(&dialog);
    listWidget->setFont(QFont("Monospace", 10));
    listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    mainLayout->addWidget(listWidget);

    for (int i = 0; i < results.size(); ++i) {
        const QMap<QChar, quint8> &mapping = results.at(i);
        QString displayString;
        for (auto it = mapping.constBegin(); it != mapping.constEnd(); ++it) {
            displayString += QString("'%1': 0x%2, ").arg(it.key()).arg(it.value(), 2, 16, QChar('0'));
        }
        if (!displayString.isEmpty())
            displayString.chop(2);
        QListWidgetItem *item = new QListWidgetItem(
            QString("Map %1: (%2)").arg(i + 1).arg(displayString),
            listWidget
        );
        item->setData(Qt::UserRole, i);
    }

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QPushButton *applyAllBtn = buttonBox->addButton(tr("Apply All"), QDialogButtonBox::ActionRole);
    mainLayout->addWidget(buttonBox);

    connect(listWidget, &QListWidget::itemDoubleClicked, [&dialog](QListWidgetItem *) {
        dialog.accept();
    });
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(applyAllBtn, &QPushButton::clicked, [&]() {
        for (const auto &mapping : results) {
            addFoundMappingToTable(mapping);
        }
        QMessageBox::information(&dialog, tr("Encoding Applied"),
                                 tr("All mappings applied successfully."));
        dialog.accept();
    });

    if (dialog.exec() == QDialog::Accepted) {
        QList<QListWidgetItem*> selectedItems = listWidget->selectedItems();
        if (selectedItems.isEmpty()) {
            addFoundMappingToTable(results.first());
            return; 
        }
        for (QListWidgetItem *selectedItem : selectedItems) {
            int mapIndex = selectedItem->data(Qt::UserRole).toInt();
            if (mapIndex >= 0 && mapIndex < results.size()) {
                addFoundMappingToTable(results.at(mapIndex));
            }
        }
        QMessageBox::information(this, tr("Encoding Applied"),
                                 tr("Selected map(s) applied successfully."));
    }
}

void hexandtabler::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void hexandtabler::dropEvent(QDropEvent *event) {
    const QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty()) {
        QString filePath = urls.first().toLocalFile();
        if (!filePath.isEmpty()) {
            loadFile(filePath);
        }
    }
}

void hexandtabler::keyPressEvent(QKeyEvent *event) {

    if (event->modifiers() == Qt::MetaModifier) {
        QScreen *scr = screen() ? screen() : QGuiApplication::primaryScreen();
        if (!scr) { QMainWindow::keyPressEvent(event); return; }
        QRect avail = scr->availableGeometry();
        int hw = avail.width() / 2;

        if (event->key() == Qt::Key_Left) {
            showNormal();
            setGeometry(avail.x(), avail.y(), hw, avail.height());
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Right) {
            showNormal();
            setGeometry(avail.x() + hw, avail.y(), hw, avail.height());
            event->accept();
            return;
        }
    }
    QMainWindow::keyPressEvent(event);
}

void hexandtabler::on_themeChanged(int index) {
    bool isDark = (index == 2);
    
    if (index == 0) {
        isDark = (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);
    }

    on_actionDarkMode_triggered(isDark);
}

#include "hexandtabler.moc"