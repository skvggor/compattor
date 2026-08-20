#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include "compattor.h"

namespace fs = std::filesystem;

class CompattorTest : public ::testing::Test {
protected:
    fs::path testDir;
    fs::path outputDir;

    void SetUp() override {
        testDir = fs::temp_directory_path() / "compattor_test";
        outputDir = testDir / "output";
        fs::create_directories(testDir);
        fs::create_directories(outputDir);
    }

    void TearDown() override {
        std::error_code errorCode;
        fs::remove_all(testDir, errorCode);
    }

    fs::path createTestPng(const std::string& name, int width = 4, int height = 4,
                           int colorType = 2, int bitDepth = 8) {
        fs::path filePath = testDir / name;

        FILE* filePointer = fopen(filePath.string().c_str(), "wb");
        EXPECT_NE(filePointer, nullptr);
        if (!filePointer) return filePath;

        png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        EXPECT_NE(png, nullptr);
        if (!png) { fclose(filePointer); return filePath; }

        png_infop info = png_create_info_struct(png);
        EXPECT_NE(info, nullptr);
        if (!info) { png_destroy_write_struct(&png, nullptr); fclose(filePointer); return filePath; }

        if (setjmp(png_jmpbuf(png))) {
            png_destroy_write_struct(&png, &info);
            fclose(filePointer);
            return filePath;
        }

        png_init_io(png, filePointer);
        png_set_IHDR(png, info, width, height, bitDepth, colorType,
                     PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                     PNG_FILTER_TYPE_DEFAULT);

        if (colorType == PNG_COLOR_TYPE_PALETTE) {
            png_color paletteColors[4] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 255}};
            png_set_PLTE(png, info, paletteColors, 4);
        }

        png_write_info(png, info);

        int channels = (colorType == PNG_COLOR_TYPE_RGB) ? 3 :
                       (colorType == PNG_COLOR_TYPE_RGBA) ? 4 :
                       (colorType == PNG_COLOR_TYPE_GRAY) ? 1 : 3;

        size_t rowBytes;
        if (colorType == PNG_COLOR_TYPE_PALETTE) {
            rowBytes = static_cast<size_t>(width);
        } else if (colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8) {
            rowBytes = (static_cast<size_t>(width) * bitDepth + 7) / 8;
        } else if (bitDepth == 16) {
            rowBytes = static_cast<size_t>(width) * channels * 2;
        } else {
            rowBytes = static_cast<size_t>(width) * channels;
        }

        std::vector<unsigned char> row(rowBytes);
        for (int y = 0; y < height; y++) {
            if (colorType == PNG_COLOR_TYPE_PALETTE) {
                for (int x = 0; x < width; x++) {
                    row[x] = static_cast<unsigned char>((x + y) % 4);
                }
            } else {
                for (size_t x = 0; x < rowBytes; x++) {
                    row[x] = static_cast<unsigned char>((x + y * 7) % 256);
                }
            }
            png_write_row(png, row.data());
        }

        png_write_end(png, nullptr);
        png_destroy_write_struct(&png, &info);
        fclose(filePointer);

        return filePath;
    }

    fs::path createRgbPngWithRow(const std::string& name, int width, int height,
                                 const std::vector<unsigned char>& row) {
        fs::path filePath = testDir / name;

        FILE* filePointer = fopen(filePath.string().c_str(), "wb");
        EXPECT_NE(filePointer, nullptr);
        if (!filePointer) return filePath;

        png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        EXPECT_NE(png, nullptr);
        if (!png) { fclose(filePointer); return filePath; }

        png_infop info = png_create_info_struct(png);
        EXPECT_NE(info, nullptr);
        if (!info) { png_destroy_write_struct(&png, nullptr); fclose(filePointer); return filePath; }

        if (setjmp(png_jmpbuf(png))) {
            png_destroy_write_struct(&png, &info);
            fclose(filePointer);
            return filePath;
        }

        png_init_io(png, filePointer);
        png_set_IHDR(png, info, width, height, 8, PNG_COLOR_TYPE_RGB,
                     PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                     PNG_FILTER_TYPE_DEFAULT);
        png_write_info(png, info);

        for (int y = 0; y < height; y++) {
            png_write_row(png, row.data());
        }

        png_write_end(png, nullptr);
        png_destroy_write_struct(&png, &info);
        fclose(filePointer);

        return filePath;
    }

    fs::path createNoisePng(const std::string& name, int width, int height, int colorType) {
        int channels = (colorType == PNG_COLOR_TYPE_RGBA) ? 4 : 3;

        std::vector<unsigned char> row(static_cast<size_t>(width) * channels);
        unsigned int state = 12345;
        auto nextByte = [&state]() {
            state = state * 1103515245 + 12345;
            return static_cast<unsigned char>((state >> 16) & 0xFF);
        };

        for (size_t i = 0; i < row.size(); i++) {
            row[i] = nextByte();
        }

        fs::path filePath = testDir / name;

        FILE* filePointer = fopen(filePath.string().c_str(), "wb");
        EXPECT_NE(filePointer, nullptr);
        if (!filePointer) return filePath;

        png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        EXPECT_NE(png, nullptr);
        if (!png) { fclose(filePointer); return filePath; }

        png_infop info = png_create_info_struct(png);
        EXPECT_NE(info, nullptr);
        if (!info) { png_destroy_write_struct(&png, nullptr); fclose(filePointer); return filePath; }

        if (setjmp(png_jmpbuf(png))) {
            png_destroy_write_struct(&png, &info);
            fclose(filePointer);
            return filePath;
        }

        png_init_io(png, filePointer);
        png_set_IHDR(png, info, width, height, 8, colorType,
                     PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                     PNG_FILTER_TYPE_DEFAULT);
        png_write_info(png, info);

        for (int y = 0; y < height; y++) {
            for (size_t i = 0; i < row.size(); i++) {
                row[i] = nextByte();
            }
            png_write_row(png, row.data());
        }

        png_write_end(png, nullptr);
        png_destroy_write_struct(&png, &info);
        fclose(filePointer);

        return filePath;
    }

    void createNonPngFile(const std::string& name) {
        fs::path filePath = testDir / name;
        std::ofstream file(filePath);
        file << "This is not a PNG file";
    }

    void createShortFile(const std::string& name) {
        fs::path filePath = testDir / name;
        std::ofstream file(filePath, std::ios::binary);
        file << "abc";
    }
};

