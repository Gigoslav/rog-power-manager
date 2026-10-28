#pragma once

#include <QObject>
#include <QLibrary>
#include <QMap>

class IdleManager : public QObject {
    Q_OBJECT
public:
    explicit IdleManager(QObject *parent = nullptr);

    void updateState(const QString &state);
    void rearm();

private slots:
    void onTimeoutReached(int id, int timeout);
    void onResumingFromIdle();

private:
    void initKIdleTime();

    QString m_currentState = "battery";
    bool m_available = false;
    bool m_isDimmed = false;
    bool m_isDpmsOff = false;
    int m_savedBrightness = -1;

    QLibrary m_lib;
    void *m_inst = nullptr;
    QObject *m_kIdleTimeObj = nullptr;

    typedef void* (*InstanceFn)();
    typedef int (*AddTimeoutFn)(void*, int);
    typedef void (*RemoveAllFn)(void*);
    typedef void (*CatchResumeFn)(void*);

    AddTimeoutFn m_addTimeout = nullptr;
    RemoveAllFn m_removeAll = nullptr;
    CatchResumeFn m_catchResume = nullptr;

    QMap<int, QString> m_timeoutActions;
};
