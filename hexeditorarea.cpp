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

HexEditorArea::HexEditorArea(QWidget *parent)
    : QAbstractScrollArea(parent)
{
    // Initial text size increased to 12.
    QFont font("Monospace", 12); 
    font.setStyleHint(QFont::Monospace);
    setFont(font);

    calculateMetrics(); 
    m_editMode = HexMode; 
    
    // Initialize selection members
    m_selectionEnd = -1;
    m_selectionStart = -1;
    m_selectionAnchor = -1;
    

    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    // Enable mouse tracking for selection dragging
    setMouseTracking(true); 
    
    // Focus policy set to StrongFocus to allow keyboard input
    setFocusPolicy(Qt::StrongFocus);
}

void HexEditorArea::setCharMapping(const QString (&mapping)[256]) {
    // Copy the contents of the mapping array
    for (int i = 0; i < 256; ++i) {
        m_charMap[i] = mapping[i];
    }
    // Force a repaint of the area
    viewport()->update();
}

// ----------------------------------------------------------------------
// --- Metrics and Data ---
// ----------------------------------------------------------------------

void HexEditorArea::calculateMetrics() {
    QFontMetrics fm = fontMetrics();
    m_charWidth = fm.horizontalAdvance('W');
    m_charHeight = fm.height();
    
    const int OFFSET_SLOTS = 10;          
    const int HEX_SLOTS_PER_BYTE = 3;     
    const int SEPARATOR_SLOTS = 3;        
    const int HEX_START_SLOT = OFFSET_SLOTS;
    const int HEX_BLOCK_SLOTS = m_bytesPerLine * HEX_SLOTS_PER_BYTE; 
    const int ASCII_START_SLOT = HEX_START_SLOT + HEX_BLOCK_SLOTS + SEPARATOR_SLOTS;  // ← ESTO ES LO NUEVO
    const int TOTAL_SLOTS = ASCII_START_SLOT + m_bytesPerLine; 
    
    m_hexStartCol = HEX_START_SLOT * m_charWidth;
    m_asciiStartCol = ASCII_START_SLOT * m_charWidth;  // ← CAMBIA ASCII_START_SLOTS por ASCII_START_SLOT
    m_lineLength = TOTAL_SLOTS * m_charWidth;

    viewport()->setMinimumWidth(m_lineLength); 
}


void HexEditorArea::updateViewMetrics() {
    calculateMetrics();
    // Update scroll bar range
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
    
    // Scroll to position
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
    // Ensure positions are valid and on nibble boundaries
    startPos = std::max(0, startPos);
    endPos = std::min(m_data.size() * 2, endPos);
    
    // Ensure selection starts and ends on a byte boundary (even nibble position)
    startPos = (startPos / 2) * 2;
    endPos = (endPos / 2) * 2;
    
    // Handle reverse selection (dragging up/backward)
    if (startPos > endPos) std::swap(startPos, endPos);

    m_selectionStart = startPos;
    m_selectionEnd = endPos;
    
    // Selection is only cleared if they are exactly the same
    if (m_selectionStart == m_selectionEnd) {
        clearSelection();
    }
    
    viewport()->update();
}

void HexEditorArea::clearSelection() {
    m_selectionStart = -1;
    m_selectionEnd = -1;
    m_selectionAnchor = -1;
}

