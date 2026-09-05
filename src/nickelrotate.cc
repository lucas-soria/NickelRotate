#include "NickelHook.h"
#include "policy.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QThread>
#include <QTransform>
#include <QMetaObject>
#include <QFile>
#include <QVariant>
#include <QWidget>
#include <QLayout>
#include <cxxabi.h>
#include <atomic>
#include <cstring>
#include <typeinfo>
#include <unistd.h>

#define NR_DIR "/mnt/onboard/.adds/nickel-rotate"
#define NR_EXPORT extern "C" __attribute__((visibility("default")))

static std::atomic<bool> enabled(false);
static std::atomic<int> orientation(nr::Portrait);
static int portraitWidth = 0, portraitHeight = 0;
static thread_local nr::Permission permission = {false, false};
static thread_local bool mappingTouch = false;

static int (*realAllowable)(const void *, void *) = nullptr;
static void (*realReadingMenu)(void *) = nullptr;
static QObject *(*realRotatePopup)(QObject *, QObject *) = nullptr;
static void (*realSelectedItem)(QWidget *, int) = nullptr;
static bool (*realHasSensor)(const void *) = nullptr;
static bool (*realForceLandscape)(void *) = nullptr;
static QPoint (*realMapFromDevice)(const void *, const QPoint &) = nullptr;
static QPoint (*realTransformMap)(const QTransform *, const QPoint &) = nullptr;
static const __cxxabiv1::__class_type_info *abstractType = nullptr;

static QObject *controllerObject(void *controller) {
    if (!controller || !abstractType) return nullptr;
    // Nickel itself cross-casts AbstractController this way. It is not a
    // QObject at offset zero, so a reinterpret_cast<QObject *> is invalid.
    const auto *qtType = static_cast<const __cxxabiv1::__class_type_info *>(&typeid(QObject));
    return static_cast<QObject *>(__cxxabiv1::__dynamic_cast(controller, abstractType, qtType, -1));
}

NR_EXPORT int nr_allowable(const void *self, void *controller) {
    if (!enabled.load(std::memory_order_acquire)) return realAllowable(self, controller);
    QObject *object = controllerObject(controller);
    const char *name = object ? object->metaObject()->className() : "unknown";
    nr::PermissionScope scope(permission, nr::readingController(name));
    return realAllowable(self, controller);
}

NR_EXPORT void nr_reading_menu(void *self) {
    if (!enabled.load(std::memory_order_acquire)) { realReadingMenu(self); return; }
    nr::PermissionScope scope(permission, true);
    realReadingMenu(self);
}

// The ARM constructor returns this. Preserve its return value and pass the
// parent through unchanged; QObject is the popup's first base via QWidget.
NR_EXPORT QObject *nr_rotate_popup(QObject *popup, QObject *parent) {
    QObject *result = realRotatePopup(popup, parent);
    if (!enabled.load(std::memory_order_acquire)) return result;
    // Auto's row and its separator are siblings. Nickel hides only the row
    // on devices without a sensor. Keep the Portrait/Landscape separator.
    QObject *separator = popup->findChild<QObject *>(QStringLiteral("dividerLine1"));
    if (separator && separator->inherits("QFrame"))
        separator->setProperty("visible", false);
    return result;
}

NR_EXPORT void nr_selected_item(QWidget *popup, int selected) {
    if (!enabled.load(std::memory_order_acquire)) { realSelectedItem(popup, selected); return; }
    // No lock and the combined Portrait/Landscape mask select Auto in Nickel.
    // Auto is hidden here, so show the current orientation without saving a lock.
    if (selected == 0 || selected == (nr::Portrait | nr::Landscape)) {
        const int current = orientation.load(std::memory_order_acquire);
        selected = current == nr::Landscape || current == nr::InvertedLandscape
            ? nr::Landscape : nr::Portrait;
    }
    realSelectedItem(popup, selected);
    // Nickel fixes the size using the hidden Auto row before updating icons.
    // Measure the visible layout now that the selected icon has its final size.
    // Keep any wider native size and leave the native height untouched.
    if (QLayout *layout = popup->layout()) {
        layout->invalidate();
        popup->setFixedWidth(qMax(popup->width(), layout->totalSizeHint().width()));
    }
}

NR_EXPORT bool nr_has_sensor(const void *self) {
    if (enabled.load(std::memory_order_acquire) && permission.inside) return permission.landscape;
    return realHasSensor(self);
}

NR_EXPORT bool nr_force_landscape(void *self) {
    // Honour each controller's mask even if ForceAllowLandscape was left in
    // the device config by an earlier experiment. Do not edit that config.
    if (enabled.load(std::memory_order_acquire) && permission.inside) return false;
    return realForceLandscape(self);
}

NR_EXPORT QPoint nr_map_from_device(const void *self, const QPoint &point) {
    if (!enabled.load(std::memory_order_acquire)) return realMapFromDevice(self, point);
    struct TouchScope {
        bool saved;
        TouchScope() : saved(mappingTouch) { mappingTouch = true; }
        ~TouchScope() { mappingTouch = saved; }
    } scope;
    return realMapFromDevice(self, point);
}

NR_EXPORT QPoint nr_transform_map(const QTransform *transform, const QPoint &point) {
    if (!enabled.load(std::memory_order_acquire) || !mappingTouch)
        return realTransformMap(transform, point);
    const int current = orientation.load(std::memory_order_acquire);
    if (!nr::validOrientation(current)) return realTransformMap(transform, point);
    // Replace the final orientation transform, not the panel mapper. Returning
    // to portrait must also discard any cached landscape transform.
    if (current == nr::Portrait) return point;
    const nr::Point mapped = nr::rotatePoint({point.x(), point.y()}, current, portraitWidth, portraitHeight);
    return QPoint(mapped.x, mapped.y);
}

