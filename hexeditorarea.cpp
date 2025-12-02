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

HexEditorArea::HexEditorArea(QWidget *parent)
    : QAbstractScrollArea(parent)
{
    // Initial text size increased to 12.
    QFont font("Monospace", 12); 
    font.setStyleHint(QFont::Monospace);
    setFont(font);

    calculateMetrics(); 
    m_editMode = HexMode; 
    
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
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
    
    // CORRECCIÓN: Cambiado de HEX_START_SLOTS a HEX_START_SLOT
    const int ASCII_START_SLOT = HEX_START_SLOT + HEX_BLOCK_SLOTS + SEPARATOR_SLOTS;
    const int TOTAL_SLOTS = ASCII_START_SLOT + m_bytesPerLine; 
    
    m_hexStartCol = HEX_START_SLOT * m_charWidth;
    m_asciiStartCol = ASCII_START_SLOT * m_charWidth;
    m_lineLength = TOTAL_SLOTS * m_charWidth;

    viewport()->setMinimumWidth(m_lineLength); 
}

void HexEditorArea::setHexData(const QByteArray &data) {
    m_data = data;
    m_cursorPos = 0;
    updateViewMetrics();
}

QByteArray HexEditorArea::hexData() const { 
    return m_data; 
}


void HexEditorArea::updateViewMetrics() {
    calculateMetrics(); 
    
    int totalLines = std::max(
        (m_data.size() + m_bytesPerLine - 1) / m_bytesPerLine, 
        1 
    );
    int totalHeight = totalLines * m_charHeight;
    
    int visibleHeight = viewport()->height();
    verticalScrollBar()->setRange(0, std::max(0, totalHeight - visibleHeight));
    verticalScrollBar()->setPageStep(visibleHeight);
    verticalScrollBar()->setSingleStep(m_charHeight);
    
    viewport()->update();
}

// Function to navigate to a specific byte offset (Go To)
void HexEditorArea::goToOffset(quint64 offset) {
    // Offset is a byte index. Cursor position is a nibble index (offset * 2)
    int nibbleIndex = (int)(offset * 2);
    
    // Ensure the offset does not exceed the data size (2 nibbles per byte)
    if (nibbleIndex > m_data.size() * 2) {
        nibbleIndex = m_data.size() * 2;
    }
    
    // Set the cursor position, which handles scrolling and updating
    setCursorPosition(nibbleIndex);
}

// ----------------------------------------------------------------------
// --- Paint Event (Resaltado Corregido) ---
// ----------------------------------------------------------------------

