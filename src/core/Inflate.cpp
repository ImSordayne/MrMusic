#include "core/Inflate.h"

#include <vector>

namespace mr {
namespace {

constexpr int kMaxOutput = 64 * 1024 * 1024; // safety limit for lyric payloads

struct BitReader
{
    const unsigned char *data = nullptr;
    int size = 0;
    int pos = 0;
    unsigned int bitBuf = 0;
    int bitCount = 0;
    bool error = false;

    int bit()
    {
        if (bitCount == 0) {
            if (pos >= size) {
                error = true;
                return 0;
            }
            bitBuf = data[pos++];
            bitCount = 8;
        }
        const int b = static_cast<int>(bitBuf & 1u);
        bitBuf >>= 1;
        --bitCount;
        return b;
    }

    unsigned int bits(int count)
    {
        unsigned int v = 0;
        for (int i = 0; i < count; ++i)
            v |= static_cast<unsigned int>(bit()) << i;
        return v;
    }

    void alignToByte()
    {
        bitBuf = 0;
        bitCount = 0;
    }
};

struct Huffman
{
    std::vector<int> counts;   // counts[len] = number of codes with `len` bits
    std::vector<int> symbols;

    bool build(const std::vector<int> &lengths)
    {
        counts.assign(16, 0);
        for (int len : lengths) {
            if (len < 0 || len > 15)
                return false;
            if (len)
                counts[static_cast<size_t>(len)] += 1;
        }
        if (counts[0] == static_cast<int>(lengths.size()))
            return true; // no codes at all

        // check for an over-subscribed set of lengths
        int left = 1;
        for (int len = 1; len <= 15; ++len) {
            left <<= 1;
            left -= counts[static_cast<size_t>(len)];
            if (left < 0)
                return false;
        }

        std::vector<int> offsets(16, 0);
        for (int len = 1; len < 15; ++len)
            offsets[static_cast<size_t>(len) + 1] = offsets[static_cast<size_t>(len)] + counts[static_cast<size_t>(len)];

        symbols.assign(lengths.size(), 0);
        for (size_t i = 0; i < lengths.size(); ++i) {
            if (lengths[i])
                symbols[static_cast<size_t>(offsets[static_cast<size_t>(lengths[i])]++)] = static_cast<int>(i);
        }
        return true;
    }

