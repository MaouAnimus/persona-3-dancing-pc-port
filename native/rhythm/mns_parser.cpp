#include "mns_parser.hpp"
#include <fstream>
#include <cstring>
#include <iostream>

namespace p3d::rhythm {

bool MnsParser::load_file(const std::string& filepath, std::vector<Note>& out_notes, float& out_bpm) {
    out_notes.clear();
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[MnsParser] Error: Could not open MNS file: " << filepath << std::endl;
        return false;
    }

    MnsHeader hdr;
    file.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (file.gcount() < static_cast<std::streamsize>(sizeof(hdr))) {
        std::cerr << "[MnsParser] Error: File too small for MNS header" << std::endl;
        return false;
    }

    if (std::memcmp(hdr.magic, "MNS", 3) != 0) {
        std::cerr << "[MnsParser] Error: Invalid MNS magic: " << hdr.magic[0] << hdr.magic[1] << hdr.magic[2] << std::endl;
        return false;
    }

    out_bpm = hdr.bpm > 0.0f ? hdr.bpm : 120.0f;
    double ms_per_beat = (60.0 / static_cast<double>(out_bpm)) * 1000.0;
    double ms_per_measure = ms_per_beat * 4.0; // 4/4 time signature standard

    // Each note record in section 1 is 8 bytes
    // Format: uint16_t lane_info, uint16_t type_info, uint16_t tick, uint16_t measure
    for (uint32_t i = 0; i < hdr.note_count; ++i) {
        uint16_t raw[4];
        file.read(reinterpret_cast<char*>(raw), sizeof(raw));
        if (file.gcount() < 8) break;

        uint16_t lane_val = raw[0];
        uint16_t type_val = raw[1];
        uint16_t tick     = raw[2];
        uint16_t measure  = raw[3];

        Note note;
        note.id = i;
        
        // Map lane_val to NoteLane (0..5 correspond to the 6 targets, 6 or 8+ to scratch)
        uint8_t lane_idx = static_cast<uint8_t>(lane_val % 7);
        note.lane = static_cast<NoteLane>(lane_idx);

        // Calculate timestamp from measure and tick (standard 480 or 4 ticks per beat)
        double measure_time = static_cast<double>(measure) * ms_per_measure;
        double beat_time = (static_cast<double>(tick) / 4.0) * ms_per_beat;
        note.target_time_ms = measure_time + beat_time;

        if (type_val == 1) {
            note.type = NoteType::Hold;
            note.hold_duration_ms = ms_per_beat;
        } else if (lane_idx == 6) {
            note.type = NoteType::Scratch;
        } else {
            note.type = NoteType::Single;
        }

        out_notes.push_back(note);
    }

    std::cout << "[MnsParser] Successfully loaded " << out_notes.size() 
              << " notes from " << filepath << " (BPM: " << out_bpm << ")" << std::endl;
    return true;
}

std::vector<Note> MnsParser::create_synthetic_chart(float bpm, int measures) {
    std::vector<Note> notes;
    double ms_per_beat = (60.0 / static_cast<double>(bpm)) * 1000.0;
    uint32_t id = 0;

    // Pattern cycling across all lanes
    const NoteLane pattern[] = {
        NoteLane::LeftUp, NoteLane::RightUp,
        NoteLane::LeftLeft, NoteLane::RightRight,
        NoteLane::LeftDown, NoteLane::RightDown,
        NoteLane::Scratch, NoteLane::RightRight
    };
    size_t pattern_len = sizeof(pattern) / sizeof(pattern[0]);

    for (int m = 1; m <= measures; ++m) {
        for (int beat = 0; beat < 4; ++beat) {
            Note n;
            n.id = id++;
            n.lane = pattern[id % pattern_len];
            n.target_time_ms = (m * 4 + beat) * ms_per_beat;
            n.type = (n.lane == NoteLane::Scratch) ? NoteType::Scratch : NoteType::Single;
            notes.push_back(n);
        }
    }
    return notes;
}

} // namespace p3d::rhythm
