#include "display.h"
#include "ui_display.h"

#include <QByteArray>
#include <QUdpSocket>
#include <QDateTime>
#include <QPainter>
#include <QPixmap>

QUdpSocket s;
QPainter *p;
int pmp = 0;

mDisplay::mDisplay(QWidget *parent) :
    QMainWindow(parent),
    pic(500, 128),
    ui(new Ui::Display)
{
    s.bind(8888);

    pic.fill(Qt::white);
    p = new QPainter(&pic);

    connect(&s, SIGNAL(readyRead()), this, SLOT(udpIn()));

    ui->setupUi(this);
    ui->statusBar->showMessage("No UDP packets received.");
    ui->treeWidget->setColumnWidth(0, 200);
    ui->label->setPixmap(pic);
    ui->frame->hide();
}

#define R2(data, x) ((unsigned char)data.at(x) + ((unsigned char)data.at(x+1) << 8))
#define R4(data, x) (quint32)(R2(data, x) + (R2(data, x + 2) << 16))
#define INV2(data, x) (R2(data, x) == 0xFFFF)

#define r2(data, x) ((unsigned char)data.at(x++) + ((unsigned char)data.at(x++) << 8))
#define r4(data, x) (quint32)(r2(data, x) + (r2(data, x) << 16))

QTreeWidgetItem* chans[12];

