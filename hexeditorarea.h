#ifndef HEXEDITORAREA_H
#define HEXEDITORAREA_H

#include <QAbstractScrollArea>
#include <QByteArray>
#include <QFont>
#include <QKeyEvent> 
#include <QString> 
#include <QMouseEvent> 
#include <QKeySequence> 

class HexEditorArea : public QAbstractScrollArea
{
    Q_OBJECT
public:
    explicit HexEditorArea(QWidget *parent = nullptr);

    void setHexData(const QByteArray &data);
    QByteArray hexData() const;
    
    // Function to set the character map
    void setCharMapping(const QString (&mapping)[256]); 
    void goToOffset(quint64 offset); 
    
    int byteIndexAt(const QPoint &point) const;
    void updateViewMetrics();
    
    // --- MEMBERS ADDED FOR FIND/REPLACE AND SELECTION ---
    int cursorPosition() const { return m_cursorPos; } // Returns position in nibbles
    void setSelection(int startPos, int endPos);        
    int m_selectionEnd = -1;                            // Public member to access selection end (in nibbles)
    
protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;   
    void mouseReleaseEvent(QMouseEvent *event) override; 
    void resizeEvent(QResizeEvent *event) override;

private:
    enum EditMode { 
        HexMode,
        AsciiMode
    };
    
    QByteArray m_data;
    int m_cursorPos = 0; // Position in nibbles (0-based)
    EditMode m_editMode = HexMode; 
    QString m_charMap[256]; 
    
    int m_charWidth = 0;
    int m_charHeight = 0;
    const int m_bytesPerLine = 16;
    int m_hexStartCol = 0; 
    int m_asciiStartCol = 0; 
    int m_lineLength = 0; 
    
    int m_selectionStart = -1; // Nibble index of selection start
    // m_selectionEnd is public

    // Helper to calculate geometry
    void calculateMetrics(); 
    void setCursorPosition(int newPos); 
    void clearSelection();

    // <<<<<<<<<<<<<<<<< DECLARACIONES AÑADIDAS >>>>>>>>>>>>>>>>>>>
    void handleAsciiInput(const QString &text);
    void handleDelete();
    // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

signals:
    void dataChanged(); // Emitted when a change occurs
};

#endif // HEXEDITORAREA_H