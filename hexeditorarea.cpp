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
#include <QStyleOptionSlider>

HexEditorArea::HexEditorArea(QWidget *parent)
    : QAbstractScrollArea(parent),
      m_bytesPerLine(16),
      m_currentNibbleIndex(0)
{
    QFont font("Monospace", 12); 
    font.setStyleHint(QFont::Monospace);
    setFont(font);

    for (int i = 0; i < 256; ++i) {
        if (i < 32 || i >= 127) {
            m_charMap[i] = ".";
        } else {
            m_charMap[i] = QChar(i);
        }
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

QSize HexEditorArea::minimumSizeHint() const {
    int minWidth = m_lineLength; 
    minWidth += 2 * frameWidth(); 
    if (verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOff) {
        minWidth += verticalScrollBar()->sizeHint().width();
    }
    return QSize(minWidth, 0); 
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
    updateGeometry(); 
    viewport()->update();
}

void HexEditorArea::changeEvent(QEvent *event) {
    QAbstractScrollArea::changeEvent(event);
    if (event->type() == QEvent::FontChange) {
        updateViewMetrics();
    }
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

void HexEditorArea::goToOffset(qint64 offset) {
    if (offset >= (qint64)m_data.size()) {
        offset = (qint64)m_data.size();
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

void HexEditorArea::setSelection(qint64 startPos, qint64 endPos) {
    qint64 dataSize2 = (qint64)m_data.size() * 2;
    startPos = std::max((qint64)0, startPos);
    endPos = std::min(dataSize2, endPos);
    
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

void HexEditorArea::setCursorPosition(qint64 newPos) {
    qint64 maxPos = (qint64)m_data.size() * 2;
    newPos = std::max((qint64)0, std::min(maxPos, newPos));
    newPos = (newPos / 2) * 2; 

    if (newPos == m_cursorPos) return;
    
    m_cursorPos = newPos; 
    m_currentNibbleIndex = 0; 
    
    qint64 offset = m_cursorPos / 2;
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

void HexEditorArea::copySelection() {
    if (m_selectionStart == -1 || m_selectionStart == m_selectionEnd)
        return;

    int startByte = (int)(m_selectionStart / 2);
    int endByte = (int)(m_selectionEnd / 2);
    int length = endByte - startByte;

    if (length <= 0) return;

    QByteArray selectedData = m_data.mid(startByte, length);
    QClipboard *clipboard = QApplication::clipboard();
    QMimeData *mimeData = new QMimeData();
    
    mimeData->setData("application/octet-stream", selectedData); 
    
    QString textToCopy;

    if (m_editMode == HexMode) {
        for (int i = 0; i < selectedData.size(); ++i) {
            textToCopy += QString("%1 ").arg((unsigned char)selectedData.at(i), 2, 16, QChar('0')).toUpper();
        }
        textToCopy = textToCopy.trimmed(); // Quita el último espacio
    } else {
        for (int i = 0; i < selectedData.size(); ++i) {
            unsigned char byte = (unsigned char)selectedData.at(i);
            textToCopy += m_charMap[byte];
        }
    }
    
    mimeData->setText(textToCopy);
    clipboard->setMimeData(mimeData);
}

void HexEditorArea::pasteFromClipboard() {
    QClipboard *clipboard = QApplication::clipboard();
    const QMimeData *mimeData = clipboard->mimeData();
    if (!mimeData) return;

    QByteArray dataToPaste;
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
                if (!m_charMap[j].isEmpty() && m_charMap[j].at(0) == ch) {
                    tempCharMappedData.append((char)j);
                    found = true;
                    mappedSuccessfully = true;
                    break;
                }
            }
            if (!found) tempCharMappedData.append('\0');
        }
        
        if (!mappedSuccessfully || tempCharMappedData.count('\0') == tempCharMappedData.size()) {
            QString cleanedText = text.simplified().remove(' ').remove('\n').remove('\r');
            QByteArray hexParsedData = QByteArray::fromHex(cleanedText.toLower().toUtf8()); 
            dataToPaste = hexParsedData.isEmpty() ? text.toUtf8() : hexParsedData;
        } else {
            dataToPaste = tempCharMappedData;
        }
    }

    if (dataToPaste.isEmpty()) return;

    if (m_selectionStart != -1 && m_selectionStart != m_selectionEnd) {
        int startByte = (int)(m_selectionStart / 2);
        int length = (int)(m_selectionEnd / 2) - startByte;
        for (int i = 0; i < length && i < dataToPaste.size(); ++i) {
            m_data[startByte + i] = dataToPaste.at(i);
        }
        setCursorPosition(m_selectionEnd);
        clearSelection();
    } else {
        int insertByte = (int)(m_cursorPos / 2);
        int copySize = std::min((int)dataToPaste.size(), (int)(m_data.size() - insertByte));
        for (int i = 0; i < copySize; ++i) {
            m_data[insertByte + i] = dataToPaste.at(i);
        }
        setCursorPosition((qint64)(insertByte + copySize) * 2);
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
    qint64 cursorByteIndex = m_cursorPos / 2;

    for (int line = firstVisibleLine; line <= lastVisibleLine; ++line) {
        int startByteIndex = line * m_bytesPerLine;
        if (startByteIndex >= totalBytes) break;
        int currentY = line * m_charHeight - scrollY;
        
        QString offsetStr = QString("%1").arg(startByteIndex, 8, 16, QChar('0')).toUpper();
        painter.setPen(pal.color(QPalette::WindowText));
        painter.drawText(0, currentY, m_charWidth * 10, m_charHeight, Qt::AlignLeft | Qt::AlignVCenter, offsetStr);

        for (int i = 0; i < m_bytesPerLine; ++i) {
            int byteIndex = startByteIndex + i;
            if (byteIndex >= totalBytes) break;
            
            unsigned char byte = (unsigned char)m_data.at(byteIndex);
            qint64 currentNibbleStart = (qint64)byteIndex * 2;
            qint64 currentNibbleEnd = currentNibbleStart + 2;
            
            bool isCursorByte = (cursorByteIndex == (qint64)byteIndex);
            bool isSelected = (m_selectionStart != -1 && 
                               std::max(m_selectionStart, currentNibbleStart) < std::min(m_selectionEnd, currentNibbleEnd));

            QColor bgColor = isSelected ? pal.color(QPalette::Highlight) : 
                             (isCursorByte ? pal.color(QPalette::Midlight) : pal.color(QPalette::Base));

            painter.fillRect(m_hexStartCol + i * (3 * m_charWidth), currentY, 3 * m_charWidth, m_charHeight, bgColor);
            painter.fillRect(m_asciiStartCol + i * m_charWidth, currentY, m_charWidth, m_charHeight, bgColor);
            
            QString hexStr = QString("%1").arg(byte, 2, 16, QChar('0')).toUpper();
            painter.setPen((isSelected || isCursorByte) ? pal.color(QPalette::HighlightedText) : pal.color(QPalette::WindowText));
            
            int hexStart = m_hexStartCol + i * (3 * m_charWidth);
            painter.drawText(hexStart, currentY, m_charWidth, m_charHeight, Qt::AlignLeft | Qt::AlignVCenter, hexStr.at(0));
            painter.drawText(hexStart + m_charWidth, currentY, m_charWidth, m_charHeight, Qt::AlignLeft | Qt::AlignVCenter, hexStr.at(1));
            
            QString charStr = m_charMap[byte];
            painter.drawText(m_asciiStartCol + i * m_charWidth, currentY, m_charWidth, m_charHeight, Qt::AlignLeft | Qt::AlignVCenter, charStr);
        }
    }
}

void HexEditorArea::handleAsciiInput(const QString &text) {
    if (text.isEmpty()) return;
    QChar inputChar = text.at(0);
    int byteValue = -1;
    for (int i = 0; i < 256; ++i) {
        if (!m_charMap[i].isEmpty() && m_charMap[i].at(0) == inputChar) {
            byteValue = i;
            break;
        }
    }
    if (byteValue != -1) {
        int byteIndex = (int)(m_cursorPos / 2);
        if (byteIndex < m_data.size()) {
            quint8 oldByte = (quint8)m_data[byteIndex];
            m_data[byteIndex] = (char)byteValue;
            setCursorPosition(m_cursorPos + 2);
            emit byteEdited(byteIndex, oldByte, (quint8)byteValue);
        }
    }
}

void HexEditorArea::handleHexInput(const QString &text) {
    if (text.isEmpty()) return;
    QChar inputChar = text.at(0);
    int hexValue = inputChar.isDigit() ? inputChar.digitValue() : 
                   (inputChar.toLower() >= 'a' && inputChar.toLower() <= 'f' ? inputChar.toLower().unicode() - 'a' + 10 : -1);
    if (hexValue < 0 || hexValue > 15) return;

    int byteIndex = (int)(m_cursorPos / 2);
    if (byteIndex < m_data.size()) {
        quint8 oldByte = (quint8)m_data[byteIndex];
        unsigned char byte = oldByte;
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
            emit byteEdited(byteIndex, oldByte, (quint8)byte); 
        }
    }
}

void HexEditorArea::handleDelete() {
    if (m_cursorPos > 0) {
        setCursorPosition(m_cursorPos - 2);
        int byteIndex = (int)(m_cursorPos / 2);
        if (byteIndex < m_data.size()) {
            quint8 oldByte = (quint8)m_data[byteIndex];
            m_data[byteIndex] = 0x00;
            emit byteEdited(byteIndex, oldByte, 0x00);
        }
    }
}

void HexEditorArea::keyPressEvent(QKeyEvent *event) {
    bool shiftIsHeld = event->modifiers() & Qt::ShiftModifier;
    qint64 newCursorPos = m_cursorPos;
    bool moved = false;
    
    if (!shiftIsHeld && event->key() != Qt::Key_Control) clearSelection();
    
    if (event->key() == Qt::Key_Tab) {
        m_editMode = (m_editMode == HexMode) ? AsciiMode : HexMode;
        m_currentNibbleIndex = 0; 
        viewport()->update();
        event->accept(); 
        return;
    }
    
    if (shiftIsHeld && m_selectionAnchor == -1) m_selectionAnchor = m_cursorPos;

    // Calculamos cuántas líneas caben en la vista actual para PageUp/PageDown
    int linesPerPage = viewport()->height() / m_charHeight;
    if (linesPerPage < 1) linesPerPage = 1;

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
        case Qt::Key_PageUp:
            newCursorPos = m_cursorPos - (linesPerPage * m_bytesPerLine * 2);
            moved = true;
            break;
        case Qt::Key_PageDown:
            newCursorPos = m_cursorPos + (linesPerPage * m_bytesPerLine * 2);
            moved = true;
            break;
        case Qt::Key_Home: 
            newCursorPos = (m_cursorPos / (m_bytesPerLine * 2)) * (m_bytesPerLine * 2); 
            moved = true; 
            break;
        case Qt::Key_End: {
            qint64 dataSize2 = (qint64)m_data.size() * 2;
            newCursorPos = std::min(dataSize2, (qint64)((m_cursorPos / (m_bytesPerLine * 2)) + 1) * (m_bytesPerLine * 2) - 2);
            moved = true; 
            break;
        }
        case Qt::Key_Backspace: handleDelete(); return;
        case Qt::Key_Delete:
            if (m_cursorPos / 2 < (qint64)m_data.size()) {
                 m_data[(int)(m_cursorPos / 2)] = 0x00;
                 setCursorPosition(m_cursorPos + 2);
                 emit dataChanged();
            }
            return;
        default: break;
    }
    
    if (moved) {
        setCursorPosition(newCursorPos); 
        if (m_selectionAnchor != -1) setSelection(m_selectionAnchor, m_cursorPos);
        return;
    }
    
    if (event->matches(QKeySequence::Copy)) { copySelection(); return; }
    if (event->matches(QKeySequence::Paste)) { pasteFromClipboard(); return; }
    
    if (!event->text().isEmpty()) {
        if (m_editMode == AsciiMode) handleAsciiInput(event->text());
        else handleHexInput(event->text());
        return;
    }
    QAbstractScrollArea::keyPressEvent(event);
}

qint64 HexEditorArea::byteIndexAt(const QPoint &point) const {
    int line = (point.y() + verticalScrollBar()->value()) / m_charHeight;
    qint64 offset = (qint64)line * m_bytesPerLine;
    if (offset >= (qint64)m_data.size()) return -1;

    int byteInLine = -1;
    if (point.x() >= m_hexStartCol && point.x() < m_asciiStartCol - (3 * m_charWidth)) {
        byteInLine = (point.x() - m_hexStartCol) / (m_charWidth * 3);
    } else if (point.x() >= m_asciiStartCol && point.x() < m_asciiStartCol + m_bytesPerLine * m_charWidth) {
        byteInLine = (point.x() - m_asciiStartCol) / m_charWidth;
    }
    
    if (byteInLine == -1 || byteInLine >= m_bytesPerLine) return -1;
    qint64 idx = offset + byteInLine;
    return (idx < (qint64)m_data.size()) ? idx : -1;
}

void HexEditorArea::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        qint64 byteIdx = byteIndexAt(event->pos());
        if (byteIdx != -1) {
            qint64 newPos = byteIdx * 2; 
            m_editMode = (event->pos().x() >= m_asciiStartCol) ? AsciiMode : HexMode;
            m_currentNibbleIndex = 0;
            setCursorPosition(newPos); 
            if (!(event->modifiers() & Qt::ShiftModifier)) {
                clearSelection();
                m_selectionAnchor = newPos;  
                setSelection(m_selectionAnchor, m_selectionAnchor + 2); 
            } else {
                if (m_selectionAnchor == -1) m_selectionAnchor = (m_selectionStart != -1) ? m_selectionStart : newPos;
                setSelection(m_selectionAnchor, newPos + 2); 
            }
        }
    }
    QAbstractScrollArea::mousePressEvent(event);
}

void HexEditorArea::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        qint64 byteIdx = byteIndexAt(event->pos());
        if (byteIdx != -1 && m_selectionAnchor != -1) {
            qint64 currentByteStart = byteIdx * 2;
            qint64 startPos = std::min(m_selectionAnchor, currentByteStart);
            qint64 endPos = std::max(m_selectionAnchor, currentByteStart) + 2;
            setSelection(startPos, endPos);
            m_cursorPos = endPos;
        }
    }
    QAbstractScrollArea::mouseMoveEvent(event);
}

void HexEditorArea::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && m_selectionStart == m_selectionEnd) clearSelection();
    QAbstractScrollArea::mouseReleaseEvent(event);
}

void HexEditorArea::resizeEvent(QResizeEvent *event) {
    QAbstractScrollArea::resizeEvent(event);
    updateViewMetrics();
}