TEST_F(CompattorTest, HasPngSignatureReturnsTrueForValidPng) {
    auto png = createTestPng("valid.png");
    EXPECT_TRUE(hasPngSignature(png.string()));
}

TEST_F(CompattorTest, HasPngSignatureReturnsFalseForNonPng) {
    createNonPngFile("not_png.txt");
    EXPECT_FALSE(hasPngSignature((testDir / "not_png.txt").string()));
}

TEST_F(CompattorTest, HasPngSignatureReturnsFalseForNonExistent) {
    EXPECT_FALSE(hasPngSignature((testDir / "nonexistent.png").string()));
}

TEST_F(CompattorTest, HasPngSignatureHandlesFileShorterThanEightBytes) {
    createShortFile("short.png");
    EXPECT_FALSE(hasPngSignature((testDir / "short.png").string()));
}

TEST_F(CompattorTest, CleanupPngDataClearsPointers) {
    PngData data;
    data.rowPointers.resize(2);
    data.imageData.resize(20);
    EXPECT_EQ(data.rowPointers.size(), 2u);

    cleanupPngData(data);
    EXPECT_TRUE(data.rowPointers.empty());
    EXPECT_TRUE(data.imageData.empty());
}

TEST_F(CompattorTest, ReadPngFileReadsValidPng) {
    auto png = createTestPng("read_test.png");
    PngData data;

    EXPECT_TRUE(readPngFile(png.string(), data));
    EXPECT_EQ(data.width, 4u);
    EXPECT_EQ(data.height, 4u);
    EXPECT_EQ(data.bitDepth, 8);
    EXPECT_EQ(data.colorType, PNG_COLOR_TYPE_RGB);
    EXPECT_FALSE(data.rowPointers.empty());

    cleanupPngData(data);
}

TEST_F(CompattorTest, ReadPngFileFailsForNonExistent) {
    PngData data;
    EXPECT_FALSE(readPngFile((testDir / "missing.png").string(), data));
}

TEST_F(CompattorTest, ReadPngFileFailsForInvalidPng) {
    createNonPngFile("invalid.png");
    PngData data;
    EXPECT_FALSE(readPngFile((testDir / "invalid.png").string(), data));
}

TEST_F(CompattorTest, ReadPngFileFailsForFileShorterThanEightBytes) {
    createShortFile("short_invalid.png");
    PngData data;
    EXPECT_FALSE(readPngFile((testDir / "short_invalid.png").string(), data));
}

TEST_F(CompattorTest, ReadPngFileConvertsPaletteToRgb) {
    auto png = createTestPng("palette_input.png", 4, 4, PNG_COLOR_TYPE_PALETTE, 8);
    PngData data;

    ASSERT_TRUE(readPngFile(png.string(), data));
    EXPECT_EQ(data.colorType, PNG_COLOR_TYPE_RGB);
    EXPECT_EQ(data.bitDepth, 8);
    EXPECT_EQ(data.imageData.size(), 4u * 4u * 3u);

    cleanupPngData(data);
}

TEST_F(CompattorTest, ReadPngFileStripsSixteenBitToEightBit) {
    auto png = createTestPng("sixteen_bit.png", 4, 4, PNG_COLOR_TYPE_RGB, 16);
    PngData data;

    ASSERT_TRUE(readPngFile(png.string(), data));
    EXPECT_EQ(data.bitDepth, 8);
    EXPECT_EQ(data.imageData.size(), 4u * 4u * 3u);

    cleanupPngData(data);
}

TEST_F(CompattorTest, ReadPngFileExpandsOneBitGrayToEightBit) {
    auto png = createTestPng("gray_one_bit.png", 8, 8, PNG_COLOR_TYPE_GRAY, 1);
    PngData data;

    ASSERT_TRUE(readPngFile(png.string(), data));
    EXPECT_EQ(data.bitDepth, 8);
    EXPECT_EQ(data.colorType, PNG_COLOR_TYPE_GRAY);
    EXPECT_EQ(data.imageData.size(), 8u * 8u);

    cleanupPngData(data);
}

TEST_F(CompattorTest, WritePngFileCreatesValidFile) {
    auto original = createTestPng("write_test.png");
    PngData data;
    ASSERT_TRUE(readPngFile(original.string(), data));

    fs::path output = outputDir / "output.png";
    EXPECT_TRUE(writePngFile(output.string(), data, 6, false));
    EXPECT_TRUE(fs::exists(output));
    EXPECT_TRUE(hasPngSignature(output.string()));

    cleanupPngData(data);
}

TEST_F(CompattorTest, WritePngFileWithStripMetadata) {
    auto original = createTestPng("strip_test.png");
    PngData data;
    ASSERT_TRUE(readPngFile(original.string(), data));

    fs::path output = outputDir / "stripped.png";
    EXPECT_TRUE(writePngFile(output.string(), data, 6, true));
    EXPECT_TRUE(fs::exists(output));

    cleanupPngData(data);
}

