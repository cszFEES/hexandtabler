#include "hexeditorarea.h"
#include <QPainter>
#include <QScrollBar>
#include <QMouseEvent>
#include <QApplication>
#include <QFontMetrics>
#include <cmath>
#include <algorithm> 
#include <QClipboard> 
#include <QKeyEvent> 
#include <QPalette> 
#include <QDebug> 
#include <QChar> // Asegurar que QChar esté incluido si se utiliza

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
    const int HEX_SLOTS_PER_BYTE = 3;     // Two hex chars + space
    const int SEPARATOR_SLOTS = 3;        // Space between hex and ASCII
    
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
    
    // Asegurar que la selección comienza y termina en un límite de byte (posición par de nibble)
    startPos = (startPos / 2) * 2;
    endPos = (endPos / 2) * 2;
    // Si la selección es de arrastre, el final puede ser 2 nibbles más (para incluir el último byte)
    // El mouseMoveEvent se encarga de ajustar el final para incluir el byte.

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
    // Do NOT update cursor position here, as it's often used with setCursorPosition
}

void HexEditorArea::setCursorPosition(int newPos) {
    // Ensure newPos is within bounds
    int maxPos = m_data.size() * 2;
    newPos = std::max(0, std::min(maxPos, newPos));
    
    // MODIFICACIÓN: Siempre forzar el cursor a un límite de byte (posición par de nibble)
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
            
            // --- LÓGICA DE RESALTADO SINCRONIZADA (HEX y ASCII) ---
            bool isCursorByte = (cursorByteIndex == byteIndex);
            // La selección cubre el byte si su inicio es anterior al final del byte y viceversa.
            bool isSelected = (m_selectionStart != -1 && 
                               std::max(m_selectionStart, currentNibbleStart) < std::min(m_selectionEnd, currentNibbleEnd));

            QColor bgColor = pal.color(QPalette::Base);
            if (isSelected) {
                bgColor = pal.color(QPalette::Highlight);
            } else if (isCursorByte) {
                // Usar Midlight (Mid) para el cursor cuando no hay selección
                bgColor = pal.color(QPalette::Midlight); 
            }

            // Dibujar fondo para HEX (3 * m_charWidth para XX )
            painter.fillRect(m_hexStartCol + i * (3 * m_charWidth), currentY, 3 * m_charWidth, m_charHeight, bgColor);
            // Dibujar fondo para ASCII (1 * m_charWidth para X)
            painter.fillRect(m_asciiStartCol + i * m_charWidth, currentY, m_charWidth, m_charHeight, bgColor);
            
            // c) HEX Area Drawing
            QString hexStr = QString("%1 ").arg(byte, 2, 16, QChar('0')).toUpper();
            
            // Si está seleccionado/cursor, el texto debe contrastar con el Highlight/Midlight
            if (isSelected || isCursorByte) {
                 painter.setPen(pal.color(QPalette::HighlightedText));
            } else {
                 painter.setPen(pal.color(QPalette::WindowText));
            }
            
            painter.drawText(m_hexStartCol + i * (3 * m_charWidth), currentY, 3 * m_charWidth, m_charHeight, Qt::AlignLeft | Qt::AlignVCenter, hexStr);

            // d) ASCII Area Drawing
            QString charStr = m_charMap[byte];
            // Asegurarse de que el color del texto para ASCII siempre sea el normal si no está seleccionado.
            if (!isSelected && !isCursorByte) {
                 painter.setPen(pal.color(QPalette::WindowText));
            }
            painter.drawText(m_asciiStartCol + i * m_charWidth, currentY, m_charWidth, m_charHeight, Qt::AlignLeft | Qt::AlignVCenter, charStr);
            
            // Resetear el color de texto a WindowText para el resto de elementos si es necesario
            painter.setPen(pal.color(QPalette::WindowText));
        }
    }
    
    // MODIFICACIÓN: Bloque eliminado para quitar el cursor de edición visual (la línea vertical)
    /*
    if (m_selectionStart == -1 || m_selectionStart == m_selectionEnd) {
        // ... Lógica del cursor eliminada ...
    }
    */
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
    
    // Buscar el valor de byte en m_charMap
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
    // MODIFICACIÓN: Comportamiento unificado byte a byte para Backspace.
    
    // Solo manejamos Backspace en esta función (comportamiento de borrar hacia la izquierda)
    if (m_cursorPos > 0) {
        
        // Backspace borra el byte anterior (pone 0x00) y mueve el cursor 2 nibbles hacia atrás.
        setCursorPosition(m_cursorPos - 2);
        int byteIndex = m_cursorPos / 2;
        
        if (byteIndex < m_data.size()) {
            m_data[byteIndex] = 0x00;
            emit dataChanged();
        }
    }
}


