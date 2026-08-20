#include "compattor.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

struct ColorBoxBounds {
    PaletteColor minimum;
    PaletteColor maximum;
};

ColorBoxBounds computeColorBoxBounds(const std::vector<PaletteColor>& colors) {
    ColorBoxBounds bounds = {{255, 255, 255, 255}, {0, 0, 0, 0}};
    for (const auto& color : colors) {
        if (color.r < bounds.minimum.r) bounds.minimum.r = color.r;
        if (color.g < bounds.minimum.g) bounds.minimum.g = color.g;
        if (color.b < bounds.minimum.b) bounds.minimum.b = color.b;
        if (color.a < bounds.minimum.a) bounds.minimum.a = color.a;
        if (color.r > bounds.maximum.r) bounds.maximum.r = color.r;
        if (color.g > bounds.maximum.g) bounds.maximum.g = color.g;
        if (color.b > bounds.maximum.b) bounds.maximum.b = color.b;
        if (color.a > bounds.maximum.a) bounds.maximum.a = color.a;
    }
    return bounds;
}

bool parseIntegerValue(const char* text, int& value) {
    char* parseEnd = nullptr;
    long parsed = std::strtol(text, &parseEnd, 10);
    if (parseEnd == text || *parseEnd != '\0') return false;
    if (parsed < std::numeric_limits<int>::min() ||
        parsed > std::numeric_limits<int>::max()) return false;
    value = static_cast<int>(parsed);
    return true;
}

int clampCompressionLevel(int compressionLevel) {
    return std::max(1, std::min(9, compressionLevel));
}

bool createPngWriter(png_structp& png, png_infop& info) {
    png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) return false;

    info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, nullptr);
        return false;
    }

    return true;
}

struct FileCloser {
    void operator()(FILE* file) const {
        if (file) fclose(file);
    }
};

} // namespace

void printUsage(const char* programName) {
    std::cout << "compattor - PNG compression tool\n\n"
              << "Usage:\n"
              << "  " << programName << " [options] <file1.png> [file2.png ...]\n"
              << "  " << programName << " [options] <directory>\n\n"
              << "Options:\n"
              << "  -l, --level <1-9>     Compression level (default: 9)\n"
              << "  -c, --colors <2-256>  Number of colors for quantization (default: 256)\n"
              << "  --lossless            Lossy compression disabled, use lossless only\n"
              << "  -s, --strip           Remove PNG metadata (default: on)\n"
              << "  -o, --output <dir>    Output directory (default: ./compressed)\n"
              << "  -w, --overwrite       Overwrite original files\n"
              << "  -h, --help            Show this help\n\n"
              << "Compression modes:\n"
              << "  Lossy (default):     256-color quantization + zlib level 9\n"
              << "  Lossless (--lossless): zlib level 9, no color reduction\n\n"
              << "Transparency is preserved: RGBA colors are quantized together with\n"
              << "their alpha values and written as a palette with a tRNS chunk.\n"
              << "Grayscale images fall back to lossless mode.\n\n"
              << "Examples:\n"
              << "  " << programName << " photo.png\n"
              << "  " << programName << " -c 128 *.png\n"
              << "  " << programName << " --lossless -o /tmp/output ./images/\n";
}

bool hasPngSignature(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;

    unsigned char signature[8];
    file.read(reinterpret_cast<char*>(signature), 8);

    return file.gcount() == 8 &&
           signature[0] == 0x89 &&
           signature[1] == 0x50 &&
           signature[2] == 0x4E &&
           signature[3] == 0x47 &&
           signature[4] == 0x0D &&
           signature[5] == 0x0A &&
           signature[6] == 0x1A &&
           signature[7] == 0x0A;
}

void cleanupPngData(PngData& data) {
    data.rowPointers.clear();
    data.imageData.clear();
}