void HexEditorArea::paintEvent(QPaintEvent *event) {
    QPainter painter(viewport());
    painter.setFont(font());
    
    int scrollY = verticalScrollBar()->value();
    int firstVisibleLine = scrollY / m_charHeight;
    int firstVisibleLineY = firstVisibleLine * m_charHeight - scrollY;
    
    int bytesProcessed = firstVisibleLine * m_bytesPerLine;
    int currentY = firstVisibleLineY;
    
    // --- COLORES ---
    // Colores basados en la paleta actual
    QColor offsetColor = palette().color(QPalette::Text).darker(150); 
    QColor hexColor = palette().color(QPalette::Text); 
    QColor asciiColor = palette().color(QPalette::Text).lighter(150); 
    
    // Primary/Active color (Color para el resaltado)
    // Este color se aplica a AMBOS lados para igualar la prominencia.
    QColor selectionColor = QColor(42, 130, 218).lighter(120); 

    while (currentY < viewport()->height() && bytesProcessed < m_data.size()) {
        
        // --- 1. Draw Offset (Address) ---
        QString offsetText = QString("%1:").arg(bytesProcessed, 8, 16, QChar('0')).toUpper();
        QRect offsetRect(0, currentY, m_charWidth * 10, m_charHeight);
        
        painter.setPen(offsetColor);
        painter.drawText(offsetRect, Qt::AlignRight | Qt::AlignVCenter, offsetText);
        
        // --- 2. Draw Separator ( | ) ---
        int separatorPosX = m_hexStartCol + m_bytesPerLine * (3 * m_charWidth);
        QRect separatorRect(separatorPosX, currentY, m_charWidth * 3, m_charHeight);
        painter.setPen(palette().color(QPalette::Text).darker(120)); 
        painter.drawText(separatorRect, Qt::AlignCenter, "|");


        // --- 3. Draw HEX and ASCII Area ---
        int lineEnd = std::min(bytesProcessed + m_bytesPerLine, m_data.size());
        
        // El cursor siempre apunta al inicio de un byte (nibble par) ya que el movimiento es por byte.
        int cursorByteIndex = m_cursorPos / 2;

        for (int i = bytesProcessed; i < lineEnd; ++i) {
            unsigned char byte = (unsigned char)m_data.at(i);
            
            // HEX Area
            QString hexStr = QString("%1 ").arg(byte, 2, 16, QChar('0')).toUpper(); // XX<space>
            
            int byteOffset = i - bytesProcessed;
            int hexPosX = m_hexStartCol + byteOffset * (3 * m_charWidth);
            QRect hexRect(hexPosX, currentY, 3 * m_charWidth, m_charHeight);

            // Highlight Logic: Siempre resalta el byte completo en el lado HEX
            if (cursorByteIndex == i) { 
                // 1. Highlight the equivalent BYTE in ASCII (Primary color)
                int asciiPosX = m_asciiStartCol + byteOffset * m_charWidth;
                painter.fillRect(asciiPosX, currentY, m_charWidth, m_charHeight, selectionColor); 
                
                // 2. Highlight the FULL BYTE in HEX (Primary color)
                // Se resaltan 2 caracteres (los dos nibbles) con el color primario
                painter.fillRect(hexPosX, currentY, 2 * m_charWidth, m_charHeight, selectionColor); 
            }
            // End of Highlighting

            // Draw HEX text
            painter.setPen(hexColor);
            painter.drawText(hexRect, Qt::AlignLeft | Qt::AlignVCenter, hexStr);

            // ASCII Area
            unsigned char byteValue = byte;
            // Use the custom map for display character
            QString asciiChar = m_charMap[byteValue]; 
            
            int asciiPosX = m_asciiStartCol + byteOffset * m_charWidth;
            QRect asciiRect(asciiPosX, currentY, m_charWidth, m_charHeight);
            
            // Draw ASCII text
            painter.setPen(asciiColor);
            painter.drawText(asciiRect, Qt::AlignCenter, asciiChar);
        }
        
        bytesProcessed += m_bytesPerLine;
        currentY += m_charHeight;
    }
}

// ----------------------------------------------------------------------
// --- Keyboard Interaction (Editing and Navigation Logic) ---
// ----------------------------------------------------------------------

