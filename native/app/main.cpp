#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include "../platform/window.hpp"
#include "../platform/timer.hpp"
#include "../platform/input_manager.hpp"
#include "../renderer/renderer.hpp"
#include "../audio/audio_clock.hpp"
#include "../rhythm/rhythm_engine.hpp"
#include "../rhythm/mns_parser.hpp"
#include "../rhythm/track_catalog.hpp"

using namespace p3d;

enum class GameState {
    TitleScreen,
    SongSelect,
    Playing,
    Paused,
    Results
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Attach to existing console if launched from batch / terminal
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE* fp;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
    }

    std::cout << "=========================================================\n";
    std::cout << "  Persona 3: Dancing in Moonlight — Windows Native Runtime\n";
    std::cout << "  Architecture: Win32 x64 | Direct3D 11 | C++20\n";
    std::cout << "=========================================================\n";

    // 1. Initialize Window
    platform::WindowConfig win_cfg;
    win_cfg.title = "Persona 3: Dancing in Moonlight (PC Native Arcade)";
    win_cfg.width = 1280;
    win_cfg.height = 720;

    platform::NativeWindow window(win_cfg);
    if (!window.initialize()) {
        std::cerr << "[Main] Failed to initialize window.\n";
        return 1;
    }

    // 2. Initialize Direct3D 11 Renderer
    renderer::D3D11Renderer renderer;
    if (!renderer.initialize(window.hwnd(), window.width(), window.height())) {
        std::cerr << "[Main] Failed to initialize Direct3D 11 renderer.\n";
        return 1;
    }

    // 3. Subsystems
    audio::AudioClock audio_clock;
    rhythm::RhythmEngine rhythm_engine;
    rhythm::TrackCatalog catalog;
    platform::InputManager input_manager;
    auto bgm_player = std::make_shared<audio::AudioPlayer>();

    GameState game_state = GameState::TitleScreen;
    int pause_selection = 0;
    size_t selected_track_idx = 0;
    int selected_diff = 1; // 0=Easy, 1=Normal, 2=Hard, 3=All Night
    const char* diff_names[4] = {"EASY", "NORMAL", "HARD", "ALL NIGHT"};

    std::vector<rhythm::Note> current_notes;
    double current_song_duration_ms = 184000.0;

    // Helper: Update Song Preview in Menu
    auto update_menu_preview = [&](size_t track_idx) {
        if (track_idx >= catalog.count()) return;
        std::string path = catalog.get_audio_path(track_idx);
        if (std::filesystem::exists(path)) {
            bgm_player->stop();
            bgm_player->close();
            if (bgm_player->load(path)) {
                bgm_player->play();
            }
        }
    };

    // Helper: Start Title Screen BGM
    auto start_title_bgm = [&]() {
        std::string title_bgm = "extracted_assets/audio/preview_138.wav";
        if (!std::filesystem::exists(title_bgm)) {
            title_bgm = "extracted_assets/audio/music301.wav";
        }
        if (std::filesystem::exists(title_bgm)) {
            bgm_player->stop();
            bgm_player->close();
            if (bgm_player->load(title_bgm)) {
                bgm_player->play();
            }
        }
    };

    // Start Title BGM immediately
    start_title_bgm();

    // Helper: Launch Selected Song
    auto start_song = [&](size_t track_idx, int diff) {
        if (track_idx >= catalog.count()) track_idx = 0;
        const auto& track = catalog.get_track(track_idx);
        std::cout << "\n[Main] Launching Track #" << track.id << ": \"" << track.title 
                  << "\" [" << diff_names[diff] << "] - Lead Dancer: " << track.dancer << "\n";

        // Stop BGM / preview player
        bgm_player->stop();
        bgm_player->close();
        audio_clock.stop();

        // Load Gameplay Audio
        std::string audio_path = catalog.get_audio_path(track_idx);
        if (std::filesystem::exists(audio_path)) {
            auto player = std::make_shared<audio::AudioPlayer>();
            if (player->load(audio_path)) {
                audio_clock.set_player(player);
                current_song_duration_ms = player->length_ms();
                std::cout << "[Main] Audio stream loaded successfully: " << audio_path 
                          << " (" << static_cast<int>(current_song_duration_ms) << " ms)\n";
            }
        } else {
            std::cerr << "[Main] Audio file missing: " << audio_path << "\n";
            current_song_duration_ms = track.duration_ms;
        }

        // Load MNS Chart
        current_notes.clear();
        std::string mns_path = catalog.get_chart_path(track_idx, diff);
        float bpm = track.bpm;

        if (std::filesystem::exists(mns_path)) {
            std::cout << "[Main] Loading official Atlus chart: " << mns_path << "\n";
            rhythm::MnsParser::load_file(mns_path, current_notes, bpm);
        } else {
            std::cout << "[Main] Generating synthetic chart for " << track.title << " (" << bpm << " BPM)...\n";
            current_notes = rhythm::MnsParser::create_synthetic_chart(bpm, 28);
        }

        rhythm_engine.load_notes(current_notes);
        rhythm_engine.reset();
        audio_clock.start();
        game_state = GameState::Playing;
    };

    // Judgment Callback
    rhythm_engine.set_judgment_callback([&](const rhythm::Note& note, rhythm::Judgment judgment) {
        const char* j_str = "UNKNOWN";
        switch (judgment) {
            case rhythm::Judgment::Perfect: j_str = "PERFECT!! (+1000)"; break;
            case rhythm::Judgment::Great:   j_str = "GREAT!   (+700)"; break;
            case rhythm::Judgment::Good:    j_str = "GOOD     (+400)"; break;
            case rhythm::Judgment::Miss:    j_str = "MISS...  (0)"; break;
            default: break;
        }
        std::cout << "[Judgment] Lane: " << static_cast<int>(note.lane) 
                  << " -> " << j_str 
                  << " (Diff: " << note.time_diff_ms << " ms)\n";

        renderer.trigger_judgment_effect(judgment, static_cast<int>(note.lane), audio_clock.position_ms());
    });

    // Wire Raw Keyboard Input
    window.set_raw_key_callback([&](int vk, bool pressed) {
        if (!pressed) return; // Only process on key down

        if (game_state == GameState::TitleScreen) {
            if (vk == VK_ESCAPE) {
                PostQuitMessage(0);
            } else {
                game_state = GameState::SongSelect;
                update_menu_preview(selected_track_idx);
                std::cout << "[Title] Advancing to Song Select.\n";
            }
            return;
        } else if (game_state == GameState::SongSelect) {
            // Menu Navigation
            if (vk == VK_UP || vk == 'W') {
                selected_track_idx = (selected_track_idx == 0) ? (catalog.count() - 1) : (selected_track_idx - 1);
                update_menu_preview(selected_track_idx);
            } else if (vk == VK_DOWN || vk == 'S') {
                selected_track_idx = (selected_track_idx + 1) % catalog.count();
                update_menu_preview(selected_track_idx);
            } else if (vk == VK_LEFT || vk == 'A') {
                selected_diff = (selected_diff == 0) ? 3 : (selected_diff - 1);
            } else if (vk == VK_RIGHT || vk == 'D') {
                selected_diff = (selected_diff + 1) % 4;
            } else if (vk == '1') {
                selected_diff = 0;
            } else if (vk == '2') {
                selected_diff = 1;
            } else if (vk == '3') {
                selected_diff = 2;
            } else if (vk == '4') {
                selected_diff = 3;
            } else if (vk == VK_RETURN || vk == VK_SPACE) {
                start_song(selected_track_idx, selected_diff);
            } else if (vk == VK_ESCAPE || vk == VK_BACK) {
                bgm_player->stop();
                game_state = GameState::TitleScreen;
                start_title_bgm();
            }
        } else if (game_state == GameState::Playing) {
            // In-Game Commands: Pause
            if (vk == VK_ESCAPE) {
                audio_clock.pause();
                pause_selection = 0;
                game_state = GameState::Paused;
                std::cout << "[Command] Game Paused.\n";
            } else if (vk == 'R') {
                start_song(selected_track_idx, selected_diff);
            } else if (vk == VK_BACK) {
                audio_clock.stop();
                game_state = GameState::SongSelect;
                update_menu_preview(selected_track_idx);
            }
        } else if (game_state == GameState::Paused) {
            if (vk == VK_UP || vk == 'W') {
                pause_selection = (pause_selection == 0) ? 2 : (pause_selection - 1);
            } else if (vk == VK_DOWN || vk == 'S') {
                pause_selection = (pause_selection + 1) % 3;
            } else if (vk == VK_RETURN || vk == VK_SPACE) {
                if (pause_selection == 0) {
                    audio_clock.resume();
                    game_state = GameState::Playing;
                } else if (pause_selection == 1) {
                    start_song(selected_track_idx, selected_diff);
                } else if (pause_selection == 2) {
                    audio_clock.stop();
                    game_state = GameState::SongSelect;
                    update_menu_preview(selected_track_idx);
                }
            } else if (vk == VK_ESCAPE) {
                audio_clock.resume();
                game_state = GameState::Playing;
            }
        } else if (game_state == GameState::Results) {
            if (vk == 'R' || vk == VK_SPACE) {
                start_song(selected_track_idx, selected_diff);
            } else if (vk == VK_RETURN || vk == VK_ESCAPE || vk == VK_BACK) {
                audio_clock.stop();
                game_state = GameState::SongSelect;
                update_menu_preview(selected_track_idx);
            }
        }
    });

    // Wire Rhythm Lane Keyboard Input (during gameplay)
    window.set_key_callback([&](rhythm::NoteLane lane, bool pressed) {
        if (game_state == GameState::Playing && pressed && audio_clock.is_playing()) {
            double current_time = audio_clock.position_ms();
            rhythm_engine.on_lane_input(lane, current_time);
        }
    });

    std::cout << "\n=========================================================\n";
    std::cout << "  CONTROLS GUIDE\n";
    std::cout << "---------------------------------------------------------\n";
    std::cout << "  TITLE SCREEN:\n";
    std::cout << "    [ANY KEY / BUTTON] Start Persona 3: Dancing in Moonlight\n";
    std::cout << "  SONG SELECT:\n";
    std::cout << "    [W/S or UP/DOWN] Browse 25 Songs (Dynamic Audio Previews)\n";
    std::cout << "    [A/D or LEFT/RIGHT or 1..4] Difficulty (Easy/Normal/Hard/All Night)\n";
    std::cout << "    [ENTER or SPACE] Start Dancing!\n";
    std::cout << "    [ESC] Quit\n";
    std::cout << "  RHYTHM GAMEPLAY:\n";
    std::cout << "    Left Side:  [W] Left-Up   | [A] Left-Left   | [S] Left-Down\n";
    std::cout << "    Right Side: [I/UP] Right-Up | [L/RIGHT] Right-Right | [K/DOWN] Right-Down\n";
    std::cout << "    Scratch:    [SPACE] or [SHIFT] (Builds Fever Gauge!)\n";
    std::cout << "    In-Game:    [ESC] Pause Menu | [R] Quick Restart | [BACKSPACE] Return to Menu\n";
    std::cout << "  GAMEPAD (Xbox / PlayStation via XInput):\n";
    std::cout << "    D-Pad / Left Stick: Navigate Carousel & Difficulty\n";
    std::cout << "    A / Cross: Confirm / Note Right-Down\n";
    std::cout << "    B / Circle: Cancel / Note Right-Right\n";
    std::cout << "    Y / Triangle: Note Right-Up\n";
    std::cout << "    LB / RB / Triggers: Scratch\n";
    std::cout << "    Start: Pause / Confirm | Back: Return to Menu\n";
    std::cout << "=========================================================\n\n";

    // 4. Main Game Loop
    platform::HighPrecisionTimer frame_timer;
    platform::HighPrecisionTimer menu_timer;
    double log_accumulator = 0.0;

    while (window.poll_events()) {
        double dt = frame_timer.tick();

        // Poll Gamepad Controller
        input_manager.update(
            // Lane input callback
            [&](rhythm::NoteLane lane, bool pressed) {
                if (game_state == GameState::Playing && pressed && audio_clock.is_playing()) {
                    rhythm_engine.on_lane_input(lane, audio_clock.position_ms());
                }
            },
            // Navigation command callback
            [&](int cmd) {
                if (game_state == GameState::TitleScreen) {
                    if (cmd == 5 || cmd == 7) { // A or Start button
                        game_state = GameState::SongSelect;
                        update_menu_preview(selected_track_idx);
                    }
                } else if (game_state == GameState::SongSelect) {
                    if (cmd == 1) { // Up
                        selected_track_idx = (selected_track_idx == 0) ? (catalog.count() - 1) : (selected_track_idx - 1);
                        update_menu_preview(selected_track_idx);
                    } else if (cmd == 2) { // Down
                        selected_track_idx = (selected_track_idx + 1) % catalog.count();
                        update_menu_preview(selected_track_idx);
                    } else if (cmd == 3) { // Left
                        selected_diff = (selected_diff == 0) ? 3 : (selected_diff - 1);
                    } else if (cmd == 4) { // Right
                        selected_diff = (selected_diff + 1) % 4;
                    } else if (cmd == 5 || cmd == 7) { // A / Start -> Launch Song
                        start_song(selected_track_idx, selected_diff);
                    } else if (cmd == 6 || cmd == 8) { // B / Back -> Return to Title
                        bgm_player->stop();
                        game_state = GameState::TitleScreen;
                        start_title_bgm();
                    }
                } else if (game_state == GameState::Playing) {
                    if (cmd == 7) { // Start -> Pause
                        audio_clock.pause();
                        pause_selection = 0;
                        game_state = GameState::Paused;
                    } else if (cmd == 8) { // Back -> Return to Menu
                        audio_clock.stop();
                        game_state = GameState::SongSelect;
                        update_menu_preview(selected_track_idx);
                    } else if (cmd == 9) { // X -> Restart
                        start_song(selected_track_idx, selected_diff);
                    }
                } else if (game_state == GameState::Paused) {
                    if (cmd == 1) { // Up
                        pause_selection = (pause_selection == 0) ? 2 : (pause_selection - 1);
                    } else if (cmd == 2) { // Down
                        pause_selection = (pause_selection + 1) % 3;
                    } else if (cmd == 5 || cmd == 7) { // A or Start -> Confirm
                        if (pause_selection == 0) {
                            audio_clock.resume();
                            game_state = GameState::Playing;
                        } else if (pause_selection == 1) {
                            start_song(selected_track_idx, selected_diff);
                        } else if (pause_selection == 2) {
                            audio_clock.stop();
                            game_state = GameState::SongSelect;
                            update_menu_preview(selected_track_idx);
                        }
                    } else if (cmd == 6) { // B -> Resume
                        audio_clock.resume();
                        game_state = GameState::Playing;
                    }
                } else if (game_state == GameState::Results) {
                    if (cmd == 9 || cmd == 7) { // X or Start -> Play Again
                        start_song(selected_track_idx, selected_diff);
                    } else if (cmd == 5 || cmd == 6 || cmd == 8) { // A, B, Back -> Song Select
                        audio_clock.stop();
                        game_state = GameState::SongSelect;
                        update_menu_preview(selected_track_idx);
                    }
                }
            }
        );

        const auto& cur_track = catalog.get_track(selected_track_idx);

        if (game_state == GameState::Playing) {
            double song_time_ms = audio_clock.position_ms();

            // Update rhythm simulation
            rhythm_engine.update(song_time_ms);

            // Check if track is completed
            bool song_completed = (song_time_ms >= current_song_duration_ms) ||
                                  (!current_notes.empty() && rhythm_engine.active_note_index() >= current_notes.size() && 
                                   (song_time_ms - current_notes.back().target_time_ms) > 3000.0);

            if (song_completed) {
                // Record score to catalog
                float acc = rhythm_engine.stats().accuracy();
                float gg = rhythm_engine.stats().groove_gauge;
                std::string rank_str = "STAGE CLEAR";
                if (gg < 50.0f) {
                    rank_str = "STAGE FAILED";
                } else if (acc >= 95.0f && rhythm_engine.stats().miss_count == 0) {
                    rank_str = "KING CRAZY";
                } else if (acc >= 85.0f) {
                    rank_str = "BRILLIANT";
                } else {
                    rank_str = "STAGE CLEAR";
                }

                catalog.update_score(selected_track_idx, selected_diff, rhythm_engine.stats().score, rank_str);
                game_state = GameState::Results;
            }

            // Periodic diagnostics logging (every 3 seconds)
            log_accumulator += dt;
            if (log_accumulator >= 3.0) {
                log_accumulator = 0.0;
                const auto& st = rhythm_engine.stats();
                std::cout << "[Gameplay] Time: " << static_cast<int>(song_time_ms) << " ms | "
                          << "Score: " << st.score << " | "
                          << "Combo: " << st.combo << " (Max: " << st.max_combo << ") | "
                          << "Acc: " << st.accuracy() << "% | Groove: " << static_cast<int>(st.groove_gauge) << "% | "
                          << "Fever: " << (st.fever_active ? "ACTIVE (2X)" : std::to_string(static_cast<int>(st.fever_gauge)) + "%") << "\n";
            }
        }

        // Render scene
        renderer.begin_frame();
        if (game_state == GameState::TitleScreen) {
            renderer.render_title_screen(menu_timer.elapsed_milliseconds());
        } else if (game_state == GameState::SongSelect) {
            renderer.render_song_select(catalog, selected_track_idx, selected_diff, menu_timer.elapsed_milliseconds());
        } else if (game_state == GameState::Playing) {
            renderer.render_rhythm_scene(rhythm_engine, audio_clock.position_ms(), current_song_duration_ms,
                                         cur_track.title, cur_track.bpm, cur_track.id, diff_names[selected_diff]);
        } else if (game_state == GameState::Paused) {
            renderer.render_rhythm_scene(rhythm_engine, audio_clock.position_ms(), current_song_duration_ms,
                                         cur_track.title, cur_track.bpm, cur_track.id, diff_names[selected_diff]);
            renderer.render_pause_menu(pause_selection);
        } else if (game_state == GameState::Results) {
            renderer.render_results_screen(rhythm_engine.stats(), cur_track.title, diff_names[selected_diff]);
        }
        renderer.end_frame();
    }

    std::cout << "\n[Main] Exiting P3D Native Runtime cleanly.\n";
    return 0;
}
