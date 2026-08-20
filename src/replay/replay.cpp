#include "replay.hpp"

#include <Geode/Geode.hpp>

#include "../util/file.hpp"
#include "../util/try.hpp"

[[nodiscard]] std::expected<Replay, Replay::Error> Replay::fromFile(
    const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);

    Replay replay;

    replay.m_fps = TRY_WITH_ERR(Error::ReadError, file_read<double>(file));

    uint32_t inputCount =
        TRY_WITH_ERR(Error::ReadError, file_read<uint32_t>(file));

    replay.m_inputs.resize(inputCount);
    for (auto& input : replay.m_inputs) {
        uint32_t state =
            TRY_WITH_ERR(Error::ReadError, file_read<uint32_t>(file));
        input = Replay::Input(state);
    }

    replay.m_seed = file_read<size_t>(file).value_or(69696969);

    return replay;
}

std::expected<std::vector<uint8_t>, Replay::Error> Replay::toBytes() {
    std::vector<uint8_t> vec;

    vec.resize(sizeof(double) + sizeof(uint32_t) +
               m_inputs.size() * sizeof(Input) + sizeof(size_t));

    std::memcpy(vec.data(), &m_fps, sizeof(double));
    uint32_t size = m_inputs.size();
    std::memcpy(vec.data() + sizeof(double), &size, sizeof(uint32_t));
    std::memcpy(vec.data() + sizeof(double) + sizeof(uint32_t), m_inputs.data(),
                sizeof(Input) * m_inputs.size());
    std::memcpy(vec.data() + sizeof(double) + sizeof(uint32_t) +
                    sizeof(Input) * m_inputs.size(),
                &m_seed, sizeof(size_t));

    return vec;
}

std::expected<void, Replay::Error> Replay::toFile(
    const std::filesystem::path& path) {
    std::ofstream file(path, std::ios::binary);

    std::vector<uint8_t> bytes;
    if (auto res = this->toBytes(); res.has_value()) {
        bytes = std::move(res.value());
    } else {
        return std::unexpected(res.error());
    }

    file.write((char*)bytes.data(), bytes.size());

    return {};
}
