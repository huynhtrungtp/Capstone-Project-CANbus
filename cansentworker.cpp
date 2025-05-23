#include "cansentworker.h"
#include <QTime>

CanWorker::CanWorker(QObject *parent) : QObject(parent), serial(nullptr) {}

void CanWorker::setSerial(QSerialPort *serialPort) {
    serial = serialPort;
}

void CanWorker::startListening() {
    if (!serial) return;
    connect(serial, &QSerialPort::readyRead, [this]() {
        QByteArray data = serial->readAll();
        emit canDataReceived(data);  // gửi sang MainWindow
    });
}
// //////////////hàm gửi dữ liệu /////////////////////////////////////////////////
void CanWorker::handleSendFrame(quint16 index, quint16 subindex, float value) {
    if (!serial) return;

    QByteArray frame;

    // Start Mark
    frame.append((char)0xAA);
    frame.append((char)0xAA);

    // Ghép Index và SubIndex thành 4 byte ID
    frame.append((char)(index & 0xFF));        // Byte thấp của index
    frame.append((char)((index >> 8) & 0xFF)); // Byte cao của index
    frame.append((char)(subindex & 0xFF));     // Byte thấp của subindex
    frame.append((char)((subindex >> 8) & 0xFF)); // Byte cao của subindex

    // Tạo frameData (Dữ liệu chính 8 byte)
    QByteArray frameData;

    // Chuyển đổi giá trị float thành 4 byte (IEEE 754)
    QByteArray valueBytes(reinterpret_cast<const char*>(&value), sizeof(float));

    // Thêm dữ liệu giá trị vào frameData
    frameData.append(valueBytes);

    // Đảm bảo frameData có đúng 8 byte
    while (frameData.size() < 8) {
        frameData.append((char)0x00);
    }

    // Ghép frameData vào Frame chính
    frame.append(frameData);

    // Các trường bổ sung
    frame.append((char)0x08); // Frame Data Length = 8 byte
    frame.append((char)0x00); // Message Type = 0x00 (CAN Message)
    frame.append((char)0x00); // CAN Frame Type = 0x01
    frame.append((char)0x00); // CAN Request Type = 0x00 (Data Frame)

    // Bắt đầu tính CRC (Không tính FrameCtrl vào CRC)
    QByteArray crcData = frame.mid(2, 16); // Lấy dữ liệu cần tính CRC (bỏ 2 byte Start Mark)

    // Kiểm tra xem có 0xA5 / 0xAA / 0x55 trong phạm vi tính CRC không
    bool insertFrameCtrl = false;
    for (char byte : crcData) {
        if (byte == 0xA5 || byte == 0xAA || byte == 0x55) {
            insertFrameCtrl = true;
            break;
        }
    }

    // Tính toán CRC (bỏ FrameCtrl vào trong tính toán)
    uchar crc = 0;
    for (char byte : crcData) {
        crc += (uchar)byte; // Tính tổng
    }
    crc &= 0xFF;

    // Nếu CRC là 0xA5, 0xAA hoặc 0x55, chèn FrameCtrl
    if (insertFrameCtrl) {
        frame.append((char)0xA5); // Thêm FrameCtrl vào
    }

    // Thêm CRC vào cuối frame
    frame.append(crc);

    // End Mark
    frame.append((char)0x55);
    frame.append((char)0x55);

    // **Gửi dữ liệu qua COM**
    if (serial->write(frame) && serial->waitForBytesWritten(1000)) // Chờ gửi xong
    {
        QString sentData = frame.toHex(' ').toUpper();
        qDebug() << "Sent Data (HEX): " << sentData;
    }
    else
    {
        qDebug() << "Loi khi gui du lieu!";
    }

    emit logMessage("Sent: " + frame.toHex(' ').toUpper());
}

void CanWorker::sendPIDFrame(quint16 index, float kp, float ki, float kd) {
    if (!serial) return;

    QByteArray frame;

    // Start Mark
    frame.append((char)0xAA);
    frame.append((char)0xAA);

    // Index
    frame.append((char)(index & 0xFF));
    frame.append((char)((index >> 8) & 0xFF));

    // SubIndex mặc định là 0x0000
    frame.append((char)0x00);
    frame.append((char)0x00);

    // ----- Tạo frameData 6 byte (uint16 scaled) -----
    auto encodeFloat = [](float val) -> QByteArray {
        quint16 raw = static_cast<quint16>(val * 100.0f);
        QByteArray b;
        b.append(static_cast<quint8>(raw & 0xFF));
        b.append(static_cast<quint8>((raw >> 8) & 0xFF));
        return b;
    };

    QByteArray frameData;
    frameData.append(encodeFloat(kp));
    frameData.append(encodeFloat(ki));
    frameData.append(encodeFloat(kd));

    // Đảm bảo đủ 8 byte
    while (frameData.size() < 8)
        frameData.append((char)0x00);

    frame.append(frameData);

    // Frame Meta
    frame.append((char)0x08); // Length
    frame.append((char)0x00); // Message Type
    frame.append((char)0x00); // CAN Frame Type
    frame.append((char)0x00); // Request Type

    // ----- CRC -----
    QByteArray crcData = frame.mid(2, 16);

    bool insertFrameCtrl = false;
    for (char byte : crcData) {
        if (byte == 0xA5 || byte == 0xAA || byte == 0x55) {
            insertFrameCtrl = true;
            break;
        }
    }

    uchar crc = 0;
    for (char byte : crcData) {
        crc += (uchar)byte;
    }
    crc &= 0xFF;

    if (insertFrameCtrl)
        frame.append((char)0xA5);

    frame.append(crc);

    // End Mark
    frame.append((char)0x55);
    frame.append((char)0x55);

    // Gửi
    if (serial->write(frame) && serial->waitForBytesWritten(1000)) {
        qDebug() << "Sent PID Frame (HEX): " << frame.toHex(' ').toUpper();
    } else {
        qDebug() << "Lỗi khi gửi PID Frame!";
    }

    emit logMessage("Sent PID: " + frame.toHex(' ').toUpper());
}