TEST_F(CompattorTest, WritePngFileClampsCompressionLevel) {
    auto original = createTestPng("clamp_test.png");
    PngData data;
    ASSERT_TRUE(readPngFile(original.string(), data));

    fs::path outputLow = outputDir / "clamp_low.png";
    EXPECT_TRUE(writePngFile(outputLow.string(), data, 0, false));
    EXPECT_TRUE(fs::exists(outputLow));

    fs::path outputHigh = outputDir / "clamp_high.png";
    EXPECT_TRUE(writePngFile(outputHigh.string(), data, 100, false));
    EXPECT_TRUE(fs::exists(outputHigh));

    cleanupPngData(data);
}

TEST_F(CompattorTest, WritePngFileFailsForInvalidPath) {
    PngData data;
    data.width = 2;
    data.height = 2;
    data.bitDepth = 8;
    data.colorType = PNG_COLOR_TYPE_RGB;
    data.rowPointers.resize(2);
    data.imageData.resize(12);
    for (size_t i = 0; i < 2; i++) {
        data.rowPointers[i] = data.imageData.data() + i * 6;
    }

    EXPECT_FALSE(writePngFile("/nonexistent/dir/file.png", data, 6, false));

    cleanupPngData(data);
}

TEST_F(CompattorTest, WritePngToBufferProducesValidPngData) {
    auto original = createTestPng("buffer_test.png");
    PngData data;
    ASSERT_TRUE(readPngFile(original.string(), data));

    std::vector<unsigned char> buffer;
    auto size = writePngToBuffer(buffer, data, 9, true, PNG_FILTER_PAETH, Z_DEFAULT_STRATEGY);

    ASSERT_TRUE(size.has_value());
    EXPECT_GT(*size, 0u);
    EXPECT_EQ(buffer.size(), *size);
    ASSERT_GE(buffer.size(), 8u);
    EXPECT_EQ(buffer[0], 0x89);
    EXPECT_EQ(buffer[1], 0x50);
    EXPECT_EQ(buffer[2], 0x4E);
    EXPECT_EQ(buffer[3], 0x47);

    cleanupPngData(data);
}

TEST_F(CompattorTest, FindBestCompressionReturnsValidResult) {
    auto original = createTestPng("best_compression.png", 32, 32);
    PngData data;
    ASSERT_TRUE(readPngFile(original.string(), data));

    auto best = findBestCompression(data, 9, true);

    ASSERT_TRUE(best.has_value());
    EXPECT_GT(best->compressedSize, 0u);
    EXPECT_GE(best->filterType, PNG_FILTER_NONE);
    EXPECT_LE(best->filterType, PNG_FILTER_PAETH);

    cleanupPngData(data);
}

TEST_F(CompattorTest, GetOutputPathUsesDefaultDir) {
    fs::path input = "/some/path/image.png";
    std::string result = getOutputPath(input.string(), "");
    EXPECT_EQ(result, "/some/path/compressed/image.png");
}

TEST_F(CompattorTest, GetOutputPathUsesCustomDir) {
    fs::path input = "/some/path/image.png";
    std::string result = getOutputPath(input.string(), "/custom/output");
    EXPECT_EQ(result, "/custom/output/image.png");
}

TEST_F(CompattorTest, ProcessFileCompressesPng) {
    std::vector<unsigned char> row(256 * 3, 128);
    auto filePath = createRgbPngWithRow("process_test.png", 256, 256, row);

    PngCompressionOptions options;
    options.compressionLevel = 9;
    options.lossless = true;
    options.outputDir = outputDir.string();

    EXPECT_TRUE(processFile(filePath.string(), options));
    EXPECT_TRUE(fs::exists(outputDir / "process_test.png"));
    EXPECT_FALSE(fs::exists(outputDir / "process_test.png.tmp"));
}

TEST_F(CompattorTest, ProcessFileSkipsNonPng) {
    createNonPngFile("not_a_png.txt");

    PngCompressionOptions options;
    EXPECT_TRUE(processFile((testDir / "not_a_png.txt").string(), options));
}

TEST_F(CompattorTest, ProcessFileFailsForNonExistent) {
    PngCompressionOptions options;
    EXPECT_FALSE(processFile((testDir / "missing.png").string(), options));
}

TEST_F(CompattorTest, ProcessFileOverwriteMode) {
    auto png = createTestPng("overwrite_test.png", 128, 128);

    PngCompressionOptions options;
    options.compressionLevel = 9;
    options.overwrite = true;

    EXPECT_TRUE(processFile(png.string(), options));
    EXPECT_TRUE(fs::exists(png));
    EXPECT_TRUE(hasPngSignature(png.string()));
    EXPECT_FALSE(fs::exists(png.string() + ".tmp"));
}

TEST_F(CompattorTest, ProcessFilePaletteInputProducesValidOutput) {
    auto png = createTestPng("palette_process.png", 64, 64, PNG_COLOR_TYPE_PALETTE, 8);

    PngCompressionOptions options;
    options.overwrite = true;

    EXPECT_TRUE(processFile(png.string(), options));
    EXPECT_TRUE(hasPngSignature(png.string()));

    PngData data;
    ASSERT_TRUE(readPngFile(png.string(), data));
    EXPECT_EQ(data.width, 64u);
    EXPECT_EQ(data.height, 64u);
    cleanupPngData(data);
}

