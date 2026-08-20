#include "compattor.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

int main(int argc, char* argv[]) {
    PngCompressionOptions options;

    switch (parseArguments(argc, argv, options)) {
        case ParseResult::ExitWithSuccess:
            return 0;
        case ParseResult::ExitWithError:
            return 1;
        case ParseResult::Continue:
            break;
    }

    if (options.inputFiles.empty()) {
        std::cerr << "Error: no input files or directories specified\n";
        std::cerr << "Use --help to see available options\n";
        return 1;
    }

    namespace fs = std::filesystem;

    std::vector<std::string> allFiles;

    for (const auto& input : options.inputFiles) {
        std::error_code errorCode;
        if (fs::is_directory(input, errorCode)) {
            auto pngFiles = findPngFiles(input);
            allFiles.insert(allFiles.end(), pngFiles.begin(), pngFiles.end());
        } else {
            allFiles.push_back(input);
        }
    }

    if (allFiles.empty()) {
        std::cout << "No PNG files found\n";
        return 0;
    }

    std::cout << "Compressing " << allFiles.size() << " file(s) with level "
              << options.compressionLevel << "...\n\n";

    int successCount = 0;
    int failCount = 0;

    for (const auto& file : allFiles) {
        if (processFile(file, options)) {
            successCount++;
        } else {
            failCount++;
        }
    }

    std::cout << "\nDone: " << successCount << " succeeded, "
              << failCount << " failed\n";

    return failCount > 0 ? 1 : 0;
}
