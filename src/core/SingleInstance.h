#pragma once

#include <QObject>
#include <QString>

class QLocalServer;

namespace mr {

/// Single instance guard (TARGET.md §9.8).
///
/// The first instance owns a `QLocalServer`; later instances connect to it,
/// ask it to raise its main window and exit immediately.
class SingleInstance : public QObject
{
    Q_OBJECT

public:
    explicit SingleInstance(const QString &key, QObject *parent = nullptr);
    ~SingleInstance() override;

    /// True when this process is the first instance.
    bool isPrimary() const { return m_primary; }

    /// Asks the running instance to show its window. Returns true on success.
    bool notifyPrimary();

signals:
    /// Emitted in the primary instance when another instance asked for the UI.
    void activationRequested();

private:
    QString m_key;
    QLocalServer *m_server = nullptr;
    bool m_primary = false;
};

} // namespace mr