TEST_F(CompattorTest, ProcessFileGrayscaleFallsBackToLossless) {
    auto png = createTestPng("grayscale.png", 8, 8, PNG_COLOR_TYPE_GRAY, 8);

    PngCompressionOptions options;
    options.outputDir = outputDir.string();

    EXPECT_TRUE(processFile(png.string(), options));

    fs::path output = outputDir / "grayscale.png";
    ASSERT_TRUE(fs::exists(output));

    PngData data;
    ASSERT_TRUE(readPngFile(output.string(), data));
    EXPECT_EQ(data.colorType, PNG_COLOR_TYPE_GRAY);
    cleanupPngData(data);
}

TEST_F(CompattorTest, ProcessFileRgbaQuantizesPreservingTransparency) {
    auto png = createNoisePng("rgba_input.png", 64, 64, PNG_COLOR_TYPE_RGBA);

    PngCompressionOptions options;
    options.outputDir = outputDir.string();

    EXPECT_TRUE(processFile(png.string(), options));

    fs::path output = outputDir / "rgba_input.png";
    ASSERT_TRUE(fs::exists(output));

    PngData data;
    ASSERT_TRUE(readPngFile(output.string(), data));
    EXPECT_EQ(data.width, 64u);
    EXPECT_EQ(data.height, 64u);
    EXPECT_EQ(data.colorType, PNG_COLOR_TYPE_RGBA);
    EXPECT_EQ(data.imageData.size(), 64u * 64u * 4u);
    cleanupPngData(data);
}

TEST_F(CompattorTest, ProcessFileRgbQuantizesToPalette) {
    auto png = createNoisePng("rgb_input.png", 64, 64, PNG_COLOR_TYPE_RGB);

    PngCompressionOptions options;
    options.outputDir = outputDir.string();

    EXPECT_TRUE(processFile(png.string(), options));

    fs::path output = outputDir / "rgb_input.png";
    ASSERT_TRUE(fs::exists(output));

    PngData data;
    ASSERT_TRUE(readPngFile(output.string(), data));
    EXPECT_EQ(data.width, 64u);
    EXPECT_EQ(data.height, 64u);
    EXPECT_EQ(data.colorType, PNG_COLOR_TYPE_RGB);
    EXPECT_EQ(data.imageData.size(), 64u * 64u * 3u);
    cleanupPngData(data);
}

TEST_F(CompattorTest, ProcessFileCompressionActuallyReducesSize) {
    std::vector<unsigned char> row(256 * 3, 128);
    auto filePath = createRgbPngWithRow("compress_test.png", 256, 256, row);

    auto originalSize = fs::file_size(filePath);

    PngCompressionOptions options;
    options.compressionLevel = 9;
    options.lossless = true;
    options.outputDir = outputDir.string();

    EXPECT_TRUE(processFile(filePath.string(), options));

    auto compressedSize = fs::file_size(outputDir / "compress_test.png");
    EXPECT_LT(compressedSize, originalSize);
}

TEST_F(CompattorTest, FindPngFilesFindsCorrectFiles) {
    createTestPng("a.png");
    createTestPng("b.png");
    createTestPng("c.PNG");
    createNonPngFile("d.txt");
    createNonPngFile("e.jpg");

    auto files = findPngFiles(testDir.string());
    EXPECT_EQ(files.size(), 3u);
}

TEST_F(CompattorTest, FindPngFilesReturnsEmptyForEmptyDir) {
    fs::path emptyDir = testDir / "empty";
    fs::create_directories(emptyDir);

    auto files = findPngFiles(emptyDir.string());
    EXPECT_TRUE(files.empty());
}

TEST_F(CompattorTest, FindPngFilesSortedAlphabetically) {
    createTestPng("z.png");
    createTestPng("a.png");
    createTestPng("m.png");

    auto files = findPngFiles(testDir.string());
    ASSERT_EQ(files.size(), 3u);

    EXPECT_TRUE(files[0].find("a.png") != std::string::npos);
    EXPECT_TRUE(files[1].find("m.png") != std::string::npos);
    EXPECT_TRUE(files[2].find("z.png") != std::string::npos);
}

TEST_F(CompattorTest, ParseArgumentsHelpReturnsExitWithSuccess) {
    char prog[] = "compattor";
    char help[] = "--help";
    char* argv[] = {prog, help, nullptr};
    int argc = 2;

    PngCompressionOptions options;
    EXPECT_EQ(parseArguments(argc, argv, options), ParseResult::ExitWithSuccess);
}

TEST_F(CompattorTest, ParseArgumentsShortHelpReturnsExitWithSuccess) {
    char prog[] = "compattor";
    char help[] = "-h";
    char* argv[] = {prog, help, nullptr};
    int argc = 2;

    PngCompressionOptions options;
    EXPECT_EQ(parseArguments(argc, argv, options), ParseResult::ExitWithSuccess);
}

TEST_F(CompattorTest, ParseArgumentsLosslessMode) {
    char prog[] = "compattor";
    char flag[] = "--lossless";
    char file[] = "test.png";
    char* argv[] = {prog, flag, file, nullptr};
    int argc = 3;

    PngCompressionOptions options;
    EXPECT_EQ(parseArguments(argc, argv, options), ParseResult::Continue);
    EXPECT_TRUE(options.lossless);
}

TEST_F(CompattorTest, ParseArgumentsColorsSetsValue) {
    char prog[] = "compattor";
    char flag[] = "--colors";
    char value[] = "128";
    char file[] = "test.png";
    char* argv[] = {prog, flag, value, file, nullptr};
    int argc = 4;

    PngCompressionOptions options;
    EXPECT_EQ(parseArguments(argc, argv, options), ParseResult::Continue);
    EXPECT_EQ(options.quantizationColors, 128);
}