void HexEditorArea::keyPressEvent(QKeyEvent *event) {
    
    // Clear selection on any keyboard press that isn't a modifier/selection
    if (event->key() != Qt::Key_Shift && event->key() != Qt::Key_Control) {
        clearSelection();
    }
    
    // --- 1. HANDLE NAVIGATION KEYS ---
    switch (event->key()) {
        case Qt::Key_Left:
            // MODIFICACIÓN: Mover siempre 2 nibbles (1 byte)
            setCursorPosition(m_cursorPos - 2);
            return;
        case Qt::Key_Right:
             // MODIFICACIÓN: Mover siempre 2 nibbles (1 byte)
            setCursorPosition(m_cursorPos + 2);
            return;
        case Qt::Key_Up:
            setCursorPosition(m_cursorPos - m_bytesPerLine * 2);
            return;
        case Qt::Key_Down:
            setCursorPosition(m_cursorPos + m_bytesPerLine * 2);
            return;
        case Qt::Key_Home:
            setCursorPosition(0);
            return;
        case Qt::Key_End:
            setCursorPosition(m_data.size() * 2);
            return;
        case Qt::Key_PageUp:
            setCursorPosition(m_cursorPos - (viewport()->height() / m_charHeight) * m_bytesPerLine * 2);
            return;
        case Qt::Key_PageDown:
            setCursorPosition(m_cursorPos + (viewport()->height() / m_charHeight) * m_bytesPerLine * 2);
            return;
        case Qt::Key_Backspace:
            handleDelete(); // >> Llama a la función declarada
            return;
        case Qt::Key_Delete:
             // Simple implementation: write 0x00 at the current position
            if (m_cursorPos / 2 < m_data.size()) {
                 m_data[m_cursorPos / 2] = 0x00;
                 setCursorPosition(m_cursorPos + 2); // Move to next byte
                 emit dataChanged();
            }
            return;
        default:
            break;
    }
    
    // --- 2. HANDLE EDITING INPUT ---
    
    // Handle printable text input
    QString text = event->text();
    if (!text.isEmpty()) {
        if (m_editMode == AsciiMode) {
            handleAsciiInput(text); // >> Llama a la función declarada
            return;
        } else if (m_editMode == HexMode) {
            // >> Edición HEX (byte a byte)
            QChar inputChar = text.at(0);
            int hexValue = inputChar.toLower().digitValue(); // -1 si no es hex.
            
            if (hexValue != -1) { 
                int byteIndex = m_cursorPos / 2;
                
                if (byteIndex < m_data.size()) {
                    unsigned char byte = (unsigned char)m_data[byteIndex];
                    
                    // MODIFICACIÓN: Al presionar un dígito, se asume que es la parte alta (high nibble)
                    // y se sobrescribe el byte con este nibble y el nibble bajo actual.
                    byte = (byte & 0x0F) | (hexValue << 4);
                    
                    m_data[byteIndex] = (char)byte;
                    
                    emit dataChanged();
                    setCursorPosition(m_cursorPos + 2); // MODIFICACIÓN: Mover 2 nibbles (1 byte)
                }
                return;
            }
        }
    }

    // Pass unhandled events up (e.g., Ctrl+C, Ctrl+V, etc.)
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
            
            // Determine the new Edit Mode based on where the click occurred
            if (colX >= m_asciiStartCol && colX < m_asciiStartCol + m_bytesPerLine * m_charWidth) {
                m_editMode = AsciiMode;
                // In AsciiMode, the cursor is always at the start of the byte
                setCursorPosition(byteIndex * 2);
            } else {
                m_editMode = HexMode;
                
                // MODIFICACIÓN: Siempre posicionar al inicio del byte (posición par de nibble)
                setCursorPosition(byteIndex * 2);
                
                /* Lógica de cálculo de nibble eliminada, ya que siempre es byte-a-byte
                // Calculate nibble position in HexMode
                int relX = colX - m_hexStartCol;
                int charIndex = relX / m_charWidth;
                int byteInLine = charIndex / 3;
                
                int nibblePos = 0; // 0 for high nibble, 1 for low nibble

                if (byteInLine == byteIndex % m_bytesPerLine) {
                     int charInByte = charIndex % 3;
                     if (charInByte == 1) { // Second hex character (low nibble)
                        nibblePos = 1;
                     } 
                     // charInByte == 0 is high nibble (nibblePos = 0)
                     // charInByte == 2 is the space (treat as clicking on the second hex character)
                     else if (charInByte == 2) {
                         nibblePos = 1;
                     }
                     
                     setCursorPosition(byteIndex * 2 + nibblePos);
                } else {
                     setCursorPosition(byteIndex * 2);
                }
                */
            }


            // Start drag selection
            m_selectionStart = m_cursorPos;
            m_selectionEnd = m_cursorPos;
        }
    }
    QAbstractScrollArea::mousePressEvent(event);
}

void HexEditorArea::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        int byteIndex = byteIndexAt(event->pos());
        if (byteIndex != -1) {
            int newCursorPos;
            
            // For selection dragging, track byte boundaries (2 nibbles)
            newCursorPos = byteIndex * 2;
            
            // Update the selection end to include the entire byte
            int currentSelectionEnd;
            if (newCursorPos < m_selectionStart) {
                // Dragging backwards
                currentSelectionEnd = m_selectionStart;
                // Since newCursorPos is the start of the current byte, we use newCursorPos for the selection start
                setSelection(newCursorPos, currentSelectionEnd); 
            } else {
                // Dragging forwards
                currentSelectionEnd = newCursorPos + 2; // +2 to include the current byte
                setSelection(m_selectionStart, currentSelectionEnd); 
            }
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