// Exercise the production hooks against small stand-ins for Nickel's callers.
// This does not load or emulate Nickel, its screen driver, or its touch device.
#include "../src/nickelrotate.cc"
#include <cassert>
#include <thread>
#include <QApplication>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

extern "C" void nh_log(const char *, ...) {}
extern "C" bool nh_delete_file(const char *) { return true; }
extern "C" bool nh_delete_dir(const char *) { return true; }

class AbstractController {
public:
    virtual ~AbstractController() {}
    virtual int allowed() const { return 15; }
};
class ReadingController : public QObject, public AbstractController {
    Q_OBJECT
public:
    bool audiobook = false;
    int allowed() const override { return audiobook ? 5 : 15; }
};
class N3SettingsReadingController : public QObject, public AbstractController {
    Q_OBJECT
};
class TableOfContentsController : public QObject, public AbstractController {
    Q_OBJECT
};
class UnknownController : public AbstractController {};

static bool stockSensor(const void *) { return false; }
static bool stockForce(void *) { return true; }
static int stockAllowable(const void *, void *controller) {
    auto *value = static_cast<AbstractController *>(controller);
    if (!value) return 1;
    if (nr_force_landscape(nullptr)) return value->allowed() | 10;
    return nr_has_sensor(nullptr) ? value->allowed() : 1;
}
static void stockMenu(void *) {
    assert(nr_has_sensor(nullptr));
    assert(!nr_force_landscape(nullptr));
    // A different thread must not inherit the menu's sensor exception.
    std::thread other([] { assert(!nr_has_sensor(nullptr)); });
    other.join();
}
static void throwingMenu(void *) { throw 42; }
static QTransform stockTransform;
static bool throwMapping = false;
static QPoint stockMap(const QTransform *self, const QPoint &point) { return self->map(point); }
static QPoint stockDeviceMap(const void *, const QPoint &raw) {
    if (throwMapping) throw 42;
    // Firmware uses mirrorXSwapMapper for both spa BW and spa Colour.
    // Preserve that panel mapping before replacing the Qt transform.
    return nr_transform_map(&stockTransform, QPoint(1080 - raw.y(), raw.x()));
}

static QPoint stockNxpDeviceMap(const void *, const QPoint &raw) {
    const QPoint portrait(qBound(0, 1072 - raw.y(), 1071), qBound(0, raw.x(), 1447));
    return nr_transform_map(&stockTransform, portrait);
}

static QObject *stockRotatePopup(QObject *object, QObject *parent) {
    assert(object->parent() == parent);
    auto *popup = static_cast<QWidget *>(object);
    auto *layout = new QVBoxLayout(popup);
    auto *autoRow = new QWidget(popup);
    autoRow->setObjectName("autoRotateContainer");
    layout->addWidget(autoRow);
    auto *first = new QFrame(popup);
    first->setObjectName("dividerLine1");
    first->setFrameShape(QFrame::HLine);
    layout->addWidget(first);
    layout->addWidget(new QWidget(popup));
    auto *second = new QFrame(popup);
    second->setObjectName("dividerLine2");
    second->setFrameShape(QFrame::HLine);
    layout->addWidget(second);
    layout->addWidget(new QWidget(popup));
    autoRow->hide();
    return object;
}

static int selectedItem = -1;
static void stockSelectedItem(QWidget *popup, int selected) {
    selectedItem = selected;
    for (const char *name : {"portraitImage", "landscapeImage"}) {
        auto *icon = popup->findChild<QLabel *>(name);
        const bool hit = selected == (std::strcmp(name, "portraitImage") == 0 ? 1 : 2);
        QPixmap pixmap(hit ? 70 : 42, 70);
        pixmap.fill(Qt::black);
        icon->setPixmap(pixmap);
    }
}

