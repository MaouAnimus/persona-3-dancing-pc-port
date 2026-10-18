#include "rhythm_engine.hpp"
#include <cmath>
#include <algorithm>

namespace p3d::rhythm {

void RhythmEngine::load_notes(const std::vector<Note>& notes) {
    notes_ = notes;
    // Ensure notes are sorted strictly by target time
    std::sort(notes_.begin(), notes_.end(), [](const Note& a, const Note& b) {
        return a.target_time_ms < b.target_time_ms;
    });
    reset();
}

void RhythmEngine::reset() {
    active_note_index_ = 0;
    stats_ = RhythmStats{};
    last_time_ms_ = 0.0;
    for (auto& n : notes_) {
        n.judged = false;
        n.result = Judgment::None;
        n.hit_time_ms = 0.0;
        n.time_diff_ms = 0.0;
    }
}

Judgment RhythmEngine::evaluate_diff(double diff_ms) const {
    double abs_diff = std::abs(diff_ms);
    if (abs_diff <= TIMING_PERFECT_MS) return Judgment::Perfect;
    if (abs_diff <= TIMING_GREAT_MS)   return Judgment::Great;
    if (abs_diff <= TIMING_GOOD_MS)    return Judgment::Good;
    return Judgment::Miss;
}

void RhythmEngine::update(double current_song_time_ms) {
    double dt_sec = (last_time_ms_ > 0.0 && current_song_time_ms > last_time_ms_) ? (current_song_time_ms - last_time_ms_) * 0.001 : 0.016;
    last_time_ms_ = current_song_time_ms;

    if (stats_.fever_active) {
        stats_.fever_gauge -= static_cast<float>(dt_sec * 6.67f); // ~15s duration
        if (stats_.fever_gauge <= 0.0f) {
            stats_.fever_gauge = 0.0f;
            stats_.fever_active = false;
        }
    }

    // Check for missed notes that passed beyond the Good timing window
    for (size_t i = active_note_index_; i < notes_.size(); ++i) {
        auto& note = notes_[i];
        if (note.judged) continue;

        double diff = current_song_time_ms - note.target_time_ms;
        if (diff > TIMING_GOOD_MS) {
            // Note passed without input -> Miss
            note.judged = true;
            note.result = Judgment::Miss;
            note.time_diff_ms = diff;
            stats_.miss_count++;
            stats_.combo = 0;
            // Reduce fever and groove gauge on miss
            stats_.fever_gauge = std::max(0.0f, stats_.fever_gauge - 5.0f);
            stats_.groove_gauge = std::max(0.0f, stats_.groove_gauge - 4.5f);

            if (on_judgment_) {
                on_judgment_(note, Judgment::Miss);
            }
        } else {
            // Since notes are sorted by target time, future notes haven't missed yet
            break;
        }
    }

    // Advance active note index
    while (active_note_index_ < notes_.size() && notes_[active_note_index_].judged) {
        active_note_index_++;
    }
}

Judgment RhythmEngine::on_lane_input(NoteLane lane, double input_time_ms) {
    // Find the closest unjudged note in this lane within the valid timing window
    int best_index = -1;
    double min_diff = 1e9;

    for (size_t i = active_note_index_; i < notes_.size(); ++i) {
        auto& note = notes_[i];
        if (note.judged) continue;
        if (note.lane != lane) continue;

        double diff = input_time_ms - note.target_time_ms;
        double abs_diff = std::abs(diff);

        // If note is too far in the future (> GOOD window), stop checking this lane
        if (note.target_time_ms - input_time_ms > TIMING_GOOD_MS) {
            break;
        }

        if (abs_diff < min_diff && abs_diff <= TIMING_GOOD_MS) {
            min_diff = abs_diff;
            best_index = static_cast<int>(i);
        }
    }

    if (best_index < 0) {
        return Judgment::None; // Stray tap or outside window
    }

    auto& note = notes_[best_index];
    double diff = input_time_ms - note.target_time_ms;
    Judgment j = evaluate_diff(diff);

    note.judged = true;
    note.result = j;
    note.hit_time_ms = input_time_ms;
    note.time_diff_ms = diff;

    // Update scoring, groove gauge, and combo
    uint32_t base_score = 0;
    if (j == Judgment::Perfect) {
        stats_.perfect_count++;
        stats_.combo++;
        base_score = 1000;
        stats_.fever_gauge = std::min(100.0f, stats_.fever_gauge + ((lane == NoteLane::Scratch) ? 20.0f : 2.5f));
        stats_.groove_gauge = std::min(100.0f, stats_.groove_gauge + 1.2f);
    } else if (j == Judgment::Great) {
        stats_.great_count++;
        stats_.combo++;
        base_score = 700;
        stats_.fever_gauge = std::min(100.0f, stats_.fever_gauge + ((lane == NoteLane::Scratch) ? 15.0f : 1.5f));
        stats_.groove_gauge = std::min(100.0f, stats_.groove_gauge + 0.8f);
    } else if (j == Judgment::Good) {
        stats_.good_count++;
        stats_.combo++; // In normal P3D, Good maintains combo
        base_score = 400;
        stats_.groove_gauge = std::min(100.0f, stats_.groove_gauge + 0.2f);
    } else {
        stats_.miss_count++;
        stats_.combo = 0;
        base_score = 0;
        stats_.fever_gauge = std::max(0.0f, stats_.fever_gauge - 5.0f);
        stats_.groove_gauge = std::max(0.0f, stats_.groove_gauge - 4.5f);
    }

    if (!stats_.fever_active && stats_.fever_gauge >= 100.0f) {
        stats_.fever_active = true;
    }

    stats_.max_combo = std::max(stats_.max_combo, stats_.combo);

    // Multiplier calculation based on combo
    uint32_t multiplier = 1;
    if (stats_.combo >= 50) multiplier = 4;
    else if (stats_.combo >= 30) multiplier = 3;
    else if (stats_.combo >= 10) multiplier = 2;

    if (stats_.fever_active) {
        multiplier *= 2;
    }

    stats_.score += base_score * multiplier;

    if (on_judgment_) {
        on_judgment_(note, j);
    }

    return j;
}

} // namespace p3d::rhythm
