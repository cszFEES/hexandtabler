#include "hexeditorarea.h"
#include <QPainter>
#include <QScrollBar>
#include <QMouseEvent>
#include <QApplication>
#include <QFontMetrics>
#include <cmath>
#include <algorithm> 
#include <QClipboard> 
#include <QMimeData> 
#include <QKeyEvent> 
#include <QPalette> 
#include <QDebug> 
#include <QChar> 
#include <QKeySequence>

HexEditorArea::HexEditorArea(QWidget *parent)
    : QAbstractScrollArea(parent),
      m_bytesPerLine(16),
      m_currentNibbleIndex(0)
{
    QFont font("Monospace", 12); 
    font.setStyleHint(QFont::Monospace);
    setFont(font);

    for (int i = 0; i < 32; ++i) {
        m_charMap[i] = ".";
    }
    for (int i = 32; i < 127; ++i) {
        m_charMap[i] = QChar(i);
    }
    for (int i = 127; i < 256; ++i) {
        m_charMap[i] = ".";
    }
    
    calculateMetrics(); 
    m_editMode = HexMode; 
    
    m_selectionEnd = -1;
    m_selectionStart = -1;
    m_selectionAnchor = -1;
    m_cursorPos = 0;
    
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    setMouseTracking(true); 
    setFocusPolicy(Qt::StrongFocus);
}

void HexEditorArea::setCharMapping(const QString (&mapping)[256]) {
    for (int i = 0; i < 256; ++i) {
        m_charMap[i] = mapping[i];
    }
    viewport()->update();
}

void HexEditorArea::calculateMetrics() {
    QFontMetrics fm = fontMetrics();
    m_charWidth = fm.horizontalAdvance('W'); 
    m_charHeight = fm.height();
    
    const int OFFSET_SLOTS = 10;          
    const int HEX_SLOTS_PER_BYTE = 3;     
    const int SEPARATOR_SLOTS = 3;        
    const int HEX_START_SLOT = OFFSET_SLOTS;
    const int HEX_BLOCK_SLOTS = m_bytesPerLine * HEX_SLOTS_PER_BYTE; 
    const int ASCII_START_SLOT = HEX_START_SLOT + HEX_BLOCK_SLOTS + SEPARATOR_SLOTS;
    const int TOTAL_SLOTS = ASCII_START_SLOT + m_bytesPerLine; 
    
    m_hexStartCol = HEX_START_SLOT * m_charWidth;
    m_asciiStartCol = ASCII_START_SLOT * m_charWidth;
    m_lineLength = TOTAL_SLOTS * m_charWidth;

    viewport()->setMinimumWidth(m_lineLength); 
}

void HexEditorArea::updateViewMetrics() {
    calculateMetrics();
    int totalLines = (m_data.size() + m_bytesPerLine - 1) / m_bytesPerLine;
    verticalScrollBar()->setRange(0, std::max(0, totalLines * m_charHeight - viewport()->height()));
    viewport()->update();
}

void HexEditorArea::setHexData(const QByteArray &data) {
    m_data = data;
    setCursorPosition(0); 
    clearSelection();
    updateViewMetrics();
    viewport()->update();
}

QByteArray HexEditorArea::hexData() const {
    return m_data;
}

void HexEditorArea::goToOffset(quint64 offset) {
    if (offset >= (quint64)m_data.size()) {
        offset = m_data.size();
    }
    setCursorPosition(offset * 2); 
    
    int line = offset / m_bytesPerLine;
    int lineY = line * m_charHeight;
    int visibleHeight = viewport()->height();
    int scrollY = verticalScrollBar()->value();

    if (lineY < scrollY) {
        verticalScrollBar()->setValue(lineY);
    } else if (lineY + m_charHeight > scrollY + visibleHeight) {
        verticalScrollBar()->setValue(lineY + m_charHeight - visibleHeight);
    }
}

void HexEditorArea::setSelection(int startPos, int endPos) {
    startPos = std::max(0, startPos);
    endPos = std::min(m_data.size() * 2, endPos);
    
    startPos = (startPos / 2) * 2;
    endPos = ((endPos + 1) / 2) * 2; 
    
    if (startPos > endPos) std::swap(startPos, endPos);

    m_selectionStart = startPos;
    m_selectionEnd = endPos;
    
    if (m_selectionStart == m_selectionEnd) {
        clearSelection(); 
    }
    
    viewport()->update();
}

