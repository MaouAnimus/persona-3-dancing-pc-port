#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace p3d::rhythm {

struct TrackInfo {
    int id{101};
    std::string title;
    std::string artist;
    std::string dancer;
    float bpm{145.0f};
    std::string audio_file;
    std::string chart_files[4]; // 0=Easy, 1=Normal, 2=Hard, 3=All Night
    double duration_ms{184000.0};
    uint32_t high_score[4]{0, 0, 0, 0};
    std::string best_rank[4]{"---", "---", "---", "---"};
};

class TrackCatalog {
public:
    TrackCatalog() {
        populate_default_tracks();
        load_save_data("user_scores.dat");
    }

    const std::vector<TrackInfo>& tracks() const { return tracks_; }
    size_t count() const { return tracks_.size(); }

    const TrackInfo& get_track(size_t index) const {
        if (index >= tracks_.size()) return tracks_[0];
        return tracks_[index];
    }

    void update_score(size_t index, int difficulty, uint32_t score, const std::string& rank) {
        if (index < tracks_.size() && difficulty >= 0 && difficulty < 4) {
            if (score > tracks_[index].high_score[difficulty]) {
                tracks_[index].high_score[difficulty] = score;
                tracks_[index].best_rank[difficulty] = rank;
                save_save_data("user_scores.dat");
            }
        }
    }

    std::string get_chart_path(size_t track_index, int difficulty) const {
        if (track_index >= tracks_.size()) return "";
        if (difficulty < 0 || difficulty > 3) difficulty = 1; // Default to Normal
        return tracks_[track_index].chart_files[difficulty];
    }

    std::string get_audio_path(size_t track_index) const {
        if (track_index >= tracks_.size()) return "";
        return tracks_[track_index].audio_file;
    }

private:
    std::vector<TrackInfo> tracks_;

    void add_track(int id, const std::string& title, const std::string& artist, 
                   const std::string& dancer, float bpm, double duration_ms) {
        TrackInfo t;
        t.id = id;
        t.title = title;
        t.artist = artist;
        t.dancer = dancer;
        t.bpm = bpm;
        t.duration_ms = duration_ms;
        t.audio_file = "extracted_assets/audio/music" + std::to_string(id) + ".wav";
        for (int d = 0; d < 4; ++d) {
            t.chart_files[d] = "extracted_assets/dance/mns/mns" + std::to_string(id) + "_" + std::to_string(d) + ".bin";
        }
        tracks_.push_back(t);
    }

    void populate_default_tracks() {
        tracks_.clear();
        add_track(101, "Burn My Dread", "Lotus Juice & Yumi Kawamura", "Makoto Yuki", 145.0f, 184000.0);
        add_track(102, "When The Moon's Reaching Out Stars", "Yumi Kawamura", "Yukari Takeba", 128.0f, 181000.0);
        add_track(103, "Mass Destruction", "Lotus Juice & Yumi Kawamura", "Junpei Iori", 104.0f, 175000.0);
        add_track(104, "Deep Breath Deep Breath", "Lotus Juice", "Akihiko Sanada", 130.0f, 175000.0);
        add_track(105, "Soul Phrase", "Shuhei Kita", "Mitsuru Kirijo", 165.0f, 196000.0);
        add_track(106, "A Way of Life", "Mayumi Fujita", "Fuuka Yamagishi", 125.0f, 171000.0);
        add_track(107, "Heartful Cry", "Atsushi Kitajoh", "Aigis", 160.0f, 178000.0);
        add_track(108, "Light the Fire Up in the Night", "Lotus Juice", "Ken Amada", 140.0f, 163000.0);
        add_track(109, "Time (ATLUS Kitajoh Remix)", "Yumi Kawamura", "Mitsuru Kirijo", 135.0f, 174000.0);
        add_track(110, "Wiping All Out (ATLUS Kozuka Remix)", "Mayumi Fujita", "Yukari Takeba", 140.0f, 211000.0);
        add_track(111, "A Serious Event (TOCCHI Remix)", "Shoji Meguro", "Elizabeth", 132.0f, 190000.0);
        add_track(112, "Unavoidable Battle", "Shoji Meguro", "Ken & Koromaru", 148.0f, 218000.0);
        add_track(113, "Mass Destruction (Tetsuya Kobayashi Remix)", "Lotus Juice", "Shinjiro Aragaki", 110.0f, 200000.0);
        add_track(114, "Deep Breath Deep Breath (Yu Sasamoto Remix)", "Lotus Juice", "Junpei Iori", 130.0f, 228000.0);
        add_track(115, "Soul Phrase (Lotus Juice Remix)", "Lotus Juice", "Akihiko Sanada", 145.0f, 185000.0);
        add_track(116, "Burn My Dread (Novoiski REMIX)", "Yumi Kawamura", "Makoto Yuki", 132.0f, 212000.0);
        add_track(117, "When The Moon's Reaching Out Stars (Hideki Naganuma)", "Yumi Kawamura", "Junpei Iori", 126.0f, 194000.0);
        add_track(118, "Battle Hymn of the Soul (Daisuke Asakura Remix)", "Shoji Meguro", "Aigis", 155.0f, 285000.0);
        add_track(119, "Brand New Days (Yukihiro Fukutomi Remix)", "Yumi Kawamura", "Fuuka Yamagishi", 128.0f, 192000.0);
        add_track(120, "Memories of You (ATLUS Meguro Remix)", "Yumi Kawamura", "Aigis & Makoto", 120.0f, 201000.0);
        add_track(121, "Today", "Lotus Juice", "Aigis", 130.0f, 101000.0);
        add_track(122, "Moonlight Serendipity", "Lotus Juice & Yumi Kawamura", "SEES Ensemble", 128.0f, 234000.0);
        add_track(123, "Brand New Days", "Yumi Kawamura", "SEES Ensemble", 130.0f, 202000.0);
        add_track(124, "Our Moment (Opening Theme)", "Yumi Kawamura", "Makoto Yuki", 130.0f, 179000.0);
        add_track(125, "Battle Hymn of the Soul (Full)", "Shoji Meguro", "Makoto Yuki", 152.0f, 354000.0);
    }

    void load_save_data(const std::string& path) {
        std::ifstream file(path);
        if (!file) return;
        std::string line;
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            int id, diff;
            uint32_t score;
            std::string rank;
            if (ss >> id >> diff >> score >> rank) {
                for (auto& t : tracks_) {
                    if (t.id == id && diff >= 0 && diff < 4) {
                        t.high_score[diff] = score;
                        t.best_rank[diff] = rank;
                    }
                }
            }
        }
    }

    void save_save_data(const std::string& path) const {
        std::ofstream file(path);
        if (!file) return;
        for (const auto& t : tracks_) {
            for (int d = 0; d < 4; ++d) {
                if (t.high_score[d] > 0) {
                    file << t.id << " " << d << " " << t.high_score[d] << " " << t.best_rank[d] << "\n";
                }
            }
        }
    }
};

} // namespace p3d::rhythm
