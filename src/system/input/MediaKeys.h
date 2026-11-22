// SPDX-License-Identifier: GPL-3.0-only
/*
 * Copyright (C) 2022-present, PenUniverse.
 * This file is part of the PenMods open source project.
 */

#pragma once

#include "common/service/Logger.h"
#include "common/service/Singleton.h"

#include <QObject>
#include <QString>
#include <QTimer>

namespace mod {

/// 蓝牙耳机 AVRCP 按键控制音乐。
/// BlueZ 5.50 的 AVRCP TG 会为已连接的耳机创建 uinput 虚拟输入设备
/// （设备名即耳机 MAC），把耳机的 播放/暂停/上一首/下一首 转成内核按键事件：
///   KEY_PLAYCD(200) / KEY_PAUSECD(201) / KEY_NEXTSONG(163) / KEY_PREVIOUSSONG(165)
/// 本模块监听该设备并驱动 MusicPlayer。
class MediaKeys : public QObject, public Singleton<MediaKeys>, private Logger {
    Q_OBJECT

public:
    explicit MediaKeys();

private:
    void rescan();
    void openDevice(const QString& eventName);
    void closeDevice();
    void poll();
    void onKey(uint16_t code);

    QTimer*     mScanTimer{nullptr};
    QTimer*     mPollTimer{nullptr};
    int         mFd{-1};
    QString     mEventName;
};

} // namespace mod