void mDisplay::udpIn()
{
    while (s.hasPendingDatagrams())
    {
        QByteArray data;
        data.resize(522);

        QHostAddress addr;
        quint16 port, len;

        data.resize(s.readDatagram(data.data(), 522, &addr, &port));
        int magicNumber = (unsigned char)data.at(0);

        if (magicNumber != 0xF7)
        {
            ui->statusBar->showMessage("Invalid packet from device: " + QDateTime::currentDateTime().toString() + " from: " + addr.toString() + ":" + QString::number(port));
            continue;
        }

        len = R2(data, 2);

        /*if (data.at(1) == 4)
        {
            float vbat = R2(data, 4);
            float curr = R2(data, 6);
            p->setPen(Qt::white);
            p->drawLine(pmp, 0, pmp, 127);
            p->drawLine((pmp+1)%500, 0, (pmp+1)%500, 127);
            p->drawLine((pmp+2)%500, 0, (pmp+2)%500, 127);
            p->drawLine((pmp+3)%500, 0, (pmp+3)%500, 127);
            p->setPen(Qt::black);
            p->drawPoint(pmp, curr / 8);
            p->setPen(Qt::darkRed);
            p->drawPoint(pmp, vbat / 8);
            ui->label->setPixmap(pic);
            ui->label_2->setText(QString::number(pmp) + ": " + QString::number(vbat) + ": " + QString::number(curr));
            ++pmp %= 500;
        }

        else*/ if (data.at(1) == 4)
        {
            static QByteArray fdata;

            quint16 id, part, parts;
            id = R2(data, 4);
            part = R2(data, 6);
            parts = R2(data, 8);

            if (part == 0)
                fdata.clear();

            fdata.append(data.mid(10));

            ui->txt->append(QString("In: %1 - %2/%3 - %4 - %5/%6 bytes:").arg(id).arg(part+1).arg(parts).arg(fdata.size() == len - 6 ? "valid" : "invalid").arg(len - 6).arg(fdata.size()));

            if (part == parts - 1)
                ui->txt->append(fdata);

            ui->txt->append("");
        }

        else if (data.at(1) == 1)
        {
            if (pingNumber == R2(data, 4))
                ui->pong->setText(QString::number(pingTime.elapsed()) + " ms");
        }

        // CMDU_SYSLOG
        else if (data.at(1) == 19)
        {
            ui->txt->append(QString::fromAscii(data.mid(4, R2(data, 2))));
        }


        else if (data.at(1) == 0x11) // CMDU_GOT_PART
        {
            quint8 id;
            quint16 part, parts;
            qint16 bytes;
            id = data.at(4);
            part = R2(data, 5);
            parts = R2(data, 7);
            bytes = R2(data, 9);

            if (id != sendFileId)
                continue;

            if (part != sendFilePart)
                continue;

            ui->txt->append("Got part " + QString::number(part+1) + " (" + QString::number(bytes) + " written)");
            if (part != parts)
            {
                sendFilePart++;
                sendPart();
            }

            else
                ui->txt->append("Transfer completed");
        }

        else if (data.at(1) == 2)
        {
            unsigned char pin, state, value;
            pin = data.at(4);
            state = data.at(5);
            value = data.at(6);

            chans[pin]->setText(1, value == 0 ? "off" : "on");
        }

        if (data.at(1) != 3)
            continue;

        if (len != data.size() - 4)
        {
            ui->statusBar->showMessage("Invalid packet length from device: " + QDateTime::currentDateTime().toString() + " from: " + addr.toString() + ":" + QString::number(port));
            continue;
        }

        ui->statusBar->showMessage("Last message from device: " + QDateTime::currentDateTime().toString() + " from: " + addr.toString() + ":" + QString::number(port));

        int nChannels = data.at(4);

        QTreeWidgetItem *top = new QTreeWidgetItem(QStringList() << "Device" << (addr.toString() + ":" + QString::number(port)));
        ui->treeWidget->clear();
        ui->treeWidget->addTopLevelItem(top);

        QTreeWidgetItem *channels = new QTreeWidgetItem(QStringList() << "Channels" << QString::number(nChannels));
        top->addChild(channels);

        for (int i = 0; i < nChannels; i++)
        {
            QTreeWidgetItem *channel = new QTreeWidgetItem(QStringList() << "Channel " + QString::number(i) << (data.at(i + 5) ? "on" : "off"));
            channel->setData(1, Qt::UserRole, i);
            chans[i] = channel;
            channel->setExpanded(true);
            channels->addChild(channel);
        }

        channels->setExpanded(true);

        float vbat = (int)R2(data, nChannels + 5) * 0.01f;
        float current = (qint16)R2(data, nChannels + 7) * 0.001f;

        QTreeWidgetItem *it = new QTreeWidgetItem(QStringList() << "Battery Voltage" << QString::number(vbat) + " V");
        it->setExpanded(true);
        top->addChild(it);

        it = new QTreeWidgetItem(QStringList() << "Current" << QString::number(current) + " A");
        it->setExpanded(true);
        top->addChild(it);

        int nSensors = data.at(nChannels + 9);

        QTreeWidgetItem *sensors = new QTreeWidgetItem(QStringList() << "OneWire Sensors" << QString::number(nSensors));
        top->addChild(sensors);

        for (int i = 0; i < nSensors; i++)
        {
            QTreeWidgetItem *sensor = new QTreeWidgetItem(QStringList() << "Sensor " + QString::number(i) << "DS18B20");

            QTreeWidgetItem *data1 = new QTreeWidgetItem(QStringList() << "Address" << QString("%1:%2")
                                                         .arg(R4(data, nChannels + 10 + i * 13), 8, 16)
                                                         .arg(R4(data, nChannels + 10 + i * 13 + 4), 8, 16));

            sensor->addChild(data1);
            data1->setExpanded(true);

            QTreeWidgetItem *data2 = new QTreeWidgetItem(QStringList() << "Temperature" << (INV2(data, nChannels + 10 + i * 13 + 8) ? "invaild" : QString::number((qint16)R2(data, nChannels + 10 + i * 13 + 8) * 0.1f) + " C"));
            sensor->addChild(data2);
            data2->setExpanded(true);

            QTreeWidgetItem *data3 = new QTreeWidgetItem(QStringList() << "Resolution" << QString::number(9 + (((unsigned char)data.at(nChannels + 10 + i * 13 + 12) >> 5) & 3)) + " bits");
            sensor->addChild(data3);
            data3->setExpanded(true);

            sensors->addChild(sensor);
            sensor->setExpanded(true);
        }

        sensors->setExpanded(true);


        int nRoles = data.at(nChannels + 10 + nSensors * 13);

        QTreeWidgetItem *roles = new QTreeWidgetItem(QStringList() << "OneWire Roles" << QString::number(nRoles));
        top->addChild(roles);

        for (int i = 0; i < nRoles; i++)
        {
            QTreeWidgetItem *role = new QTreeWidgetItem(QStringList() << "Role " + QString::number(i) << "");

            QTreeWidgetItem *data1 = new QTreeWidgetItem(QStringList() << "Address" << QString("%1:%2")
                                                         .arg(R4(data, nChannels + 11 + nSensors * 13 + i * 15), 8, 16)
                                                         .arg(R4(data, nChannels + 11 + nSensors * 13 + i * 15 + 4), 8, 16));

            role->addChild(data1);
            data1->setExpanded(true);

            QTreeWidgetItem *data2 = new QTreeWidgetItem(QStringList() << "Role" << QString::number(data.at(nChannels + 11 + nSensors * 13 + i * 15 + 8)));
            role->addChild(data2);
            data2->setExpanded(true);

            QTreeWidgetItem *data3;

            data3 = new QTreeWidgetItem(QStringList() << "Param 1" << QString::number((qint16)R2(data, nChannels + 11 + nSensors * 13 + i * 15 + 9)));
            role->addChild(data3);
            data3->setExpanded(true);

            data3 = new QTreeWidgetItem(QStringList() << "Param 2" << QString::number((qint16)R2(data, nChannels + 11 + nSensors * 13 + i * 15 + 11)));
            role->addChild(data3);
            data3->setExpanded(true);

            data3 = new QTreeWidgetItem(QStringList() << "Param 3" << QString::number((qint16)R2(data, nChannels + 11 + nSensors * 13 + i * 15 + 13)));
            role->addChild(data3);
            data3->setExpanded(true);

            roles->addChild(role);
            role->setExpanded(true);
        }
        roles->setExpanded(true);

        quint16 lastPong = R2(data, nChannels + 11 + nSensors * 13 + nRoles * 15);
        float vbatTC = (qint16)R2(data, nChannels + 11 + nSensors * 13 + nRoles * 15 + 2) * 10;

        it = new QTreeWidgetItem(QStringList() << "Battery Voltage (TC)" << QString::number(vbatTC) + " mV");
        it->setExpanded(true);
        top->addChild(it);

        it = new QTreeWidgetItem(QStringList() << "Last pong (LR)" << (lastPong == 0xFFFF ? "timeout" : (QString::number(lastPong) + " ms")));
        it->setExpanded(true);
        top->addChild(it);

        QStringList states;
        states
                << "STATE_BOOTUP"
                << "STATE_POWERSAFE"
                << "STATE_POWERSAFE_COMMUNICATING"
                << "STATE_NORMAL"
                << "STATE_OVERPOWER";

        quint8 v = data.at(nChannels + 10 + nSensors * 13 + 1 + nRoles * 15 + 4);
        quint8 chargerState = data.at(nChannels + 10 + nSensors * 13 + 1 + nRoles * 15 + 5);

        it = new QTreeWidgetItem(QStringList() << "Device state" << (v < states.size() ? states[v] + " (" + QString::number(v) + ")" : "invalid/unkown"));
        it->setExpanded(true);
        top->addChild(it);

        it = new QTreeWidgetItem(QStringList() << "Charger state" << (chargerState < states.size() ? states[chargerState] + " (" + QString::number(chargerState) + ")" : "invalid/unkown"));
        it->setExpanded(true);
        top->addChild(it);


        QTreeWidgetItem *charger = new QTreeWidgetItem(QStringList() << "Charger Status" << "");
        top->addChild(charger);

        int pos = nChannels + 10 + nSensors * 13 + 1 + nRoles * 15 + 6;
        v = data.at(pos++);

        it = new QTreeWidgetItem(QStringList() << "Device state" << (v < states.size() ? states[v] + " (" + QString::number(v) + ")" : "invalid/unkown"));
        it->setExpanded(true);
        charger->addChild(it);

        v = data.at(pos++);

        QStringList outputNames;
        outputNames << "Main"
                    << "Output 1"
                    << "Output 2"
                    << "Wind Short"
                    << "Current Sensors";

        QTreeWidgetItem *mosfets = new QTreeWidgetItem(QStringList() << "Output States" << "5");
        charger->addChild(mosfets);
        for (int i = 0; i < 5; i++)
        {
            it = new QTreeWidgetItem(QStringList() << outputNames.at(i) << ((v & (1 << i)) ? "on" : "off"));
            it->setExpanded(true);
            mosfets->addChild(it);

        }

        mosfets->setExpanded(true);

        quint32 m = r4(data, pos);
        int days = m / 86400000;
        it = new QTreeWidgetItem(QStringList() << "millis()" << QString::number(m) + "ms / " + QString::number(days) + " days");
        it->setExpanded(true);
        charger->addChild(it);

        float vv = r2(data, pos) * 0.01f;
        it = new QTreeWidgetItem(QStringList() << "Vbat" << QString::number(vv) + "V");
        it->setExpanded(true);
        charger->addChild(it);

        vv = r2(data, pos) * 0.01f;
        it = new QTreeWidgetItem(QStringList() << "Vgen" << QString::number(vv) + "V");
        it->setExpanded(true);
        charger->addChild(it);

        vv = r2(data, pos) * 0.1f;
        it = new QTreeWidgetItem(QStringList() << "Battery Temperature" << QString::number(vv) + " C");
        it->setExpanded(true);
        charger->addChild(it);

        QTreeWidgetItem *currents = new QTreeWidgetItem(QStringList() << "Input Currents" << "");
        charger->addChild(currents);

        for (int i = 0; i < 6; i++)
        {
            vv = (qint16)r2(data, pos) * 0.001f;
            it = new QTreeWidgetItem(QStringList() << "Channel " + QString::number(i + 1) << QString::number(vv) + " A");
            it->setExpanded(true);
            currents->addChild(it);
        }

        currents->setExpanded(true);
        charger->setExpanded(true);


        top->setExpanded(true);
    }
}

