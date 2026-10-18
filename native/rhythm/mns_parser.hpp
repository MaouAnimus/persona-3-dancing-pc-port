#pragma once
#include "rhythm_engine.hpp"
#include <string>
#include <vector>

namespace p3d::rhythm {

struct MnsHeader {
    char magic[4]{'M', 'N', 'S', 0};
    uint32_t unknown1{0};
    uint32_t version{1};
    uint32_t song_id{0};
    float bpm{120.0f};
    uint32_t song_id_repeat{0};
    uint32_t unknown2{0};
    uint32_t note_count{0};
};

class MnsParser {
public:
    // Load notes from an Atlus MNS binary file (dance/mns/mns*.bin)
    static bool load_file(const std::string& filepath, std::vector<Note>& out_notes, float& out_bpm);

    // Create a synthetic chart with regular beats for latency calibration and testing
    static std::vector<Note> create_synthetic_chart(float bpm, int measures = 16);
};

} // namespace p3d::rhythm
