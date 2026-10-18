#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <functional>

namespace p3d::rhythm {

// Note target lanes matching Persona 3/5 Dancing radial layout
enum class NoteLane : uint8_t {
    LeftUp = 0,    // D-Pad Up / Keyboard W
    LeftLeft = 1,  // D-Pad Left / Keyboard A
    LeftDown = 2,  // D-Pad Down / Keyboard S
    RightUp = 3,   // Triangle / Keyboard I
    RightRight = 4,// Circle / Keyboard L
    RightDown = 5, // Cross / Keyboard K
    Scratch = 6    // Analog Stick flick / Spacebar / Bumpers
};

enum class Judgment : uint8_t {
    None = 0,
    Perfect,
    Great,
    Good,
    Miss
};

enum class NoteType : uint8_t {
    Single = 0,
    Hold,
    Unison,
    Scratch,
    FeverScratch
};

struct Note {
    uint32_t id{0};
    NoteLane lane{NoteLane::LeftUp};
    NoteType type{NoteType::Single};
    double target_time_ms{0.0}; // Absolute timestamp in song (ms)
    double hold_duration_ms{0.0};
    
    // Runtime state
    bool judged{false};
    Judgment result{Judgment::None};
    double hit_time_ms{0.0};
    double time_diff_ms{0.0};
};

struct RhythmStats {
    uint32_t perfect_count{0};
    uint32_t great_count{0};
    uint32_t good_count{0};
    uint32_t miss_count{0};
    uint32_t combo{0};
    uint32_t max_combo{0};
    uint32_t score{0};
    float fever_gauge{0.0f}; // 0.0 to 100.0%
    bool fever_active{false};
    float groove_gauge{50.0f}; // 0.0 to 100.0%, starts at 50%
    
    float accuracy() const {
        uint32_t total = perfect_count + great_count + good_count + miss_count;
        if (total == 0) return 100.0f;
        float weighted = (perfect_count * 1.0f) + (great_count * 0.7f) + (good_count * 0.4f);
        return (weighted / total) * 100.0f;
    }
};

class RhythmEngine {
public:
    // P3D Authentic Timing Windows (ms)
    static constexpr double TIMING_PERFECT_MS = 35.0;
    static constexpr double TIMING_GREAT_MS   = 75.0;
    static constexpr double TIMING_GOOD_MS    = 125.0;
    static constexpr double TIMING_MISS_MS    = 135.0;

    RhythmEngine() = default;

    void load_notes(const std::vector<Note>& notes);
    void reset();

    // Call each frame with current song audio clock position (ms)
    void update(double current_song_time_ms);

    // Process user input tap on a lane at a specific timestamp
    Judgment on_lane_input(NoteLane lane, double input_time_ms);

    // Get current statistics
    const RhythmStats& stats() const { return stats_; }
    const std::vector<Note>& notes() const { return notes_; }
    size_t active_note_index() const { return active_note_index_; }

    // Event callback
    using JudgmentCallback = std::function<void(const Note& note, Judgment judgment)>;
    void set_judgment_callback(JudgmentCallback cb) { on_judgment_ = cb; }

private:
    std::vector<Note> notes_;
    size_t active_note_index_{0};
    RhythmStats stats_{};
    JudgmentCallback on_judgment_{nullptr};
    double last_time_ms_{0.0};

    Judgment evaluate_diff(double diff_ms) const;
};

} // namespace p3d::rhythm