mDisplay::~mDisplay()
{
    delete ui;
}

void mDisplay::on_treeWidget_itemClicked(QTreeWidgetItem* item, int column)
{
    if (column != 1)
        return;

    if (item->data(1, Qt::UserRole).isNull())
        return;

    QByteArray d = QByteArray::fromHex("F70t50200");
    d.append(item->data(1, Qt::UserRole).toInt());
    d.append(item->text(1) == "off" ? 1 : 0);

    s.writeDatagram(d, QHostAddress("131.188.117.119"), 8888);
}

void mDisplay::on_pushButton_clicked()
{
    ui->txt->clear();
    QByteArray head = QByteArray::fromHex("F707");
    QByteArray body = QByteArray::fromHex("00100000");
    body.append((char)1);
    body.append((char)0);
    body.append("system.log");
    //body.append("temp.cfg");
    body.append((char)0);

    head.append((unsigned char)(body.size() & 0x00FF));
    head.append((unsigned char)((body.size() & 0xFF00) >> 8));
    head.append(body);

    s.writeDatagram(head, QHostAddress("131.188.117.119"), 8888);
}

void mDisplay::on_ping_clicked()
{
    QByteArray head = QByteArray::fromHex("F7000200");
    pingTime.start();
    head.append((unsigned char) (++pingNumber & 0x00FF));
    head.append((unsigned char)((  pingNumber & 0xFF00) >> 8));

    s.writeDatagram(head, QHostAddress("131.188.117.119"), 8888);
    ui->pong->setText("...");
}

