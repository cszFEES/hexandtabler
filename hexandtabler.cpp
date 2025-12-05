#include "hexandtabler.h" 
#include "ui_hexandtabler.h" 

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

#include "hexeditorarea.h" 

const char organizationName[] = "FEES"; 
const char applicationName[] = "hexandtabler"; 
const int MAX_UNDO_STATES = 50; 
const int MIN_CHARS_FOR_RELATIVE_SEARCH = 3; 


class FindReplaceDialog : public QDialog
{
    Q_OBJECT

public:
    enum SearchType {
        HexSearch,
        CharSearch,
        RelativeSearch 
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
        return CharSearch; 
    } 
    
    void setFindMode() { 
        setWindowTitle(tr("Find")); 
        replaceLabel->hide(); 
        replaceLineEdit->hide(); 
        replaceButton->hide(); 
        replaceAllButton->hide();
    }
    
    void setReplaceMode() { 
        setWindowTitle(tr("Find and Replace"));
        replaceLabel->show(); 
        replaceLineEdit->show(); 
        replaceButton->show(); 
        replaceAllButton->show();
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
    relativeRadioButton = new QRadioButton(tr("Relative (ADA -> 000300)")); 
    hexRadioButton->setChecked(true); 

    QHBoxLayout *typeLayout = new QHBoxLayout;
    typeLayout->addWidget(new QLabel(tr("Search Type:")));
    typeLayout->addWidget(hexRadioButton);
    typeLayout->addWidget(charRadioButton);
    typeLayout->addWidget(relativeRadioButton); 

    caseSensitiveCheckBox = new QCheckBox(tr("Case sensitive"));
    wrapCheckBox = new QCheckBox(tr("Wrap around"));
    backwardsCheckBox = new QCheckBox(tr("Search backwards"));
    
    QVBoxLayout *optionsLayout = new QVBoxLayout;
    optionsLayout->addWidget(caseSensitiveCheckBox);
    optionsLayout->addWidget(wrapCheckBox);
    optionsLayout->addWidget(backwardsCheckBox);
    
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
    mainLayout->addLayout(optionsLayout);
    mainLayout->addLayout(buttonLayout);
    setLayout(mainLayout);
    
    connect(findNextButton, &QPushButton::clicked, this, &FindReplaceDialog::onFindNext);
    connect(replaceButton, &QPushButton::clicked, this, &FindReplaceDialog::replaceClicked);
    connect(replaceAllButton, &QPushButton::clicked, this, &FindReplaceDialog::replaceAllClicked);
    connect(closeButton, &QPushButton::clicked, this, &FindReplaceDialog::close);
    
    connect(findLineEdit, &QLineEdit::textEdited, [=](){
        backwardsCheckBox->setChecked(false);
    });
    
    setFindMode();
    setFixedSize(sizeHint());
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
    setWindowIcon(QIcon(":/icon.png"));
    
    m_tableWidget = new QTableWidget(this); 
    m_tableDock = new QDockWidget(tr("Conversion Table"), this);
    
    m_tableDock->setFeatures(QDockWidget::DockWidgetMovable); 
    
    m_tableDock->setWidget(m_tableWidget);
    addDockWidget(Qt::RightDockWidgetArea, m_tableDock);
    setupConversionTable();
    
    if (ui->hexEdit) {
        QWidget *placeholder = ui->hexEdit;
        QVBoxLayout *layout = qobject_cast<QVBoxLayout*>(placeholder->parentWidget()->layout());
        
        if (layout) {
            m_hexEditorArea = new HexEditorArea(this);
            m_hexEditorArea->setHexData(m_fileData);
            
            int index = layout->indexOf(placeholder);
            if (index != -1) {
                layout->insertWidget(index, m_hexEditorArea);
            } else {
                layout->addWidget(m_hexEditorArea);
            }
            
            delete placeholder;
            ui->hexEdit = nullptr; 
        } else {
            m_hexEditorArea = nullptr; 
        }
    } else {
        m_hexEditorArea = nullptr;
    }
    
    m_findReplaceDialog = new FindReplaceDialog(this); 
    
    if (m_hexEditorArea) {
         connect(m_hexEditorArea, &HexEditorArea::dataChanged, this, &hexandtabler::handleDataEdited);
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
                QMessageBox::warning(m_findReplaceDialog, tr("Input Error"), tr("Invalid search pattern."));
                return;
            }
            this->replaceAll(needle, replacement);
        });
        
        connect(m_findReplaceDialog, &FindReplaceDialog::replaceClicked, this, &hexandtabler::replaceOne); 
    }

    on_actionDarkMode_triggered(ui->actionDarkMode->isChecked());
    
    if (m_hexEditorArea) {
        m_hexEditorArea->setCharMapping(m_charMap); 
    } 
    
    connect(ui->actionToggleTable, &QAction::toggled, m_tableDock, &QDockWidget::setVisible);
    connect(m_tableDock, &QDockWidget::visibilityChanged, ui->actionToggleTable, &QAction::setChecked);
    
    createRecentFileActions();
    loadRecentFiles();
    updateUndoRedoActions();
    
    setWindowTitle(QString("%1 - %2").arg(applicationName).arg(tr("No File")));
    setWindowIcon(QApplication::style()->standardIcon(QStyle::SP_DriveCDIcon));
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
    if (m_hexEditorArea) {
        m_fileData = m_hexEditorArea->hexData();
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
    m_isModified = false;
    updateUndoRedoActions();
    return true;
}