TEST_F(CompattorTest, ParseArgumentsLevelSetsCustomValue) {
    char prog[] = "compattor";
    char flag[] = "--level";
    char value[] = "7";
    char file[] = "test.png";
    char* argv[] = {prog, flag, value, file, nullptr};
    int argc = 4;

    PngCompressionOptions options;
    EXPECT_EQ(parseArguments(argc, argv, options), ParseResult::Continue);
    EXPECT_EQ(options.compressionLevel, 7);
}

TEST_F(CompattorTest, ParseArgumentsLevelShortFlag) {
    char prog[] = "compattor";
    char flag[] = "-l";
    char value[] = "3";
    char file[] = "test.png";
    char* argv[] = {prog, flag, value, file, nullptr};
    int argc = 4;

    PngCompressionOptions options;
    EXPECT_EQ(parseArguments(argc, argv, options), ParseResult::Continue);
    EXPECT_EQ(options.compressionLevel, 3);
}

TEST_F(CompattorTest, ParseArgumentsInvalidLevelReturnsExitWithError) {
    char prog[] = "compattor";
    char flag[] = "--level";
    char value[] = "10";
    char* argv[] = {prog, flag, value, nullptr};
    int argc = 3;

    PngCompressionOptions options;
    EXPECT_EQ(parseArguments(argc, argv, options), ParseResult::ExitWithError);
}

TEST_F(CompattorTest, ParseArgumentsNonNumericLevelReturnsExitWithError) {
    char prog[] = "compattor";
    char flag[] = "--level";
    char value[] = "abc";
    char* argv[] = {prog, flag, value, nullptr};
    int argc = 3;

    PngCompressionOptions options;
    EXPECT_EQ(parseArguments(argc, argv, options), ParseResult::ExitWithError);
}

TEST_F(CompattorTest, ParseArgumentsLevelWithTrailingJunkReturnsExitWithError) {
    char prog[] = "compattor";
    char flag[] = "--level";
    char value[] = "3abc";
    char* argv[] = {prog, flag, value, nullptr};
    int argc = 3;

    PngCompressionOptions options;
    EXPECT_EQ(parseArguments(argc, argv, options), ParseResult::ExitWithError);
}

TEST_F(CompattorTest, ParseArgumentsNonNumericColorsReturnsExitWithError) {
    char prog[] = "compattor";
    char flag[] = "--colors";
    char value[] = "abc";
    char* argv[] = {prog, flag, value, nullptr};
    int argc = 3;

    PngCompressionOptions options;
    EXPECT_EQ(parseArguments(argc, argv, options), ParseResult::ExitWithError);
}

TEST_F(CompattorTest, ParseArgumentsLevelMissingValueReturnsExitWithError) {
    char prog[] = "compattor";
    char flag[] = "--level";
    char* argv[] = {prog, flag, nullptr};
    int argc = 2;

    PngCompressionOptions options;
    EXPECT_EQ(parseArguments(argc, argv, options), ParseResult::ExitWithError);
}

TEST_F(CompattorTest, ParseArgumentsOutputSetsDirectory) {
    char prog[] = "compattor";
    char flag[] = "--output";
    char directory[] = "/tmp/out";
    char file[] = "test.png";
    char* argv[] = {prog, flag, directory, file, nullptr};
    int argc = 4;

    PngCompressionOptions options;
    EXPECT_EQ(parseArguments(argc, argv, options), ParseResult::Continue);
    EXPECT_EQ(options.outputDir, "/tmp/out");
}

TEST_F(CompattorTest, ParseArgumentsOutputMissingValueReturnsExitWithError) {
    char prog[] = "compattor";
    char flag[] = "--output";
    char* argv[] = {prog, flag, nullptr};
    int argc = 2;

    PngCompressionOptions options;
    EXPECT_EQ(parseArguments(argc, argv, options), ParseResult::ExitWithError);
}

TEST_F(CompattorTest, ParseArgumentsStripEnablesStrip) {
    char prog[] = "compattor";
    char flag[] = "--strip";
    char file[] = "test.png";
    char* argv[] = {prog, flag, file, nullptr};
    int argc = 3;

    PngCompressionOptions options;
    EXPECT_EQ(parseArguments(argc, argv, options), ParseResult::Continue);
    EXPECT_TRUE(options.stripMetadata);
}

TEST_F(CompattorTest, ParseArgumentsOverwriteEnablesOverwrite) {
    char prog[] = "compattor";
    char flag[] = "--overwrite";
    char file[] = "test.png";
    char* argv[] = {prog, flag, file, nullptr};
    int argc = 3;

    PngCompressionOptions options;
    EXPECT_EQ(parseArguments(argc, argv, options), ParseResult::Continue);
    EXPECT_TRUE(options.overwrite);
}

TEST_F(CompattorTest, ParseArgumentsUnknownFlagReturnsExitWithError) {
    char prog[] = "compattor";
    char flag[] = "--unknown";
    char* argv[] = {prog, flag, nullptr};
    int argc = 2;

    PngCompressionOptions options;
    EXPECT_EQ(parseArguments(argc, argv, options), ParseResult::ExitWithError);
}

TEST_F(CompattorTest, ParseArgumentsCollectsMultipleFiles) {
    char prog[] = "compattor";
    char firstFile[] = "a.png";
    char secondFile[] = "b.png";
    char thirdFile[] = "c.png";
    char* argv[] = {prog, firstFile, secondFile, thirdFile, nullptr};
    int argc = 4;

    PngCompressionOptions options;
    EXPECT_EQ(parseArguments(argc, argv, options), ParseResult::Continue);
    EXPECT_EQ(options.inputFiles.size(), 3u);
}

