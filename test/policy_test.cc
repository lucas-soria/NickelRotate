#include "../src/policy.h"
#include <cassert>
#include <climits>
#include <cstdio>
#include <set>
#include <utility>

static std::string record(const char *firmware, const char *id) {
    return std::string("N365-test,4.9.77,") + firmware + ",4.9.77,4.9.77," + id;
}
static void same(nr::Point point, int x, int y) { assert(point.x == x && point.y == y); }

int main() {
    const char *bw = "00000000-0000-0000-0000-000000000391";
    const char *bwRevision = "00000000-0000-0000-0000-000000000395";
    assert(nr::supportedVersionRecord(record("4.46.23836", bw)));
    assert(nr::supportedVersionRecord(record("4.46.23836", bwRevision) + "\r\n"));
    assert(nr::supportedVersionRecord(record("4.45.23697", bw)));
    assert(nr::supportedVersionRecord(record("4.45.23697", bwRevision)));
    assert(!nr::supportedVersionRecord(record("4.44.23602", bw)));
    for (const char *version : {"4.45.0", "4.47.1", "4.100.99999"})
        assert(nr::supportedVersionRecord(record(version, bw)));
    for (const char *version : {"3.99.99999", "5.45.1", "40.45.1", "4.44.99999", "4.45", "4..1", "4.45.", "4.45.1.extra", "4.+45.1", "4.45.-1", "4.45.1x", "4.4294967296.1", "4.45.4294967296"})
        assert(!nr::supportedVersionRecord(record(version, bw)));
    assert(!nr::supportedVersionRecord(record("5.0.0", bw)));
    const char *colour = "00000000-0000-0000-0000-000000000393";
    assert(nr::supportedVersionRecord(record("4.45.23697", colour)));
    assert(nr::supportedVersionRecord(record("4.46.23836", colour)));
    assert(!nr::supportedVersionRecord(record("4.44.23602", colour)));
    assert(!nr::supportedVersionRecord(record("5.0.0", colour)));
    assert(!nr::supportedVersionRecord(record("4.46.23836", "00000000-0000-0000-0000-000000000390")));
    assert(!nr::supportedVersionRecord(record("4.46.23836", "00000000-0000-0000-0000-000000000396")));
    assert(!nr::supportedVersionRecord(record("4.46.23836", bw) + ",extra"));
    assert(!nr::supportedVersionRecord(record("4.46.23836", bw) + '\0'));
    assert(!nr::supportedVersionRecord("\n" + record("4.46.23836", bw)));
    assert(!nr::supportedVersionRecord(std::string(1025, 'x')));
    assert(!nr::supportedVersionRecord("4.46.23836," + std::string(bw)));
    assert(!nr::supportedVersionRecord(""));

    const char *hd = "00000000-0000-0000-0000-000000000376";
    const char *twoE = "00000000-0000-0000-0000-000000000386";
    for (const char *model : {hd, twoE}) {
        for (const char *version : {"4.38.23684", "4.38.23697", "4.45.23697"})
            assert(nr::supportedVersionRecord(record(version, model)));
        for (const char *version : {"4.37.99999", "4.38.23683", "5.0.0", "4.38.23684x"})
            assert(!nr::supportedVersionRecord(record(version, model)));
    }
    assert(nr::touchMappingForVersionRecord(record("4.38.23684", hd)) == nr::ReversedLandscapeTouch);
    assert(nr::touchMappingForVersionRecord(record("4.38.23697", twoE)) == nr::NativeTouch);
    for (const char *model : {bw, bwRevision, colour}) {
        assert(nr::touchMappingForVersionRecord(record("4.45.23697", model)) == nr::ReversedLandscapeTouch);
        assert(nr::touchMappingForVersionRecord(record("4.38.23697", model)) == nr::UnsupportedDevice);
    }

    assert(nr::readingController("ReadingController"));
    assert(nr::readingController("TableOfContentsController"));
    assert(nr::readingController("AdvancedFontSettingsController"));
    assert(!nr::readingController("N3SettingsReadingController"));
    assert(!nr::readingController("N3SettingsDeveloperOptionsController"));
    assert(!nr::readingController("HomeController"));
    assert(!nr::readingController("LibraryController"));
    assert(!nr::readingController(nullptr));
    assert(!nr::readingController("ReadingControllerExtra"));

    nr::Permission permission = {false, false};
    try {
        nr::PermissionScope outer(permission, true);
        assert(permission.inside && permission.landscape);
        { nr::PermissionScope inner(permission, false); assert(permission.inside && !permission.landscape); }
        assert(permission.inside && permission.landscape);
        throw 1;
    } catch (int) {}
    assert(!permission.inside && !permission.landscape);

    const int w = 1072, h = 1448;
    same(nr::rotatePoint({0,0}, nr::Landscape, w,h), h-1,0);
    same(nr::rotatePoint({w-1,0}, nr::Landscape, w,h), h-1,w-1);
    same(nr::rotatePoint({w-1,h-1}, nr::Landscape, w,h), 0,w-1);
    same(nr::rotatePoint({0,h-1}, nr::Landscape, w,h), 0,0);
    same(nr::rotatePoint({0,0}, nr::InvertedLandscape, w,h), 0,w-1);
    same(nr::rotatePoint({w-1,h-1}, nr::InvertedLandscape, w,h), h-1,0);
    same(nr::rotatePoint({INT_MIN,INT_MAX}, nr::Landscape, w,h), 0,0);
    // Captured Back-button test: raw (1222,979), panel-mapped (101,1222).
    // 0.1.2 emitted (1222,970), hitting the chapter control. The corrected
    // mapping puts it at (225,101). The user reported the fix worked.
    same(nr::rotatePoint({101,1222}, nr::Landscape, w,h), 225,101);
    same(nr::rotatePoint({101,1222}, nr::Portrait, w,h), 101,1222);
    same(nr::rotatePoint({7,9}, 3, w,h), 7,9);
    same(nr::rotatePoint({7,9}, nr::Landscape, 0,h), 7,9);
    // Every pixel on a non-square test panel stays in bounds, appears once,
    // and returns to its original position after the inverse rotation.
    for (int o : {1,2,4,8}) {
        std::set<std::pair<int,int>> pixels;
        const int tw = (o == 2 || o == 8) ? 17 : 11;
        const int th = (o == 2 || o == 8) ? 11 : 17;
        for (int y=0; y<17; ++y) for (int x=0; x<11; ++x) {
            const auto p = nr::rotatePoint({x,y}, o, 11,17);
            assert(p.x >= 0 && p.x < tw && p.y >= 0 && p.y < th);
            assert(pixels.insert({p.x,p.y}).second);
            same(nr::rotatePoint(p, o == 2 ? 8 : o == 8 ? 2 : o, tw,th), x,y);
        }
    }
    puts("PASS: device gates, reader scope, nested permissions, touch corners and inverse mappings");
}
