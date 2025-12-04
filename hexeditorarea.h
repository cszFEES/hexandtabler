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
    
    void setCharMapping(const QString (&mapping)[256]); 
    void goToOffset(quint64 offset); 
    
    int byteIndexAt(const QPoint &point) const;
    void updateViewMetrics();
    
    int cursorPosition() const { return m_cursorPos; }
    void setSelection(int startPos, int endPos);        
    
    int selectionStart() const { return m_selectionStart; } 
    int selectionEnd() const { return m_selectionEnd; } 
    
    void copySelection();                                  
    void pasteFromClipboard();

signals:
    void dataChanged();

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
    int m_cursorPos = 0;
    EditMode m_editMode = HexMode; 
    QString m_charMap[256]; 
    
    int m_charWidth = 0;
    int m_charHeight = 0;
    const int m_bytesPerLine = 16;
    int m_hexStartCol = 0; 
    int m_asciiStartCol = 0; 
    int m_lineLength = 0; 
    
    int m_selectionAnchor = -1; 
    int m_selectionStart = -1;
    int m_selectionEnd = -1;   
    

    void calculateMetrics(); 
    void setCursorPosition(int newPos); 
    void clearSelection();

    void handleAsciiInput(const QString &text); 
    void handleDelete();
    int nibbleIndexAt(const QPoint &point, bool *isHexArea) const; 
    void handleHexInput(const QString &text); 
};

#endif