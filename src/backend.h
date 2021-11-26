#pragma once

#include <player/playera.hpp>

#include <QCoreApplication>
#include <QString>

#include <memory>
#include <vector>

class Metadata;
class Job;

class JobHandle {
    std::shared_ptr<Job> _job;

public:
    ~JobHandle();
    void cancel();
};

class Backend {
    Q_DECLARE_TR_FUNCTIONS(Backend)

    QByteArray _file_data;
    std::unique_ptr<Metadata> _metadata;
    std::vector<JobHandle> _channels;

public:
    Backend();
    ~Backend();

    /// If non-empty, holds error message.
    QString load_path(QString path);
};

