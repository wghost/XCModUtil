#include "UPKLZOUtils.h"

#include <fstream>
#include <sstream>
#include <stdlib.h>
#include "minilzo.h"

#define IN_LEN      (131072u)                              /// max input block size
#define OUT_LEN     (IN_LEN + IN_LEN / 16 + 64 + 3)        /// max output block size

static unsigned char __LZO_MMODEL in  [ IN_LEN ];          /// input data
static unsigned char __LZO_MMODEL out [ OUT_LEN ];         /// output data

#define HEAP_ALLOC(var,size) \
    lzo_align_t __LZO_MMODEL var [ ((size) + (sizeof(lzo_align_t) - 1)) / sizeof(lzo_align_t) ]

std::string ToString(int i)
{
    std::ostringstream ss;
    ss << i;
    return ss.str();
}

bool DecompressLZOCompressedPackage(UPKReader *Package)
{
    if (!Package->IsCompressed())
    {
        _LogError("Package is not compressed!", "DecompressLZO");
        return false;
    }
    if (!Package->IsLZOCompressed() && !Package->IsFullyCompressed())
    {
        _LogError("Cannot decompress non-LZO compressed packages!", "DecompressLZO");
        return false;
    }
    /// init lzo library
    int lzo_err;
    lzo_uint in_len;
    lzo_uint out_len;
    lzo_uint new_len;
    if (lzo_init() != LZO_E_OK)
    {
        _LogError("LZO library internal error: lzo_init() failed!", "DecompressLZO");
        return false;
    }
    lzo_memset(in, 0, IN_LEN);
    std::stringstream decompressed_stream;
    unsigned int NumCompressedChunks = Package->Summary.NumCompressedChunks;
    if (Package->IsFullyCompressed())
    {
        NumCompressedChunks = 1;
    }
    else
    {
        _LogDebug("Resetting package compression flags...", "DecompressLZO");
        /// reset compression flags
        Package->Summary.CompressionFlags = 0;
        Package->Summary.PackageFlags ^= (uint32_t)UPackageFlags::Compressed;
        Package->Summary.NumCompressedChunks = 0;
        /// serialize package summary
        std::vector<char> sVect = Package->SerializeSummary();
        decompressed_stream.write(sVect.data(), sVect.size());
    }
    _LogDebug("Decompressing...", "DecompressLZO");
    for (unsigned int i = 0; i < NumCompressedChunks; ++i)
    {
        if (Package->IsFullyCompressed())
        {
            Package->UPKStream.seekg(0);
        }
        else
        {
            Package->UPKStream.seekg(Package->Summary.CompressedChunks[i].CompressedOffset);
        }
        _LogDebug("Decompressing chunk #" + ToString(i), "DecompressLZO");
        uint32_t tag = 0;
        Package->UPKStream.read(reinterpret_cast<char*>(&tag), 4);
        if (tag != 0x9E2A83C1)
        {
            _LogError("Missing 0x9E2A83C1 signature!", "DecompressLZO");
            return false;
        }
        uint32_t blockSize = 0;
        Package->UPKStream.read(reinterpret_cast<char*>(&blockSize), 4);
        if (blockSize != IN_LEN)
        {
            _LogError("Incorrect max block size!", "DecompressLZO");
            return false;
        }
        std::vector<uint32_t> sizes(2); /// compressed/uncompressed pairs
        Package->UPKStream.read(reinterpret_cast<char*>(sizes.data()), 4 * sizes.size());
        size_t dataSize = sizes[1]; /// uncompressed data chunk size
        unsigned numBlocks = (dataSize + blockSize - 1) / blockSize;
        _LogDebug("numBlocks = " + ToString(numBlocks), "DecompressLZO");
        if (numBlocks < 1)
        {
            _LogError("Bad data!", "DecompressLZO");
            return false;
        }
        sizes.resize((numBlocks + 1)*2);
        Package->UPKStream.read(reinterpret_cast<char*>(sizes.data()) + 8, 4 * sizes.size() - 8);
        for (unsigned i = 0; i <= numBlocks; ++i)
        {
            _LogDebug("Compressed size = " + ToString(sizes[i * 2]) +
                        + "\tUncompressed size = " + ToString(sizes[i * 2 + 1]), "DecompressLZO");
        }
        std::vector<unsigned char> dataChunk(dataSize);
        std::vector<unsigned char> compressedData(sizes[0]);
        Package->UPKStream.read(reinterpret_cast<char*>(compressedData.data()), compressedData.size());
        size_t blockOffset = 0;
        size_t dataOffset = 0;
        for (unsigned i = 1; i <= numBlocks; ++i)
        {
            out_len = sizes[i * 2]; /// compressed size
            lzo_memcpy(out, compressedData.data() + blockOffset, out_len);
            in_len = sizes[i * 2 + 1]; /// uncompressed size
            new_len = in_len;
            lzo_err = lzo1x_decompress(out, out_len, in, &new_len, NULL);
            if (lzo_err == LZO_E_OK && new_len == in_len)
            {
                _LogDebug("Decompressed " + ToString(out_len) + " bytes back into "
                     + ToString(in_len), "DecompressLZO");
            }
            else
            {
                _LogError("LZO library internal error: decompression failed!", "DecompressLZO");
                return false;
            }
            lzo_memcpy(dataChunk.data() + dataOffset, in, in_len);
            blockOffset += out_len;
            dataOffset += in_len;
        }
        decompressed_stream.write(reinterpret_cast<char*>(dataChunk.data()), dataSize);
    }
    _LogDebug("Package decompressed successfully.", "DecompressLZO");
    Package->UPKStream.str(decompressed_stream.str());
    return Package->ReadPackageHeader();
}
