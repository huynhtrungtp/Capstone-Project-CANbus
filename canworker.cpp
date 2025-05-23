#include "canworker.h"
#include <QDebug>
#include <QByteArray>
#include <QDataStream>
#include <cstring>

CanWorker::CanWorker(QObject *parent)
    : QObject{parent}
{
}

void CanWorker::processCanDataRaw(const QByteArray &data)
{
    if (data.size() < 20) {
        qDebug() << "Dữ liệu nhận được không hợp lệ!";
        return;
    }

    int index = (uchar)data[3] << 8 | (uchar)data[2];
    int subindex = (uchar)data[5] << 8 | (uchar)data[4];
    QByteArray valueData = data.mid(6, 8);
    QString dataHex = valueData.toHex(' ').toUpper();

    qint64 valueDecimal = 0;
    for (int i = 0; i < 4; i++) {
        valueDecimal = (valueDecimal << 8) | (uchar)valueData[i];
    }

    float floatValue = 0.0f;
    if (valueData.size() >= 4) {
        QByteArray floatData = valueData.left(4);
        memcpy(&floatValue, floatData.data(), sizeof(float));
    }

    emit parsedCanDataReady(index, subindex, dataHex, QString::number(valueDecimal), floatValue);

    if (floatValue > -1000 && floatValue < 1000 && (floatValue < -0.001 || floatValue > 0.001)) {
        if (index == 0x01A0 && subindex == 0x0000) {
            emit parsedAngle(floatValue);
        } else if (index == 0x02A0 && subindex == 0x0000) {
            emit parsedId(floatValue);
        } else if (index == 0x03A0 && subindex == 0x0000) {
            emit parsedIq(id_tmp_local, floatValue);
        }
    }

    if (index == 0x02A0 && subindex == 0x0000) {
        id_tmp_local = floatValue;
    }
}