TEST_F(CompattorTest, ParseArgumentsDefaultValues) {
    char prog[] = "compattor";
    char file[] = "test.png";
    char* argv[] = {prog, file, nullptr};
    int argc = 2;

    PngCompressionOptions options;
    EXPECT_EQ(parseArguments(argc, argv, options), ParseResult::Continue);
    EXPECT_EQ(options.compressionLevel, 9);
    EXPECT_EQ(options.quantizationColors, 256);
    EXPECT_TRUE(options.stripMetadata);
    EXPECT_FALSE(options.overwrite);
    EXPECT_TRUE(options.outputDir.empty());
}

TEST_F(CompattorTest, WritePngFileDifferentCompressionLevels) {
    std::vector<unsigned char> row(128 * 3);
    for (size_t i = 0; i < 128 * 3; i++) {
        row[i] = static_cast<unsigned char>((i * 13) % 256);
    }
    auto filePath = createRgbPngWithRow("levels_test.png", 128, 128, row);

    PngData data;
    ASSERT_TRUE(readPngFile(filePath.string(), data));

    std::vector<size_t> sizes;
    for (int level = 1; level <= 9; level++) {
        fs::path output = outputDir / ("level_" + std::to_string(level) + ".png");
        ASSERT_TRUE(writePngFile(output.string(), data, level, false));
        sizes.push_back(fs::file_size(output));
    }

    cleanupPngData(data);

    EXPECT_LE(sizes[8], sizes[0]);
}

TEST_F(CompattorTest, ReadWriteRoundtripPreservesDimensions) {
    auto original = createTestPng("roundtrip.png", 8, 8, PNG_COLOR_TYPE_RGBA);
    PngData data;
    ASSERT_TRUE(readPngFile(original.string(), data));

    EXPECT_EQ(data.width, 8u);
    EXPECT_EQ(data.height, 8u);
    EXPECT_EQ(data.colorType, PNG_COLOR_TYPE_RGBA);

    fs::path output = outputDir / "roundtrip_out.png";
    ASSERT_TRUE(writePngFile(output.string(), data, 6, false));
    cleanupPngData(data);

    PngData rereadData;
    ASSERT_TRUE(readPngFile(output.string(), rereadData));
    EXPECT_EQ(rereadData.width, 8u);
    EXPECT_EQ(rereadData.height, 8u);
    cleanupPngData(rereadData);
}

TEST_F(CompattorTest, CompareImagesReturnsLosslessForIdenticalImages) {
    PngData original;
    original.width = 4;
    original.height = 4;
    original.colorType = PNG_COLOR_TYPE_RGB;
    original.bitDepth = 8;
    original.imageData.resize(4 * 4 * 3);
    for (size_t i = 0; i < original.imageData.size(); i++) {
        original.imageData[i] = static_cast<unsigned char>(i % 256);
    }
    original.rowPointers.resize(4);
    for (png_uint_32 y = 0; y < 4; y++) {
        original.rowPointers[y] = original.imageData.data() + y * 12;
    }

    PngData compressed = original;

    ImageQualityResult result = compareImages(original, compressed);
    EXPECT_TRUE(result.isLossless);
    EXPECT_EQ(result.mse, 0.0);
    EXPECT_EQ(result.psnr, std::numeric_limits<double>::infinity());
}

TEST_F(CompattorTest, CompareImagesCalculatesMseForDifferentImages) {
    PngData original;
    original.width = 2;
    original.height = 2;
    original.colorType = PNG_COLOR_TYPE_RGB;
    original.bitDepth = 8;
    original.imageData = {100, 100, 100, 100, 100, 100,
                          100, 100, 100, 100, 100, 100};
    original.rowPointers.resize(2);
    original.rowPointers[0] = original.imageData.data();
    original.rowPointers[1] = original.imageData.data() + 6;

    PngData compressed;
    compressed.width = 2;
    compressed.height = 2;
    compressed.colorType = PNG_COLOR_TYPE_RGB;
    compressed.bitDepth = 8;
    compressed.imageData = {110, 110, 110, 110, 110, 110,
                            110, 110, 110, 110, 110, 110};
    compressed.rowPointers.resize(2);
    compressed.rowPointers[0] = compressed.imageData.data();
    compressed.rowPointers[1] = compressed.imageData.data() + 6;

    ImageQualityResult result = compareImages(original, compressed);
    EXPECT_FALSE(result.isLossless);
    EXPECT_GT(result.mse, 0.0);
    EXPECT_LT(result.psnr, std::numeric_limits<double>::infinity());
}

TEST_F(CompattorTest, CompareImagesReturnsDifferentPsnrForDifferentDiffs) {
    PngData original;
    original.width = 2;
    original.height = 2;
    original.colorType = PNG_COLOR_TYPE_RGB;
    original.bitDepth = 8;
    original.imageData = {100, 100, 100, 100, 100, 100,
                          100, 100, 100, 100, 100, 100};
    original.rowPointers.resize(2);
    original.rowPointers[0] = original.imageData.data();
    original.rowPointers[1] = original.imageData.data() + 6;

    PngData compressedSmallDifference;
    compressedSmallDifference.width = 2;
    compressedSmallDifference.height = 2;
    compressedSmallDifference.colorType = PNG_COLOR_TYPE_RGB;
    compressedSmallDifference.bitDepth = 8;
    compressedSmallDifference.imageData = {101, 101, 101, 101, 101, 101,
                                           101, 101, 101, 101, 101, 101};
    compressedSmallDifference.rowPointers.resize(2);
    compressedSmallDifference.rowPointers[0] = compressedSmallDifference.imageData.data();
    compressedSmallDifference.rowPointers[1] = compressedSmallDifference.imageData.data() + 6;

    PngData compressedLargeDifference;
    compressedLargeDifference.width = 2;
    compressedLargeDifference.height = 2;
    compressedLargeDifference.colorType = PNG_COLOR_TYPE_RGB;
    compressedLargeDifference.bitDepth = 8;
    compressedLargeDifference.imageData = {150, 150, 150, 150, 150, 150,
                                           150, 150, 150, 150, 150, 150};
    compressedLargeDifference.rowPointers.resize(2);
    compressedLargeDifference.rowPointers[0] = compressedLargeDifference.imageData.data();
    compressedLargeDifference.rowPointers[1] = compressedLargeDifference.imageData.data() + 6;

    ImageQualityResult smallDifference = compareImages(original, compressedSmallDifference);
    ImageQualityResult largeDifference = compareImages(original, compressedLargeDifference);

    EXPECT_GT(smallDifference.psnr, largeDifference.psnr);
}

