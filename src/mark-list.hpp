#pragma once
#include "marks/mark.hpp"
#include <memory>
#include <mutex>
#include <vector>

struct MarkList {
    std::vector<std::unique_ptr<Mark>> marks;
    std::unique_ptr<Mark>              in_progress;
    mutable std::mutex                 mutex;

    void begin_mark(std::unique_ptr<Mark> mark);
    void commit_mark();
    void clear_all();
};