void mDisplay::on_update_clicked()
{
    QByteArray head = QByteArray::fromHex("F70F0000");
    s.writeDatagram(head, QHostAddress("131.188.117.119"), 8888);
}

#include <QFileDialog>
#define ADD(head, u16) head.append((unsigned char) ((u16) & 0x00FF)); head.append((unsigned char)((  (u16) & 0xFF00) >> 8));
void mDisplay::on_sendf_clicked()
{
    sendFileName = QFileDialog::getOpenFileName();

    QFile f(sendFileName);
    f.open(QFile::ReadOnly);

    sendFileData = f.readAll();
    sendFilePart = 0;
    sendFileId++;

    sendPart();
}

void mDisplay::sendPart()
{
    quint16 parts = (sendFileData.size() - 1) / 1024;
    QString fn = QFileInfo(sendFileName).fileName().toAscii();
    ui->txt->append(QString("Sending part %1 of %2 of file %3").arg(sendFilePart+1).arg(parts+1).arg(fn));

    QByteArray head = QByteArray::fromHex("F710");
    QByteArray subdata = sendFileData.mid(sendFilePart * 1024, 1024);
    ADD(head, (quint16)subdata.size() + 5 + fn.size() + 1);

    head.append(sendFileId);
    ADD(head, sendFilePart);
    ADD(head, parts);
    head.append(fn);
    head.append((char)0);
    head.append(subdata);
    s.writeDatagram(head, QHostAddress("131.188.117.119"), 8888);
}

void mDisplay::on_clear_clicked()
{
    ui->txt->clear();
}















