void HexEditorArea::clearSelection() {
    m_selectionStart = -1;
    m_selectionEnd = -1;
    m_selectionAnchor = -1;
    viewport()->update();
}

void HexEditorArea::setCursorPosition(int newPos) {
    int maxPos = m_data.size() * 2;
    newPos = std::max(0, std::min(maxPos, newPos));
    
    newPos = (newPos / 2) * 2; 

    if (newPos == m_cursorPos) return;
    
    m_cursorPos = newPos; 
    m_currentNibbleIndex = 0; 
    
    int offset = m_cursorPos / 2;
    int line = offset / m_bytesPerLine;
    int lineY = line * m_charHeight;
    int scrollY = verticalScrollBar()->value();
    int visibleHeight = viewport()->height();
    
    if (lineY < scrollY) {
        verticalScrollBar()->setValue(lineY);
    } else if (lineY + m_charHeight > scrollY + visibleHeight) {
        verticalScrollBar()->setValue(lineY + m_charHeight - visibleHeight);
    }

    viewport()->update();
}

void HexEditorArea::copySelection()
{
    if (m_selectionStart == -1 || m_selectionStart == m_selectionEnd)
        return;

    int startByte = m_selectionStart / 2;
    int endByte = m_selectionEnd / 2;
    int length = endByte - startByte;

    QByteArray selectedData = m_data.mid(startByte, length);

    QClipboard *clipboard = QApplication::clipboard();
    
    QMimeData *mimeData = new QMimeData();
    mimeData->setData("application/octet-stream", selectedData); 
    
    QString hexText;
    for (int i = 0; i < selectedData.size(); ++i) {
        hexText += QString("%1 ").arg((unsigned char)selectedData.at(i), 2, 16, QChar('0')).toUpper();
    }
    mimeData->setText(hexText.trimmed());
    
    clipboard->setMimeData(mimeData);
}

void HexEditorArea::pasteFromClipboard() {
    QClipboard *clipboard = QApplication::clipboard();
    const QMimeData *mimeData = clipboard->mimeData();
    QByteArray dataToPaste;

    if (!mimeData) return;

    if (mimeData->hasFormat("application/octet-stream")) {
        dataToPaste = mimeData->data("application/octet-stream");
    } else if (mimeData->hasText()) {
        QString text = mimeData->text();
        QByteArray tempCharMappedData;
        bool mappedSuccessfully = false;
        
        for (int i = 0; i < text.length(); ++i) {
            QChar ch = text.at(i);
            bool found = false;
            for (int j = 0; j < 256; ++j) {
                if (m_charMap[j].length() > 0 && m_charMap[j].at(0) == ch) {
                    tempCharMappedData.append((char)j);
                    found = true;
                    mappedSuccessfully = true;
                    break;
                }
            }
            if (!found) {
                tempCharMappedData.append('\0');
            }
        }
        
        if (!mappedSuccessfully || tempCharMappedData.toStdString() == std::string(tempCharMappedData.size(), '\0')) {
            QString cleanedText = text.simplified();
            cleanedText.remove(' ');
            cleanedText.remove('\n');
            cleanedText.remove('\r');
            QByteArray hexParsedData = QByteArray::fromHex(cleanedText.toLower().toUtf8()); 
            if (!hexParsedData.isEmpty()) {
                dataToPaste = hexParsedData;
            } else {
                dataToPaste = text.toUtf8();
            }
        } else {
            dataToPaste = tempCharMappedData;
        }
    }

    if (dataToPaste.isEmpty()) return;

    if (m_selectionStart != -1 && m_selectionStart != m_selectionEnd) {
        int startByte = m_selectionStart / 2;
        int endByte = m_selectionEnd / 2;
        int length = endByte - startByte;
        
        for (int i = 0; i < length && i < dataToPaste.size(); ++i) {
            m_data[startByte + i] = dataToPaste.at(i);
        }
        setCursorPosition(m_selectionEnd);
        clearSelection();
    } else {
        int insertByte = m_cursorPos / 2;
        int pasteSize = dataToPaste.size();
        int availableSize = m_data.size() - insertByte;
        int copySize = std::min(pasteSize, availableSize);

        for (int i = 0; i < copySize; ++i) {
            m_data[insertByte + i] = dataToPaste.at(i);
        }
        setCursorPosition((insertByte + copySize) * 2);
    }

    updateViewMetrics();
    emit dataChanged();
}