TEST_F(CompattorTest, CompareImagesReturnsDefaultForEmptyData) {
    PngData original;
    original.width = 4;
    original.height = 4;

    PngData compressed;
    compressed.width = 4;
    compressed.height = 4;

    ImageQualityResult result = compareImages(original, compressed);
    EXPECT_FALSE(result.isLossless);
    EXPECT_EQ(result.psnr, 0.0);
}

TEST_F(CompattorTest, CompareImagesReturnsDefaultForDimensionMismatch) {
    PngData original;
    original.width = 4;
    original.height = 4;
    original.imageData.resize(48);

    PngData compressed;
    compressed.width = 2;
    compressed.height = 2;
    compressed.imageData.resize(12);

    ImageQualityResult result = compareImages(original, compressed);
    EXPECT_FALSE(result.isLossless);
    EXPECT_EQ(result.psnr, 0.0);
}

TEST_F(CompattorTest, QuantizeMedianCutConvertsRgbToPalette) {
    PngData data;
    data.width = 4;
    data.height = 4;
    data.colorType = PNG_COLOR_TYPE_RGB;
    data.bitDepth = 8;
    data.imageData.resize(4 * 4 * 3);

    for (size_t i = 0; i < data.imageData.size(); i += 3) {
        data.imageData[i] = 255;
        data.imageData[i + 1] = 0;
        data.imageData[i + 2] = 0;
    }

    data.rowPointers.resize(4);
    for (png_uint_32 y = 0; y < 4; y++) {
        data.rowPointers[y] = data.imageData.data() + y * 12;
    }

    auto result = quantizeMedianCut(data, 256);
    EXPECT_EQ(result.data.colorType, PNG_COLOR_TYPE_PALETTE);
    EXPECT_EQ(result.colorCount, 1);
}

TEST_F(CompattorTest, QuantizeMedianCutCreatesPaletteWithFewerColors) {
    PngData data;
    data.width = 16;
    data.height = 16;
    data.colorType = PNG_COLOR_TYPE_RGB;
    data.bitDepth = 8;
    data.imageData.resize(16 * 16 * 3);

    for (png_uint_32 y = 0; y < 16; y++) {
        for (png_uint_32 x = 0; x < 16; x++) {
            size_t index = (y * 16 + x) * 3;
            data.imageData[index] = static_cast<unsigned char>(y * 16);
            data.imageData[index + 1] = static_cast<unsigned char>(x * 16);
            data.imageData[index + 2] = 128;
        }
    }

    data.rowPointers.resize(16);
    for (png_uint_32 y = 0; y < 16; y++) {
        data.rowPointers[y] = data.imageData.data() + y * 48;
    }

    auto result = quantizeMedianCut(data, 16);
    EXPECT_EQ(result.data.colorType, PNG_COLOR_TYPE_PALETTE);
    EXPECT_LE(result.colorCount, 16);
    EXPECT_GT(result.colorCount, 0);
}

TEST_F(CompattorTest, QuantizeMedianCutReturnsPaletteForPaletteImage) {
    PngData data;
    data.width = 4;
    data.height = 4;
    data.colorType = PNG_COLOR_TYPE_PALETTE;
    data.bitDepth = 8;
    data.imageData.resize(16);

    data.rowPointers.resize(4);
    for (png_uint_32 y = 0; y < 4; y++) {
        data.rowPointers[y] = data.imageData.data() + y * 4;
    }

    auto result = quantizeMedianCut(data, 256);
    EXPECT_EQ(result.data.colorType, PNG_COLOR_TYPE_PALETTE);
    EXPECT_EQ(result.colorCount, 0);
}

TEST_F(CompattorTest, QuantizeMedianCutRejectsGrayscaleInput) {
    PngData data;
    data.width = 4;
    data.height = 4;
    data.colorType = PNG_COLOR_TYPE_GRAY;
    data.bitDepth = 8;
    data.imageData.resize(16);

    data.rowPointers.resize(4);
    for (png_uint_32 y = 0; y < 4; y++) {
        data.rowPointers[y] = data.imageData.data() + y * 4;
    }

    auto result = quantizeMedianCut(data, 256);
    EXPECT_EQ(result.colorCount, 0);
}

