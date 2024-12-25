module;

#include <format>
#include <lzma.h>
#include <span>
#include <stdexcept>
#include <vector>

export module lzma;

export std::vector<uint8_t> lzma_decompress(std::span<uint8_t> compressed_data)
{
    // init decoder.
    lzma_stream stream = LZMA_STREAM_INIT;
    if (auto ret = lzma_stream_decoder(&stream, /*memlimit=*/UINT64_MAX, /*flags=*/0); ret != LZMA_OK) {
        lzma_end(&stream);
        throw std::runtime_error { std::format("Init lzma decoder failed: {}", (int)ret) };
    }

    std::vector<uint8_t> out;
    std::array<uint8_t, 1024> buffer;

    stream.next_in = compressed_data.data();
    stream.avail_in = compressed_data.size();
    stream.next_out = buffer.data();
    stream.avail_out = buffer.size();

    while (true) {
        auto ret = lzma_code(&stream, LZMA_RUN);
        if (!stream.avail_out || ret == LZMA_STREAM_END) {
            out.insert(out.end(), buffer.data(), buffer.data() + (buffer.size() - stream.avail_out));
            if (ret == LZMA_STREAM_END) {
                lzma_end(&stream);
                return out;
            }

            // continue to decompress.
            stream.next_out = buffer.data();
            stream.avail_out = buffer.size();
        } else {
            lzma_end(&stream);
            throw std::runtime_error { std::format("lzma decompress failed: {}", (int)ret) };
        }
    }
}