static void popupSelectionAndWidth() {
    QWidget popup;
    auto *layout = new QVBoxLayout(&popup);
    layout->setContentsMargins(12, 8, 12, 8);
    for (const char *name : {"portraitImage", "landscapeImage"}) {
        auto *row = new QHBoxLayout;
        row->setSpacing(14);
        auto *icon = new QLabel(&popup);
        icon->setObjectName(name);
        row->addWidget(icon);
        auto *label = new QLabel(std::strcmp(name, "portraitImage") == 0 ? "Portrait" : "Landscape", &popup);
        row->addWidget(label);
        layout->addLayout(row);
    }
    realSelectedItem = stockSelectedItem;
    enabled.store(true);
    for (int current : {nr::Portrait, nr::Landscape, nr::InvertedLandscape}) {
        orientation.store(current);
        for (int requested : {0, 3, 1, 2}) {
            // Native setVisible fixes the popup size before selecting an icon.
            // Its width comes from the hidden Auto row, not these visible rows.
            popup.setFixedSize(100, 200);
            nr_selected_item(&popup, requested);
            const int expected = requested == 0 || requested == 3
                ? (current == nr::Portrait ? 1 : 2) : requested;
            assert(selectedItem == expected);
            assert(popup.width() >= layout->totalSizeHint().width());
            assert(popup.height() == 200);
            const int width = popup.width();
            nr_selected_item(&popup, requested);
            assert(popup.width() == width);
        }
    }
    // A larger native width stays intact; translated labels can grow it.
    popup.setFixedSize(900, 200);
    nr_selected_item(&popup, 1);
    assert(popup.width() == 900);
    const auto labels = popup.findChildren<QLabel *>();
    for (auto *label : labels) if (label->objectName().isEmpty())
        label->setText("A longer translated orientation label");
    popup.setFixedSize(100, 200);
    nr_selected_item(&popup, 2);
    assert(popup.width() >= layout->totalSizeHint().width());
    enabled.store(false);
    popup.setFixedSize(100, 200);
    nr_selected_item(&popup, 3);
    assert(selectedItem == 3 && popup.width() == 100);
    puts("PASS: default orientation highlight, selected icon widths, translations and disabled passthrough");
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    abstractType = static_cast<const __cxxabiv1::__class_type_info *>(&typeid(AbstractController));
    realAllowable = stockAllowable;
    realReadingMenu = stockMenu;
    realHasSensor = stockSensor;
    realForceLandscape = stockForce;
    realMapFromDevice = stockDeviceMap;
    realTransformMap = stockMap;
    touchMapping = nr::ReversedLandscapeTouch;
    portraitWidth = 1072;
    portraitHeight = 1448;
    enabled.store(true);

    ReadingController reader;
    N3SettingsReadingController settings;
    TableOfContentsController contents;
    UnknownController unknown;
    auto *controller = static_cast<AbstractController *>(&reader);
    assert(static_cast<void *>(controller) != static_cast<void *>(&reader));
    assert(controllerObject(controller) == static_cast<QObject *>(&reader));
    assert(nr_allowable(nullptr, controller) == 15);
    reader.audiobook = true;
    assert(nr_allowable(nullptr, controller) == 5);
    assert(nr_allowable(nullptr, static_cast<AbstractController *>(&settings)) == 1);
    assert(nr_allowable(nullptr, static_cast<AbstractController *>(&contents)) == 15);
    assert(nr_allowable(nullptr, &unknown) == 1);
    assert(nr_allowable(nullptr, nullptr) == 1);
    assert(!nr_has_sensor(nullptr));  // Auto remains absent outside the scope.
    assert(nr_force_landscape(nullptr)); // The user's config was not changed.
    nr_reading_menu(nullptr);
    assert(!nr_has_sensor(nullptr));
    realReadingMenu = throwingMenu;
    try { nr_reading_menu(nullptr); assert(false); } catch (int) {}
    assert(!permission.inside && !nr_has_sensor(nullptr));

    const QPoint raw(100,200), portrait(880,100);
    // Deliberately wrong cached transform. The correction must replace it.
    stockTransform.translate(400,600);
    orientation.store(nr::Landscape);
    assert(nr_map_from_device(nullptr, raw) == QPoint(1347,880));
    orientation.store(nr::Portrait);
    assert(nr_map_from_device(nullptr, raw) == portrait);
    orientation.store(nr::InvertedLandscape);
    assert(nr_map_from_device(nullptr, raw) == QPoint(100,191));
    orientation.store(nr::Landscape);
    assert(nr_map_from_device(nullptr, QPoint(1222,979)) == QPoint(225,101));
    assert(!mappingTouch);
    assert(nr_transform_map(&stockTransform, portrait) == stockTransform.map(portrait));
    throwMapping = true;
    try { nr_map_from_device(nullptr, raw); assert(false); } catch (int) {}
    assert(!mappingTouch);
    throwMapping = false;
    // Clara 2E must preserve the complete native mapping in every orientation.
    touchMapping = nr::NativeTouch;
    for (int current : {nr::Portrait, nr::Landscape, nr::InvertedLandscape}) {
        orientation.store(current);
        assert(nr_map_from_device(nullptr, raw) == stockTransform.map(portrait));
        assert(!mappingTouch);
    }
    realMapFromDevice = stockNxpDeviceMap;
    stockTransform = QTransform(0, -1, 1, 0, 0, 1072);
    orientation.store(nr::Landscape);
    touchMapping = nr::ReversedLandscapeTouch;
    assert(nr_map_from_device(nullptr, raw) == QPoint(1347,872));
    touchMapping = nr::NativeTouch;
    assert(nr_map_from_device(nullptr, raw) == QPoint(100,200));
    realMapFromDevice = stockDeviceMap;
    touchMapping = nr::ReversedLandscapeTouch;
    enabled.store(false);
    assert(nr_allowable(nullptr, static_cast<AbstractController *>(&settings)) == 15);
    assert(nr_map_from_device(nullptr, raw) == stockTransform.map(portrait));
    realRotatePopup = stockRotatePopup;
    for (bool active : {false, true}) {
        enabled.store(active);
        QWidget parent;
        QWidget popup(&parent);
        assert(nr_rotate_popup(&popup, &parent) == &popup);
        auto *first = popup.findChild<QFrame *>("dividerLine1");
        auto *second = popup.findChild<QFrame *>("dividerLine2");
        assert(first && second && popup.layout()->count() == 5);
        assert(first->isHidden() == active);
        assert(!second->isHidden());
        // Showing the popup must not restore Auto or its separator.
        assert(popup.findChild<QWidget *>("autoRotateContainer")->isHidden());
        parent.show();
        popup.show();
        assert(first->isVisible() == !active && second->isVisible());
    }
    popupSelectionAndWidth();
    puts("PASS: rotation popup hides only the orphan separator, including after show");
    puts("PASS: production hook scopes, controller cross-cast, audiobook mask, touch replacement, disabled passthrough and exception cleanup");
}
#include "hooks_test.moc"