void HexEditorArea::paintEvent(QPaintEvent *event) {
    QPainter painter(viewport());
    painter.setFont(font());
    
    int scrollY = verticalScrollBar()->value();
    int firstVisibleLine = scrollY / m_charHeight;
    int lastVisibleLine = (scrollY + viewport()->height()) / m_charHeight;
    
    int totalBytes = m_data.size();
    
    QPalette pal = palette();
    int cursorByteIndex = m_cursorPos / 2;

    for (int line = firstVisibleLine; line <= lastVisibleLine; ++line) {
        int startByteIndex = line * m_bytesPerLine;
        if (startByteIndex >= totalBytes) break;

        int currentY = line * m_charHeight - scrollY;
        
        // 1. DRAW OFFSET
        QString offsetStr = QString("%1").arg(startByteIndex, 8, 16, QChar('0')).toUpper();
        painter.setPen(pal.color(QPalette::WindowText));
        painter.drawText(0, currentY, m_charWidth * 10, m_charHeight, Qt::AlignLeft | Qt::AlignVCenter, offsetStr);

        for (int i = 0; i < m_bytesPerLine; ++i) {
            int byteIndex = startByteIndex + i;
            if (byteIndex >= totalBytes) break;
            
            unsigned char byte = (unsigned char)m_data.at(byteIndex);
            int currentNibbleStart = byteIndex * 2;
            int currentNibbleEnd = currentNibbleStart + 2;
            
            bool isCursorByte = (cursorByteIndex == byteIndex);
            bool isSelected = (m_selectionStart != -1 && 
                               std::max(m_selectionStart, currentNibbleStart) < std::min(m_selectionEnd, currentNibbleEnd));

            QColor bgColor = pal.color(QPalette::Base);
            if (isSelected) {
                bgColor = pal.color(QPalette::Highlight);
            } else if (isCursorByte) { // Resaltar el byte completo si el cursor está en él
                bgColor = pal.color(QPalette::Midlight);
            }

            // Fill hex area (3 chars: high nibble, low nibble, space)
            painter.fillRect(m_hexStartCol + i * (3 * m_charWidth), currentY, 3 * m_charWidth, m_charHeight, bgColor);
            // Fill ascii area (1 char)
            painter.fillRect(m_asciiStartCol + i * m_charWidth, currentY, m_charWidth, m_charHeight, bgColor);
            
            QString hexStr = QString("%1").arg(byte, 2, 16, QChar('0')).toUpper();
            
            if (isSelected || isCursorByte) {
                 painter.setPen(pal.color(QPalette::HighlightedText));
            } else {
                 painter.setPen(pal.color(QPalette::WindowText));
            }
            
            // Draw high nibble (slot 0)
            int hexStart = m_hexStartCol + i * (3 * m_charWidth);
            painter.drawText(hexStart, currentY, m_charWidth, m_charHeight, Qt::AlignLeft | Qt::AlignVCenter, hexStr.at(0));
            
            // Draw low nibble (slot 1)
            painter.drawText(hexStart + m_charWidth, currentY, m_charWidth, m_charHeight, Qt::AlignLeft | Qt::AlignVCenter, hexStr.at(1));
            
            // Draw space (slot 2)
            painter.drawText(hexStart + 2 * m_charWidth, currentY, m_charWidth, m_charHeight, Qt::AlignLeft | Qt::AlignVCenter, " ");
            
            // NO DRAW NIBBLE CURSOR
            
            // Draw ASCII
            QString charStr = m_charMap[byte];
            if (!isSelected && !isCursorByte) {
                 painter.setPen(pal.color(QPalette::WindowText));
            }
            painter.drawText(m_asciiStartCol + i * m_charWidth, currentY, m_charWidth, m_charHeight, Qt::AlignLeft | Qt::AlignVCenter, charStr);
            
            painter.setPen(pal.color(QPalette::WindowText));
        }
    }
}

void HexEditorArea::handleAsciiInput(const QString &text) {
    if (text.isEmpty()) return;

    QChar inputChar = text.at(0);
        
    int byteValue = -1;
    for (int i = 0; i < 256; ++i) {
        if (m_charMap[i].length() > 0 && m_charMap[i].at(0) == inputChar) {
            byteValue = i;
            break;
        }
    }

    if (byteValue != -1) {
        int byteIndex = m_cursorPos / 2;
        
        if (byteIndex < m_data.size()) {
            m_data[byteIndex] = (char)byteValue;
            setCursorPosition(m_cursorPos + 2);
            emit dataChanged();
        }
    }
}