bool readPngFile(const std::string& filePath, PngData& data) {
    std::unique_ptr<FILE, FileCloser> filePointer(fopen(filePath.c_str(), "rb"));
    if (!filePointer) {
        std::cerr << "Error: cannot open " << filePath << "\n";
        return false;
    }

    unsigned char header[8];
    if (fread(header, 1, 8, filePointer.get()) != 8 || png_sig_cmp(header, 0, 8)) {
        std::cerr << "Error: " << filePath << " is not a valid PNG\n";
        return false;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) {
        return false;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, nullptr, nullptr);
        return false;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, nullptr);
        return false;
    }

    png_init_io(png, filePointer.get());
    png_set_sig_bytes(png, 8);
    png_read_info(png, info);

    int originalColorType = png_get_color_type(png, info);
    int originalBitDepth = png_get_bit_depth(png, info);

    if (originalBitDepth == 16) {
        png_set_strip_16(png);
    }

    if (originalColorType == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png);
    }

    if (originalColorType == PNG_COLOR_TYPE_GRAY && originalBitDepth < 8) {
        png_set_expand_gray_1_2_4_to_8(png);
    }

    if (png_get_valid(png, info, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png);
    }

    png_read_update_info(png, info);

    data.width = png_get_image_width(png, info);
    data.height = png_get_image_height(png, info);
    data.colorType = png_get_color_type(png, info);
    data.bitDepth = png_get_bit_depth(png, info);

    size_t rowBytes = png_get_rowbytes(png, info);

    data.rowPointers.resize(data.height);
    data.imageData.resize(data.height * rowBytes);

    for (png_uint_32 y = 0; y < data.height; y++) {
        data.rowPointers[y] = data.imageData.data() + y * rowBytes;
    }

    png_read_image(png, data.rowPointers.data());

    png_destroy_read_struct(&png, &info, nullptr);
    return true;
}

std::optional<size_t> writePngToBuffer(std::vector<unsigned char>& buffer, const PngData& data,
                                       int compressionLevel, bool stripMetadata,
                                       int filterType, int deflateStrategy) {
    volatile int clampedLevel = clampCompressionLevel(compressionLevel);

    png_structp png = nullptr;
    png_infop info = nullptr;
    if (!createPngWriter(png, info)) return std::nullopt;

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        return std::nullopt;
    }

    buffer.clear();
    png_set_write_fn(png, &buffer, [](png_structp pngPtr, png_bytep rowData, png_size_t length) {
        auto* destinationBuffer = reinterpret_cast<std::vector<unsigned char>*>(png_get_io_ptr(pngPtr));
        destinationBuffer->insert(destinationBuffer->end(), rowData, rowData + length);
    }, nullptr);

    png_set_compression_level(png, clampedLevel);
    png_set_compression_strategy(png, deflateStrategy);
    png_set_filter(png, 0, filterType);

    if (stripMetadata) {
        png_set_keep_unknown_chunks(png, PNG_HANDLE_CHUNK_NEVER, nullptr, 0);
    }

    png_set_IHDR(png, info, data.width, data.height,
                 data.bitDepth, data.colorType,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png, info);

    for (png_uint_32 y = 0; y < data.height; y++) {
        png_write_row(png, data.rowPointers[y]);
    }

    png_write_end(png, nullptr);
    png_destroy_write_struct(&png, &info);

    return buffer.size();
}

bool writePngFile(const std::string& filePath, const PngData& data,
                  int compressionLevel, bool stripMetadata,
                  int filterType, int deflateStrategy) {
    std::vector<unsigned char> buffer;
    if (!writePngToBuffer(buffer, data, compressionLevel, stripMetadata,
                          filterType, deflateStrategy)) {
        return false;
    }

    std::ofstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: cannot create " << filePath << "\n";
        return false;
    }

    file.write(reinterpret_cast<const char*>(buffer.data()),
               static_cast<std::streamsize>(buffer.size()));

    if (!file.good()) {
        std::cerr << "Error: cannot write " << filePath << "\n";
        return false;
    }

    return true;
}

std::optional<BestCompressionResult> findBestCompression(const PngData& data, int compressionLevel,
                                                          bool stripMetadata) {
    const int filterTypes[] = {
        PNG_FILTER_NONE,
        PNG_FILTER_SUB,
        PNG_FILTER_UP,
        PNG_FILTER_AVG,
        PNG_FILTER_PAETH
    };

    const int deflateStrategies[] = {
        Z_DEFAULT_STRATEGY,
        Z_FILTERED,
        Z_RLE
    };

    std::optional<BestCompressionResult> best;
    std::vector<unsigned char> buffer;
    buffer.reserve(data.width * data.height * 4);

    auto consider = [&](int filterType, int deflateStrategy) {
        auto size = writePngToBuffer(buffer, data, compressionLevel, stripMetadata,
                                     filterType, deflateStrategy);
        if (size && (!best || *size < best->compressedSize)) {
            best = BestCompressionResult{filterType, deflateStrategy, *size};
        }
    };

    for (int filter : filterTypes) {
        consider(filter, Z_DEFAULT_STRATEGY);
    }

    if (best) {
        for (int strategy : deflateStrategies) {
            consider(best->filterType, strategy);
        }
    }

    return best;
}

