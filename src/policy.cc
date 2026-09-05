#include "policy.h"
#include <cstring>
#include <climits>
#include <vector>

namespace nr {

static bool versionNumber(const std::string &text, std::size_t start, std::size_t end, unsigned &value) {
    if (start == end) return false;
    value = 0;
    for (std::size_t i = start; i < end; ++i) {
        if (text[i] < '0' || text[i] > '9') return false;
        const unsigned digit = static_cast<unsigned>(text[i] - '0');
        if (value > (UINT_MAX - digit) / 10) return false;
        value = value * 10 + digit;
    }
    return true;
}

bool supportedVersionRecord(const std::string &record) {
    if (record.empty() || record.size() > 1024 || record.find('\0') != std::string::npos)
        return false;
    std::string line = record;
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
    if (line.find_first_of("\r\n") != std::string::npos) return false;
    std::vector<std::string> fields;
    std::size_t start = 0, end;
    while ((end = line.find(',', start)) != std::string::npos) {
        fields.push_back(line.substr(start, end - start));
        start = end + 1;
    }
    fields.push_back(line.substr(start));
    // This is the format read by KoboPatch Web UI's kobo/version.js.
    if (fields.size() != 6) return false;
    const std::string &version = fields[2];
    if (version.compare(0, 2, "4.") != 0) return false;
    const std::size_t dot = version.find('.', 2);
    if (dot == std::string::npos) return false;
    unsigned minor, patch;
    if (!versionNumber(version, 2, dot, minor) || minor < 45 ||
        !versionNumber(version, dot + 1, version.size(), patch)) return false;
    return fields[5] == "00000000-0000-0000-0000-000000000391" ||
           fields[5] == "00000000-0000-0000-0000-000000000395" ||
           fields[5] == "00000000-0000-0000-0000-000000000393";
}

bool readingController(const char *name) {
    if (!name) return false;
    const char *allowed[] = {
        "ReadingController", "TableOfContentsController",
        "AdvancedFontSettingsController", "AnnotationInputController",
        "BookmarkController"
    };
    for (const char *candidate : allowed)
        if (std::strcmp(name, candidate) == 0) return true;
    return false;
}

bool validOrientation(int orientation) {
    return orientation == Portrait || orientation == Landscape ||
           orientation == InvertedPortrait || orientation == InvertedLandscape;
}

static int clamp(int value, int size) {
    return value < 0 ? 0 : value >= size ? size - 1 : value;
}

Point rotatePoint(Point p, int orientation, int width, int height) {
    if (width <= 0 || height <= 0 || !validOrientation(orientation)) return p;
    // Kobo has already converted the panel's axes to portrait coordinates.
    // The Clara BW device test puts landscape taps opposite their visible
    // controls. Use the opposite quarter-turn from Qt in both landscape
    // modes. The user confirmed this correction on firmware 4.45.23697.
    p.x = clamp(p.x, width);
    p.y = clamp(p.y, height);
    switch (orientation) {
    case Landscape:         return {height - 1 - p.y, p.x};
    case InvertedPortrait:  return {width - 1 - p.x, height - 1 - p.y};
    case InvertedLandscape: return {p.y, width - 1 - p.x};
    default:                return p;
    }
}

}
