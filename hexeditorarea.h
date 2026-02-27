#ifndef HEXEDITORAREA_H
#define HEXEDITORAREA_H

#include <QAbstractScrollArea>
#include <QByteArray>
#include <QFont>
#include <QKeyEvent> 
#include <QString> 
#include <QMouseEvent>
#include <QMap>

class HexEditorArea : public QAbstractScrollArea
{
    Q_OBJECT
public:
    explicit HexEditorArea(QWidget *parent = nullptr);
    QSize minimumSizeHint() const override; 

    void setHexData(const QByteArray &data);
    QByteArray hexData() const;
    
    void setCharMapping(const QString (&mapping)[256]); 
    void setCharMapping16(const QMap<uint16_t, QString> &mapping16, bool bigEndian);
    void goToOffset(qint64 offset); 
    
    qint64 byteIndexAt(const QPoint &point) const;
    void updateViewMetrics();
    
    qint64 cursorPosition() const { return m_cursorPos; }
    void setCursorPosition(qint64 newPos);
    void setSelection(qint64 startPos, qint64 endPos);        
    
    qint64 selectionStart() const { return m_selectionStart; } 
    qint64 selectionEnd() const { return m_selectionEnd; } 
    
    void copySelection();
    void pasteFromClipboard();
    void pasteBytes(const QByteArray &data);
    void clearSelection();

signals:
    void dataChanged();
    void byteEdited(qint64 offset, quint8 oldByte, quint8 newByte);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;   
    void mouseReleaseEvent(QMouseEvent *event) override; 
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    enum EditMode { HexMode, AsciiMode };
    
    void calculateMetrics();
    void handleAsciiInput(const QString &text);
    void handleHexInput(const QString &text);
    void handleDelete();

    QByteArray m_data;
    qint64 m_cursorPos = 0;
    EditMode m_editMode = HexMode; 
    QString m_charMap[256];
    QMap<uint16_t, QString> m_charMap16;
    bool m_charMap16BigEndian = false; // false = little endian
    
    int m_charWidth = 0;
    int m_charHeight = 0;
    int m_lineLength = 0;
    const int m_bytesPerLine = 16;
    int m_hexStartCol = 0; 
    int m_asciiStartCol = 0; 
    
    qint64 m_selectionAnchor = -1; 
    qint64 m_selectionStart = -1;
    qint64 m_selectionEnd = -1;   
    int m_currentNibbleIndex = 0;
    qint64 m_hoverByteIndex = -1; // byte under mouse, for 16-bit pair hover highlight
};

#endif