std::string getOutputPath(const std::string& inputPath, const std::string& outputDir) {
    namespace fs = std::filesystem;

    fs::path input(inputPath);
    std::string filename = input.filename().string();

    if (outputDir.empty()) {
        return (input.parent_path() / "compressed" / filename).string();
    }

    return (fs::path(outputDir) / filename).string();
}

bool processFile(const std::string& inputPath, const PngCompressionOptions& options) {
    namespace fs = std::filesystem;

    if (!fs::exists(inputPath)) {
        std::cerr << "Error: " << inputPath << " does not exist\n";
        return false;
    }

    if (!hasPngSignature(inputPath)) {
        std::cerr << "Warning: " << inputPath << " is not a PNG, skipping\n";
        return true;
    }

    auto originalSize = fs::file_size(inputPath);

    std::string outputPath;
    std::string tempPath;

    if (options.overwrite) {
        tempPath = inputPath + ".tmp";
        outputPath = inputPath;
    } else {
        outputPath = getOutputPath(inputPath, options.outputDir);
        tempPath = outputPath + ".tmp";

        std::error_code errorCode;
        fs::create_directories(fs::path(outputPath).parent_path(), errorCode);
        if (errorCode) {
            std::cerr << "Error: cannot create directory: " << errorCode.message() << "\n";
            return false;
        }
    }

    PngData pngData;
    if (!readPngFile(inputPath, pngData)) {
        return false;
    }

    bool success = false;

    if (options.lossless) {
        auto best = findBestCompression(pngData, options.compressionLevel, options.stripMetadata);
        success = best && writePngFile(tempPath, pngData, options.compressionLevel,
                                       options.stripMetadata, best->filterType,
                                       best->deflateStrategy);
    } else {
        auto quantized = quantizeMedianCut(pngData, options.quantizationColors);
        if (quantized.colorCount > 0) {
            success = writePaletteFile(tempPath, quantized.data, quantized.palette,
                                       quantized.colorCount, options.compressionLevel).has_value();
        } else {
            auto best = findBestCompression(pngData, options.compressionLevel, options.stripMetadata);
            success = best && writePngFile(tempPath, pngData, options.compressionLevel,
                                           options.stripMetadata, best->filterType,
                                           best->deflateStrategy);
        }
    }

    cleanupPngData(pngData);

    if (!success) return false;

    std::error_code errorCode;
    auto compressedSize = fs::file_size(tempPath, errorCode);

    if (errorCode) return false;

    if (compressedSize >= originalSize) {
        fs::remove(tempPath, errorCode);
        std::cout << "  " << fs::path(inputPath).filename().string()
                  << ": " << originalSize << " bytes (already optimal)\n";
        return true;
    }

    if (tempPath != outputPath) {
        fs::rename(tempPath, outputPath, errorCode);
        if (errorCode) {
            fs::remove(tempPath, errorCode);
            return false;
        }
    }

    double ratio = (1.0 - static_cast<double>(compressedSize) / originalSize) * 100.0;
    std::cout << "  " << fs::path(inputPath).filename().string()
              << ": " << originalSize << " -> " << compressedSize
              << " bytes (-" << ratio << "%)\n";

    return true;
}

std::vector<std::string> findPngFiles(const std::string& directory) {
    namespace fs = std::filesystem;
    std::vector<std::string> pngFiles;

    std::error_code errorCode;
    for (const auto& entry : fs::directory_iterator(directory, errorCode)) {
        if (entry.is_regular_file()) {
            std::string extension = entry.path().extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

            if (extension == ".png") {
                pngFiles.push_back(entry.path().string());
            }
        }
    }

    std::sort(pngFiles.begin(), pngFiles.end());
    return pngFiles;
}

