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
#include <QLabel>
#include <QFontMetrics>
#include <cxxabi.h>
#include <dlfcn.h>
#include <atomic>
#include <cstring>
#include <typeinfo>
#include <unistd.h>

#ifndef NR_DIR
#define NR_DIR "/mnt/onboard/.adds/nickel-rotate"
#endif
#ifndef NR_VERSION_FILE
#define NR_VERSION_FILE "/mnt/onboard/.kobo/version"
#endif
#define NR_EXPORT extern "C" __attribute__((visibility("default")))

static std::atomic<bool> enabled(false);
static std::atomic<int> orientation(nr::Portrait);
static int portraitWidth = 0, portraitHeight = 0;
static nr::TouchMapping touchMapping = nr::UnsupportedDevice;
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

// NickelHook has already loaded every hooked library. Tests replace this.
static void *openLoadedLib(const char *name) { return dlopen(name, RTLD_LAZY | RTLD_NOLOAD); }
static void *(*openHookLib)(const char *) = openLoadedLib;

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

static void invalidatePopupLayout(QLayout *layout) {
    // Refresh nested widget and row hints before measuring an unshown popup.
    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem *item = layout->itemAt(i);
        QLayout *child = item->layout();
        if (!child && item->widget()) child = item->widget()->layout();
        if (child) invalidatePopupLayout(child);
        if (item->widget()) item->widget()->updateGeometry();
    }
    layout->invalidate();
}

static void preparePopupWidth(QWidget *popup) {
    QLayout *layout = popup->layout();
    auto *autoRow = popup->findChild<QWidget *>(QStringLiteral("autoRotateContainer"));
    auto *autoIcon = popup->findChild<QWidget *>(QStringLiteral("autoRotateImage"));
    auto *autoLabel = popup->findChild<QLabel *>(QStringLiteral("labelAuto"));
    if (!layout || !autoRow || !autoRow->isHidden() || !autoIcon || !autoLabel) return;

    popup->ensurePolished();
    for (const char *name : {"labelPortrait", "labelLandscape"}) {
        auto *label = popup->findChild<QLabel *>(QString::fromLatin1(name));
        if (!label) continue;
        const QMargins margins = label->contentsMargins();
        label->setContentsMargins(margins.left(), margins.top(), margins.right() + 12, margins.bottom());
    }
    int width = 0;
    // setSelectedItem only changes pixmaps. Measure both states before any
    // paint, then show the current orientation until Nickel loads its lock.
    for (int item : {nr::Portrait, nr::Landscape}) {
        realSelectedItem(popup, item);
        invalidatePopupLayout(layout);
        width = qMax(width, layout->totalSizeHint().width());
    }
    const int current = orientation.load(std::memory_order_acquire);
    realSelectedItem(popup, current == nr::Landscape || current == nr::InvertedLandscape
        ? nr::Landscape : nr::Portrait);
    invalidatePopupLayout(layout);

    // Nickel's sizeHint is Auto's text width + 3 * Auto's icon geometry width.
    // The hidden icon starts at 100 px and later shrinks to 41 px on the BW.
    // Fix that input so both the initial popup size and setVisible's later
    // setFixedSize agree. Round up to fit the visible rows, keeping native height.
    const QFontMetrics metrics(autoLabel->font());
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    const int textWidth = metrics.horizontalAdvance(autoLabel->text());
#else
    const int textWidth = metrics.width(autoLabel->text());
#endif
    autoIcon->setFixedWidth((qMax(0, width - textWidth) + 2) / 3);
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
    preparePopupWidth(static_cast<QWidget *>(popup));
    return result;
}

