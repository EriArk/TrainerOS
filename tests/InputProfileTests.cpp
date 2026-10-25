#include "features/settings/GameInputProfiles.h"
#include "features/settings/SettingsController.h"
#include "core/input/ControllerInput.h"
#include <QtTest>
#include <QSettings>
#include <QTemporaryDir>
#include <QDir>
#include <QJsonArray>
using namespace trainer;

class InputLibrary final : public LibraryRepository {
public:
    QList<World> worlds() const override {return {};}
    QList<Adventure> adventures() const override {
        Adventure a;a.id="pc:test";a.title="Local game";a.platformId="pc";a.adapterId="pc-local";
        return {a};
    }
    QList<ResumePoint> resumePoints() const override{return {};}
    HomeSnapshot home() const override{return {};}
};
class InputProfileTests final : public QObject {
    Q_OBJECT
private slots:
    void settingsSectionsKeepStableRoutes() {
        SettingsController settings;
        QCOMPARE(settings.sections().size(),6);
        settings.selectPage("controller");QCOMPARE(settings.sectionIndex(),1);
        QCOMPARE(settings.categoryId(),QString("controller"));
        settings.dispatch(Action::Back);settings.dispatch(Action::Down);
        QCOMPARE(settings.sectionIndex(),2);QCOMPARE(settings.categoryId(),QString("trainer"));
        settings.selectPage("clock");QCOMPARE(settings.sectionIndex(),5);
        settings.selectPage("nonexistent");QCOMPARE(settings.categoryId(),QString("clock"));
    }
    void profilePersistsPerOwnerAndGameAndDoesNotSaveDraft() {
        QTemporaryDir dir;InputLibrary library;GameInputProfiles p;
        p.configure(&library,dir.path(),"one");p.activate(0);p.activate(0);
        QCOMPARE(p.draft()["mode"].toString(),QString("keyboard_mouse"));
        QCOMPARE(p.snapshot("pc:test")["mode"].toString(),QString("native"));
        p.activate(p.rows().size()-2);
        QCOMPARE(p.snapshot("pc:test")["mode"].toString(),QString("keyboard_mouse"));
        QCOMPARE(p.snapshot("pc:other")["mode"].toString(),QString("native"));
        p.configure(&library,dir.path(),"two");QCOMPARE(p.snapshot("pc:test")["mode"].toString(),QString("native"));
        p.configure(&library,dir.path(),"one");p.activate(0);QCOMPARE(p.draft()["mode"].toString(),QString("keyboard_mouse"));
        p.activate(p.rows().size()-3);QCOMPARE(p.draft()["mode"].toString(),QString("native"));
        p.activate(p.rows().size()-1);QCOMPARE(p.snapshot("pc:test")["mode"].toString(),QString("keyboard_mouse"));
    }
    void ringAndPaletteAreEditableWithController() {
        InputLibrary library;GameInputProfiles p;p.configure(&library,{},{});p.activate(0);p.setPage(2);
        p.activate(p.rows().size()-5);QCOMPARE(p.draft()["rings"].toList().size(),1);
        p.activate(23);QVERIFY(p.choosing());p.setTargetGroup(2);
        p.dispatch(Action::Confirm);QVERIFY(!p.choosing());
        QCOMPARE(p.draft()["rings"].toList()[0].toList()[0].toString(),QString("A"));
        p.selectBinding("select");p.setTargetGroup(5);p.dispatch(Action::Confirm);
        QCOMPARE(p.draft()["bindings"].toMap()["select"].toString(),QString("Ring 1"));
        p.activate(p.rows().size()-4);
        QCOMPARE(p.draft()["bindings"].toMap()["select"].toString(),QString("None"));
    }
    void malformedProfilesNeverAuthorizeOutput() {
        auto p=GameInputProfiles::defaults();QVERIFY(GameInputProfiles::valid(p));
        p["deadzone"]=-1;QVERIFY(!GameInputProfiles::valid(p));
        p=GameInputProfiles::defaults();p["bindings"]=QJsonObject{{"home","Escape"}};
        QVERIFY(!GameInputProfiles::valid(p));
        p=GameInputProfiles::defaults();p["rings"]=QJsonArray{QJsonArray{"Ring 1","A"}};
        QVERIFY(!GameInputProfiles::valid(p));
        QCOMPARE(ControllerProfile::read({{"family","bad"},{"deadzone",100}}).family,QString("auto"));
        QCOMPARE(ControllerProfile::read({{"deadzone",100}}).deadzone,20);
    }
    void controllerSelectionChangesGlyphsAndRequiresRelease() {
        QCoreApplication::setOrganizationName("TrainerOSTests");
        QCoreApplication::setApplicationName("InputProfiles");
        QTemporaryDir dir;QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,dir.path());
        SDL_SetMainReady();QVERIFY(SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER)==0);
        const int device=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,SDL_CONTROLLER_AXIS_MAX,SDL_CONTROLLER_BUTTON_MAX,0);
        QVERIFY(device>=0);auto* pad=SDL_JoystickOpen(device);
        SDL_JoystickSetVirtualAxis(pad,SDL_CONTROLLER_AXIS_TRIGGERLEFT,-32768);
        SDL_JoystickSetVirtualAxis(pad,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);
        {
            ControllerInput input(nullptr,SDL_JoystickInstanceID(pad));input.poll();
            auto p=input.profile();p["family"]="playstation";QVERIFY(input.setProfile(p));
            QCOMPARE(input.glyph("A"),QString::fromUtf8("×"));QCOMPARE(input.confirmButton(),int(SDL_CONTROLLER_BUTTON_A));
            QSignalSpy actions(&input,&ControllerInput::action);
            SDL_JoystickSetVirtualButton(pad,SDL_CONTROLLER_BUTTON_A,1);input.poll();QVERIFY(actions.isEmpty());
            SDL_JoystickSetVirtualButton(pad,SDL_CONTROLLER_BUTTON_A,0);input.poll();
            SDL_JoystickSetVirtualButton(pad,SDL_CONTROLLER_BUTTON_A,1);input.poll();QCOMPARE(actions.size(),1);
            QCOMPARE(qvariant_cast<Action>(actions[0][0]),Action::Confirm);
            p["family"]="nintendo";QVERIFY(input.setProfile(p));QCOMPARE(input.glyph("A"),QString("A"));
            QCOMPARE(input.confirmButton(),int(SDL_CONTROLLER_BUTTON_B));
            SDL_JoystickSetVirtualButton(pad,SDL_CONTROLLER_BUTTON_A,0);input.poll();
            p["deadzone"]=40;QVERIFY(input.setProfile(p));input.poll();actions.clear();
            SDL_JoystickSetVirtualAxis(pad,SDL_CONTROLLER_AXIS_LEFTX,19660);input.poll();
            QVERIFY(actions.isEmpty()); // 60% deflection is below the adjusted threshold.
            SDL_JoystickSetVirtualAxis(pad,SDL_CONTROLLER_AXIS_LEFTX,32767);input.poll();
            QCOMPARE(actions.size(),1);QCOMPARE(qvariant_cast<Action>(actions[0][0]),Action::Right);
        }
        SDL_JoystickClose(pad);SDL_JoystickDetachVirtual(device);SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    }
};
QTEST_MAIN(InputProfileTests)
#include "InputProfileTests.moc"