ParseResult parseArguments(int argc, char* argv[], PngCompressionOptions& options) {
    for (int i = 1; i < argc; i++) {
        std::string argument = argv[i];

        if (argument == "-h" || argument == "--help") {
            printUsage(argv[0]);
            return ParseResult::ExitWithSuccess;
        } else if (argument == "--lossless") {
            options.lossless = true;
        } else if (argument == "-s" || argument == "--strip") {
            options.stripMetadata = true;
        } else if (argument == "-w" || argument == "--overwrite") {
            options.overwrite = true;
        } else if (argument == "-l" || argument == "--level") {
            if (i + 1 < argc) {
                int level = 0;
                if (!parseIntegerValue(argv[++i], level) || level < 1 || level > 9) {
                    std::cerr << "Error: level must be a number between 1 and 9\n";
                    return ParseResult::ExitWithError;
                }
                options.compressionLevel = level;
            } else {
                std::cerr << "Error: --level requires a value (1-9)\n";
                return ParseResult::ExitWithError;
            }
        } else if (argument == "-c" || argument == "--colors") {
            if (i + 1 < argc) {
                int colors = 0;
                if (!parseIntegerValue(argv[++i], colors) || colors < 2 || colors > 256) {
                    std::cerr << "Error: colors must be a number between 2 and 256\n";
                    return ParseResult::ExitWithError;
                }
                options.quantizationColors = colors;
            } else {
                std::cerr << "Error: --colors requires a value (2-256)\n";
                return ParseResult::ExitWithError;
            }
        } else if (argument == "-o" || argument == "--output") {
            if (i + 1 < argc) {
                options.outputDir = argv[++i];
            } else {
                std::cerr << "Error: --output requires a directory\n";
                return ParseResult::ExitWithError;
            }
        } else if (argument[0] != '-') {
            options.inputFiles.push_back(argument);
        } else {
            std::cerr << "Error: unknown option: " << argument << "\n";
            std::cerr << "Use --help to see available options\n";
            return ParseResult::ExitWithError;
        }
    }

    return ParseResult::Continue;
}

ImageQualityResult compareImages(const PngData& original, const PngData& compressed) {
    ImageQualityResult result = { 0.0, 0.0, false };

    if (original.width != compressed.width || original.height != compressed.height) {
        return result;
    }

    if (original.imageData.empty() || compressed.imageData.empty()) {
        return result;
    }

    size_t pixelCount = original.width * original.height;
    double sumSquaredDiff = 0.0;

    size_t rowBytesOriginal = original.imageData.size() / original.height;
    size_t rowBytesCompressed = compressed.imageData.size() / compressed.height;

    size_t channelsOriginal = rowBytesOriginal / original.width;
    size_t channelsCompressed = rowBytesCompressed / compressed.width;

    for (png_uint_32 y = 0; y < original.height; y++) {
        for (png_uint_32 x = 0; x < original.width; x++) {
            size_t originalIndex = y * rowBytesOriginal + x * channelsOriginal;
            size_t compressedIndex = y * rowBytesCompressed + x * channelsCompressed;

            for (size_t c = 0; c < std::min(channelsOriginal, channelsCompressed); c++) {
                double difference = static_cast<double>(original.imageData[originalIndex + c]) -
                                    static_cast<double>(compressed.imageData[compressedIndex + c]);
                sumSquaredDiff += difference * difference;
            }
        }
    }

    result.mse = sumSquaredDiff / (pixelCount * std::min(channelsOriginal, channelsCompressed));

    if (result.mse == 0.0) {
        result.psnr = std::numeric_limits<double>::infinity();
        result.isLossless = true;
    } else {
        result.psnr = 10.0 * std::log10((255.0 * 255.0) / result.mse);
    }

    return result;
}