NR_EXPORT void nr_selected_item(QWidget *popup, int selected) {
    if (!enabled.load(std::memory_order_acquire)) { realSelectedItem(popup, selected); return; }
    // No lock, or any mask allowing both portrait and landscape, selects Auto
    // in Nickel. Auto is hidden here, so show the current orientation without
    // saving a lock. Paired masks such as 5 and 10 pass through unchanged.
    const bool anyPortrait = selected & (nr::Portrait | nr::InvertedPortrait);
    const bool anyLandscape = selected & (nr::Landscape | nr::InvertedLandscape);
    if (selected == 0 || (anyPortrait && anyLandscape)) {
        const int current = orientation.load(std::memory_order_acquire);
        selected = current == nr::Landscape || current == nr::InvertedLandscape
            ? nr::Landscape : nr::Portrait;
    }
    realSelectedItem(popup, selected);
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
    if (!enabled.load(std::memory_order_acquire) || touchMapping != nr::ReversedLandscapeTouch)
        return realMapFromDevice(self, point);
    struct TouchScope {
        bool saved;
        TouchScope() : saved(mappingTouch) { mappingTouch = true; }
        ~TouchScope() { mappingTouch = saved; }
    } scope;
    return realMapFromDevice(self, point);
}

NR_EXPORT QPoint nr_transform_map(const QTransform *transform, const QPoint &point) {
    if (!enabled.load(std::memory_order_acquire) || touchMapping != nr::ReversedLandscapeTouch || !mappingTouch)
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

static void restoreHooks();

// NickelHook applies every hook before init. Undo them when staying inactive
// so no Nickel call passes through this plugin. Returning an error would do
// the same, but NickelHook then dumps syslog to the user's storage each boot.
static int inactive(const char *reason) {
    nh_log("NickelRotate: %s; inactive", reason);
    restoreHooks();
    return 0;
}

static int init() {
    QFile version(NR_VERSION_FILE);
    if (!version.open(QIODevice::ReadOnly)) return inactive("cannot read device version");
    const QByteArray record = version.read(1025);
    touchMapping = nr::touchMappingForVersionRecord(std::string(record.constData(), record.size()));
    if (touchMapping == nr::UnsupportedDevice) return inactive("unsupported Clara model or firmware");
    if (access(NR_DIR "/disabled", F_OK) == 0) return inactive("disabled marker");
    // The touch hooks are optional so Clara 2E, which keeps Nickel's touch
    // transform, does not depend on them. Rotation without the correction is
    // unsafe everywhere else, so let NickelHook roll back every hook.
    if (touchMapping == nr::ReversedLandscapeTouch && (!realMapFromDevice || !realTransformMap)) {
        nh_log("NickelRotate: touch correction hooks unavailable; rollback");
        return 1;
    }
    QCoreApplication *app = QCoreApplication::instance();
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!app || !screen || QThread::currentThread() != app->thread()) {
        nh_log("NickelRotate: no GUI thread/screen; rollback");
        return 1;
    }
    QSize size = screen->size();
    if (size.width() > size.height()) size.transpose();
    if (size != QSize(1072, 1448) || screen->nativeOrientation() != Qt::PortraitOrientation)
        return inactive("unexpected panel geometry/orientation");
    portraitWidth = size.width();
    portraitHeight = size.height();
    const QByteArray firmware = record.split(',').value(2);
    nh_log("NickelRotate " NH_VERSION ": firmware %s; reader rotation enabled", firmware.constData());
    // Signal callbacks run on the GUI thread. The touch thread only reads the
    // atomic orientation and immutable panel dimensions/mapping, never a QScreen.
    QObject::connect(screen, &QScreen::orientationChanged, app, [screen](Qt::ScreenOrientation) { screenChanged(screen); });
    QObject::connect(screen, &QScreen::geometryChanged, app, [screen](const QRect &) { screenChanged(screen); });
    screenChanged(screen);
    enabled.store(true, std::memory_order_release);
    return 0;
}