void hexandtabler::loadFile(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, tr("Error"), tr("Could not read file %1:\n%2.").arg(filePath).arg(file.errorString()));
        return;
    }

    m_fileData = file.readAll();
    file.close();

    if (m_hexEditorArea) {
        m_hexEditorArea->setHexData(m_fileData);
        m_hexEditorArea->goToOffset(0); 
    }
    
    m_currentFilePath = filePath;
    m_isModified = false;
    m_undoStack.clear();
    m_redoStack.clear();
    pushUndoState();
    updateUndoRedoActions();
    
    setWindowTitle(QString("%1 - %2").arg(applicationName).arg(QFileInfo(filePath).fileName()));
    prependToRecentFiles(filePath);
}

bool hexandtabler::maybeSave()
{
    if (!m_isModified)
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
    QMessageBox::about(this, tr("About ") + applicationName,
                       tr("<h2>") + applicationName + tr(" 1.0</h2>"
                          "<p>A simple Hex Editor and Table Conversion Utility.</p>"
                          "<p>Developed by [FEES] for testing purposes.</p>"));
}

void hexandtabler::on_actionDarkMode_triggered(bool checked) {
    QPalette p = QApplication::palette();
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
        p = QApplication::style()->standardPalette();
    }
    QApplication::setPalette(p);
    
    QSettings settings(organizationName, applicationName);
    settings.setValue("darkMode", checked);
}

void hexandtabler::on_actionZoomIn_triggered() {
    if (m_hexEditorArea) m_hexEditorArea->setFont(QFont(m_hexEditorArea->font().family(), m_hexEditorArea->font().pointSize() + 1));
}

void hexandtabler::on_actionZoomOut_triggered() {
    if (m_hexEditorArea) m_hexEditorArea->setFont(QFont(m_hexEditorArea->font().family(), std::max(8, m_hexEditorArea->font().pointSize() - 1)));
}

void hexandtabler::on_actionGoTo_triggered() {
    if (!m_hexEditorArea) return;

    bool ok;
    QString text = QInputDialog::getText(this, tr("Go To Offset"),
                                         tr("Enter offset in hexadecimal:"), QLineEdit::Normal,
                                         QString(), &ok);
    if (ok && !text.isEmpty()) {
        bool hexOk;
        quint64 offset = text.toULongLong(&hexOk, 16);
        
        if (hexOk) {
            m_hexEditorArea->goToOffset(offset);
        } else {
            QMessageBox::warning(this, tr("Invalid Input"), tr("The input is not a valid hexadecimal number."));
        }
    }
}


void hexandtabler::pushUndoState() {
    if (!m_hexEditorArea) return;
    
    m_fileData = m_hexEditorArea->hexData();
    
    if (!m_undoStack.isEmpty() && m_undoStack.last() == m_fileData) {
        return; 
    }
    
    m_undoStack.append(m_fileData);
    
    if (m_undoStack.size() > MAX_UNDO_STATES) {
        m_undoStack.removeFirst();
    }
    m_redoStack.clear(); 
    updateUndoRedoActions();
}

void hexandtabler::updateUndoRedoActions() {
    if (ui->actionUndo) {
        ui->actionUndo->setEnabled(m_undoStack.size() > 1);
    }
    if (ui->actionRedo) {
        ui->actionRedo->setEnabled(!m_redoStack.isEmpty());
    }
}

void hexandtabler::on_actionUndo_triggered() {
    if (!m_hexEditorArea || m_undoStack.size() <= 1) return;
    
    QByteArray currentState = m_undoStack.takeLast();
    m_redoStack.append(currentState);
    
    QByteArray newState = m_undoStack.last();
    m_fileData = newState;
    m_hexEditorArea->setHexData(m_fileData);
    
    m_isModified = true; 
    updateUndoRedoActions();
}

