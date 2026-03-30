#include "mark-list.hpp"
#include <limits>

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

void MarkList::undo_mark()
{
    std::lock_guard<std::mutex> lock(mutex);
    if (!marks.empty()) {
        marks.pop_back();
    }
}

void MarkList::delete_closest(float x, float y, float threshold)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (marks.empty())
        return;

    float min_dist = std::numeric_limits<float>::max();
    size_t min_idx = 0;

    for (size_t i = 0; i < marks.size(); ++i) {
        float d = marks[i]->distance_to(x, y);
        if (d < min_dist) {
            min_dist = d;
            min_idx  = i;
        }
    }

    if (min_dist < threshold) {
        marks.erase(marks.begin() + static_cast<ptrdiff_t>(min_idx));
    }
}