static void screenChanged(QScreen *screen) {
    const int current = screen->orientation();
    if (nr::validOrientation(current)) orientation.store(current, std::memory_order_release);
}

static int init() {
    QFile version("/mnt/onboard/.kobo/version");
    if (!version.open(QIODevice::ReadOnly)) { nh_log("NickelRotate: cannot read device version; inactive"); return 0; }
    const QByteArray record = version.read(1025);
    if (!nr::supportedVersionRecord(std::string(record.constData(), record.size()))) {
        nh_log("NickelRotate: requires Clara BW or Clara Colour and firmware 4.45 or newer within 4.x; inactive");
        return 0;
    }
    if (access(NR_DIR "/disabled", F_OK) == 0) { nh_log("NickelRotate: disabled marker; inactive"); return 0; }
    QCoreApplication *app = QCoreApplication::instance();
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!app || !screen || QThread::currentThread() != app->thread()) {
        nh_log("NickelRotate: no GUI thread/screen; rollback");
        return 1;
    }
    QSize size = screen->size();
    if (size.width() > size.height()) size.transpose();
    if (size != QSize(1072, 1448) || screen->nativeOrientation() != Qt::PortraitOrientation) {
        nh_log("NickelRotate: unexpected panel geometry/orientation; inactive");
        return 0;
    }
    portraitWidth = size.width();
    portraitHeight = size.height();
    const QByteArray firmware = record.split(',').value(2);
    nh_log("NickelRotate " NH_VERSION ": firmware %s; reader rotation enabled", firmware.constData());
    // Signal callbacks run on the GUI thread. The touch thread only reads the
    // atomic orientation and immutable panel dimensions, never a QScreen.
    QObject::connect(screen, &QScreen::orientationChanged, app, [screen](Qt::ScreenOrientation) { screenChanged(screen); });
    QObject::connect(screen, &QScreen::geometryChanged, app, [screen](const QRect &) { screenChanged(screen); });
    screenChanged(screen);
    enabled.store(true, std::memory_order_release);
    return 0;
}

static bool uninstall() {
    // Preserve user-created files. NickelHook removes the plugin itself.
    const char *files[] = {"doc", "uninstall", "disabled", "diagnostics", "observe-touch",
                           "nickel-rotate.log", "nickel-rotate.log.old"};
    bool ok = true;
    for (const char *name : files) {
        const std::string path = std::string(NR_DIR) + "/" + name;
        if (access(path.c_str(), F_OK) == 0) ok = nh_delete_file(path.c_str()) && ok;
    }
    rmdir(NR_DIR);
    return ok;
}

static struct nh_info info = {
    .name = "NickelRotate",
    .desc = "Manual reader rotation for Clara BW and Clara Colour (experimental)",
    .uninstall_flag = NR_DIR "/uninstall-now",
    .uninstall_xflag = NR_DIR "/uninstall",
    .failsafe_delay = 3,
};
static struct nh_hook hooks[] = {
    { .sym = "_ZN11RotatePopup15setSelectedItemE6QFlagsIN2Qt17ScreenOrientationEE", .sym_new = "nr_selected_item", .lib = "libnickel.so.1.0.0", .out = nh_symoutptr(realSelectedItem) },
    { .sym = "_ZN11RotatePopupC1EP7QWidget", .sym_new = "nr_rotate_popup", .lib = "libnickel.so.1.0.0", .out = nh_symoutptr(realRotatePopup) },
    { .sym = "_ZNK20MainWindowController21allowableOrientationsEP18AbstractController", .sym_new = "nr_allowable", .lib = "libnickel.so.1.0.0", .out = nh_symoutptr(realAllowable) },
    { .sym = "_ZN15ReadingMenuView17updateReadingMenuEv", .sym_new = "nr_reading_menu", .lib = "libnickel.so.1.0.0", .out = nh_symoutptr(realReadingMenu) },
    { .sym = "_ZNK6Device20hasOrientationSensorEv", .sym_new = "nr_has_sensor", .lib = "libnickel.so.1.0.0", .out = nh_symoutptr(realHasSensor) },
    { .sym = "_ZN11DevSettings19forceAllowLandscapeEv", .sym_new = "nr_force_landscape", .lib = "libnickel.so.1.0.0", .out = nh_symoutptr(realForceLandscape) },
    { .sym = "_ZNK21QEvdevTouchScreenData13mapFromDeviceERK6QPoint", .sym_new = "nr_map_from_device", .lib = "/usr/local/Kobo/generic/libkevdevtouch.so", .out = nh_symoutptr(realMapFromDevice) },
    { .sym = "_ZNK10QTransform3mapERK6QPoint", .sym_new = "nr_transform_map", .lib = "/usr/local/Kobo/generic/libkevdevtouch.so", .out = nh_symoutptr(realTransformMap) },
    {0},
};
static struct nh_dlsym symbols[] = {
    { .name = "_ZTI18AbstractController", .out = nh_symoutptr(abstractType) },
    {0},
};
NickelHook(
    .init = &init,
    .info = &info,
    .hook = &hooks[0],
    .dlsym = &symbols[0],
    .uninstall = &uninstall,
)