void HexEditorArea::keyPressEvent(QKeyEvent *event) {
    // Check for the PASTE shortcut (Ctrl+V)
    if (event->matches(QKeySequence::Paste)) {
        QClipboard *clipboard = QApplication::clipboard();
        QString textToPaste = clipboard->text();
        
        if (textToPaste.isEmpty()) return;

        // Start processing from the current cursor position
        int currentByteIndex = m_cursorPos / 2;
        
        int pastePos = m_cursorPos; 
        
        // --- 1. Handle HEX Mode Paste ---
        if (m_editMode == HexMode) {
            QString hexString;
            for (QChar c : textToPaste) {
                // Only keep valid hex digits (0-9, A-F)
                if (c.isDigit() || (c.toUpper() >= 'A' && c.toUpper() <= 'F')) {
                    hexString.append(c.toUpper());
                }
            }
            
            if (hexString.isEmpty()) return;
            
            for (QChar hexChar : hexString) {
                if (pastePos / 2 >= m_data.size()) {
                    // Stop if we run out of space (not inserting new bytes, only overwriting)
                    break;
                }
                
                int byteIndex = pastePos / 2;
                int nibble = pastePos % 2;
                
                char byte = m_data.at(byteIndex);
                // Convert hex char to its integer value (0-15)
                int value = hexChar.toLatin1() >= 'A' ? hexChar.toLatin1() - 'A' + 10 : hexChar.digitValue();
                
                if (nibble == 0) { 
                    // Set high nibble
                    byte = (byte & 0x0F) | (value << 4);
                    pastePos++; // Advance to low nibble
                } else { 
                    // Set low nibble
                    byte = (byte & 0xF0) | value;
                    pastePos += 2; // Advance to the start of the next byte (high nibble)
                }
                
                m_data[byteIndex] = byte;
            }
            
        } 
        // --- 2. Handle ASCII Mode Paste (CORREGIDO y usando m_charMap) ---
        else if (m_editMode == AsciiMode) {
            
            for (QChar charPressed : textToPaste) {
                if (currentByteIndex >= m_data.size()) {
                    break; // Stop if we run out of space (only overwriting)
                }
                
                // CORRECCIÓN: Usar la conversión implícita de QChar a QString.
                QString charStr = charPressed; 
                int byteValueToWrite = -1;
                
                // Buscar el carácter pegado en el mapa de conversión
                for (int i = 0; i < 256; ++i) {
                    if (m_charMap[i] == charStr) { 
                        byteValueToWrite = i;
                        break;
                    }
                }

                if (byteValueToWrite != -1) {
                    // Si se encuentra una coincidencia, escribir el valor del byte correspondiente
                    m_data[currentByteIndex] = (char)byteValueToWrite;
                } else {
                    // Si el carácter no está mapeado, se salta/ignora, manteniendo el byte actual
                }

                // Siempre avanzar a la siguiente posición de byte
                currentByteIndex++;
            }
            // Actualizar la posición del cursor
            pastePos = currentByteIndex * 2; 
        }

        // Finalize
        emit dataChanged(); 
        // Ensure cursor ends up on an even nibble boundary (start of a byte)
        setCursorPosition(pastePos);
        return; 
    }
    
    int byteIndex = m_cursorPos / 2;
    int nibble = m_cursorPos % 2; 

    // 1. Navigation Handling (FIX: Movimiento byte por byte en ambos modos)
    switch (event->key()) {
    case Qt::Key_Left:
        // Siempre mueve 2 nibbles (1 byte) a la izquierda.
        setCursorPosition(m_cursorPos - 2); 
        return;
    case Qt::Key_Right:
        // Siempre mueve 2 nibbles (1 byte) a la derecha.
        setCursorPosition(m_cursorPos + 2); 
        return;
    case Qt::Key_Up:
        setCursorPosition(m_cursorPos - m_bytesPerLine * 2);
        return;
    case Qt::Key_Down:
        setCursorPosition(m_cursorPos + m_bytesPerLine * 2);
        return;
        
    case Qt::Key_Backspace: {
        if (m_cursorPos == 0) return;
        
        // En ambos modos, el retroceso debería borrar el byte anterior y moverse a él.
        int targetPos = m_cursorPos - 2; 
        int targetByteIndex = targetPos / 2;
        
        if (targetByteIndex >= 0) {
             m_data[targetByteIndex] = 0x00; 
             emit dataChanged();
             setCursorPosition(targetPos); 
        }
        return;
    }
        
    case Qt::Key_Delete: {
        if (byteIndex >= m_data.size()) return;
        // En ambos modos, Delete borra el byte actual.
        m_data[byteIndex] = 0x00; 
        emit dataChanged();
        setCursorPosition(byteIndex * 2); 
        return;
    }
        
    default:
        break; 
    }
    
    // 2. Data Input Handling (Text Editing)
    QString text = event->text();
    byteIndex = m_cursorPos / 2;
    nibble = m_cursorPos % 2; // Volvemos a calcularlo ya que el cursor puede haberse movido internamente

    if (text.size() == 1 && byteIndex < m_data.size()) {
        QChar charPressed = text.at(0); 
        
        QChar hexChar = charPressed.toUpper();
        bool isHexDigit = (hexChar.isDigit() || (hexChar >= 'A' && hexChar <= 'F'));
        
        if (m_editMode == HexMode) {
            if (isHexDigit) {
                char byte = m_data.at(byteIndex);
                int value = hexChar.toLatin1() >= 'A' ? hexChar.toLatin1() - 'A' + 10 : charPressed.digitValue();
                
                if (nibble == 0) { 
                    // High nibble
                    byte = (byte & 0x0F) | (value << 4);
                    m_data[byteIndex] = byte;
                    emit dataChanged(); 
                    setCursorPosition(m_cursorPos + 1); // Move to the low nibble for next input
                    return; 
                } else { 
                    // Low nibble
                    byte = (byte & 0xF0) | value;
                    m_data[byteIndex] = byte;
                    emit dataChanged();
                    // FIX: Después de editar el nibble bajo, saltar al inicio del siguiente byte.
                    setCursorPosition(m_cursorPos + 1); 
                    return;
                }
            }
        }
        
        // --- Handle ASCII Mode Typing (CORREGIDO y usando m_charMap) ---
        if (m_editMode == AsciiMode) {
            // CORRECCIÓN: Usar la conversión implícita de QChar a QString.
            QString charStr = charPressed; 

            for (int i = 0; i < 256; ++i) {
                // Si el carácter está en el mapa, escribimos el byte (i) correspondiente
                if (m_charMap[i] == charStr) { 
                    m_data[byteIndex] = (char)i;
                    emit dataChanged();
                    setCursorPosition(m_cursorPos + 2); // Mover al siguiente byte
                    return;
                }
            }
            // Si el carácter no se encuentra en el mapa, ignorar la pulsación de tecla.
            return;
        }
    }

    QAbstractScrollArea::keyPressEvent(event);
}

