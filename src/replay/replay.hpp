#ifndef REPLAY_REPLAY_HPP
#define REPLAY_REPLAY_HPP

#include <algorithm>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

struct Replay {
    enum class Error {
        FileError,
        ReadError,
    };

    enum class InputType : uint8_t { Reserved = 0, Click, Left, Right };

    struct alignas(4) Input {
        static constexpr uint32_t FRAME_MASK = 0xfffffff0;
        static constexpr uint32_t PLAYER2_MASK = 0b1000;
        static constexpr uint32_t BUTTON_MASK = 0b110;
        static constexpr uint32_t HOLDING_MASK = 0b1;

        uint32_t m_state;

        uint32_t m_frame = 0;
        bool m_player2 = false;
        InputType m_button = InputType::Reserved;
        bool m_holding = false;

        Input(uint32_t frame, bool p2, uint8_t btn, bool pressed) {
            m_state = (frame << 4) | (p2 << 3) | (btn << 1) | pressed;
            m_frame = frame;
            m_player2 = p2;
            m_button = static_cast<InputType>(btn);
            m_holding = pressed;
        }
        Input(uint32_t state) : m_state(state) {
            m_frame = (state & FRAME_MASK) >> 4;
            m_player2 = (state & PLAYER2_MASK) >> 3;
            m_button = static_cast<InputType>((state & BUTTON_MASK) >> 1);
            m_holding = state & HOLDING_MASK;
        }
        Input() = default;
    };

    std::vector<Input> m_inputs;
    double m_fps = 240.0;
    size_t m_seed;

    double fps() const { return m_fps; }
    uint32_t length() const { return m_inputs.size(); }

    Replay() = default;
    Replay(double fps) : m_fps(fps) {}

    static constexpr size_t BUFFER_SIZE = 8192;

    [[nodiscard]] static std::expected<Replay, Error> fromFile(
        const std::filesystem::path& path);

    std::expected<std::vector<uint8_t>, Error> toBytes();
    std::expected<void, Error> toFile(const std::filesystem::path& path);

    inline void addInput(const Input input) { m_inputs.emplace_back(input); }
    inline void addInput(const Input&& input) { m_inputs.emplace_back(input); }
};

#endif  // REPLAY_REPLAY_HPP
