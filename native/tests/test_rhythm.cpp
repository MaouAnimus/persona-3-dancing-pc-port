#include <iostream>
#include <cassert>
#include <vector>
#include <cmath>
#include "../rhythm/rhythm_engine.hpp"
#include "../rhythm/mns_parser.hpp"

using namespace p3d::rhythm;

void test_timing_windows() {
    std::cout << "[Test] Running test_timing_windows...\n";
    RhythmEngine engine;
    
    std::vector<Note> notes = {
        {0, NoteLane::LeftUp, NoteType::Single, 1000.0},
        {1, NoteLane::LeftLeft, NoteType::Single, 2000.0},
        {2, NoteLane::LeftDown, NoteType::Single, 3000.0},
        {3, NoteLane::RightUp, NoteType::Single, 4000.0}
    };
    engine.load_notes(notes);

    // 1. Exact hit -> Perfect
    Judgment j1 = engine.on_lane_input(NoteLane::LeftUp, 1000.0);
    assert(j1 == Judgment::Perfect);
    assert(engine.stats().perfect_count == 1);
    assert(engine.stats().combo == 1);
    assert(engine.stats().score == 1000);

    // 2. +30ms late -> still Perfect (window is ±35ms)
    Judgment j2 = engine.on_lane_input(NoteLane::LeftLeft, 2030.0);
    assert(j2 == Judgment::Perfect);
    assert(engine.stats().perfect_count == 2);
    assert(engine.stats().combo == 2);

    // 3. -60ms early -> Great (window is ±75ms)
    Judgment j3 = engine.on_lane_input(NoteLane::LeftDown, 2940.0);
    assert(j3 == Judgment::Great);
    assert(engine.stats().great_count == 1);
    assert(engine.stats().combo == 3);

    // 4. +110ms late -> Good (window is ±125ms)
    Judgment j4 = engine.on_lane_input(NoteLane::RightUp, 4110.0);
    assert(j4 == Judgment::Good);
    assert(engine.stats().good_count == 1);
    assert(engine.stats().combo == 4);

    std::cout << "  -> test_timing_windows PASSED!\n";
}

void test_miss_progression() {
    std::cout << "[Test] Running test_miss_progression...\n";
    RhythmEngine engine;
    
    std::vector<Note> notes = {
        {0, NoteLane::LeftUp, NoteType::Single, 1000.0},
        {1, NoteLane::LeftDown, NoteType::Single, 2000.0}
    };
    engine.load_notes(notes);

    // Hit note 0
    engine.on_lane_input(NoteLane::LeftUp, 1000.0);
    assert(engine.stats().combo == 1);

    // Advance time past note 1 without input (time = 2200ms, note was 2000ms -> diff is 200ms > 125ms)
    engine.update(2200.0);
    assert(engine.stats().miss_count == 1);
    assert(engine.stats().combo == 0); // Combo reset on miss

    std::cout << "  -> test_miss_progression PASSED!\n";
}

void test_mns_synthetic_generation() {
    std::cout << "[Test] Running test_mns_synthetic_generation...\n";
    auto notes = MnsParser::create_synthetic_chart(120.0f, 4);
    assert(notes.size() == 16);
    // BPM 120 -> 500ms per beat. Measure 1 beat 0 starts at 2000ms
    assert(std::abs(notes[0].target_time_ms - 2000.0) < 0.001);
    std::cout << "  -> test_mns_synthetic_generation PASSED!\n";
}

void test_real_mns_chart() {
    std::cout << "[Test] Running test_real_mns_chart with extracted_assets/dance/mns/mns101_0.bin...\n";
    std::vector<Note> notes;
    float bpm = 0.0f;
    bool ok = MnsParser::load_file("extracted_assets/dance/mns/mns101_0.bin", notes, bpm);
    assert(ok);
    assert(notes.size() == 201);
    assert(bpm == 145.0f);

    RhythmEngine engine;
    engine.load_notes(notes);

    // Simulate perfect hits on all 201 notes
    for (const auto& n : notes) {
        Judgment j = engine.on_lane_input(n.lane, n.target_time_ms);
        assert(j == Judgment::Perfect);
    }

    assert(engine.stats().perfect_count == 201);
    assert(engine.stats().combo == 201);
    assert(engine.stats().max_combo == 201);
    assert(engine.stats().miss_count == 0);
    assert(engine.stats().accuracy() == 100.0f);

    std::cout << "  -> Loaded 201 notes, simulated 100% PERFECT playthrough: Score = " 
              << engine.stats().score << ", Max Combo = " << engine.stats().max_combo << "\n";
    std::cout << "  -> test_real_mns_chart PASSED!\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "  P3D Native Rhythm Engine Test Suite   \n";
    std::cout << "========================================\n";

    test_timing_windows();
    test_miss_progression();
    test_mns_synthetic_generation();
    test_real_mns_chart();

    std::cout << "\nALL TESTS PASSED SUCCESSFULLY! (100% Deterministic)\n";
    return 0;
}
