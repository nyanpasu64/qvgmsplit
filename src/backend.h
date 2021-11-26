#pragma once

#include <player/playera.hpp>

#include <QString>

#include <memory>
#include <vector>

class Job;
class JobHandle;

class Backend {
    std::unique_ptr<Job> _master_audio;
    std::vector<std::unique_ptr<Job>> _channels;

public:
    Backend();
    ~Backend();
    void load_path(QString path);
};