QuantizedResult quantizeMedianCut(const PngData& source, int colorCount) {
    if (source.colorType == PNG_COLOR_TYPE_PALETTE) {
        QuantizedResult result;
        result.data.width = source.width;
        result.data.height = source.height;
        result.data.bitDepth = source.bitDepth;
        result.data.colorType = source.colorType;
        result.data.imageData = source.imageData;
        result.data.rowPointers.resize(source.height);
        for (png_uint_32 y = 0; y < source.height; y++) {
            result.data.rowPointers[y] = result.data.imageData.data() + y * source.width;
        }
        result.colorCount = 0;
        return result;
    }

    size_t rowBytes = source.imageData.size() / source.height;
    size_t channels = rowBytes / source.width;

    if (channels < 3) {
        QuantizedResult result;
        result.colorCount = 0;
        return result;
    }

    size_t pixelCount = source.width * source.height;

    struct ColorCount {
        unsigned char r, g, b, a;
        size_t count;
    };

    std::unordered_map<uint32_t, ColorCount> colorFrequency;
    for (size_t i = 0; i < pixelCount; i++) {
        png_uint_32 y = i / source.width;
        png_uint_32 x = i % source.width;
        size_t byteIndex = y * rowBytes + x * channels;
        unsigned char r = source.imageData[byteIndex];
        unsigned char g = source.imageData[byteIndex + 1];
        unsigned char b = source.imageData[byteIndex + 2];
        unsigned char a = (channels >= 4) ? source.imageData[byteIndex + 3] : 255;
        uint32_t key = (static_cast<uint32_t>(r) << 24) | (static_cast<uint32_t>(g) << 16) |
                       (static_cast<uint32_t>(b) << 8) | a;
        auto iterator = colorFrequency.find(key);
        if (iterator != colorFrequency.end()) {
            iterator->second.count++;
        } else {
            colorFrequency[key] = {r, g, b, a, 1};
        }
    }

    std::vector<ColorCount> sortedColors;
    sortedColors.reserve(colorFrequency.size());
    for (const auto& pair : colorFrequency) {
        sortedColors.push_back(pair.second);
    }
    std::sort(sortedColors.begin(), sortedColors.end(),
        [](const ColorCount& a, const ColorCount& b) { return a.count > b.count; });

    int totalUniqueColors = static_cast<int>(sortedColors.size());
    int frequencySlots = std::min(totalUniqueColors, colorCount / 2);
    int medianSlots = colorCount - frequencySlots;

    std::vector<PaletteColor> palette;
    palette.reserve(colorCount);
    for (int i = 0; i < frequencySlots; i++) {
        palette.push_back({double(sortedColors[i].r), double(sortedColors[i].g),
                           double(sortedColors[i].b), double(sortedColors[i].a)});
    }

    if (medianSlots > 0 && totalUniqueColors > frequencySlots) {
        std::vector<PaletteColor> lessCommon;
        for (int i = frequencySlots; i < totalUniqueColors; i++) {
            lessCommon.push_back({double(sortedColors[i].r), double(sortedColors[i].g),
                                  double(sortedColors[i].b), double(sortedColors[i].a)});
        }

        std::vector<PaletteColor> boxes[256];
        boxes[0] = std::move(lessCommon);
        int numberOfBoxes = 1;

        while (numberOfBoxes < medianSlots && numberOfBoxes < 256) {
            int largestBoxIndex = 0;
            double largestVolume = 0;
            for (int i = 0; i < numberOfBoxes; i++) {
                if (boxes[i].size() < 2) continue;
                ColorBoxBounds bounds = computeColorBoxBounds(boxes[i]);
                double redRange = bounds.maximum.r - bounds.minimum.r;
                double greenRange = bounds.maximum.g - bounds.minimum.g;
                double blueRange = bounds.maximum.b - bounds.minimum.b;
                double alphaRange = bounds.maximum.a - bounds.minimum.a;
                double volume = redRange * redRange + greenRange * greenRange +
                                blueRange * blueRange + alphaRange * alphaRange;
                if (volume > largestVolume) { largestVolume = volume; largestBoxIndex = i; }
            }
            if (largestVolume == 0) break;

            ColorBoxBounds bounds = computeColorBoxBounds(boxes[largestBoxIndex]);
            double redRange = bounds.maximum.r - bounds.minimum.r;
            double greenRange = bounds.maximum.g - bounds.minimum.g;
            double blueRange = bounds.maximum.b - bounds.minimum.b;
            double alphaRange = bounds.maximum.a - bounds.minimum.a;
            int splitChannel = 0;
            double largestRange = redRange;
            if (greenRange > largestRange) { largestRange = greenRange; splitChannel = 1; }
            if (blueRange > largestRange) { largestRange = blueRange; splitChannel = 2; }
            if (alphaRange > largestRange) { largestRange = alphaRange; splitChannel = 3; }

            auto& box = boxes[largestBoxIndex];
            std::sort(box.begin(), box.end(), [splitChannel](const PaletteColor& a, const PaletteColor& b) {
                switch (splitChannel) {
                    case 0: return a.r < b.r;
                    case 1: return a.g < b.g;
                    case 2: return a.b < b.b;
                    default: return a.a < b.a;
                }
            });

            size_t middle = box.size() / 2;
            boxes[numberOfBoxes].assign(box.begin() + middle, box.end());
            box.resize(middle);
            numberOfBoxes++;
        }

        for (int i = 0; i < numberOfBoxes; i++) {
            double redSum = 0, greenSum = 0, blueSum = 0, alphaSum = 0;
            for (const auto& color : boxes[i]) {
                redSum += color.r;
                greenSum += color.g;
                blueSum += color.b;
                alphaSum += color.a;
            }
            size_t elementCount = boxes[i].size();
            if (elementCount > 0) {
                palette.push_back({redSum / elementCount, greenSum / elementCount,
                                   blueSum / elementCount, alphaSum / elementCount});
            }
        }
    }

    int effectiveColors = static_cast<int>(palette.size());
    if (effectiveColors > colorCount) effectiveColors = colorCount;

    std::vector<int> indices(pixelCount);
    for (size_t i = 0; i < pixelCount; i++) {
        png_uint_32 y = i / source.width;
        png_uint_32 x = i % source.width;
        size_t byteIndex = y * rowBytes + x * channels;
        PaletteColor pixel = {double(source.imageData[byteIndex]),
                              double(source.imageData[byteIndex + 1]),
                              double(source.imageData[byteIndex + 2]),
                              (channels >= 4) ? double(source.imageData[byteIndex + 3]) : 255.0};
        int best = 0;
        double bestDistance = DBL_MAX;
        for (int c = 0; c < effectiveColors; c++) {
            double redDifference = pixel.r - palette[c].r;
            double greenDifference = pixel.g - palette[c].g;
            double blueDifference = pixel.b - palette[c].b;
            double alphaDifference = pixel.a - palette[c].a;
            double distance = 2.0 * redDifference * redDifference +
                              4.0 * greenDifference * greenDifference +
                              3.0 * blueDifference * blueDifference +
                              4.0 * alphaDifference * alphaDifference;
            if (distance < bestDistance) {
                bestDistance = distance;
                best = c;
            }
        }
        indices[i] = best;
    }

    QuantizedResult result;
    result.data.width = source.width;
    result.data.height = source.height;
    result.data.bitDepth = 8;
    result.data.colorType = PNG_COLOR_TYPE_PALETTE;
    result.data.imageData.resize(pixelCount);
    for (size_t i = 0; i < pixelCount; i++) {
        result.data.imageData[i] = static_cast<unsigned char>(indices[i]);
    }
    result.data.rowPointers.resize(source.height);
    for (png_uint_32 y = 0; y < source.height; y++) {
        result.data.rowPointers[y] = result.data.imageData.data() + y * source.width;
    }
    result.palette.resize(effectiveColors);
    for (int i = 0; i < effectiveColors; i++) {
        result.palette[i] = palette[i];
    }
    result.colorCount = effectiveColors;
    return result;
}

