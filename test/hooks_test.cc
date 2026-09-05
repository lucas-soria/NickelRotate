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
#include <QFontMetrics>
#include <QResizeEvent>
#include <QShowEvent>

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
        const int item = std::strcmp(name, "portraitImage") == 0 ? 1 : 2;
        const bool hit = (selected & 3) == item;
        QPixmap pixmap(hit ? 70 : 42, 70);
        pixmap.fill(Qt::black);
        icon->setPixmap(pixmap);
    }
}

// The firmware uses the hidden Auto label and icon for sizeHint, then applies
// that size again AFTER showing the popup. Record every visible width, not
// only the width after our selection hook returns.
class NativeWidthPopup : public QWidget {
public:
    int savedMask = 5;
    bool fixedIcons = true;
    QString portraitText = "Portrait";
    QString landscapeText = "Landscape";
    QString autoText = "Automatic";
    QList<int> visibleWidths;

    QSize sizeHint() const override {
        auto *label = findChild<QLabel *>("labelAuto");
        auto *icon = findChild<QWidget *>("autoRotateImage");
        return QSize(QFontMetrics(label->font()).horizontalAdvance(label->text()) + 3 * icon->width(),
                     layout()->totalSizeHint().height());
    }
    void setVisible(bool visible) override {
        QWidget::setVisible(visible);
        if (visible) {
            setFixedSize(sizeHint());
            nr_selected_item(this, savedMask);
        }
    }
    void open() {
        // QMenu obtains the native size hint before showing the popup.
        resize(sizeHint());
        show();
        QApplication::processEvents();
    }
protected:
    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        if (isVisible()) visibleWidths.append(width());
    }
    void showEvent(QShowEvent *event) override {
        QWidget::showEvent(event);
        visibleWidths.append(width());
    }
};

static QObject *stockWidthPopup(QObject *object, QObject *) {
    auto *popup = static_cast<NativeWidthPopup *>(object);
    auto *layout = new QVBoxLayout(popup);
    layout->setContentsMargins(12, 12, 12, 12);
    auto *container = new QWidget(popup);
    auto *rows = new QVBoxLayout(container);
    rows->setContentsMargins(10, 0, 10, 0);
    layout->addWidget(container);
    auto *autoRow = new QWidget(container);
    autoRow->setObjectName("autoRotateContainer");
    auto *autoLayout = new QHBoxLayout(autoRow);
    auto *autoIcon = new QLabel(autoRow);
    autoIcon->setObjectName("autoRotateImage");
    autoIcon->resize(100, 30);
    autoLayout->addWidget(autoIcon);
    auto *autoLabel = new QLabel(popup->autoText, autoRow);
    autoLabel->setObjectName("labelAuto");
    autoLayout->addWidget(autoLabel);
    rows->addWidget(autoRow);
    autoRow->hide();
    auto *first = new QFrame(container);
    first->setObjectName("dividerLine1");
    first->setFrameShape(QFrame::HLine);
    rows->addWidget(first);
    for (const char *name : {"portraitImage", "landscapeImage"}) {
        auto *row = new QHBoxLayout;
        row->setSpacing(18);
        auto *icon = new QLabel(container);
        icon->setObjectName(name);
        if (popup->fixedIcons) icon->setFixedSize(70, 70);
        row->addWidget(icon);
        const bool portrait = std::strcmp(name, "portraitImage") == 0;
        auto *label = new QLabel(portrait ? popup->portraitText : popup->landscapeText, container);
        label->setObjectName(portrait ? "labelPortrait" : "labelLandscape");
        row->addWidget(label);
        rows->addLayout(row);
    }
    stockSelectedItem(popup, 0);
    return object;
}

static void popupSelectionAndWidth() {
    realSelectedItem = stockSelectedItem;
    realRotatePopup = stockWidthPopup;
    enabled.store(true);
    for (bool fixedIcons : {true, false}) {
        for (int translation : {0, 1, 2}) {
            for (int initial : {nr::Portrait, nr::Landscape}) {
                orientation.store(initial);
                NativeWidthPopup popup;
                popup.fixedIcons = fixedIcons;
                if (translation == 1)
                    popup.portraitText = "A longer translated portrait label";
                if (translation == 2)
                    popup.autoText = "A longer translated automatic rotation label";
                assert(nr_rotate_popup(&popup, nullptr) == &popup);
                const int firstHint = popup.sizeHint().width();
                auto *autoIcon = popup.findChild<QWidget *>("autoRotateImage");
                for (int current : {initial, initial == 1 ? 2 : 1, initial}) {
                    orientation.store(current);
                    // Device traces contain paired orientation masks, 5 and 10.
                    for (int requested : {current == 1 ? 5 : 10, 0, 3}) {
                        popup.savedMask = requested;
                        popup.visibleWidths.clear();
                        popup.open();
                        assert(!popup.visibleWidths.isEmpty());
                        for (int width : popup.visibleWidths) {
                            if (width != firstHint)
                                fprintf(stderr, "Visible resize: native hint %d, width %d\n", firstHint, width);
                            assert(width == firstHint);
                        }
                        assert(popup.width() == firstHint);
                        assert(popup.width() >= popup.layout()->totalSizeHint().width());
                        assert(selectedItem == (requested == 0 || requested == 3 ? current : requested));
                        popup.hide();
                        // The device's hidden icon goes from 100 to 41 pixels
                        // after layout. The next open must keep the same visible width.
                        autoIcon->resize(41, 30);
                        QApplication::processEvents();
                    }
                }
            }
        }
    }
    enabled.store(false);
    NativeWidthPopup popup;
    nr_rotate_popup(&popup, nullptr);
    auto *autoIcon = popup.findChild<QWidget *>("autoRotateImage");
    assert(autoIcon->minimumWidth() == 0 && autoIcon->maximumWidth() == QWIDGETSIZE_MAX);
    popup.savedMask = 3;
    popup.open();
    assert(selectedItem == 3);
    assert(!popup.findChild<QFrame *>("dividerLine1")->isHidden());
    popup.hide();
    autoIcon->resize(41, 30);
    assert(autoIcon->width() == 41);
    puts("PASS: native sizing before show, no visible width changes, paired masks, translations and disabled passthrough");
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
