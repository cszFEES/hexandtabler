#include "hexeditorarea.h"
#include <QPainter>
#include <QScrollBar>
#include <QMouseEvent>
#include <QApplication>
#include <QFontMetrics>
#include <cmath>
#include <algorithm> 

HexEditorArea::HexEditorArea(QWidget *parent)
    : QAbstractScrollArea(parent)
{
    // CAMBIO 1: Tamaño de texto inicial aumentado a 12.
    QFont font("Monospace", 12); 
    font.setStyleHint(QFont::Monospace);
    setFont(font);

    calculateMetrics(); 
    m_editMode = HexMode; 
    
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

// ----------------------------------------------------------------------
// --- Métricas y Data (Sin cambios) ---
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

// ----------------------------------------------------------------------
// --- Paint Event (Doble Sombreado implementado) ---
// ----------------------------------------------------------------------

void HexEditorArea::paintEvent(QPaintEvent *event) {
    QPainter painter(viewport());
    painter.setFont(font());
    
    int scrollY = verticalScrollBar()->value();
    int firstVisibleLine = scrollY / m_charHeight;
    int firstVisibleLineY = firstVisibleLine * m_charHeight - scrollY;
    
    int bytesProcessed = firstVisibleLine * m_bytesPerLine;
    int currentY = firstVisibleLineY;
    
    QColor offsetColor = Qt::darkGray;
    QColor hexColor = Qt::black;
    QColor asciiColor = Qt::darkGreen;
    
    // CAMBIO 2: Definición de colores para sombreado principal y equivalente
    QColor selectionColor = QColor(42, 130, 218).lighter(150); // Azul claro para el campo de edición principal
    QColor equivalentColor = selectionColor.lighter(120);    // Azul aún más claro para el campo equivalente

    while (currentY < viewport()->height() && bytesProcessed < m_data.size()) {
        
        // --- 1. Dibujar el Offset (Dirección) ---
        QString offsetText = QString("%1:").arg(bytesProcessed, 8, 16, QChar('0'));
        QRect offsetRect(0, currentY, m_charWidth * 10, m_charHeight);
        
        painter.setPen(offsetColor);
        painter.drawText(offsetRect, Qt::AlignRight | Qt::AlignVCenter, offsetText.toUpper());
        
        // --- 2. Dibujar el Separador ( | ) ---
        int separatorPosX = m_hexStartCol + m_bytesPerLine * (3 * m_charWidth);
        QRect separatorRect(separatorPosX, currentY, m_charWidth * 3, m_charHeight);
        painter.setPen(Qt::black);
        painter.drawText(separatorRect, Qt::AlignCenter, "|");


        // --- 3. Dibujar el Área HEX y ASCII ---
        int lineEnd = std::min(bytesProcessed + m_bytesPerLine, m_data.size());
        
        int cursorByteIndex = m_cursorPos / 2;

        for (int i = bytesProcessed; i < lineEnd; ++i) {
            unsigned char byte = (unsigned char)m_data.at(i);
            
            // Área HEX
            QString hexStr = QString("%1 ").arg(byte, 2, 16, QChar('0')).toUpper(); // XX<space>
            
            int byteOffset = i - bytesProcessed;
            int hexPosX = m_hexStartCol + byteOffset * (3 * m_charWidth);
            QRect hexRect(hexPosX, currentY, 3 * m_charWidth, m_charHeight);

            // Lógica de Doble Sombreado
            if (cursorByteIndex == i) { 
                if (m_editMode == HexMode) {
                    // 1. Sombrear el NIBBLE editado en HEX (Color principal)
                    int nibble = m_cursorPos % 2;
                    int highlightX_Hex = hexPosX + nibble * m_charWidth; 
                    painter.fillRect(highlightX_Hex, currentY, m_charWidth, m_charHeight, selectionColor);
                    
                    // 2. Sombrear el BYTE equivalente en ASCII (Color secundario)
                    int asciiPosX = m_asciiStartCol + byteOffset * m_charWidth;
                    painter.fillRect(asciiPosX, currentY, m_charWidth, m_charHeight, equivalentColor); 
                    
                } else { // AsciiMode
                    // 1. Sombrear el BYTE editado en ASCII (Color principal)
                    int asciiPosX = m_asciiStartCol + byteOffset * m_charWidth;
                    painter.fillRect(asciiPosX, currentY, m_charWidth, m_charHeight, selectionColor);
                    
                    // 2. Sombrear el BYTE equivalente en HEX (Color secundario)
                    // Sombrear los dos dígitos HEX (2 * m_charWidth)
                    painter.fillRect(hexPosX, currentY, 2 * m_charWidth, m_charHeight, equivalentColor); 
                }
            }
            // Fin de Sombreado

            // Dibujar texto HEX
            painter.setPen(hexColor);
            painter.drawText(hexRect, Qt::AlignLeft | Qt::AlignVCenter, hexStr);

            // Área ASCII
            char c = (char)byte;
            bool isSafePrint = (byte >= 0x20 && byte <= 0x7E); 
            QString asciiChar = isSafePrint ? QString(c) : QString("."); 
            
            int asciiPosX = m_asciiStartCol + byteOffset * m_charWidth;
            QRect asciiRect(asciiPosX, currentY, m_charWidth, m_charHeight);
            
            // Dibujar texto ASCII
            painter.setPen(asciiColor);
            painter.drawText(asciiRect, Qt::AlignCenter, asciiChar);
        }
        
        bytesProcessed += m_bytesPerLine;
        currentY += m_charHeight;
    }
}

// ----------------------------------------------------------------------
// --- Interacción con el Teclado (Lógica de Edición y Navegación) ---
// ----------------------------------------------------------------------

void HexEditorArea::keyPressEvent(QKeyEvent *event) {
    int byteIndex = m_cursorPos / 2;
    int nibble = m_cursorPos % 2; 

    // 1. Manejo de Navegación
    switch (event->key()) {
    case Qt::Key_Left:
        if (m_editMode == AsciiMode) {
            setCursorPosition(m_cursorPos - 2); 
        } else {
            setCursorPosition(m_cursorPos - 1); 
        }
        return;
    case Qt::Key_Right:
        if (m_editMode == AsciiMode) {
            setCursorPosition(m_cursorPos + 2); 
        } else {
            setCursorPosition(m_cursorPos + 1); 
        }
        return;
    case Qt::Key_Up:
        setCursorPosition(m_cursorPos - m_bytesPerLine * 2);
        return;
    case Qt::Key_Down:
        setCursorPosition(m_cursorPos + m_bytesPerLine * 2);
        return;
        
    case Qt::Key_Backspace: {
        if (m_cursorPos == 0) return;
        
        if (m_editMode == AsciiMode) {
            int targetPos = m_cursorPos - 2; 
            int targetByteIndex = targetPos / 2;
            
            if (targetByteIndex >= 0) {
                 m_data[targetByteIndex] = 0x00; 
                 emit dataChanged();
                 setCursorPosition(targetPos); 
            }
        } else {
            char byte = m_data.at(byteIndex);
            
            if (nibble == 1) { 
                byte = (byte & 0xF0) | 0x00;
            } else { 
                byte = (byte & 0x0F) | 0x00;
            }
            
            m_data[byteIndex] = byte;
            emit dataChanged();
            setCursorPosition(m_cursorPos - 1);
        }
        return;
    }
        
    case Qt::Key_Delete: {
        if (byteIndex >= m_data.size()) return;

        if (m_editMode == AsciiMode) {
            m_data[byteIndex] = 0x00; 
            emit dataChanged();
            setCursorPosition(byteIndex * 2); 
        } else {
            char byte = m_data.at(byteIndex);
            
            if (nibble == 0) { 
                byte = (byte & 0x0F) | 0x00;
            } else { 
                byte = (byte & 0xF0) | 0x00;
            }
            
            m_data[byteIndex] = byte;
            emit dataChanged();
            setCursorPosition(m_cursorPos); 
        }
        return;
    }
        
    default:
        break; 
    }
    
    // 2. Manejo de Entrada de Datos (Edición de texto)
    QString text = event->text();
    byteIndex = m_cursorPos / 2;

    if (text.size() == 1 && byteIndex < m_data.size()) {
        QChar charPressed = text.at(0); 
        
        QChar hexChar = charPressed.toUpper();
        bool isHexDigit = (hexChar.isDigit() || (hexChar >= 'A' && hexChar <= 'F'));
        
        if (m_editMode == HexMode) {
            if (isHexDigit) {
                int nibbleVal = m_cursorPos % 2; 
                char byte = m_data.at(byteIndex);
                int value = hexChar.toLatin1() >= 'A' ? hexChar.toLatin1() - 'A' + 10 : charPressed.digitValue();
                
                if (nibbleVal == 0) { 
                    byte = (byte & 0x0F) | (value << 4);
                } else { 
                    byte = (byte & 0xF0) | value;
                }
                
                m_data[byteIndex] = byte;
                emit dataChanged(); 
                setCursorPosition(m_cursorPos + 1); 
                return; 
            }
        }
        
        if (m_editMode == AsciiMode) {
            if (charPressed.isPrint()) {
                char asciiValue = charPressed.toLatin1();
                
                m_data[byteIndex] = asciiValue;
                emit dataChanged();
                
                setCursorPosition(m_cursorPos + 2); 
                return; 
            }
        }
    }

    QAbstractScrollArea::keyPressEvent(event);
}

void HexEditorArea::setCursorPosition(int newPos) {
    newPos = std::min(newPos, m_data.size() * 2);
    newPos = std::max(newPos, 0);
    
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
// --- Interacción con el Ratón ---
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
    
    // Lógica de Clic en Área HEX
    const int HEX_MAX_X = m_asciiStartCol - 3 * m_charWidth;
    if (colX >= m_hexStartCol && colX < HEX_MAX_X) {
        m_editMode = HexMode;
        int relX = colX - m_hexStartCol;
        int charIndex = relX / m_charWidth; 
        byteInLine = charIndex / 3; 
        
        int nibbleInByte = (charIndex % 3 == 0) ? 0 : 1; 
        
        int newPos = (offset + byteInLine) * 2 + nibbleInByte;
        setCursorPosition(newPos);
        return;
    }
    
    // Lógica de Clic en Área ASCII
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