void HexEditorArea::handleHexInput(const QString &text) {
    if (text.isEmpty()) return;

    QChar inputChar = text.at(0);
    int hexValue = -1;

    if (inputChar.isDigit()) {
        hexValue = inputChar.digitValue(); // 0-9
    } else {
        // CORRECCIÓN: Manejo manual de A-F para evitar errores de digitValue() en letras.
        QChar lowerChar = inputChar.toLower();
        if (lowerChar >= 'a' && lowerChar <= 'f') {
            hexValue = lowerChar.unicode() - QChar('a').unicode() + 10;
        }
    }

    if (hexValue < 0 || hexValue > 15) {
        return;
    }

    int byteIndex = m_cursorPos / 2;

    if (byteIndex < m_data.size()) {
        unsigned char byte = (unsigned char)m_data[byteIndex];

        if (m_currentNibbleIndex == 0) {
            byte = (byte & 0x0F) | (hexValue << 4);
            m_data[byteIndex] = (char)byte;
            
            m_currentNibbleIndex = 1;
            viewport()->update();
        } else {
            byte = (byte & 0xF0) | hexValue;
            m_data[byteIndex] = (char)byte;
            
            setCursorPosition(m_cursorPos + 2);
            m_currentNibbleIndex = 0; 
        }
        
        emit dataChanged();
    }
}

void HexEditorArea::handleDelete() {
    if (m_cursorPos > 0) {
        setCursorPosition(m_cursorPos - 2); 
        int byteIndex = m_cursorPos / 2;
        
        if (byteIndex < m_data.size()) {
            m_data[byteIndex] = 0x00;
            emit dataChanged();
        }
    }
}

void HexEditorArea::keyPressEvent(QKeyEvent *event) {
    bool shiftIsHeld = event->modifiers() & Qt::ShiftModifier;
    int newCursorPos = m_cursorPos;
    bool moved = false;
    
    if (!shiftIsHeld && event->key() != Qt::Key_Control) {
        clearSelection(); 
    }
    
    // CORRECCIÓN ROBUSTA DE TAB: Interceptar QKey_Tab con el código de tecla.
    // Esto previene que el QAbstractScrollArea y QMainWindow muevan el foco.
    if (event->key() == Qt::Key_Tab) {
        if (m_editMode == HexMode) {
            m_editMode = AsciiMode;
        } else {
            m_editMode = HexMode;
        }
        m_currentNibbleIndex = 0; 
        viewport()->update();
        event->accept(); 
        return;
    }
    
    if (shiftIsHeld && m_selectionAnchor == -1) {
        m_selectionAnchor = m_cursorPos; 
    }
    
    switch (event->key()) {
        case Qt::Key_Left:
            newCursorPos = m_cursorPos - 2;
            moved = true;
            break;
        case Qt::Key_Right:
            newCursorPos = m_cursorPos + 2;
            moved = true;
            break;
        case Qt::Key_Up:
            newCursorPos = m_cursorPos - m_bytesPerLine * 2;
            moved = true;
            break;
        case Qt::Key_Down:
            newCursorPos = m_cursorPos + m_bytesPerLine * 2;
            moved = true;
            break;
        case Qt::Key_Home:
            newCursorPos = (m_cursorPos / (m_bytesPerLine * 2)) * (m_bytesPerLine * 2);
            moved = true;
            break;
        case Qt::Key_End:
            newCursorPos = std::min(m_data.size() * 2, ((m_cursorPos / (m_bytesPerLine * 2)) + 1) * (m_bytesPerLine * 2));
            moved = true;
            break;
        case Qt::Key_PageUp: {
            int linesPerPage = viewport()->height() / m_charHeight;
            int currentByteIndex = m_cursorPos / 2;
            int currentLine = currentByteIndex / m_bytesPerLine;
            int offsetInLine = currentByteIndex % m_bytesPerLine;
            int targetLine = std::max(0, currentLine - linesPerPage);
            int newByteIndex = targetLine * m_bytesPerLine + offsetInLine;
            newByteIndex = std::min((int)m_data.size(), newByteIndex);
            newCursorPos = newByteIndex * 2;
            moved = true;
            break;
        }
        case Qt::Key_PageDown: {
            int linesPerPage = viewport()->height() / m_charHeight;
            int currentByteIndex = m_cursorPos / 2;
            int currentLine = currentByteIndex / m_bytesPerLine;
            int offsetInLine = currentByteIndex % m_bytesPerLine;
            int totalLines = (m_data.size() + m_bytesPerLine - 1) / m_bytesPerLine;
            int targetLine = std::min(totalLines, currentLine + linesPerPage);
            int newByteIndex = targetLine * m_bytesPerLine + offsetInLine;
            newByteIndex = std::min((int)m_data.size(), newByteIndex);
            newCursorPos = newByteIndex * 2;
            moved = true;
            break;
        }
        case Qt::Key_Backspace:
            handleDelete(); 
            return;
        case Qt::Key_Delete:
            if (m_cursorPos / 2 < m_data.size()) {
                 m_data[m_cursorPos / 2] = 0x00;
                 setCursorPosition(m_cursorPos + 2);
                 emit dataChanged();
            }
            return;
        default:
            break;
    }
    
    if (moved) {
        int maxPos = m_data.size() * 2;
        newCursorPos = std::max(0, std::min(maxPos, newCursorPos));
        newCursorPos = (newCursorPos / 2) * 2;
        
        setCursorPosition(newCursorPos); 
        
        if (m_selectionAnchor != -1) {
            setSelection(m_selectionAnchor, m_cursorPos);
        }
        
        return;
    }
    
    // --- commands ---
    if (event->matches(QKeySequence::Copy)) {
        copySelection();
        return;
    }
    
    if (event->matches(QKeySequence::Paste)) {
        pasteFromClipboard();
        return;
    }
    
    // --- text entry (ASCII/HEX) ---
    QString text = event->text();
    if (!text.isEmpty()) {
        if (m_editMode == AsciiMode) {
            handleAsciiInput(text); 
            return;
        } else if (m_editMode == HexMode) {
            handleHexInput(text);
            return;
        }
    }

    QAbstractScrollArea::keyPressEvent(event);
}

