#include "mark-list.hpp"

void MarkList::begin_mark(std::unique_ptr<Mark> mark)
{
    std::lock_guard<std::mutex> lock(mutex);
    in_progress = std::move(mark);
}

void MarkList::commit_mark()
{
    std::lock_guard<std::mutex> lock(mutex);
    if (in_progress) {
        marks.push_back(std::move(in_progress));
        in_progress.reset();
    }
}

void MarkList::clear_all()
{
    std::lock_guard<std::mutex> lock(mutex);
    marks.clear();
    in_progress.reset();
}