void HexEditorArea::setCursorPosition(int newPos) {
    // Ensure newPos is within bounds
    int maxPos = m_data.size() * 2;
    newPos = std::max(0, std::min(maxPos, newPos));
    
    // Always force cursor to a byte boundary (even nibble position)
    newPos = (newPos / 2) * 2; 

    if (newPos == m_cursorPos) return;
    
    m_cursorPos = newPos; 
    
    // Auto-scroll logic
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

// ----------------------------------------------------------------------
// --- PASTE/COPY OPERATIONS (CORRECTED) --- 
// ----------------------------------------------------------------------

/**
 * @brief Copies the selected data to the clipboard as raw QByteArray.
 */
void HexEditorArea::copySelection()
{
    if (m_selectionStart == -1 || m_selectionStart == m_selectionEnd)
        return;

    int startByte = m_selectionStart / 2;
    int endByte = m_selectionEnd / 2;
    int length = endByte - startByte;

    QByteArray selectedData = m_data.mid(startByte, length);

    QClipboard *clipboard = QApplication::clipboard();
    
    // Use QMimeData to explicitly define binary data type
    QMimeData *mimeData = new QMimeData();
    // 1. Add the native binary format (octet-stream)
    mimeData->setData("application/octet-stream", selectedData); 
    
    // 2. Add Hexadecimal representation as text for other programs
    QString hexText;
    for (int i = 0; i < selectedData.size(); ++i) {
        hexText += QString("%1 ").arg((unsigned char)selectedData.at(i), 2, 16, QChar('0')).toUpper();
    }
    mimeData->setText(hexText.trimmed());
    
    clipboard->setMimeData(mimeData);
}

/**
 * @brief Pastes data from the clipboard at the current cursor position.
 * It tries to read native binary, then hex strings, and finally plain text (as UTF-8 bytes).
 */
void HexEditorArea::pasteFromClipboard() {
    QClipboard *clipboard = QApplication::clipboard();
    const QMimeData *mimeData = clipboard->mimeData();
    QByteArray dataToPaste;

    if (!mimeData) return;

    if (mimeData->hasFormat("application/octet-stream")) {
        dataToPaste = mimeData->data("application/octet-stream");
    } else if (mimeData->hasText()) {
        QString text = mimeData->text();
        
        for (int i = 0; i < text.length(); ++i) {
            QChar ch = text.at(i);
            bool found = false;
            for (int j = 0; j < 256; ++j) {
                if (m_charMap[j].contains(ch)) {
                    dataToPaste.append((char)j);
                    found = true;
                    break;
                }
            }
            if (!found) {
                dataToPaste.append('\0');  // ← CAMBIO: '\0' en vez de 0x00
            }
        }
        
        if (dataToPaste.isEmpty() || dataToPaste == QByteArray(text.length(), '\0')) {
            QString cleanedText = text.simplified();
            cleanedText.remove(' ');
            cleanedText.remove('\n');
            cleanedText.remove('\r');
            QByteArray hexParsedData = QByteArray::fromHex(cleanedText.toUtf8());
            if (!hexParsedData.isEmpty()) {
                dataToPaste = hexParsedData;
            } else {
                dataToPaste = text.toUtf8();
            }
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
        clearSelection();
        setCursorPosition(m_selectionEnd);
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
    



// ----------------------------------------------------------------------
// --- Events ---
// ----------------------------------------------------------------------

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

        // --- 1. Draw Offset ---
        QString offsetStr = QString("%1").arg(startByteIndex, 8, 16, QChar('0')).toUpper();
        painter.setPen(pal.color(QPalette::WindowText));
        painter.drawText(0, currentY, m_charWidth * 10, m_charHeight, Qt::AlignLeft | Qt::AlignVCenter, offsetStr);

        // --- 2. Draw Hex and ASCII data ---
        for (int i = 0; i < m_bytesPerLine; ++i) {
            int byteIndex = startByteIndex + i;
            if (byteIndex >= totalBytes) break;
            
            unsigned char byte = (unsigned char)m_data.at(byteIndex);
            int currentNibbleStart = byteIndex * 2;
            int currentNibbleEnd = currentNibbleStart + 2;
            
            // --- SYNCHRONIZED HIGHLIGHTING LOGIC (HEX and ASCII) ---
            bool isCursorByte = (cursorByteIndex == byteIndex);
            // The selection covers the byte if its start is before the byte end and vice versa.
            bool isSelected = (m_selectionStart != -1 && 
                               std::max(m_selectionStart, currentNibbleStart) < std::min(m_selectionEnd, currentNibbleEnd));

            QColor bgColor = pal.color(QPalette::Base);
            if (isSelected) {
                bgColor = pal.color(QPalette::Highlight);
            } else if (isCursorByte) {
                // Use Midlight (Mid) for the cursor when no selection
                bgColor = pal.color(QPalette::Midlight); 
            }

            // Draw background for HEX (3 * m_charWidth for XX )
            painter.fillRect(m_hexStartCol + i * (3 * m_charWidth), currentY, 3 * m_charWidth, m_charHeight, bgColor);
            // Draw background for ASCII (1 * m_charWidth for X)
            painter.fillRect(m_asciiStartCol + i * m_charWidth, currentY, m_charWidth, m_charHeight, bgColor);
            
            // c) HEX Area Drawing
            QString hexStr = QString("%1 ").arg(byte, 2, 16, QChar('0')).toUpper();
            
            // If selected/cursor, text must contrast with Highlight/Midlight
            if (isSelected || isCursorByte) {
                 painter.setPen(pal.color(QPalette::HighlightedText));
            } else {
                 painter.setPen(pal.color(QPalette::WindowText));
            }
            
            painter.drawText(m_hexStartCol + i * (3 * m_charWidth), currentY, 3 * m_charWidth, m_charHeight, Qt::AlignLeft | Qt::AlignVCenter, hexStr);

            // d) ASCII Area Drawing
            QString charStr = m_charMap[byte];
            // Ensure ASCII text color is normal if not selected.
            if (!isSelected && !isCursorByte) {
                 painter.setPen(pal.color(QPalette::WindowText));
            }
            painter.drawText(m_asciiStartCol + i * m_charWidth, currentY, m_charWidth, m_charHeight, Qt::AlignLeft | Qt::AlignVCenter, charStr);
            
            // Reset text color to WindowText for remaining elements if needed
            painter.setPen(pal.color(QPalette::WindowText));
        }
    }
}

// ----------------------------------------------------------------------
// --- Editing Logic Helpers ---
// ----------------------------------------------------------------------

/**
 * @brief Handles printable character input when in AsciiMode, using the custom char map.
 */
void HexEditorArea::handleAsciiInput(const QString &text) {
    if (text.isEmpty()) return;

    QChar inputChar = text.at(0);
        
    // Look up the byte value in m_charMap
    int byteValue = -1;
    for (int i = 0; i < 256; ++i) {
        if (m_charMap[i].length() > 0 && m_charMap[i].at(0) == inputChar) {
            byteValue = i;
            break;
        }
    }

    if (byteValue != -1) {
        int byteIndex = m_cursorPos / 2;
        
        // Ensure we are not past the data end
        if (byteIndex < m_data.size()) {
            // Replace the byte with the value found in the map
            m_data[byteIndex] = (char)byteValue;
            
            // Move the cursor to the next byte (2 nibbles)
            setCursorPosition(m_cursorPos + 2); 
            
            // Notify listeners of the change
            emit dataChanged();
        }
    }
}

/**
 * @brief Handles Backspace/Delete operations.
 */
void HexEditorArea::handleDelete() {
    
    // Only handle Backspace in this function (delete left behavior)
    if (m_cursorPos > 0) {
        
        // Backspace deletes the previous byte (sets 0x00) and moves the cursor back 2 nibbles.
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
    
    if (!shiftIsHeld && event->key() != Qt::Key_Control) {
        clearSelection(); 
    }
    
    if (shiftIsHeld && m_selectionAnchor == -1) {
        m_selectionAnchor = m_cursorPos; 
    }
    
    int newCursorPos = m_cursorPos;
    bool moved = false;
    
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
            newCursorPos = 0;
            moved = true;
            break;
        case Qt::Key_End:
            newCursorPos = m_data.size() * 2;
            moved = true;
            break;
        case Qt::Key_PageUp:
            newCursorPos = m_cursorPos - (viewport()->height() / m_charHeight) * m_bytesPerLine * 2;
            moved = true;
            break;
        case Qt::Key_PageDown:
            newCursorPos = m_cursorPos + (viewport()->height() / m_charHeight) * m_bytesPerLine * 2;
            moved = true;
            break;
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

        m_cursorPos = newCursorPos;

        if (m_selectionAnchor != -1) {
            setSelection(m_selectionAnchor, m_cursorPos);
        }
        viewport()->update();
        return;
    }
    
    if (event->matches(QKeySequence::Copy)) {
        copySelection();
        return;
    }
    
    if (event->matches(QKeySequence::Paste)) {
        pasteFromClipboard();
        return;
    }
    
    QString text = event->text();
    if (!text.isEmpty()) {
        if (m_editMode == AsciiMode) {
            handleAsciiInput(text); 
            return;
        } else if (m_editMode == HexMode) {
            QChar inputChar = text.at(0);
            int hexValue = inputChar.toLower().digitValue(); 
            
            if (hexValue != -1) { 
                int byteIndex = m_cursorPos / 2;
                
                if (byteIndex < m_data.size()) {
                    unsigned char byte = (unsigned char)m_data[byteIndex];
                    byte = (byte & 0x0F) | (hexValue << 4);
                    m_data[byteIndex] = (char)byte;
                    emit dataChanged();
                    setCursorPosition(m_cursorPos + 2);
                }
                return;
            }
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
    
    // The space between hex and ASCII starts 3 slots after the last hex slot
    const int HEX_MAX_X = m_asciiStartCol - 3 * m_charWidth;
    
    // Check if click is in the HEX area
    if (colX >= m_hexStartCol && colX < HEX_MAX_X) {
        int relX = colX - m_hexStartCol;
        int charIndex = relX / m_charWidth;
        byteInLine = charIndex / 3; 
    }
    
    // Check if click is in the ASCII area
    else if (colX >= m_asciiStartCol && colX < m_asciiStartCol + m_bytesPerLine * m_charWidth) {
        int relX = colX - m_asciiStartCol;
        byteInLine = relX / m_charWidth;
    }
    
    if (byteInLine == -1) return -1;
    
    int byteIndex = offset + byteInLine;
    return (byteIndex < m_data.size()) ? byteIndex : -1;
}


void HexEditorArea::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        int byteIndex = byteIndexAt(event->pos());
        if (byteIndex != -1) {
            int colX = event->pos().x();

            m_editMode = (colX >= m_asciiStartCol && colX < m_asciiStartCol + m_bytesPerLine * m_charWidth) ? AsciiMode : HexMode;

            m_selectionAnchor = byteIndex * 2;  
            m_selectionStart = m_selectionAnchor;
            m_selectionEnd = m_selectionAnchor;
            m_cursorPos = m_selectionAnchor;
            
            viewport()->update();  // actualiza para mostrar la posición inicial
        }
    }
    QAbstractScrollArea::mousePressEvent(event);
}


void HexEditorArea::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        int byteIndex = byteIndexAt(event->pos());
        if (byteIndex != -1 && m_selectionAnchor != -1) {
            int newEnd = byteIndex * 2 + 2;
            setSelection(m_selectionAnchor, newEnd);
            m_cursorPos = newEnd;
        }
    }
    QAbstractScrollArea::mouseMoveEvent(event);
}



void HexEditorArea::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        // If selection is zero length, clear it
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