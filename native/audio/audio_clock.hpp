#pragma once
#include <cstdint>
#include <memory>
#include "../platform/timer.hpp"
#include "audio_player.hpp"

namespace p3d::audio {

class AudioClock {
public:
    AudioClock() = default;

    void set_player(std::shared_ptr<AudioPlayer> player) {
        player_ = player;
    }

    void start() {
        timer_.reset();
        is_playing_ = true;
        is_paused_ = false;
        pause_offset_ms_ = 0.0;
        if (player_) player_->play();
    }

    void pause() {
        if (is_playing_ && !is_paused_) {
            pause_offset_ms_ = position_ms();
            is_paused_ = true;
            if (player_) player_->pause();
        }
    }

    void resume() {
        if (is_playing_ && is_paused_) {
            timer_.reset();
            is_paused_ = false;
            if (player_) player_->resume();
        }
    }

    void stop() {
        is_playing_ = false;
        is_paused_ = false;
        pause_offset_ms_ = 0.0;
        if (player_) player_->stop();
    }

    void set_latency_offset_ms(double offset_ms) {
        latency_offset_ms_ = offset_ms;
    }

    double latency_offset_ms() const { return latency_offset_ms_; }

    double position_ms() const {
        if (!is_playing_) return 0.0;
        if (is_paused_) return pause_offset_ms_;
        
        // If hardware player has a valid position, use it
        if (player_ && player_->is_playing()) {
            double hw_pos = player_->get_position_ms();
            if (hw_pos > 0.0) {
                return hw_pos - latency_offset_ms_;
            }
        }
        return pause_offset_ms_ + timer_.elapsed_milliseconds() - latency_offset_ms_;
    }

    bool is_playing() const { return is_playing_ && !is_paused_; }
    bool is_paused() const { return is_paused_; }
    double duration_ms() const { return player_ ? player_->length_ms() : 0.0; }

private:
    platform::HighPrecisionTimer timer_;
    std::shared_ptr<AudioPlayer> player_{nullptr};
    bool is_playing_{false};
    bool is_paused_{false};
    double pause_offset_ms_{0.0};
    double latency_offset_ms_{0.0};
};

} // namespace p3d::audio