std::optional<size_t> writePaletteFile(const std::string& path, const PngData& data,
                                       const std::vector<PaletteColor>& palette, int colorCount,
                                       int compressionLevel) {
    volatile int clampedLevel = clampCompressionLevel(compressionLevel);

    std::unique_ptr<FILE, FileCloser> filePointer(fopen(path.c_str(), "wb"));
    if (!filePointer) return std::nullopt;

    png_structp png = nullptr;
    png_infop info = nullptr;
    if (!createPngWriter(png, info)) return std::nullopt;

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        return std::nullopt;
    }

    png_init_io(png, filePointer.get());
    png_set_compression_level(png, clampedLevel);
    png_set_filter(png, 0, PNG_FILTER_NONE);
    png_set_keep_unknown_chunks(png, PNG_HANDLE_CHUNK_NEVER, nullptr, 0);

    png_set_IHDR(png, info, data.width, data.height, data.bitDepth, data.colorType,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    std::vector<png_color> paletteColors(colorCount);
    std::vector<unsigned char> alphaValues(colorCount);
    bool hasTransparency = false;
    for (int i = 0; i < colorCount; i++) {
        paletteColors[i].red = static_cast<png_byte>(std::lround(std::max(0.0, std::min(255.0, palette[i].r))));
        paletteColors[i].green = static_cast<png_byte>(std::lround(std::max(0.0, std::min(255.0, palette[i].g))));
        paletteColors[i].blue = static_cast<png_byte>(std::lround(std::max(0.0, std::min(255.0, palette[i].b))));
        alphaValues[i] = static_cast<unsigned char>(std::lround(std::max(0.0, std::min(255.0, palette[i].a))));
        if (alphaValues[i] < 255) hasTransparency = true;
    }
    png_set_PLTE(png, info, paletteColors.data(), colorCount);
    if (hasTransparency) {
        png_set_tRNS(png, info, alphaValues.data(), colorCount, nullptr);
    }

    png_write_info(png, info);

    for (png_uint_32 y = 0; y < data.height; y++) {
        png_write_row(png, data.rowPointers[y]);
    }

    png_write_end(png, nullptr);
    png_destroy_write_struct(&png, &info);
    filePointer.reset();

    std::error_code errorCode;
    auto fileSize = std::filesystem::file_size(path, errorCode);
    if (errorCode) return std::nullopt;
    return fileSize;
}