TEST_F(CompattorTest, WritePaletteFileCreatesReadablePalettePng) {
    PngData data;
    data.width = 8;
    data.height = 8;
    data.colorType = PNG_COLOR_TYPE_RGB;
    data.bitDepth = 8;
    data.imageData.resize(8 * 8 * 3);

    for (size_t i = 0; i < data.imageData.size(); i += 3) {
        data.imageData[i] = 255;
        data.imageData[i + 1] = 128;
        data.imageData[i + 2] = 0;
    }

    data.rowPointers.resize(8);
    for (png_uint_32 y = 0; y < 8; y++) {
        data.rowPointers[y] = data.imageData.data() + y * 24;
    }

    auto quantized = quantizeMedianCut(data, 16);
    ASSERT_GT(quantized.colorCount, 0);

    fs::path output = outputDir / "palette_output.png";
    auto size = writePaletteFile(output.string(), quantized.data, quantized.palette,
                                 quantized.colorCount, 9);
    ASSERT_TRUE(size.has_value());
    EXPECT_GT(*size, 0u);
    EXPECT_TRUE(fs::exists(output));

    PngData rereadData;
    ASSERT_TRUE(readPngFile(output.string(), rereadData));
    EXPECT_EQ(rereadData.width, 8u);
    EXPECT_EQ(rereadData.height, 8u);
    EXPECT_EQ(rereadData.colorType, PNG_COLOR_TYPE_RGB);
    EXPECT_EQ(rereadData.imageData[0], 255);
    cleanupPngData(rereadData);
}

TEST_F(CompattorTest, QuantizeMedianCutPreservesAlphaInPalette) {
    PngData data;
    data.width = 4;
    data.height = 4;
    data.colorType = PNG_COLOR_TYPE_RGBA;
    data.bitDepth = 8;
    data.imageData.resize(4 * 4 * 4);

    for (png_uint_32 y = 0; y < 4; y++) {
        for (png_uint_32 x = 0; x < 4; x++) {
            size_t index = (y * 4 + x) * 4;
            bool opaqueHalf = (x < 2);
            data.imageData[index] = opaqueHalf ? 255 : 0;
            data.imageData[index + 1] = 0;
            data.imageData[index + 2] = opaqueHalf ? 0 : 255;
            data.imageData[index + 3] = opaqueHalf ? 255 : 128;
        }
    }

    data.rowPointers.resize(4);
    for (png_uint_32 y = 0; y < 4; y++) {
        data.rowPointers[y] = data.imageData.data() + y * 16;
    }

    auto result = quantizeMedianCut(data, 256);
    ASSERT_EQ(result.colorCount, 2);

    bool hasOpaqueRed = false;
    bool hasSemiTransparentBlue = false;
    for (const auto& entry : result.palette) {
        if (entry.r == 255 && entry.a == 255) hasOpaqueRed = true;
        if (entry.b == 255 && entry.a == 128) hasSemiTransparentBlue = true;
    }
    EXPECT_TRUE(hasOpaqueRed);
    EXPECT_TRUE(hasSemiTransparentBlue);
}

TEST_F(CompattorTest, WritePaletteFileRoundTripsTransparency) {
    PngData data;
    data.width = 4;
    data.height = 4;
    data.colorType = PNG_COLOR_TYPE_RGBA;
    data.bitDepth = 8;
    data.imageData.resize(4 * 4 * 4);

    for (png_uint_32 y = 0; y < 4; y++) {
        for (png_uint_32 x = 0; x < 4; x++) {
            size_t index = (y * 4 + x) * 4;
            bool opaqueHalf = (x < 2);
            data.imageData[index] = opaqueHalf ? 255 : 0;
            data.imageData[index + 1] = 0;
            data.imageData[index + 2] = opaqueHalf ? 0 : 255;
            data.imageData[index + 3] = opaqueHalf ? 255 : 128;
        }
    }

    data.rowPointers.resize(4);
    for (png_uint_32 y = 0; y < 4; y++) {
        data.rowPointers[y] = data.imageData.data() + y * 16;
    }

    auto quantized = quantizeMedianCut(data, 256);
    ASSERT_EQ(quantized.colorCount, 2);

    fs::path output = outputDir / "transparency_roundtrip.png";
    auto size = writePaletteFile(output.string(), quantized.data, quantized.palette,
                                 quantized.colorCount, 9);
    ASSERT_TRUE(size.has_value());
    EXPECT_GT(*size, 0u);

    PngData rereadData;
    ASSERT_TRUE(readPngFile(output.string(), rereadData));
    EXPECT_EQ(rereadData.colorType, PNG_COLOR_TYPE_RGBA);
    EXPECT_EQ(rereadData.imageData.size(), 4u * 4u * 4u);
    EXPECT_EQ(rereadData.imageData[3], 255);
    EXPECT_EQ(rereadData.imageData[11], 128);
    cleanupPngData(rereadData);
}

TEST_F(CompattorTest, WritePaletteFileFailsForInvalidPath) {
    PngData data;
    data.width = 2;
    data.height = 2;
    data.colorType = PNG_COLOR_TYPE_PALETTE;
    data.bitDepth = 8;
    data.imageData.resize(4);
    data.rowPointers.resize(2);
    data.rowPointers[0] = data.imageData.data();
    data.rowPointers[1] = data.imageData.data() + 2;

    std::vector<PaletteColor> palette = {{255, 0, 0, 255}, {0, 255, 0, 255}};

    auto size = writePaletteFile("/nonexistent/dir/palette.png", data, palette, 2, 9);
    EXPECT_FALSE(size.has_value());
}

TEST_F(CompattorTest, ParseArgumentsHugeNumberReturnsExitWithError) {
    char prog[] = "compattor";
    char flag[] = "--level";
    char value[] = "99999999999999999999";
    char* argv[] = {prog, flag, value, nullptr};
    int argc = 3;

    PngCompressionOptions options;
    EXPECT_EQ(parseArguments(argc, argv, options), ParseResult::ExitWithError);
}