void hexandtabler::on_actionRedo_triggered() {
    if (!m_hexEditorArea || m_redoStack.isEmpty()) return;
    
    QByteArray newState = m_redoStack.takeLast();
    
    m_undoStack.append(newState);
    m_fileData = newState;
    m_hexEditorArea->setHexData(m_fileData);
    
    m_isModified = true;
    updateUndoRedoActions();
}

QVector<qint8> hexandtabler::calculateRelativeOffsets(const QString &input) const {
    QVector<qint8> offsets;
    
    if (input.length() < MIN_CHARS_FOR_RELATIVE_SEARCH) {
        return offsets; 
    }

    qint8 baseValue = input.at(0).toLatin1();
    
    for (int i = 0; i < input.length(); ++i) {
        qint8 currentValue = input.at(i).toLatin1();
        qint8 offset = currentValue - baseValue; 
        
        offsets.append(offset);
    }

    return offsets;
}

void hexandtabler::findNextRelative(const QString &searchText, bool wrap, bool backwards) {
    
    QVector<qint8> offsets = calculateRelativeOffsets(searchText);
    
    if (offsets.isEmpty()) {
        QMessageBox::information(this, tr("Relative Search"), 
            tr("It requires %1 at least.")
            .arg(MIN_CHARS_FOR_RELATIVE_SEARCH));
        return;
    }

    int n = offsets.size();
    const QByteArray &data = m_fileData;
    int dataSize = data.size();
    
    if (dataSize < n) {
        QMessageBox::information(this, tr("Relative Search"), 
            tr("File too small for a sequence of %1 bytes.")
            .arg(n));
        return;
    }

    qint64 currentBytePos = m_hexEditorArea->cursorPosition() / 2;
    qint64 startIndex = currentBytePos; 
    
    if (!backwards) {
        startIndex = std::min((qint64)dataSize - n, startIndex + 1); 
    } else {
        QMessageBox::warning(this, tr("Relative search"), tr("Not implemented."));
        return;
    }


    qint64 foundPos = -1;
    
    for (qint64 i = startIndex; i <= dataSize - n; ++i) {
        
        qint8 baseByte = data.at(i);
        bool match = true;
        
        for (int k = 0; k < n; ++k) {
            qint8 currentByte = data.at(i + k); 
            qint8 actualOffset = currentByte - baseByte; 
            
            if (actualOffset != offsets.at(k)) {
                match = false;
                break;
            }
        }
        
        if (match) {
            foundPos = i;
            break; 
        }
    }
    
    if (foundPos == -1 && wrap) {
        for (qint64 i = 0; i < startIndex; ++i) {
            qint8 baseByte = data.at(i);
            bool match = true;
            
            for (int k = 0; k < n; ++k) {
                qint8 currentByte = data.at(i + k); 
                qint8 actualOffset = currentByte - baseByte; 
                
                if (actualOffset != offsets.at(k)) {
                    match = false;
                    break;
                }
            }
            
            if (match) {
                foundPos = i;
                break; 
            }
        }
    }

    if (foundPos != -1) {
        m_hexEditorArea->goToOffset(foundPos);
        m_hexEditorArea->setSelection(foundPos * 2, (foundPos + n) * 2);
    } else {
        QMessageBox::information(this, tr("Relative Search"), 
            tr("No coincidences found for this relative search \"%1\".")
            .arg(searchText));
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
    if (!m_hexEditorArea || needle.isEmpty()) return;
    
    QByteArray data = m_hexEditorArea->hexData();
    qint64 dataSize = data.size();
    qint64 needleSize = needle.size();
    
    qint64 currentBytePos = m_hexEditorArea->cursorPosition() / 2; 

    QByteArray searchData = caseSensitive ? data : data.toLower();
    QByteArray searchNeedle = caseSensitive ? needle : needle.toLower();

    qint64 foundPos = -1;
    
    if (!backwards) {
        qint64 searchStart;
        
        if (m_hexEditorArea->selectionEnd() != -1) { 
            searchStart = m_hexEditorArea->selectionEnd() / 2; 
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
        
        if (m_hexEditorArea->selectionStart() != -1) { 
            searchEnd = m_hexEditorArea->selectionStart() / 2 - 1; 
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
        m_hexEditorArea->goToOffset(foundPos);
        m_hexEditorArea->setSelection(foundPos * 2, (foundPos + needleSize) * 2);
    } else {
        QMessageBox::information(this, tr("Find Result"), tr("Search pattern not found."));
    }
}

void hexandtabler::replaceOne() {
    if (!m_hexEditorArea || !m_findReplaceDialog) return;
    
    if (m_findReplaceDialog->searchType() == FindReplaceDialog::RelativeSearch) {
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

    qint64 currentBytePos = (m_hexEditorArea->selectionStart() != -1) 
                            ? (m_hexEditorArea->selectionStart() / 2) 
                            : (m_hexEditorArea->cursorPosition() / 2); 
    
    QByteArray data = m_hexEditorArea->hexData();

    QByteArray currentDataCheck = caseSensitive ? data.mid(currentBytePos, needle.size()) : data.mid(currentBytePos, needle.size()).toLower();
    QByteArray searchNeedle = caseSensitive ? needle : needle.toLower();

    bool replaced = false;
    if (currentDataCheck == searchNeedle) {
        data.replace(currentBytePos, needle.size(), replacement);
        m_hexEditorArea->setHexData(data);
        pushUndoState();
        
        m_hexEditorArea->goToOffset(currentBytePos + replacement.size());
        m_hexEditorArea->setSelection(-1, -1); 
        
        replaced = true;
    }

    qint64 searchStart = replaced ? currentBytePos + replacement.size() : currentBytePos;
    if (!replaced) {
        if (currentDataCheck == searchNeedle) {
             searchStart = currentBytePos + 1;
        } else {
             searchStart = m_hexEditorArea->cursorPosition() / 2;
        }
    }
    
    QByteArray searchData = caseSensitive ? m_hexEditorArea->hexData() : m_hexEditorArea->hexData().toLower();
    qint64 foundPos = searchData.indexOf(searchNeedle, searchStart);
    
    if (foundPos != -1) {
        m_hexEditorArea->goToOffset(foundPos);
        m_hexEditorArea->setSelection(foundPos * 2, (foundPos + needle.size()) * 2);
    } else if (wrap) {
        foundPos = searchData.indexOf(searchNeedle, 0);
        if (foundPos != -1 && foundPos < searchStart) {
             m_hexEditorArea->goToOffset(foundPos);
             m_hexEditorArea->setSelection(foundPos * 2, (foundPos + needle.size()) * 2);
        } else {
            QMessageBox::information(this, tr("Replace Result"), tr("No more matches found."));
        }
    } else {
        QMessageBox::information(this, tr("Replace Result"), tr("No more matches found."));
    }
}

void hexandtabler::replaceAll(const QByteArray &needle, const QByteArray &replacement) {
    if (!m_hexEditorArea || needle.isEmpty()) return;
    
    if (m_findReplaceDialog->searchType() == FindReplaceDialog::RelativeSearch) {
        QMessageBox::warning(this, tr("Replace Error"), tr("Replace All function is not available for Relative Search mode."));
        return;
    }

    QByteArray data = m_hexEditorArea->hexData();
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

    m_hexEditorArea->setHexData(data);
    pushUndoState();
    m_hexEditorArea->goToOffset(0); 
    m_hexEditorArea->setSelection(-1, -1);
}


void hexandtabler::on_actionCopy_triggered()
{
    if (m_hexEditorArea) {
        m_hexEditorArea->copySelection();
    }
}

void hexandtabler::on_actionPaste_triggered()
{
    if (m_hexEditorArea) {
        m_hexEditorArea->pasteFromClipboard();
    }
}

void hexandtabler::setupConversionTable() {
    if (!m_tableWidget) return;
    
    m_tableWidget->setRowCount(256); 
    m_tableWidget->setColumnCount(2); 
    m_tableWidget->setHorizontalHeaderLabels({tr("Hex"), tr("Assigned")});
    m_tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
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

void hexandtabler::on_actionToggleTable_triggered() {
    if (m_tableDock) {
        m_tableDock->setVisible(ui->actionToggleTable->isChecked());
    }
}

void hexandtabler::on_actionLoadTable_triggered() {
    QString fileName = QFileDialog::getOpenFileName(this, tr("Load Conversion Table"), m_currentTablePath.isEmpty() ? QDir::homePath() : QFileInfo(m_currentTablePath).absoluteDir().path(), tr("Table Files (*.tbl);;All Files (*.*)"));
    if (fileName.isEmpty()) {
        return;
    }

    if (loadTableFile(fileName)) {
        m_currentTablePath = fileName;
        QMessageBox::information(this, tr("Table Loaded"), tr("Conversion table loaded successfully from %1.").arg(QFileInfo(fileName).fileName()));
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
    out << "# Conversion Table File\n";
    
    for (int i = 0; i < 256; ++i) {
        out << QString("%1=%2\n").arg(i, 2, 16, QChar('0')).toUpper().arg(m_charMap[i]);
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
    
    QString newMap[256];
    
    for (int i = 0; i < 256; ++i) {
        newMap[i] = m_charMap[i];
    }
    
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith("#")) continue;

        QStringList parts = line.split("=");
        if (parts.size() == 2) {
            QString hexCode = parts.at(0).trimmed();
            QString charStr = parts.at(1).trimmed();
            
            bool ok;
            int byteValue = hexCode.toInt(&ok, 16); 

            if (ok && byteValue >= 0 && byteValue <= 255) {
                QString displayChar = charStr.left(1); 
                if (displayChar.isEmpty()) {
                    displayChar = "."; 
                }
                newMap[byteValue] = displayChar;
            }
        }
    }
    file.close();

    QSignalBlocker blocker(m_tableWidget);
    for (int i = 0; i < 256; ++i) {
        m_charMap[i] = newMap[i];
        QTableWidgetItem *item = m_tableWidget->item(i, 1); 
        if (item) {
            item->setText(m_charMap[i]);
        }
    }
    
    if (m_hexEditorArea) {
        m_hexEditorArea->setCharMapping(m_charMap);
    }

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
    
    if (m_hexEditorArea) { 
        m_hexEditorArea->setCharMapping(m_charMap); 
    }
    m_isModified = true;
}
void hexandtabler::on_actionClearTable_triggered() {
    clearCharMappingTable();
}

void hexandtabler::insertSeries(const QList<QString> &series) {
    if (!m_tableWidget || series.isEmpty()) return;

    QModelIndexList selectedIndexes = m_tableWidget->selectionModel()->selectedIndexes(); 
    int startRow = 0;
    if (!selectedIndexes.isEmpty()) {
        startRow = selectedIndexes.at(0).row();
        for (const QModelIndex &index : selectedIndexes) {
            if (index.row() < startRow) {
                startRow = index.row();
            }
        }
    }

    QSignalBlocker blocker(m_tableWidget); 
    
    for (int i = 0; i < series.size(); ++i) {
        int currentRow = startRow + i;
        if (currentRow >= 0 && currentRow < 256) {
            QString character = series.at(i).left(1);
            if (character.isEmpty()) character = ".";

            QTableWidgetItem *item = m_tableWidget->item(currentRow, 1); 
            if (item) {
                item->setText(character);
            }
            
            m_charMap[currentRow] = character;
        }
    }

    if (m_hexEditorArea) {
        m_hexEditorArea->setCharMapping(m_charMap);
    }
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

void hexandtabler::on_actionInsertHiragana_triggered() {
    QList<QString> series;
    int startCode = 0x3042; 
    int count = 50; 
    for (int i = 0; i < count; ++i) {
        series.append(QString(QChar(startCode + i)));
    }
    insertSeries(series);
}

void hexandtabler::on_actionInsertKatakana_triggered() {
    QList<QString> series;
    int startCode = 0x30A2; 
    int count = 50; 
    for (int i = 0; i < count; ++i) {
        series.append(QString(QChar(startCode + i)));
    }
    insertSeries(series);
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

void hexandtabler::handleDataEdited() {
    m_isModified = true;
    pushUndoState();
}

void hexandtabler::handleTableItemChanged(QTableWidgetItem *item) {
    if (!m_tableWidget || !item || item->column() != 1) {
        return;
    }
    
    int row = item->row();
    QString newChar = item->text();

    if (newChar.isEmpty()) {
        newChar = ".";
    }

    QString finalChar = newChar.left(1);

    QSignalBlocker blocker(m_tableWidget); 
    if (item->text() != finalChar) {
        item->setText(finalChar);
    }
    
    m_charMap[row] = finalChar;
    
    if (m_hexEditorArea) {
        m_hexEditorArea->setCharMapping(m_charMap);
    }
}


void hexandtabler::openRecentFile() {
    QAction *action = qobject_cast<QAction *>(sender());
    if (action) {
        if (maybeSave())
            loadFile(action->data().toString());
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
    
    QStringList existingFiles;
    for (const QString &filePath : qAsConst(files)) {
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
    
    int numRecentFiles = std::min(files.size(), (int)MaxRecentFiles);
    
    QAction *separator = nullptr;
    QList<QAction*> actions = ui->menuFile->actions();
    
    for (QAction *action : actions) {
        if (action->isSeparator() && ui->menuFile->actions().indexOf(action) > ui->menuFile->actions().indexOf(ui->actionSaveAs)) { 
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

void hexandtabler::refreshModelFromArea() {
}

#include "hexandtabler.moc"