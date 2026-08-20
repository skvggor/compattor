#pragma once

#include <png.h>
#include <zlib.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct PngCompressionOptions {
    int compressionLevel = 9;
    bool stripMetadata = true;
    bool overwrite = false;
    bool lossless = false;
    int quantizationColors = 256;
    std::string outputDir;
    std::vector<std::string> inputFiles;
};

enum class ParseResult {
    Continue,
    ExitWithSuccess,
    ExitWithError
};

struct PngData {
    std::vector<unsigned char*> rowPointers;
    std::vector<unsigned char> imageData;
    png_uint_32 width = 0;
    png_uint_32 height = 0;
    int colorType = 0;
    int bitDepth = 0;
};

struct PaletteColor { double r, g, b, a; };

struct QuantizedResult {
    PngData data;
    std::vector<PaletteColor> palette;
    int colorCount = 0;
};

struct BestCompressionResult {
    int filterType;
    int deflateStrategy;
    size_t compressedSize;
};

struct ImageQualityResult {
    double psnr;
    double mse;
    bool isLossless;
};

bool hasPngSignature(const std::string& filePath);
void cleanupPngData(PngData& data);
bool readPngFile(const std::string& filePath, PngData& data);
bool writePngFile(const std::string& filePath, const PngData& data,
                  int compressionLevel, bool stripMetadata,
                  int filterType = PNG_FILTER_NONE,
                  int deflateStrategy = Z_DEFAULT_STRATEGY);
std::string getOutputPath(const std::string& inputPath, const std::string& outputDir);
bool processFile(const std::string& inputPath, const PngCompressionOptions& options);
std::vector<std::string> findPngFiles(const std::string& directory);
void printUsage(const char* programName);
ParseResult parseArguments(int argc, char* argv[], PngCompressionOptions& options);
std::optional<BestCompressionResult> findBestCompression(const PngData& data, int compressionLevel,
                                                          bool stripMetadata);
std::optional<size_t> writePngToBuffer(std::vector<unsigned char>& buffer, const PngData& data,
                                       int compressionLevel, bool stripMetadata,
                                       int filterType, int deflateStrategy);
ImageQualityResult compareImages(const PngData& original, const PngData& compressed);
QuantizedResult quantizeMedianCut(const PngData& source, int colorCount);
std::optional<size_t> writePaletteFile(const std::string& path, const PngData& data,
                                       const std::vector<PaletteColor>& palette, int colorCount,
                                       int compressionLevel);