void HexEditorArea::setCursorPosition(int newPos) {
    newPos = std::min(newPos, m_data.size() * 2);
    newPos = std::max(newPos, 0);
    
    // FIX: Si estamos en HexMode o AsciiMode, forzar el cursor a un nibble par (inicio de byte),
    // excepto si estamos activamente escribiendo el nibble bajo en HexMode (newPos es impar).
    if (m_editMode == HexMode || m_editMode == AsciiMode) {
        // Solo forzamos si el nuevo índice es par (movimiento) O si ya hemos pasado
        // un nibble bajo y queremos ir al siguiente byte (m_cursorPos impar + 1)
        if (newPos % 2 != 0) { 
            // Si el cursor cae en nibble impar, lo dejamos allí SOLO si está en HexMode 
            // para la edición del segundo nibble.
            // Si el modo es HexMode y la posición es impar, lo dejamos para permitir la edición.
            // Si no estamos en HexMode o si el cursor ya pasó el límite, lo forzamos.
            if (m_editMode != HexMode || newPos % 2 == 0) {
                 newPos = newPos - (newPos % 2); // Force to start of byte
            }
        }
    }

    if (newPos == m_cursorPos) return;
    
    m_cursorPos = newPos;
    
    int line = m_cursorPos / (2 * m_bytesPerLine);
    int requiredY = line * m_charHeight;
    int scrollY = verticalScrollBar()->value();
    
    if (requiredY < scrollY) {
        verticalScrollBar()->setValue(requiredY);
    } else if (requiredY + m_charHeight > scrollY + viewport()->height()) {
        verticalScrollBar()->setValue(requiredY + m_charHeight - viewport()->height());
    }
    
    viewport()->update();
}

// ----------------------------------------------------------------------
// --- Mouse Interaction ---
// ----------------------------------------------------------------------

void HexEditorArea::mousePressEvent(QMouseEvent *event) {
    int scrollY = verticalScrollBar()->value();
    int line = (event->pos().y() + scrollY) / m_charHeight;
    int offset = line * m_bytesPerLine;
    
    if (offset >= m_data.size()) {
        setCursorPosition(m_data.size() * 2);
        m_editMode = HexMode; 
        return;
    }

    int colX = event->pos().x();
    int byteInLine = -1;
    
    // Click Logic in HEX Area (FIX: Click siempre selecciona el inicio del byte)
    const int HEX_MAX_X = m_asciiStartCol - 3 * m_charWidth;
    if (colX >= m_hexStartCol && colX < HEX_MAX_X) {
        m_editMode = HexMode;
        int relX = colX - m_hexStartCol;
        int charIndex = relX / m_charWidth; 
        byteInLine = charIndex / 3; 
        
        // Forzar cursor al inicio del byte (nibble par)
        int newPos = (offset + byteInLine) * 2;
        setCursorPosition(newPos);
        return;
    }
    
    // Click Logic in ASCII Area
    else if (colX >= m_asciiStartCol) {
        m_editMode = AsciiMode;
        int relX = colX - m_asciiStartCol;
        byteInLine = relX / m_charWidth;
        
        int index = offset + byteInLine;
        if (index < m_data.size()) {
            int newPos = index * 2;
            setCursorPosition(newPos);
            return;
        }
    }
    
    m_editMode = HexMode; 
    setCursorPosition(offset * 2);
}

void HexEditorArea::resizeEvent(QResizeEvent *event) {
    QAbstractScrollArea::resizeEvent(event);
    updateViewMetrics();
}


int HexEditorArea::byteIndexAt(const QPoint &point) const {
    int scrollY = verticalScrollBar()->value();
    int line = (point.y() + scrollY) / m_charHeight;
    int offset = line * m_bytesPerLine;
    
    if (offset >= m_data.size())
        return -1;

    int colX = point.x();
    int byteInLine = -1;
    
    const int HEX_MAX_X = m_asciiStartCol - 3 * m_charWidth;
    if (colX >= m_hexStartCol && colX < HEX_MAX_X) {
        int relX = colX - m_hexStartCol;
        int charIndex = relX / m_charWidth;
        byteInLine = charIndex / 3; 
    }
    
    else if (colX >= m_asciiStartCol) {
        int relX = colX - m_asciiStartCol;
        byteInLine = relX / m_charWidth;
    }
    
    if (byteInLine >= 0 && byteInLine < m_bytesPerLine) {
        int index = offset + byteInLine;
        return index < m_data.size() ? index : -1;
    }

    return -1;
}