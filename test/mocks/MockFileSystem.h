#pragma once

#ifdef UNIT_TEST

#include <map>
#include <string>
#include <vector>

/**
 * MockFileSystem - In-memory file system for unit testing
 *
 * Simulates LittleFS behavior without actual flash operations.
 * Files are stored in static maps that persist across object instances.
 */
class MockFileSystemClass {
private:
    static std::map<std::string, std::vector<char>> files;
    static bool formatted;
    static bool mounted;

public:
    bool begin(bool formatOnFail = false) {
        if (!mounted) {
            mounted = true;
            // Simulate format on first mount if requested
            if (formatOnFail && !formatted) {
                format();
            }
        }
        return mounted;
    }

    bool format() {
        files.clear();
        formatted = true;
        mounted = false;
        return true;
    }

    bool exists(const char* path) {
        return files.find(path) != files.end();
    }

    bool remove(const char* path) {
        auto it = files.find(path);
        if (it != files.end()) {
            files.erase(it);
            return true;
        }
        return false;
    }

    size_t totalBytes() {
        return 512 * 1024; // 512KB simulated
    }

    size_t usedBytes() {
        size_t total = 0;
        for (const auto& pair : files) {
            total += pair.second.size();
        }
        return total;
    }

    // Mock File class
    class File {
    private:
        std::string path;
        std::vector<char>* data;
        size_t readPos;
        bool writeMode;
        bool valid;

    public:
        File() : data(nullptr), readPos(0), writeMode(false), valid(false) {}

        File(const std::string& p, const char* mode)
            : path(p), readPos(0), valid(true) {

            writeMode = (mode[0] == 'w');

            if (writeMode) {
                // Create or clear file
                files[path] = std::vector<char>();
                data = &files[path];
            } else {
                // Read mode
                auto it = files.find(path);
                if (it != files.end()) {
                    data = &it->second;
                } else {
                    valid = false;
                    data = nullptr;
                }
            }
        }

        operator bool() const {
            return valid && data != nullptr;
        }

        size_t write(uint8_t c) {
            if (!valid || !writeMode || !data) return 0;
            data->push_back(static_cast<char>(c));
            return 1;
        }

        size_t write(const uint8_t* buf, size_t size) {
            if (!valid || !writeMode || !data) return 0;
            data->insert(data->end(), reinterpret_cast<const char*>(buf),
                        reinterpret_cast<const char*>(buf) + size);
            return size;
        }

        size_t write(const char* buf, size_t size) {
            if (!valid || !writeMode || !data) return 0;

            data->insert(data->end(), buf, buf + size);
            return size;
        }

        size_t print(const char* str) {
            return write(str, strlen(str));
        }

        size_t print(const String& str) {
            return write(str.c_str(), str.length());
        }

        size_t print(const std::string& str) {
            return write(str.c_str(), str.length());
        }

        int read() {
            if (!valid || !data || readPos >= data->size()) return -1;
            return (*data)[readPos++];
        }

        size_t readBytes(char* buffer, size_t length) {
            if (!valid || !data) return 0;

            size_t available = data->size() - readPos;
            size_t toRead = (length < available) ? length : available;

            std::copy(data->begin() + readPos, data->begin() + readPos + toRead, buffer);
            readPos += toRead;

            return toRead;
        }

        size_t available() {
            if (!valid || !data) return 0;
            return data->size() - readPos;
        }

        void close() {
            valid = false;
            data = nullptr;
        }
    };

    File open(const char* path, const char* mode) {
        return File(path, mode);
    }
};

// Static member initialization
std::map<std::string, std::vector<char>> MockFileSystemClass::files;
bool MockFileSystemClass::formatted = false;
bool MockFileSystemClass::mounted = false;

// Create a global instance to mimic LittleFS singleton
static MockFileSystemClass LittleFS;

// Typedef File class for convenience
using File = MockFileSystemClass::File;

#endif // UNIT_TEST
