#ifndef DISPLAY_H
#define DISPLAY_H

#include <QMainWindow>
#include <QPixmap>
#include <QTreeWidgetItem>
#include <QTime>

namespace Ui {
    class Display;
}

class mDisplay : public QMainWindow
{
    Q_OBJECT

public:
    explicit mDisplay(QWidget *parent = 0);
    ~mDisplay();

    QPixmap pic;

public slots:
    void udpIn();

private slots:
    void on_treeWidget_itemClicked(QTreeWidgetItem* item, int column);

    void on_pushButton_clicked();

    void on_ping_clicked();

    void on_update_clicked();

    void on_sendf_clicked();

    void on_clear_clicked();

private:
    qint16 pingNumber;
    QTime pingTime;
    Ui::Display *ui;
    QString sendFileName;
    QByteArray sendFileData;
    quint8 sendFileId;
    quint16 sendFilePart;
    void sendPart();
};

#endif // DISPLAY_H