int HexEditorArea::byteIndexAt(const QPoint &point) const {
    int scrollY = verticalScrollBar()->value();
    int line = (point.y() + scrollY) / m_charHeight;
    int offset = line * m_bytesPerLine;
    
    if (offset >= m_data.size())
        return -1;

    int colX = point.x();
    int byteInLine = -1;
    
    const int HEX_MAX_X = m_asciiStartCol - (3 * m_charWidth); 
    
    if (colX >= m_hexStartCol && colX < HEX_MAX_X) {
        int relX = colX - m_hexStartCol;
        int charIndex = relX / m_charWidth;
        byteInLine = charIndex / 3;
    }
    else if (colX >= m_asciiStartCol && colX < m_asciiStartCol + m_bytesPerLine * m_charWidth) {
        int relX = colX - m_asciiStartCol;
        byteInLine = relX / m_charWidth;
    }
    
    if (byteInLine == -1 || byteInLine >= m_bytesPerLine) return -1;
    
    int byteIndex = offset + byteInLine;
    return (byteIndex < m_data.size()) ? byteIndex : -1;
}

void HexEditorArea::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        int byteIndex = byteIndexAt(event->pos());
        if (byteIndex != -1) {
            int colX = event->pos().x();

            m_editMode = (colX >= m_asciiStartCol) ? AsciiMode : HexMode;
            m_currentNibbleIndex = 0;

            m_selectionAnchor = byteIndex * 2;  
            setCursorPosition(m_selectionAnchor);
            
            if (!(event->modifiers() & Qt::ShiftModifier)) {
                clearSelection();
            }
            
            if (event->modifiers() & Qt::ShiftModifier) {
                setSelection(m_selectionAnchor, m_selectionAnchor + 2);
            }
        }
    }
    QAbstractScrollArea::mousePressEvent(event);
}

void HexEditorArea::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        int byteIndex = byteIndexAt(event->pos());
        if (byteIndex != -1 && m_selectionAnchor != -1) {
            int newEnd = byteIndex * 2 + 2; 
            
            setCursorPosition(newEnd);
            
            setSelection(m_selectionAnchor, newEnd);
        }
    }
    QAbstractScrollArea::mouseMoveEvent(event);
}

void HexEditorArea::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        if (m_selectionStart == m_selectionEnd) {
             clearSelection();
        }
    }
    QAbstractScrollArea::mouseReleaseEvent(event);
}

void HexEditorArea::resizeEvent(QResizeEvent *event) {
    QAbstractScrollArea::resizeEvent(event);
    updateViewMetrics();
}