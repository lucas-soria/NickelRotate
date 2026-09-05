#ifndef NICKELROTATE_POLICY_H
#define NICKELROTATE_POLICY_H

#include <string>

namespace nr {

enum Orientation { Portrait = 1, Landscape = 2, InvertedPortrait = 4, InvertedLandscape = 8 };
struct Point { int x; int y; };

enum TouchMapping { UnsupportedDevice, NativeTouch, ReversedLandscapeTouch };
TouchMapping touchMappingForVersionRecord(const std::string &record);
bool supportedVersionRecord(const std::string &record);
bool readingController(const char *name);
bool validOrientation(int orientation);
Point rotatePoint(Point point, int orientation, int portraitWidth, int portraitHeight);

// Used only while calling Nickel's permission or reading-menu functions.
// Nesting must restore the previous policy, including when a call throws.
struct Permission {
    bool inside;
    bool landscape;
};
class PermissionScope {
public:
    PermissionScope(Permission &state, bool landscape) : state_(state), saved_(state) {
        state_ = {true, landscape};
    }
    ~PermissionScope() { state_ = saved_; }
    PermissionScope(const PermissionScope &) = delete;
    PermissionScope &operator=(const PermissionScope &) = delete;
private:
    Permission &state_;
    Permission saved_;
};

}
#endif