    int decode(BitReader &br) const
    {
        int code = 0;
        int first = 0;
        int index = 0;
        for (int len = 1; len <= 15; ++len) {
            code |= br.bit();
            const int count = counts[static_cast<size_t>(len)];
            if (code - first < count)
                return symbols[static_cast<size_t>(index + (code - first))];
            index += count;
            first = (first + count) << 1;
            code <<= 1;
        }
        br.error = true;
        return -1;
    }
};

void buildFixedTables(Huffman &lit, Huffman &dist)
{
    std::vector<int> lengths(288);
    for (int i = 0; i < 144; ++i)
        lengths[static_cast<size_t>(i)] = 8;
    for (int i = 144; i < 256; ++i)
        lengths[static_cast<size_t>(i)] = 9;
    for (int i = 256; i < 280; ++i)
        lengths[static_cast<size_t>(i)] = 7;
    for (int i = 280; i < 288; ++i)
        lengths[static_cast<size_t>(i)] = 8;
    lit.build(lengths);

    std::vector<int> dlengths(30, 5);
    dist.build(dlengths);
}

const int kLengthBase[29] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                             31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
const int kLengthExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                              2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
const int kDistBase[30] = {1,    2,    3,    4,    5,    7,     9,     13,    17,    25,
                           33,   49,   65,   97,   129,  193,   257,   385,   513,   769,
                           1025, 1537, 2049, 3073, 4097, 6145,  8193,  12289, 16385, 24577};
const int kDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
                            6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
const int kCodeLengthOrder[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

bool inflateBlockData(BitReader &br, const Huffman &lit, const Huffman &dist, QByteArray &out)
{
    for (;;) {
        const int symbol = lit.decode(br);
        if (symbol < 0)
            return false;
        if (symbol < 256) {
            if (out.size() >= kMaxOutput)
                return false;
            out.append(static_cast<char>(symbol));
        } else if (symbol == 256) {
            return true;
        } else {
            const int li = symbol - 257;
            if (li >= 29)
                return false;
            const int length = kLengthBase[li] + static_cast<int>(br.bits(kLengthExtra[li]));
            const int ds = dist.decode(br);
            if (ds < 0 || ds >= 30)
                return false;
            const int distance = kDistBase[ds] + static_cast<int>(br.bits(kDistExtra[ds]));
            if (distance <= 0 || distance > out.size())
                return false;
            if (out.size() + length > kMaxOutput)
                return false;
            const int start = out.size() - distance;
            for (int i = 0; i < length; ++i)
                out.append(out.at(start + i));
        }
        if (br.error)
            return false;
    }
}

} // namespace

bool zlibInflate(const QByteArray &input, QByteArray &output)
{
    output.clear();
    if (input.size() < 2)
        return false;

    const auto *data = reinterpret_cast<const unsigned char *>(input.constData());
    int offset = 0;

    // zlib header: CMF/FLG, valid when (CMF*256+FLG) % 31 == 0 and CM == 8
    if ((data[0] & 0x0f) == 8 && ((data[0] * 256 + data[1]) % 31) == 0)
        offset = 2;

    BitReader br;
    br.data = data;
    br.size = input.size();
    br.pos = offset;

    for (;;) {
        const int final = br.bit();
        const int type = static_cast<int>(br.bits(2));
        if (br.error)
            return false;

        if (type == 0) {
            br.alignToByte();
            if (br.pos + 4 > br.size)
                return false;
            const int len = data[br.pos] | (data[br.pos + 1] << 8);
            const int nlen = data[br.pos + 2] | (data[br.pos + 3] << 8);
            br.pos += 4;
            if ((len ^ 0xffff) != nlen)
                return false;
            if (br.pos + len > br.size)
                return false;
            output.append(reinterpret_cast<const char *>(data + br.pos), len);
            br.pos += len;
        } else if (type == 1 || type == 2) {
            Huffman lit;
            Huffman dist;
            if (type == 1) {
                buildFixedTables(lit, dist);
            } else {
                const int hlit = static_cast<int>(br.bits(5)) + 257;
                const int hdist = static_cast<int>(br.bits(5)) + 1;
                const int hclen = static_cast<int>(br.bits(4)) + 4;
                if (br.error)
                    return false;

                std::vector<int> clen(19, 0);
                for (int i = 0; i < hclen; ++i)
                    clen[static_cast<size_t>(kCodeLengthOrder[i])] = static_cast<int>(br.bits(3));
                if (br.error)
                    return false;

                Huffman clh;
                if (!clh.build(clen))
                    return false;

                std::vector<int> lengths(static_cast<size_t>(hlit + hdist), 0);
                int index = 0;
                while (index < hlit + hdist) {
                    const int sym = clh.decode(br);
                    if (sym < 0)
                        return false;
                    if (sym < 16) {
                        lengths[static_cast<size_t>(index++)] = sym;
                    } else if (sym == 16) {
                        if (index == 0)
                            return false;
                        const int repeat = 3 + static_cast<int>(br.bits(2));
                        const int prev = lengths[static_cast<size_t>(index - 1)];
                        for (int i = 0; i < repeat && index < hlit + hdist; ++i)
                            lengths[static_cast<size_t>(index++)] = prev;
                    } else if (sym == 17) {
                        const int repeat = 3 + static_cast<int>(br.bits(3));
                        index += repeat;
                    } else {
                        const int repeat = 11 + static_cast<int>(br.bits(7));
                        index += repeat;
                    }
                    if (index > hlit + hdist)
                        return false;
                    if (br.error)
                        return false;
                }

                std::vector<int> litLengths(lengths.begin(), lengths.begin() + hlit);
                std::vector<int> distLengths(lengths.begin() + hlit, lengths.end());
                if (!lit.build(litLengths) || !dist.build(distLengths))
                    return false;
            }

            if (!inflateBlockData(br, lit, dist, output))
                return false;
        } else {
            return false; // reserved block type
        }

        if (br.error)
            return false;
        if (final)
            break;
    }

    return true;
}

} // namespace mr
