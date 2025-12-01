#ifndef HEXEDITORAREA_H
#define HEXEDITORAREA_H

#include <QAbstractScrollArea>
#include <QByteArray>
#include <QFont>
#include <QKeyEvent> 
#include <QString> 

class HexEditorArea : public QAbstractScrollArea
{
    Q_OBJECT
public:
    explicit HexEditorArea(QWidget *parent = nullptr);

    void setHexData(const QByteArray &data);
    QByteArray hexData() const;
    
    // Función para establecer el mapa de caracteres
    void setCharMapping(const QString (&mapping)[256]); 
    void goToOffset(quint64 offset); 
    
    int byteIndexAt(const QPoint &point) const;
    void updateViewMetrics();

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    enum EditMode { 
        HexMode,
        AsciiMode
    };
    
    QByteArray m_data;
    int m_cursorPos = 0; 
    EditMode m_editMode = HexMode; 
    QString m_charMap[256]; // Mapa de caracteres para la visualización ASCII
    
    int m_charWidth = 0;
    int m_charHeight = 0;
    const int m_bytesPerLine = 16;
    int m_hexStartCol = 0; 
    int m_asciiStartCol = 0; 
    int m_lineLength = 0; 

    void calculateMetrics(); 
    void setCursorPosition(int newPos);
    
signals:
    void dataChanged();
};

#endif // HEXEDITORAREA_H