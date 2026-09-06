#include "core/navigation/ShellController.h"
#include "integrations/adventure/mock/MockAdventureAdapter.h"
#include <QtTest>

using namespace trainer;

namespace {
int keyIndex(const TextEntryController& keyboard, const QString& id) {
    const auto keys = keyboard.keys();
    for (int i = 0; i < keys.size(); ++i) if (keys[i].toMap()["id"] == id) return i;
    return -1;
}
QString focusedKey(const TextEntryController& keyboard) {
    return keyboard.keys().at(keyboard.focusIndex()).toMap()["id"].toString();
}
void tap(TextEntryController& keyboard, Action action, int count = 1) {
    for (int i = 0; i < count; ++i) keyboard.dispatch(action);
}
}

class InteractionTests : public QObject {
    Q_OBJECT
private slots:
    void everyLetterAndNumber() {
        TextEntryController keyboard;
        keyboard.begin("Search", "", 80);
        const auto row = [&](int count) {
            for (int i = 0; i < count; ++i) {
                keyboard.dispatch(Action::Confirm);
                if (i + 1 < count) keyboard.dispatch(Action::Right);
            }
        };
        row(10); // A-J
        tap(keyboard, Action::Down); tap(keyboard, Action::Left, 9);
        row(10); // K-T
        tap(keyboard, Action::Down); tap(keyboard, Action::Left, 7);
        QCOMPARE(focusedKey(keyboard), "U");
        row(6); // U-Z
        QCOMPARE(keyboard.text(), "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
        tap(keyboard, Action::Right, 3); tap(keyboard, Action::Up, 2);
        QCOMPARE(focusedKey(keyboard), "1");
        row(3); // 123
        tap(keyboard, Action::Down);
        for (int i = 0; i < 3; ++i) {
            tap(keyboard, Action::Confirm);
            if (i < 2) tap(keyboard, Action::Left);
        }
        tap(keyboard, Action::Down); row(3); tap(keyboard, Action::Down); tap(keyboard, Action::Confirm);
        QCOMPARE(keyboard.text(), "ABCDEFGHIJKLMNOPQRSTUVWXYZ1236547890");
    }
    void spatialEdgesAndWideKeys() {
        TextEntryController keyboard;
        keyboard.begin("Name", "", 24);
        tap(keyboard, Action::Up); tap(keyboard, Action::Left);
        QCOMPARE(focusedKey(keyboard), "A");
        tap(keyboard, Action::Right, 5); tap(keyboard, Action::Down, 3);
        QCOMPARE(focusedKey(keyboard), "space");
        tap(keyboard, Action::Up, 3);
        QCOMPARE(focusedKey(keyboard), "F"); // Preserve F's column through Space.
        tap(keyboard, Action::Right, 3); tap(keyboard, Action::Down, 3);
        QCOMPARE(focusedKey(keyboard), "apply");
        tap(keyboard, Action::Up, 3);
        QCOMPARE(focusedKey(keyboard), "I"); // Through Clear and Apply.
        tap(keyboard, Action::Right, 2);
        QCOMPARE(focusedKey(keyboard), "1"); // Across the physical gap.
        tap(keyboard, Action::Left);
        QCOMPARE(focusedKey(keyboard), "J");
        tap(keyboard, Action::Right, 3);
        QCOMPARE(focusedKey(keyboard), "3");
        tap(keyboard, Action::Right);
        QCOMPARE(focusedKey(keyboard), "3");
        tap(keyboard, Action::Down, 3);
        QCOMPARE(focusedKey(keyboard), "0");
        tap(keyboard, Action::Down);
        QCOMPARE(focusedKey(keyboard), "0");
        tap(keyboard, Action::Up, 3);
        QCOMPARE(focusedKey(keyboard), "3"); // 0 remembers the third numeric column.
        tap(keyboard, Action::Down, 3); tap(keyboard, Action::Left); tap(keyboard, Action::Up);
        QCOMPARE(focusedKey(keyboard), "clear"); // Enter Apply horizontally at its middle column.
    }
    void bufferApplyCancelAndLimit() {
        TextEntryController keyboard;
        QSignalSpy accepted(&keyboard, &TextEntryController::accepted);
        keyboard.begin("Name", "ER", 3);
        keyboard.activate(keyIndex(keyboard, "I"));
        keyboard.activate(keyIndex(keyboard, "2"));
        QCOMPARE(keyboard.text(), "ERI");
        QVERIFY(!keyboard.hint().isEmpty());
        keyboard.activate(keyIndex(keyboard, "delete"));
        QCOMPARE(keyboard.text(), "ER");
        keyboard.activate(keyIndex(keyboard, "space"));
        QCOMPARE(keyboard.text(), "ER ");
        keyboard.activate(keyIndex(keyboard, "clear"));
        QVERIFY(keyboard.text().isEmpty());
        keyboard.activate(keyIndex(keyboard, "0"));
        keyboard.activate(keyIndex(keyboard, "apply"));
        QCOMPARE(accepted.size(), 1);
        QCOMPARE(accepted.first().first().toString(), "0");
        QVERIFY(!keyboard.isOpen());
        keyboard.begin("Name", "ORIGINAL", 24);
        keyboard.activate(keyIndex(keyboard, "clear"));
        keyboard.dispatch(Action::Back);
        QCOMPARE(accepted.size(), 1);
        QVERIFY(!keyboard.isOpen());
        keyboard.activate(0);
        QCOMPARE(accepted.size(), 1);
    }
    void existingUnicodeAndOversizedInput() {
        TextEntryController keyboard;
        const QString name = QString::fromUtf8("Aé🇯🇵é");
        keyboard.begin("Name", name, 4);
        QCOMPARE(keyboard.count(), 4);
        keyboard.activate(keyIndex(keyboard, "delete"));
        QCOMPARE(keyboard.text(), QString::fromUtf8("Aé🇯🇵"));
        keyboard.activate(keyIndex(keyboard, "delete"));
        QCOMPARE(keyboard.text(), QString::fromUtf8("Aé"));
        keyboard.begin("Name", "TOO LONG", 4);
        QCOMPARE(keyboard.text(), "TOO LONG");
        keyboard.activate(keyIndex(keyboard, "apply"));
        QVERIFY(keyboard.isOpen());
        QVERIFY(!keyboard.hint().isEmpty());
    }
    void profileCreateEditAndCancel() {
        MockTrainerRepository repository;
        TrainerController trainer(repository);
        QVERIFY(!trainer.exists());
        trainer.beginEdit();
        trainer.setDraftName("  ERI 2  ");
        trainer.activate(1); trainer.activate(2);
        QVERIFY(!repository.load());
        trainer.activate(3);
        QVERIFY(trainer.exists());
        QVERIFY(!trainer.editing());
        const auto saved = repository.load().value();
        QCOMPARE(saved.name, "ERI 2");
        QCOMPARE(saved.emblemId, "leaf");
        QCOMPARE(saved.favoritePokemonId, "treecko");
        QVERIFY(!saved.id.isEmpty());
        QVERIFY(saved.createdAt.isValid());
        trainer.beginEdit();
        trainer.setDraftName("DISCARD"); trainer.activate(1); trainer.activate(2); trainer.activate(4);
        QCOMPARE(repository.load()->name, saved.name);
        QCOMPARE(repository.load()->emblemId, saved.emblemId);
        QCOMPARE(repository.load()->favoritePokemonId, saved.favoritePokemonId);
        trainer.beginEdit();
        trainer.setDraftName("ERI 3"); trainer.activate(3);
        QCOMPARE(repository.load()->name, "ERI 3");
        QCOMPARE(repository.load()->id, saved.id);
        QCOMPARE(repository.load()->createdAt, saved.createdAt);
        TrainerController reopened(repository);
        QCOMPARE(reopened.profile()["name"].toString(), "ERI 3");
    }
    void profileValidationAndFailedWrite() {
        MockTrainerRepository repository;
        TrainerController trainer(repository);
        trainer.beginEdit(); trainer.activate(3);
        QVERIFY(!trainer.error().isEmpty());
        QCOMPARE(trainer.focusIndex(), 0);
        QVERIFY(!repository.load());
        trainer.setDraftName(" \t "); trainer.activate(3);
        QVERIFY(!repository.load());
        trainer.setDraftName(QString(25, 'A')); trainer.activate(3);
        QVERIFY(!repository.load());
        trainer.setDraftName("A\nB"); trainer.activate(3);
        QVERIFY(!repository.load());
        trainer.setDraftName("ERI"); trainer.activate(3);
        const auto id = repository.load()->id;
        trainer.beginEdit(); trainer.setDraftName("ERI 2");
        repository.failNextSave(); trainer.activate(3);
        QVERIFY(trainer.editing());
        QVERIFY(!trainer.error().isEmpty());
        QCOMPARE(trainer.draftName(), "ERI 2");
        QCOMPARE(trainer.profile()["name"].toString(), "ERI");
        QCOMPARE(repository.load()->name, "ERI");
        QCOMPARE(trainer.focusIndex(), 3); // Retry stays reachable.
        trainer.activate(3);
        QCOMPARE(repository.load()->name, "ERI 2");
        QCOMPARE(repository.load()->id, id);
        QVERIFY(!trainer.editing());
    }
    void shellLayersAndProgressIsolation() {
        MockLibraryRepository library;
        MockTrainerRepository profiles;
        MockAdventureAdapter adapter;
        DevelopmentPlatformService platform;
        MockPokedexRepository dex;
        MockHallOfFameRepository shellArchive;
        MockAchievementProvider shellAchievements;
        ShellController shell(library, profiles, adapter, platform, dex, dex, shellArchive, shellAchievements);
        const auto badges = shell.home()["badges"];
        const auto caught = shell.home()["caught"];
        shell.goToPage(3); shell.dispatch(Action::Confirm); shell.dispatch(Action::Confirm);
        auto* keyboard = shell.keyboard();
        keyboard->activate(keyIndex(*keyboard, "E"));
        shell.dispatch(Action::SystemMenu); shell.dispatch(Action::Down); shell.dispatch(Action::Down); shell.dispatch(Action::Confirm);
        shell.dispatch(Action::Back); // Notice -> menu.
        QVERIFY(shell.menuOpen());
        shell.dispatch(Action::Back); // Menu -> keyboard.
        QCOMPARE(keyboard->text(), "E");
        QCOMPARE(focusedKey(*keyboard), "E");
        shell.dispatch(Action::ToggleContinue);
        QCOMPARE(keyboard->text(), "E");
        keyboard->activate(keyIndex(*keyboard, "apply"));
        QCOMPARE(shell.trainer()->draftName(), "E");
        shell.trainer()->activate(3);
        QCOMPARE(shell.home()["trainer"].toString(), "E");
        QCOMPARE(shell.home()["badges"], badges);
        QCOMPARE(shell.home()["caught"], caught);
        shell.dispatch(Action::Confirm); shell.dispatch(Action::Confirm); shell.dispatch(Action::Confirm);
        shell.dispatch(Action::PreviousPage);
        QCOMPARE(shell.page(), 2);
        QVERIFY(!keyboard->isOpen());
        QVERIFY(!shell.trainer()->editing());
        QCOMPARE(profiles.load()->name, "E");
        shell.dispatch(Action::NextPage);
        QCOMPARE(shell.page(), 3);
        QCOMPARE(shell.focusIndex(), 0);
    }
};
QTEST_GUILESS_MAIN(InteractionTests)
#include "InteractionTests.moc"
