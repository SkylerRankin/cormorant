#include <format>
#include <fstream>
#include "application.h"
#include "export.h"

namespace fs = std::filesystem;

namespace Export {
    namespace {
        const std::string baseFilename = "saved_images";
        const std::string textExtension = ".txt";
        const std::string savedDirectoryPostfix = " (saved)";

        std::atomic_bool inProgress(false);
        float progress = 0.0f;
    }

    // Forward declarations
    void threadExportFilenames(fs::path directoryPath, const std::map<int, Image>& images);
    void threadExportImages(fs::path directoryPath, const std::map<int, Image>& images, bool copyAllMatchingFiles);

    bool exportInProgress() { return inProgress.load(); }
    float exportProgress() { return progress; }

    fs::path filenameOutputPath(fs::path directoryPath) {
        // TODO: not ideal to do this file system interaction every frame while the save filenames UI is showing.
        fs::path outputPath;

        // Files with same name may already exist, so find the largest number currently used
        if (fs::exists(fs::path(directoryPath).append(baseFilename + textExtension))) {
            int largest = 0;
            std::string prefix = std::format("{} (", baseFilename);
            std::string postfix = std::format("){}", textExtension);

            for (const auto& item : fs::directory_iterator{ directoryPath }) {
                if (!item.is_regular_file()) continue;
                std::string filename = item.path().filename().string();
                if (filename.starts_with(prefix) && filename.ends_with(postfix)) {
                    try {
                        int value = std::stoi(filename.substr(prefix.length(), filename.length() - prefix.length() - postfix.length()));
                        if (value > largest) largest = value;
                    } catch (...) {}
                }
            }
            outputPath = fs::path(directoryPath).append(baseFilename + std::format(" ({})", largest + 1) + textExtension);
        } else {
            outputPath = fs::path(directoryPath).append(baseFilename + textExtension);
        }

        return outputPath;
    }

    fs::path imageOutputPath(fs::path directoryPath) {
        return fs::path(directoryPath).parent_path().append(directoryPath.filename().string() + savedDirectoryPostfix);
    }

    void exportFilenames(fs::path directory, const std::map<int, Image>& images) {
        inProgress.store(true);
        progress = 0.0f;
        std::thread thread(threadExportFilenames, directory, images);
        thread.detach();
    }

    void exportImages(fs::path directory, const std::map<int, Image>& images, bool copyAllMatchingFiles) {
        inProgress.store(true);
        progress = 0.0f;
        std::thread thread(threadExportImages, directory, images, copyAllMatchingFiles);
        thread.detach();
    }

    void threadExportFilenames(fs::path directoryPath, const std::map<int, Image>& images) {
        fs::path outputPath = filenameOutputPath(directoryPath);
        std::ofstream outputFile;
        outputFile.open(outputPath);

        outputFile << "-- Cormorant --" << std::endl;
        outputFile << "Saved images for directory \"" << directoryPath.string() << "\"" << std::endl;

        auto zonedTime = std::chrono::zoned_time{
            std::chrono::current_zone(),
            std::chrono::system_clock::now()
        };
        outputFile << std::format("This file was generated on {:%Y-%m-%d %H:%M}", zonedTime.get_local_time()) << "." << std::endl;
        outputFile << std::endl;

        float increment = 1.0f / images.size();
        for (auto& e : images) {
            if (e.second.saved) {
                outputFile << e.second.filename << std::endl;
            }
            progress += increment;
        }

        outputFile.close();
        inProgress.store(false);
        progress = 1.0f;
    }

    void threadExportImages(fs::path directoryPath, const std::map<int, Image>& images, bool copyAllMatchingFiles) {
        fs::path outputPath = imageOutputPath(directoryPath);

        if (!fs::exists(outputPath)) {
            fs::create_directory(outputPath);
        }

        // Generate map from file stem to full filename so that directory search can easily check if
        // a file has a matching filename in the image set, but is just not the same file.
        std::map<std::string, std::string> fileStemToFilename;
        std::map<std::string, std::vector<std::string>> additionalFiles;
        for (const auto& e : images) {
            additionalFiles.emplace(e.second.filename, std::vector<std::string>());
            fileStemToFilename.emplace(fs::path(e.second.path).stem().string(), e.second.filename);
        }

        if (copyAllMatchingFiles) {
            for (const auto& entry : fs::directory_iterator{ directoryPath }) {
                std::string entryPathString{ reinterpret_cast<const char*>(entry.path().u8string().c_str()) };
                fs::path entryPath{ entryPathString };

                if (fs::is_regular_file(entryPath) &&
                    fileStemToFilename.contains(entryPath.stem().string()) &&
                    !additionalFiles.contains(entryPathString)) {
                    additionalFiles.at(fileStemToFilename.at(entryPath.stem().string())).push_back(entryPathString);
                }
            }
        }

        int totalSaved = 0;
        for (auto& e : images) {
            if (e.second.saved) totalSaved++;
        }

        float increment = 1.0f / totalSaved;
        for (const auto& e : images) {
            if (e.second.saved) {
                fs::copy(e.second.path, fs::path(outputPath).append(e.second.filename), fs::copy_options::skip_existing);

                for (const auto& p : additionalFiles.at(e.second.filename)) {
                    fs::path path = fs::path(p);
                    fs::copy(path, fs::path(outputPath).append(path.filename().string()), fs::copy_options::skip_existing);
                }
            }
            progress += increment;
        }

        inProgress.store(false);
        progress = 1.0f;
    }
}