static bool uninstall() {
    // Preserve user-created files. NickelHook removes the plugin itself.
    const char *files[] = {"doc", "uninstall", "disabled", "diagnostics", "observe-touch",
                           "nickel-rotate.log", "nickel-rotate.log.old", "popup-width.log"};
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
    .desc = "Manual reader rotation for Kobo Clara devices (experimental)",
    .uninstall_flag = NR_DIR "/uninstall-now",
    .uninstall_xflag = NR_DIR "/uninstall",
    .failsafe_delay = 3,
};
static struct nh_hook hooks[] = {
    //nb hook libnickel 4.38.23684 * _ZN11RotatePopup15setSelectedItemE6QFlagsIN2Qt17ScreenOrientationEE
    { .sym = "_ZN11RotatePopup15setSelectedItemE6QFlagsIN2Qt17ScreenOrientationEE", .sym_new = "nr_selected_item", .lib = "libnickel.so.1.0.0", .out = nh_symoutptr(realSelectedItem) },
    //nb hook libnickel 4.38.23684 * _ZN11RotatePopupC1EP7QWidget
    { .sym = "_ZN11RotatePopupC1EP7QWidget", .sym_new = "nr_rotate_popup", .lib = "libnickel.so.1.0.0", .out = nh_symoutptr(realRotatePopup) },
    //nb hook libnickel 4.38.23684 * _ZNK20MainWindowController21allowableOrientationsEP18AbstractController
    { .sym = "_ZNK20MainWindowController21allowableOrientationsEP18AbstractController", .sym_new = "nr_allowable", .lib = "libnickel.so.1.0.0", .out = nh_symoutptr(realAllowable) },
    //nb hook libnickel 4.38.23684 * _ZN15ReadingMenuView17updateReadingMenuEv
    { .sym = "_ZN15ReadingMenuView17updateReadingMenuEv", .sym_new = "nr_reading_menu", .lib = "libnickel.so.1.0.0", .out = nh_symoutptr(realReadingMenu) },
    //nb hook libnickel 4.38.23684 * _ZNK6Device20hasOrientationSensorEv
    { .sym = "_ZNK6Device20hasOrientationSensorEv", .sym_new = "nr_has_sensor", .lib = "libnickel.so.1.0.0", .out = nh_symoutptr(realHasSensor) },
    //nb hook libnickel 4.38.23684 * _ZN11DevSettings19forceAllowLandscapeEv
    { .sym = "_ZN11DevSettings19forceAllowLandscapeEv", .sym_new = "nr_force_landscape", .lib = "libnickel.so.1.0.0", .out = nh_symoutptr(realForceLandscape) },
    // NickelBench does not record libkevdevtouch. Both touch hooks are checked
    // against extracted firmware by test/verify_firmware.py. They are optional
    // for Clara 2E only; init rolls back when another model is missing them.
    { .sym = "_ZNK21QEvdevTouchScreenData13mapFromDeviceERK6QPoint", .sym_new = "nr_map_from_device", .lib = "/usr/local/Kobo/generic/libkevdevtouch.so", .out = nh_symoutptr(realMapFromDevice), .desc = nullptr, .optional = true },
    { .sym = "_ZNK10QTransform3mapERK6QPoint", .sym_new = "nr_transform_map", .lib = "/usr/local/Kobo/generic/libkevdevtouch.so", .out = nh_symoutptr(realTransformMap), .desc = nullptr, .optional = true },
    {0},
};
// Hidden in NickelHook, but linked into this plugin.
extern "C" __attribute__((visibility("hidden"))) void *nh_dlhook(void *handle, const char *symname, void *target);

static void restoreHooks() {
    for (nh_hook *hook = hooks; hook->sym; ++hook) {
        if (!*hook->out) continue; // optional hook that was not applied
        // NOLOAD only finds the library NickelHook already opened. The extra
        // reference is kept, matching NickelHook's own handling.
        void *lib = openHookLib(hook->lib);
        if (!lib || !nh_dlhook(lib, hook->sym, *hook->out))
            nh_log("NickelRotate: could not restore %s", hook->sym);
    }
}

static struct nh_dlsym symbols[] = {
    //nb lookup libnickel 4.38.23684 * _ZTI18AbstractController
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
