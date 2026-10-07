#include "core/navigation/ShellController.h"
#include "integrations/adventure/mock/MockAdventureAdapter.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>

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
    void messageKeyboardSendsOnceAndKeepsEmojiWhole() {
        TextEntryController keyboard;QSignalSpy accepted(&keyboard,&TextEntryController::accepted);
        keyboard.begin("Message",{},10,false,"Send",true);
        QCOMPARE(keyboard.keys()[keyIndex(keyboard,"apply")].toMap()["label"].toString(),"Send");
        tap(keyboard,Action::ToggleContinue,2);QCOMPARE(keyboard.nextLayout(),"Emoji");
        keyboard.dispatch(Action::ToggleContinue);keyboard.activate(keyIndex(keyboard,"Q"));
        QCOMPARE(keyboard.text(),QString::fromUtf8("❤️"));QCOMPARE(keyboard.count(),1);
        keyboard.activate(keyIndex(keyboard,"delete"));QVERIFY(keyboard.text().isEmpty());
        keyboard.activate(keyIndex(keyboard,"A"));keyboard.dispatch(Action::LocalAction);
        QCOMPARE(accepted.size(),1);QCOMPARE(accepted.first()[0].toString(),QString::fromUtf8("😀"));
        keyboard.dispatch(Action::LocalAction);QCOMPARE(accepted.size(),1);
        keyboard.begin("Name","draft",24);QCOMPARE(keyboard.submitLabel(),"Apply");
        tap(keyboard,Action::ToggleContinue,3);QCOMPARE(keyboard.nextLayout(),"Symbols 1/2");
        keyboard.dispatch(Action::Back);QCOMPARE(accepted.size(),1);
    }
    void boxNameControllerRequiresConfirmAndCancelsStaleDrafts() {
        class Library final : public LibraryRepository {
        public:
            MockLibraryRepository sample;
            bool editable() const override{return true;}
            QList<World> worlds() const override{return sample.worlds();}
            QList<Adventure> adventures() const override{return sample.adventures();}
            QList<ResumePoint> resumePoints() const override{return {};}
            HomeSnapshot home() const override{return sample.home();}
            std::optional<AdventureRegistration> registration(const QString& id) const override{AdventureRegistration r;r.adventure.id=id;r.revision=1;return r;}
        } library;
        class Service final : public SaveBackupService {
        public:
            bool working=false;int moves=0;BoxNameChange request;std::function<void(SaveBackupResult)> pending;
            bool busy() const override{return working;}
            bool supports(const AdventureRegistration&) const override{return true;}
            void inspect(const AdventureRegistration&,QObject*,std::function<void(SaveBackupSnapshot)> done) override{SaveBackupSnapshot s;s.hasSave=true;s.token="token";done(s);}
            void create(const AdventureRegistration&,const QString&,QObject*,std::function<void(SaveBackupResult)>) override{}
            void restore(const AdventureRegistration&,const SaveBackup&,const QString&,QObject*,std::function<void(SaveBackupResult)>) override{}
            void renameBox(const AdventureRegistration&,const QString&,const BoxNameChange& r,QObject*,std::function<void(SaveBackupResult)> done) override{++moves;request=r;working=true;pending=std::move(done);}
        } service;
        MockTrainerRepository profiles;MockAdventureAdapter adapter;DevelopmentPlatformService platform;
        MockPokedexRepository dex;MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,dex,dex,archive,achievements);
        shell.goToPage(2);shell.dispatch(Action::NextFace);shell.dispatch(Action::NextFace);
        auto& party=*shell.party();party.configureMovement(&service,&library);party.setAdventure("one","Emerald");
        GameProgress observation;observation.availability=ProgressAvailability::Available;observation.contextRevision="owner1";observation.saveRevision="save1";
        PartySnapshot data;data.boxes=QList<PokemonBox>(14);
        for(auto& box:data.boxes){box.members=QList<PokemonRecord>(30);box.name="BOX";}
        observation.party=data;party.setProgress("one",observation);party.showSection("storage");
        QVERIFY(!party.canRenameBox());party.beginBoxName();QVERIFY(!party.moveOpen());
        data.boxNameLimit=8;data.boxNameCharacters="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ";
        observation.party=data;observation.saveRevision="save2";party.setProgress("one",observation);
        QSignalSpy entry(&party,&PartyPresentation::boxNameRequested);
        party.changeBox(13);shell.dispatch(Action::Secondary);QCOMPARE(party.moveStage(),"name-edit");
        QVERIFY(shell.keyboard()->isOpen());QCOMPARE(shell.keyboard()->maximumLength(),8);
        shell.dispatch(Action::NextPage);QCOMPARE(shell.page(),2);
        QCOMPARE(entry.size(),1);QCOMPARE(entry.first()[0].toString(),"BOX");QCOMPARE(entry.first()[1].toInt(),8);
        shell.keyboard()->activate(keyIndex(*shell.keyboard(),"clear"));
        shell.keyboard()->activate(keyIndex(*shell.keyboard(),"T"));
        shell.keyboard()->activate(keyIndex(*shell.keyboard(),"apply"));
        QVERIFY(!shell.keyboard()->isOpen());QCOMPARE(party.moveStage(),"name-confirm");QCOMPARE(service.moves,0);
        party.changeBox(-1);QCOMPARE(party.box(),13);
        party.dispatch(Action::Back);QCOMPARE(party.moveStage(),"name-edit");shell.dispatch(Action::Back);QVERIFY(!party.moveOpen());QVERIFY(!shell.keyboard()->isOpen());
        party.beginBoxName();party.applyBoxName("$");QCOMPARE(party.moveStage(),"name-error");
        party.dispatch(Action::Confirm);QCOMPARE(party.moveStage(),"name-edit");
        party.applyBoxName("TEAM 123");party.dispatch(Action::Confirm);QCOMPARE(service.moves,1);
        QCOMPARE(service.request.box,13);QCOMPARE(service.request.name,"TEAM 123");QCOMPARE(service.request.saveRevision,"save2");
        party.dispatch(Action::Confirm);party.dispatch(Action::Back);QCOMPARE(service.moves,1);QVERIFY(party.moving());
        service.working=false;service.pending({true,true,"Box renamed"});party.dispatch(Action::Confirm);
        QVERIFY(!party.moveOpen());party.beginBoxName();observation.contextRevision="owner2";party.setProgress("one",observation);
        QVERIFY(!party.moveOpen());QVERIFY(!shell.keyboard()->isOpen());party.applyBoxName("STALE");party.dispatch(Action::Confirm);QCOMPARE(service.moves,1);
    }
    void partyMovementConfirmationCancelBusyAndOwnerGates() {
        class Library final : public LibraryRepository {
        public:
            MockLibraryRepository sample;
            QList<World> worlds() const override{return sample.worlds();}
            QList<Adventure> adventures() const override{return sample.adventures();}
            QList<ResumePoint> resumePoints() const override{return {};}
            HomeSnapshot home() const override{return sample.home();}
            std::optional<AdventureRegistration> registration(const QString& id) const override{AdventureRegistration r;r.adventure.id=id;r.revision=1;return r;}
        } library;
        class Service final : public SaveBackupService {
        public:
            bool working=false;int moves=0;PartyMove request;std::function<void(SaveBackupResult)> pending;
            bool busy() const override{return working;}
            bool supports(const AdventureRegistration&) const override{return true;}
            void inspect(const AdventureRegistration&,QObject*,std::function<void(SaveBackupSnapshot)> done) override{SaveBackupSnapshot s;s.hasSave=true;s.token="token";done(s);}
            void create(const AdventureRegistration&,const QString&,QObject*,std::function<void(SaveBackupResult)>) override{}
            void restore(const AdventureRegistration&,const SaveBackup&,const QString&,QObject*,std::function<void(SaveBackupResult)>) override{}
            void movePokemon(const AdventureRegistration&,const QString&,const PartyMove& r,QObject*,std::function<void(SaveBackupResult)> done) override{++moves;request=r;working=true;pending=std::move(done);}
        } service;
        PartyPresentation party(false);party.configureMovement(&service,&library);party.setAdventure("one","Emerald");
        GameProgress observation;observation.availability=ProgressAvailability::Available;observation.contextRevision="owner1";observation.saveRevision="save1";
        PartySnapshot data;data.canManage=true;data.party=QList<PokemonRecord>(6);data.boxes=QList<PokemonBox>(14);
        for(auto& box:data.boxes){box.members=QList<PokemonRecord>(30);box.name="Box";}
        for(int i=0;i<2;++i){auto& mon=data.party[i];mon.kind=PokemonSlotKind::Known;mon.speciesName=QString("Member %1").arg(i);}
        observation.party=data;party.setProgress("one",observation);QVERIFY(party.canMove());
        party.activate(0);QCOMPARE(party.menuIndex(),3);party.dispatch(Action::Confirm);QCOMPARE(party.moveStage(),"places");
        party.dispatch(Action::Confirm);party.dispatch(Action::Right);party.dispatch(Action::Confirm);QCOMPARE(party.moveStage(),"confirm");QCOMPARE(service.moves,0);
        party.dispatch(Action::Back);QCOMPARE(party.moveStage(),"slots");party.dispatch(Action::Confirm);party.dispatch(Action::Confirm);
        QCOMPARE(service.moves,1);QCOMPARE(service.request.from.slot,0);QCOMPARE(service.request.to.slot,1);QCOMPARE(service.request.saveRevision,"save1");
        party.dispatch(Action::Back);party.dispatch(Action::Confirm);QVERIFY(party.moving());QCOMPARE(service.moves,1);
        service.working=false;service.pending({true,true,"Moved"});QCOMPARE(party.moveStage(),"result");party.dispatch(Action::Confirm);QVERIFY(!party.moveOpen());
        party.beginMove();party.moveActivate(14);QCOMPARE(party.moveStage(),"slots");party.moveActivate(29);QCOMPARE(party.moveStage(),"confirm");
        observation.contextRevision="owner2";party.setProgress("one",observation);QVERIFY(!party.moveOpen());QCOMPARE(service.moves,1);
        party.beginMove();party.dispatch(Action::Back);QVERIFY(!party.moveOpen());QCOMPARE(service.moves,1);
        data.boxes[0].members[0]=data.party[1];observation.saveRevision="save2";observation.party=data;party.setProgress("one",observation);
        party.beginMove();party.moveActivate(1);party.moveActivate(0);QCOMPARE(party.moveStage(),"slots"); // capability absent
        party.dispatch(Action::Back);party.dispatch(Action::Back);
        data.canSwapOccupied=true;observation.saveRevision="save3";observation.party=data;party.setProgress("one",observation);
        party.beginMove();party.moveActivate(1);party.moveActivate(0);QCOMPARE(party.moveStage(),"confirm");
        QCOMPARE(party.movePair().size(),2);QCOMPARE(party.movePair()[0].toMap()["name"].toString(),"Member 0");
        QCOMPARE(party.movePair()[1].toMap()["name"].toString(),"Member 1");QCOMPARE(service.moves,1);
        party.dispatch(Action::Back);QCOMPARE(party.moveStage(),"slots");QVERIFY(party.movePair().isEmpty());
        party.moveActivate(1);party.dispatch(Action::Confirm);QVERIFY(!service.request.exchangeOccupied); // empty slot resets intent
        service.working=false;service.pending({true,true,"Moved"});party.dispatch(Action::Confirm);
        party.beginMove();party.moveActivate(1);party.moveActivate(0);party.dispatch(Action::Confirm);
        QVERIFY(service.request.exchangeOccupied);QCOMPARE(service.request.from.box,-1);QCOMPARE(service.request.to.box,0);
        party.dispatch(Action::Confirm);QCOMPARE(service.moves,3); // no repeated write
        service.working=false;service.pending({true,true,"Swapped"});party.dispatch(Action::Confirm);
        party.showSection("storage");party.changeBox(13);QCOMPARE(party.box(),13);
        auto checking=observation;checking.availability=ProgressAvailability::Checking;checking.party.reset();party.setProgress("one",checking);
        QCOMPARE(party.box(),13);party.setProgress("one",observation);QCOMPARE(party.box(),13);
    }

    void partyReleaseRequiresSeparateConfirmationAndKeepsModalGates() {
        class Library final : public LibraryRepository {
        public:
            MockLibraryRepository sample;
            QList<World> worlds() const override{return sample.worlds();}
            QList<Adventure> adventures() const override{return sample.adventures();}
            QList<ResumePoint> resumePoints() const override{return {};}
            HomeSnapshot home() const override{return sample.home();}
            std::optional<AdventureRegistration> registration(const QString& id) const override{AdventureRegistration r;r.adventure.id=id;r.revision=1;return r;}
        } library;
        class Service final : public SaveBackupService {
        public:
            bool working=false;int moves=0;PokemonRelease request;std::function<void(SaveBackupResult)> pending;
            bool busy() const override{return working;}
            bool supports(const AdventureRegistration&) const override{return true;}
            void inspect(const AdventureRegistration&,QObject*,std::function<void(SaveBackupSnapshot)> done) override{SaveBackupSnapshot s;s.hasSave=true;s.token="token";done(s);}
            void create(const AdventureRegistration&,const QString&,QObject*,std::function<void(SaveBackupResult)>) override{}
            void restore(const AdventureRegistration&,const SaveBackup&,const QString&,QObject*,std::function<void(SaveBackupResult)>) override{}
            void releasePokemon(const AdventureRegistration&,const QString&,const PokemonRelease& r,QObject*,std::function<void(SaveBackupResult)> done) override{++moves;request=r;working=true;pending=std::move(done);}
        } service;
        PartyPresentation party(false);party.configureMovement(&service,&library);party.setAdventure("one","Emerald");
        GameProgress observation;observation.availability=ProgressAvailability::Available;observation.contextRevision="owner1";observation.saveRevision="save1";
        PartySnapshot data;data.canManage=true;data.party=QList<PokemonRecord>(6);data.boxes=QList<PokemonBox>(14);
        for(auto& box:data.boxes){box.members=QList<PokemonRecord>(30);box.name="Box";}
        data.party[0].kind=PokemonSlotKind::Known;data.party[0].speciesName="Pikachu";data.party[0].nickname="Sparky";
        observation.party=data;party.setProgress("one",observation);QVERIFY(!party.canRelease());party.beginRelease();QVERIFY(!party.moveOpen());
        data.canRelease=true;observation.party=data;observation.saveRevision="save2";party.setProgress("one",observation);
        party.activate(0);party.dispatch(Action::Down);QCOMPARE(party.menuIndex(),4);party.dispatch(Action::Confirm);
        QCOMPARE(party.moveStage(),"release-confirm");QVERIFY(party.moveTitle().contains("Sparky"));
        for(auto action:{Action::Confirm,Action::Confirm,Action::Down,Action::ContextMenu,Action::ToggleContinue,Action::Home,Action::NextPage,Action::NextFace})party.dispatch(action);
        QCOMPARE(service.moves,0);QCOMPARE(party.moveStage(),"release-confirm");
        party.dispatch(Action::Back);QVERIFY(!party.moveOpen());QCOMPARE(service.moves,0);
        party.beginRelease();party.dispatch(Action::Secondary);QCOMPARE(service.moves,1);QVERIFY(party.moving());
        QCOMPARE(service.request.from.box,-1);QCOMPARE(service.request.from.slot,0);QCOMPARE(service.request.saveRevision,"save2");
        party.dispatch(Action::Secondary);party.dispatch(Action::Back);QCOMPARE(service.moves,1);QVERIFY(party.moving());
        service.working=false;service.pending({true,true,"Released"});QCOMPARE(party.moveStage(),"result");party.dispatch(Action::Confirm);QVERIFY(!party.moveOpen());
        party.beginRelease();observation.contextRevision="owner2";party.setProgress("one",observation);QVERIFY(!party.moveOpen());
        party.beginRelease();party.setAdventure("two","FireRed");QVERIFY(!party.moveOpen());QCOMPARE(service.moves,1);
    }
    void heldItemsChooseConfirmCancelAndKeepModalGates() {
        class Library final : public LibraryRepository {
        public:
            MockLibraryRepository sample;
            QList<World> worlds() const override{return sample.worlds();}
            QList<Adventure> adventures() const override{return sample.adventures();}
            QList<ResumePoint> resumePoints() const override{return {};}
            HomeSnapshot home() const override{return sample.home();}
            std::optional<AdventureRegistration> registration(const QString& id) const override{AdventureRegistration r;r.adventure.id=id;r.revision=1;return r;}
        } library;
        class Service final : public SaveBackupService {
        public:
            bool working=false;int moves=0;HeldItemChange request;std::function<void(SaveBackupResult)> pending;
            bool busy() const override{return working;}
            bool supports(const AdventureRegistration&) const override{return true;}
            void inspect(const AdventureRegistration&,QObject*,std::function<void(SaveBackupSnapshot)> done) override{SaveBackupSnapshot s;s.hasSave=true;s.token="token";done(s);}
            void create(const AdventureRegistration&,const QString&,QObject*,std::function<void(SaveBackupResult)>) override{}
            void restore(const AdventureRegistration&,const SaveBackup&,const QString&,QObject*,std::function<void(SaveBackupResult)>) override{}
            void changeHeldItem(const AdventureRegistration&,const QString&,const HeldItemChange& r,QObject*,std::function<void(SaveBackupResult)> done) override{++moves;request=r;working=true;pending=std::move(done);}
        } service;
        PartyPresentation party(false);party.configureMovement(&service,&library);party.setAdventure("one","Emerald");
        GameProgress observation;observation.availability=ProgressAvailability::Available;observation.contextRevision="owner1";observation.saveRevision="save1";
        PartySnapshot data;data.canManage=true;data.party=QList<PokemonRecord>(6);data.boxes=QList<PokemonBox>(14);
        for(auto& box:data.boxes){box.members=QList<PokemonRecord>(30);box.name="Box";}
        data.party[0].kind=PokemonSlotKind::Known;data.party[0].speciesName="Pikachu";data.party[0].nickname="Sparky";
        observation.party=data;party.setProgress("one",observation);QVERIFY(!party.canHoldItems());party.beginHeldItems();QVERIFY(!party.moveOpen());
        data.canHoldItems=true;data.party[0].itemId=215;data.party[0].item="Charcoal";data.bag.items={{13,20,"Potion","Items"}};observation.party=data;observation.saveRevision="save2";party.setProgress("one",observation);
        party.activate(0);party.dispatch(Action::Down);QCOMPARE(party.menuIndex(),5);party.dispatch(Action::Confirm);
        QCOMPARE(party.moveStage(),"items");QCOMPARE(party.moveRows().size(),2);
        party.dispatch(Action::Down);party.dispatch(Action::Confirm);QCOMPARE(party.moveStage(),"item-confirm");
        QVERIFY(party.moveMessage().contains("Potion"));QVERIFY(party.moveMessage().contains("Charcoal"));QCOMPARE(service.moves,0);
        party.dispatch(Action::Back);QCOMPARE(party.moveStage(),"items");QCOMPARE(party.moveIndex(),1);
        for(auto action:{Action::ContextMenu,Action::ToggleContinue,Action::Home,Action::NextPage,Action::NextFace})party.dispatch(action);
        QCOMPARE(service.moves,0);QCOMPARE(party.moveStage(),"items");
        party.dispatch(Action::Confirm);party.dispatch(Action::Confirm);QCOMPARE(service.moves,1);QVERIFY(party.moving());
        QCOMPARE(service.request.pokemon.box,-1);QCOMPARE(service.request.pokemon.slot,0);QCOMPARE(service.request.saveRevision,"save2");QCOMPARE(service.request.itemId,13);
        party.dispatch(Action::Confirm);party.dispatch(Action::Back);QCOMPARE(service.moves,1);QVERIFY(party.moving());
        service.working=false;service.pending({true,true,"Released"});QCOMPARE(party.moveStage(),"result");party.dispatch(Action::Confirm);QVERIFY(!party.moveOpen());
        party.beginHeldItems();observation.contextRevision="owner2";party.setProgress("one",observation);QVERIFY(!party.moveOpen());
        party.beginHeldItems();party.setAdventure("two","FireRed");QVERIFY(!party.moveOpen());QCOMPARE(service.moves,1);
    }

    void cyclicFacesRestoreIndependentlyAndStartContainsNoGameServices() {
        MockLibraryRepository library;MockTrainerRepository profiles;MockAdventureAdapter adapter;
        DevelopmentPlatformService platform;MockPokedexRepository dex;MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,dex,dex,archive,achievements);
        QVERIFY(!shell.multiverseHome());shell.dispatch(Action::Secondary);QVERIFY(!shell.multiverseHome());
        shell.dispatch(Action::NextFace);QVERIFY(shell.multiverseHome());shell.dispatch(Action::NextFace);QVERIFY(!shell.multiverseHome());
        shell.goToPage(2);const QStringList faces{"dex","party","boxes","center","playroom","shops"};
        for(int i=0;i<12;++i){QCOMPARE(shell.pokemonFace(),faces[i%6]);shell.dispatch(Action::NextFace);}
        for(int i=0;i<12;++i){shell.dispatch(Action::PreviousFace);QCOMPARE(shell.pokemonFace(),faces[(11-i)%6]);}
        shell.dispatch(Action::NextFace);shell.party()->dispatch(Action::Right);QCOMPARE(shell.party()->focusIndex(),1);
        shell.dispatch(Action::NextFace);shell.party()->changeBox(1);shell.party()->dispatch(Action::Right);
        QCOMPARE(shell.party()->box(),1);QCOMPARE(shell.party()->focusIndex(),1);
        shell.dispatch(Action::NextPage);shell.dispatch(Action::PreviousPage);QCOMPARE(shell.pokemonFace(),"boxes");QCOMPARE(shell.party()->box(),1);
        const auto state=shell.navigationState();shell.goToPage(0);shell.restoreNavigation(state);QCOMPARE(shell.pokemonFace(),"boxes");QCOMPARE(shell.party()->box(),1);
        shell.dispatch(Action::PreviousFace);QCOMPARE(shell.party()->focusIndex(),1);
        shell.goToPage(3);for(int i=0;i<12;++i){QCOMPARE(shell.faceIndex(),i%4);shell.dispatch(Action::NextFace);}
        shell.dispatch(Action::PreviousFace);QCOMPARE(shell.faceIndex(),3);shell.dispatch(Action::PreviousFace);QCOMPARE(shell.faceIndex(),2);
        shell.dispatch(Action::Back);QCOMPARE(shell.faceIndex(),2);
        shell.goToPage(2);shell.dispatch(Action::PreviousFace);QCOMPARE(shell.pokemonFace(),"dex");
        shell.pokedex()->dispatch(Action::Down);const auto zone=shell.pokedex()->zone();shell.dispatch(Action::Back);QCOMPARE(shell.pokedex()->zone(),zone);
        QVERIFY(shell.menuItems().contains("Switch Trainer"));QVERIFY(!shell.menuItems().contains("Pokémon Center"));
        shell.dispatch(Action::SystemMenu);shell.activate(6);QVERIFY(shell.powerMenu());QVERIFY(!shell.menuItems().contains("Switch Trainer"));
        auto legacy=state;legacy.remove("pokemonFace");legacy.remove("party");legacy["pokedexFace"]="center";
        shell.restoreNavigation(legacy);QCOMPARE(shell.pokemonFace(),"party");
    }
    void legacyHistoryRoutesMigrateWithoutBecomingSocial() {
        MockLibraryRepository library;MockTrainerRepository profiles;MockAdventureAdapter adapter;
        DevelopmentPlatformService platform;MockPokedexRepository dex;MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,dex,dex,archive,achievements);
        const QList<QPair<QString,QString>> routes{{"archive-journey","journey"},{"archive-champions","journey"},
            {"archive-detail","hall"},{"sets","ra"},{"achievements","ra"},{"achievement-detail","ra"}};
        for(const auto& [route,face]:routes) {
            const QJsonObject history{{"route",route},{"archive","crystal-champion"},{"set","emerald-sample"},
                {"achievements",QJsonObject{{"emerald-sample","first-trail"}}},{"zone","actions"}};
            for(const auto& oldPage:QList<QJsonValue>{QJsonValue("hall"),QJsonValue(4)}) {
                shell.restoreNavigation({{"version",1},{"page",oldPage},{"hall",history}});
                QCOMPARE(shell.page(),3);QCOMPARE(shell.trainerFace(),face);
                const auto state=shell.navigationState();QCOMPARE(state["version"].toInt(),2);
                QCOMPARE(state["page"].toString(),"trainer");
                QCOMPARE(state["hall"].toObject()["archive"].toString(),"crystal-champion");
                QCOMPARE(state["hall"].toObject()["set"].toString(),"emerald-sample");
                shell.goToPage(4);shell.restoreNavigation(state);
                QCOMPARE(shell.trainerFace(),face);QCOMPARE(shell.navigationState()["hall"],state["hall"]);
            }
        }
        shell.restoreNavigation({{"version",1},{"page","trainer"},{"hall",QJsonObject{{"route","sets"}}}});
        QCOMPARE(shell.trainerFace(),"profile");
        shell.restoreNavigation({{"version",1},{"page","pokedex"}});QCOMPARE(shell.page(),2);
    }
    void trainerAndSocialFacesKeepSeparateContextAndModalPriority() {
        MockLibraryRepository library;MockTrainerRepository profiles;MockAdventureAdapter adapter;
        DevelopmentPlatformService platform;MockPokedexRepository dex;MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,dex,dex,archive,achievements);
        QCOMPARE(shell.primaryNames(),QStringList({"Home","Worlds","Companions","Trainer","Social"}));
        shell.goToPage(3);QCOMPARE(shell.trainerFace(),"profile");
        shell.dispatch(Action::Confirm);QVERIFY(shell.trainer()->editing());
        shell.dispatch(Action::NextFace);QCOMPARE(shell.trainerFace(),"profile");
        shell.dispatch(Action::Back);shell.dispatch(Action::NextFace);QCOMPARE(shell.trainerFace(),"journey");
        shell.goToTrainerFace("hall");shell.dispatch(Action::Confirm);
        const auto history=shell.hall()->navigationState();QCOMPARE(shell.hall()->route(),"archive-detail");
        shell.dispatch(Action::NextPage);QCOMPARE(shell.page(),4);QCOMPARE(shell.socialFace(),"chats");
        QVERIFY(!shell.chooseAdventureAvailable());shell.dispatch(Action::ToggleContinue);QVERIFY(!shell.drawerOpen());
        shell.dispatch(Action::Confirm);QVERIFY(shell.notice().isEmpty());QVERIFY(!shell.trainer()->editing());
        shell.dispatch(Action::NextFace);QCOMPARE(shell.socialFace(),"communities");
        shell.dispatch(Action::NextFace);QCOMPARE(shell.socialFace(),"friends");
        shell.dispatch(Action::NextFace);QCOMPARE(shell.socialFace(),"chats");
        shell.dispatch(Action::PreviousFace);QCOMPARE(shell.socialFace(),"friends");
        shell.dispatch(Action::NextFace);QCOMPARE(shell.socialFace(),"chats");
        shell.dispatch(Action::Back);QCOMPARE(shell.socialFace(),"chats");
        const auto checkpoint=shell.navigationState();
        shell.dispatch(Action::PreviousPage);QCOMPARE(shell.trainerFace(),"hall");QCOMPARE(shell.hall()->navigationState(),history);
        shell.goToTrainerFace("ra");QCOMPARE(shell.faceIndex(),3);
        shell.goToTrainerFace("profile");shell.goToPage(4);QCOMPARE(shell.socialFace(),"chats");
        shell.restoreNavigation(checkpoint);QCOMPARE(shell.page(),4);QCOMPARE(shell.trainerFace(),"hall");
        QCOMPARE(shell.hall()->navigationState(),history);QCOMPARE(shell.socialFace(),"chats");
        shell.goToTrainerFace("ra");shell.restoreNavigation({{"version",99},{"page",4},{"trainerFace","ra"}});
        QCOMPARE(shell.page(),0);QCOMPARE(shell.trainerFace(),"profile");QCOMPARE(shell.socialFace(),"chats");
        shell.restoreNavigation({{"version",2},{"page","removed"},{"trainerFace","removed"}});
        QCOMPARE(shell.page(),0);QCOMPARE(shell.trainerFace(),"profile");
    }
    void clinicConfirmationAndStaleCompletionStayWithTheirAdventure() {
        class Library final : public LibraryRepository {
        public:
            MockLibraryRepository sample;
            QList<World> worlds() const override{return sample.worlds();}
            QList<Adventure> adventures() const override{return sample.adventures();}
            QList<ResumePoint> resumePoints() const override{return {};}
            HomeSnapshot home() const override{return sample.home();}
            std::optional<AdventureRegistration> registration(const QString& id) const override {
                AdventureRegistration r;r.adventure.id=id;r.adventure.title=id;r.revision=1;return r;
            }
        } library;
        class Service final : public SaveBackupService {
        public:
            bool working=false,lessons=false,itemPayment=false;QString target;std::function<void(SaveBackupResult)> pending;
            bool busy() const override{return working;}
            bool supports(const AdventureRegistration&) const override{return true;}
            void inspect(const AdventureRegistration&,QObject*,std::function<void(SaveBackupSnapshot)> done) override {
                SaveBackupSnapshot s;s.hasSave=true;s.supported=true;s.token="token";s.canHeal=true;s.needsHealing=true;s.partyCount=6;
                s.shops.supported=true;s.shops.balance=5000;Merchant m;m.balance=5000;m.id="oldale";m.name="Oldale";m.discovered=true;m.available=true;m.stock.append({13,300,0,16,"Potion","Items"});s.shops.merchants.append(m);m.id="dept";m.name="Medicine";m.group="Department store";s.shops.merchants.append(m);m.id="dept2";m.name="Vitamins";m.currency=MerchantCurrency::BattlePoints;m.balance=7;s.shops.merchants.append(m);
                if(lessons){
                    Merchant tutor;tutor.id="tutor";tutor.name="Move tutor";tutor.section="services";tutor.discovered=tutor.available=true;tutor.balance=100;tutor.currency=MerchantCurrency::BattlePoints;
                    MerchantStock lesson{5,24,0,1,"Mega Punch","Lesson","tutor"};
                    if(itemPayment){tutor.itemPayment=true;lesson.kind="relearn";lesson.price=0;lesson.payments.append({111,1,2,"Heart Scale"});}
                    lesson.recipients.append({0,"fingerprint","Pikachu",{},true,{{"Thunder Shock",true},{"Surf",false},{"Growl",true},{"Tail Whip",true}}});
                    lesson.recipients.append({1,{},"Egg","Cannot take a lesson",false,{}});tutor.stock.append(lesson);s.shops.merchants.append(tutor);
                    Merchant hidden;hidden.section="services";s.shops.merchants.append(hidden);
                }
                done(s);
            }
            void create(const AdventureRegistration&,const QString&,QObject*,std::function<void(SaveBackupResult)>) override{}
            void restore(const AdventureRegistration&,const SaveBackup&,const QString&,QObject*,std::function<void(SaveBackupResult)>) override{}
            void heal(const AdventureRegistration& r,const QString&,QObject*,std::function<void(SaveBackupResult)> done) override {
                target=r.adventure.id;working=true;pending=std::move(done);emit busyChanged();
            }
            void finish(){working=false;auto done=std::move(pending);done({true,true,"Recovered"});emit busyChanged();}
            MerchantPurchase request;
            void purchase(const AdventureRegistration& r,const QString&,const MerchantPurchase& p,QObject*,std::function<void(SaveBackupResult)> done) override {
                target=r.adventure.id;request=p;working=true;pending=std::move(done);emit busyChanged();
            }
        } service;
        SaveCenterController center(library);center.configure(&service);center.beginSelected("one");
        center.visitClinic();QVERIFY(center.clinicOpen());QVERIFY(center.canHeal());
        center.dispatch(Action::Back);QVERIFY(!center.clinicOpen());QVERIFY(service.target.isEmpty());
        center.visitClinic();center.dispatch(Action::Confirm);QCOMPARE(center.treatment(),"healing");QCOMPARE(service.target,"one");
        center.dispatch(Action::Back);QVERIFY(center.clinicOpen());center.dispatch(Action::Confirm);QVERIFY(service.working);
        center.close();center.beginSelected("two");QVERIFY(!center.clinicOpen());service.finish();
        QCoreApplication::processEvents();QCOMPARE(center.title(),"two");QCOMPARE(center.treatment(),"ready");
        QVERIFY(!center.clinicMessage().contains("Recovered"));
        center.visitShops();QVERIFY(center.shopsOpen());QCOMPARE(center.merchants().size(),2);
        center.dispatch(Action::Down);center.dispatch(Action::Confirm);QCOMPARE(center.shopGroup(),"Department store");QCOMPARE(center.merchants().size(),2);
        center.dispatch(Action::Down);center.dispatch(Action::Confirm);QCOMPARE(center.shopSelection()["name"].toString(),"Vitamins");
        QCOMPARE(center.shopBalance(),7);QCOMPARE(center.shopSelection()["currency"].toString(),"BP");
        center.dispatch(Action::Back);center.dispatch(Action::Back);QVERIFY(center.shopGroup().isEmpty());QCOMPARE(center.merchantIndex(),1);
        center.dispatch(Action::Up);center.dispatch(Action::Confirm);QCOMPARE(center.shopRoute(),"stock");
        QCOMPARE(center.shopBalance(),5000);QCOMPARE(center.shopSelection()["currency"].toString(),QString::fromUtf8("₽"));
        center.dispatch(Action::Right);QCOMPARE(center.quantity(),2);center.dispatch(Action::Confirm);QCOMPARE(center.shopRoute(),"stock");QVERIFY(!service.working);QCOMPARE(center.basketCount(),2);
        center.dispatch(Action::LocalAction);QCOMPARE(center.shopRoute(),"basket");center.dispatch(Action::Back);QCOMPARE(center.shopRoute(),"stock");
        center.dispatch(Action::LocalAction);center.dispatch(Action::Confirm);
        QVERIFY(service.working);QCOMPARE(service.request.kind,"basket");QCOMPARE(service.request.basket.size(),1);QCOMPARE(service.request.basket[0].quantity,2);QCOMPARE(service.request.basket[0].itemId,13);QCOMPARE(service.target,"two");
        center.dispatch(Action::Back);QCOMPARE(center.shopRoute(),"basket");service.finish();QCOMPARE(center.shopRoute(),"receipt");
        center.close();QVERIFY(!center.shopsOpen());
        service.lessons=true;center.beginSelected("two");center.visitShops();QCOMPARE(center.shopCategories().size(),3);
        center.applyShopSearch("potion");QCOMPARE(center.shopCategory(),"all");QCOMPARE(center.merchants().size(),2);QCOMPARE(center.shopStock().size(),1);
        center.applyShopSearch("unseen secret");QVERIFY(center.merchants().isEmpty());QVERIFY(center.shopStock().isEmpty());QVERIFY(center.shopSelection().isEmpty());
        center.applyShopSearch("");QCOMPARE(center.shopCategory(),"marts");
        center.dispatch(Action::ToggleContinue);QCOMPARE(center.shopRoute(),"locations");center.dispatch(Action::Back);QCOMPARE(center.shopRoute(),"merchants");

        center.dispatch(Action::Left);QCOMPARE(center.shopCategory(),"unknown");QCOMPARE(center.merchants()[0].toMap()["name"].toString(),"???");QVERIFY(center.shopStock().isEmpty());
        center.dispatch(Action::Left);QCOMPARE(center.shopCategory(),"services");QCOMPARE(center.shopStock().size(),1); // Preview before entering.
        center.dispatch(Action::Confirm);QCOMPARE(center.shopRoute(),"stock");center.dispatch(Action::Confirm);QCOMPARE(center.shopRoute(),"recipients");
        center.chooseShopCategory(0);QCOMPARE(center.shopCategory(),"services");
        center.dispatch(Action::Down);center.dispatch(Action::Confirm);QCOMPARE(center.shopRoute(),"recipients"); // Egg stays blocked.
        center.dispatch(Action::Up);center.dispatch(Action::Confirm);QCOMPARE(center.shopRoute(),"moves");
        center.dispatch(Action::Down);center.dispatch(Action::Confirm);QCOMPARE(center.shopRoute(),"moves"); // HM stays blocked.
        center.dispatch(Action::Up);center.dispatch(Action::Confirm);QCOMPARE(center.shopRoute(),"confirm");QVERIFY(!service.working);
        center.dispatch(Action::Back);QCOMPARE(center.shopRoute(),"moves");center.dispatch(Action::Confirm);center.dispatch(Action::Confirm);
        QVERIFY(service.working);QCOMPARE(service.request.kind,"tutor");QCOMPARE(service.request.partySlot,0);QCOMPARE(service.request.moveSlot,0);QCOMPARE(service.request.recipientIdentity,"fingerprint");
        center.dispatch(Action::Back);center.chooseShopCategory(0);QCOMPARE(center.shopRoute(),"confirm");QCOMPARE(center.shopCategory(),"services");
        service.finish();center.close();
        service.lessons=false;center.beginSelected("two");center.visitShops();
        QCOMPARE(center.shopCategory(),"marts");QVERIFY(!center.merchants().isEmpty());center.close();
        service.lessons=service.itemPayment=true;center.beginSelected("two");center.visitShops();center.dispatch(Action::Right);
        QCOMPARE(center.shopBalance(),-1);QVERIFY(center.shopStock()[0].toMap()["payment"].toString().contains("Heart Scale"));
        center.dispatch(Action::Confirm);center.dispatch(Action::Confirm);QCOMPARE(center.shopRoute(),"recipients");
        center.dispatch(Action::Confirm);center.dispatch(Action::Confirm);QCOMPARE(center.shopRoute(),"confirm");QVERIFY(!service.working);
        QVERIFY(center.shopSelection()["paymentRemaining"].toString().contains("2"));
        center.dispatch(Action::Confirm);QCOMPARE(service.request.kind,"relearn");QCOMPARE(service.request.recipientIdentity,"fingerprint");service.finish();center.close();
        PartyPresentation party(true);QSignalSpy requested(&party,&PartyPresentation::healingRequested);
        party.dispatch(Action::Confirm);party.dispatch(Action::Down);party.dispatch(Action::Confirm);
        QCOMPARE(requested.count(),1);QVERIFY(!party.detailOpen());
    }
    void pageTransitionPublishesOnlyItsCompletedState() {
        MockLibraryRepository library;MockTrainerRepository profiles;MockAdventureAdapter adapter;
        DevelopmentPlatformService platform;MockPokedexRepository dex;MockHallOfFameRepository archive;
        MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,dex,dex,archive,achievements);
        shell.goToPage(1);shell.activate(0);shell.dispatch(Action::Confirm);shell.dispatch(Action::Secondary);
        QVERIFY(shell.keyboard()->isOpen());
        QSignalSpy updates(&shell,&ShellController::changed);
        shell.goToPage(3);
        QCOMPARE(updates.size(),1);QCOMPARE(shell.page(),3);QVERIFY(!shell.keyboard()->isOpen());
        QVERIFY(!shell.menuOpen());QVERIFY(!shell.serviceOpen());
    }
    void centerActivitiesAreAnIsolatedRehearsal() {
        PartyPresentation party(true);party.setAdventure("one","First");party.showSection("activities");
        auto* activities=party.activities();activities->showPlace("playroom");activities->dispatch(Action::Right);activities->dispatch(Action::Confirm);
        QCOMPARE(activities->focusIndex(),1);QVERIFY(!activities->reaction().isEmpty());
        activities->dispatch(Action::Secondary);QCOMPARE(activities->gesture(),"greet");
        activities->dispatch(Action::Back);QCOMPARE(activities->route(),"playroom");
        party.setAdventure("one","Renamed");QCOMPARE(activities->route(),"playroom");
        party.setAdventure("two","Other");QVERIFY(activities->reaction().isEmpty());
        activities->showPlace("practice");activities->dispatch(Action::Confirm);
        QVERIFY(!activities->practice()->ready());QCOMPARE(activities->practice()->stage(),"first");
        QCOMPARE(activities->route(),"playroom");
        activities->dispatch(Action::Back);QCOMPARE(activities->stage(),"setup");
        activities->dispatch(Action::Back);QCOMPARE(activities->route(),"playroom");
        PartyPresentation personal(false);personal.showSection("activities");personal.activities()->showPlace("playroom");
        personal.dispatch(Action::Confirm);QVERIFY(personal.activities()->reaction().isEmpty());QVERIFY(personal.entries().isEmpty());
    }
    void partyPresentationStaysReadOnlyAndContextBound() {
        PartyPresentation sample(true);
        sample.setAdventure("one", "First Adventure");
        QCOMPARE(sample.entries().size(),6);
        sample.activate(1); QCOMPARE(sample.detail()["hp"],"0 / 38");
        sample.dispatch(Action::Secondary); QCOMPARE(sample.section(),"party"); // Detail owns local input.
        sample.dispatch(Action::Back); QCOMPARE(sample.focusIndex(),1);
        sample.showSection("storage"); QCOMPARE(sample.entries().size(),30);
        sample.dispatch(Action::Up); QVERIFY(sample.boxFocused());
        sample.dispatch(Action::Right); QCOMPARE(sample.box(),1);
        sample.dispatch(Action::Down); QVERIFY(!sample.boxFocused());
        sample.dispatch(Action::Down); sample.dispatch(Action::Right); sample.activate(7);
        QCOMPARE(sample.detail()["kind"],"unreadable");
        sample.dispatch(Action::Back); sample.openSaves(); sample.returnFromSaves();
        QCOMPARE(sample.section(),"storage"); QCOMPARE(sample.focusIndex(),7);
        sample.setAdventure("one", "Same Adventure"); QCOMPARE(sample.focusIndex(),7);
        sample.setAdventure("two", "Other Adventure"); QCOMPARE(sample.focusIndex(),0); QCOMPARE(sample.box(),0); QVERIFY(!sample.detailOpen());
        PartyPresentation personal(false); personal.setAdventure("one", "Real Adventure");
        QVERIFY(personal.entries().isEmpty()); QVERIFY(personal.detail().isEmpty());
        QSignalSpy backups(&personal,&PartyPresentation::backupsRequested);personal.activate(0);QCOMPARE(backups.size(),1);
        personal.returnFromSaves(); personal.dispatch(Action::Secondary);
        QVERIFY(personal.entries().isEmpty()); QVERIFY(!personal.sample());
        personal.setAdventure("missing", {}); QVERIFY(personal.status().contains("no longer linked"));
    }
    void partyInlineDetailsAndBoxFocusStayIndependent() {
        PartyPresentation sample(true);
        sample.setAdventure("one", "First");
        sample.dispatch(Action::Right);
        QVERIFY(!sample.detailOpen());
        QCOMPARE(sample.detail()["hp"], "0 / 38"); // Focus alone updates the inline summary.
        sample.dispatch(Action::Right);
        QCOMPARE(sample.focusIndex(), 1); // Never wrap unexpectedly into the next row.
        sample.showSection("storage");
        sample.dispatch(Action::Up); QVERIFY(sample.boxFocused());
        sample.dispatch(Action::Right); QCOMPARE(sample.box(), 1);
        sample.dispatch(Action::Back); QVERIFY(!sample.boxFocused());
        sample.activate(7); sample.dispatch(Action::Back);
        sample.changeBox(-1); QCOMPARE(sample.focusIndex(), 0);
        sample.changeBox(1); QCOMPARE(sample.focusIndex(), 7);
        sample.dispatch(Action::Confirm); QVERIFY(sample.detailOpen());
        QSignalSpy backup(&sample,&PartyPresentation::backupsRequested);sample.dispatch(Action::Up);sample.dispatch(Action::Confirm);
        QCOMPARE(backup.size(),1);QCOMPARE(sample.section(),"storage");QCOMPARE(sample.focusIndex(),7);
        QVERIFY(!sample.detailOpen());
        sample.showSection("party"); QCOMPARE(sample.focusIndex(), 1);
        PartyPresentation personal(false);
        personal.dispatch(Action::Secondary); personal.changeBox(1);
        QCOMPARE(personal.box(), 0); QVERIFY(personal.entries().isEmpty());
    }
    void repeatedTeamReadKeepsPresentationButBlocksActions() {
        PartyPresentation party(false); party.setAdventure("emerald", "Emerald");
        GameProgress p; p.availability = ProgressAvailability::Available;
        p.contextRevision = "owner"; p.contentRevision = "rom"; p.saveRevision = "one";
        PartySnapshot data; data.party = QList<PokemonRecord>(6); data.boxes = QList<PokemonBox>(14);
        for (auto& box : data.boxes) box.members = QList<PokemonRecord>(30);
        auto& mon = data.party[0]; mon.kind = PokemonSlotKind::Known; mon.nickname = "Partner";
        mon.speciesId = "pikachu"; mon.formId = "25"; mon.level = 5;
        p.party = data; party.setProgress("emerald", p);
        const auto entries = party.entries();
        QSignalSpy rowsChanged(&party, &PartyPresentation::entriesChanged);
        QSignalSpy actorsChanged(party.activities(), &CenterActivities::actorsChanged);
        QSignalSpy backups(&party, &PartyPresentation::backupsRequested);
        party.dispatch(Action::Right);
        QCOMPARE(rowsChanged.size(), 0); QCOMPARE(party.focusIndex(), 1);
        GameProgress checking; checking.availability = ProgressAvailability::Checking;
        checking.contextRevision = p.contextRevision; checking.contentRevision = p.contentRevision;
        checking.saveRevision = p.saveRevision;
        party.setProgress("emerald", checking);
        QVERIFY(!party.available()); QVERIFY(!party.canMove()); QVERIFY(!party.canRelease());
        QVERIFY(party.displayAvailable());
        QCOMPARE(party.entries(), entries); QCOMPARE(party.activities()->actors().size(), 1);
        party.dispatch(Action::Confirm); QCOMPARE(backups.size(), 0); QVERIFY(!party.detailOpen());
        party.setProgress("emerald", p);
        QVERIFY(party.available()); QCOMPARE(party.focusIndex(), 1);
        QCOMPARE(rowsChanged.size(), 0); QCOMPARE(actorsChanged.size(), 0);
        p.saveRevision = "new"; p.party->party[0].nickname = "Updated";
        party.setProgress("emerald", p);
        QCOMPARE(party.entries()[0].toMap()["name"], "Updated");
        QCOMPARE(rowsChanged.size(), 1); QCOMPARE(actorsChanged.size(), 1);
        checking.contextRevision = "another-owner";
        party.setProgress("emerald", checking);
        QVERIFY(party.entries().isEmpty()); QVERIFY(party.activities()->actors().isEmpty());
        QVERIFY(!party.displayAvailable());
    }
    void realPartyClearsOnSourceChangeAndNeverFallsBackToSamples() {
        PartyPresentation party(false); party.setAdventure("emerald", "Emerald");
        GameProgress observation; observation.availability=ProgressAvailability::Available;
        observation.contentRevision="exact-rom"; observation.saveRevision="save-one";
        observation.contextRevision="owner-one";
        PartySnapshot data; data.party=QList<PokemonRecord>(6); data.boxes=QList<PokemonBox>(14);
        for(auto& box:data.boxes)box.members=QList<PokemonRecord>(30);
        auto& mon=data.party[0];mon.kind=PokemonSlotKind::Known;mon.speciesName="Pikachu";mon.speciesId="pikachu";mon.formId="25";
        mon.level=5;mon.hp=0;mon.stats[0]=20;mon.condition="Fainted";
        data.currentBox=13;data.boxes[13].name="Last box";observation.party=data;
        party.setProgress("emerald",observation);QVERIFY(party.available());QVERIFY(!party.sample());
        auto* room = party.activities();
        QCOMPARE(room->actors().size(),1); QCOMPARE(room->actors()[0].toMap()["name"],"Pikachu");
        room->activate(0);room->dispatch(Action::Confirm);QCOMPARE(room->gesture(),"rest");
        room->dispatch(Action::Secondary);QCOMPARE(room->gesture(),"rest");
        QCOMPARE(observation.party->party[0].hp,std::optional<int>(0));
        QCOMPARE(party.entries().size(),6);QCOMPARE(party.detail()["hp"],"0 / 20");
        party.showSection("storage");QCOMPARE(party.boxCount(),14);QCOMPARE(party.box(),13);
        QCOMPARE(party.boxName(),"Last box");QCOMPARE(party.entries().size(),30);
        party.changeBox(1);QCOMPARE(party.box(),0);
        GameProgress refresh; refresh.availability=ProgressAvailability::Checking;
        party.setProgress("emerald",refresh);party.setProgress("emerald",observation);QCOMPARE(party.box(),0);
        observation.contextRevision="owner-two";party.setProgress("emerald",observation);QCOMPARE(party.box(),13);
        party.setAdventure("other","Other");QVERIFY(!party.available());QVERIFY(party.entries().isEmpty());
        QVERIFY(room->actors().isEmpty());QVERIFY(room->reaction().isEmpty());
        party.setProgress("emerald",observation);QVERIFY(party.detail().isEmpty());
        party.setAdventure("emerald","Emerald");party.setProgress("emerald",observation);
        GameProgress checking;checking.availability=ProgressAvailability::Checking;
        party.setProgress("emerald",checking);QVERIFY(!party.available());QVERIFY(party.entries().isEmpty());
        QVERIFY(room->actors().isEmpty());
        observation.party.reset();party.setProgress("emerald",observation);
        QVERIFY(!party.available());QVERIFY(party.entries().isEmpty()); // Supported badges do not imply Party support.
    }
    void playroomUsesOnlyKnownPartySlotsAndClearsOnSameGameOwnerChange() {
        PartyPresentation party(false);party.setAdventure("emerald","Emerald");
        GameProgress p;p.availability=ProgressAvailability::Available;p.contextRevision="owner-one";p.saveRevision="one";
        PartySnapshot data;data.party=QList<PokemonRecord>(6);
        for(int i=0;i<6;++i) {auto& mon=data.party[i];mon.kind=PokemonSlotKind::Known;mon.nickname=QString("Partner %1").arg(i);mon.speciesId="pikachu";mon.formId="25";}
        data.party[1].kind=PokemonSlotKind::Egg;data.party[2].kind=PokemonSlotKind::Unreadable;data.party[3].kind=PokemonSlotKind::Empty;
        p.party=data;party.setProgress("emerald",p);party.openActivities();auto* room=party.activities();room->activate(0);
        QCOMPARE(room->actors().size(),3);QCOMPARE(room->actors()[1].toMap()["index"],4);
        room->dispatch(Action::Left);QCOMPARE(room->focusIndex(),2);room->dispatch(Action::Right);QCOMPARE(room->focusIndex(),0);
        room->dispatch(Action::Confirm);const auto serial=room->reactionSerial();room->dispatch(Action::Confirm);QVERIFY(room->reactionSerial()>serial);
        p.contextRevision="owner-two";p.party->party[0].nickname="Other trainer";party.setProgress("emerald",p);
        QCOMPARE(room->actors()[0].toMap()["name"],"Other trainer");QVERIFY(room->reaction().isEmpty());QCOMPARE(room->focusIndex(),0);
        p.saveRevision="bad";p.party->error="Unreadable";party.setProgress("emerald",p);QVERIFY(room->actors().isEmpty());
        room->dispatch(Action::Secondary);QVERIFY(room->reaction().isEmpty());room->dispatch(Action::Confirm);QCOMPARE(room->route(),"playroom");
        p.saveRevision="empty";p.party->error.clear();p.party->party.clear();party.setProgress("emerald",p);QVERIFY(room->actors().isEmpty());
    }
    void playroomReactionsKeepActorsAndRespectRestingPartners() {
        CenterActivities room(false);
        QVariantList team{QVariantMap{{"name","One"},{"condition","Healthy"}},
            QVariantMap{{"name","Two"},{"condition","Sleep"}},
            QVariantMap{{"name","Three"},{"condition","Healthy"}}};
        room.setParty(team,"save-one",{}); room.activate(0);
        QSignalSpy roster(&room,&CenterActivities::actorsChanged);
        QSignalSpy reactions(&room,&CenterActivities::reactionRequested);
        room.dispatch(Action::LocalAction);
        QCOMPARE(room.gesture(),"play"); QCOMPARE(reactions.last()[1].toInt(),2);
        room.dispatch(Action::Secondary); QCOMPARE(room.gesture(),"greet");
        room.dispatch(Action::Right);room.dispatch(Action::LocalAction);
        QCOMPARE(room.gesture(),"rest");QCOMPARE(reactions.last()[1].toInt(),-1);
        QCOMPARE(room.actors(),team);QCOMPARE(roster.count(),0);
        room.setParty(team,"other-owner",{});QCOMPARE(roster.count(),1);
        QVERIFY(room.reaction().isEmpty());QCOMPARE(room.focusIndex(),0);
        room.setParty({team[0]},"solo",{});room.dispatch(Action::LocalAction);
        QCOMPARE(room.gesture(),"play");QCOMPARE(reactions.last()[1].toInt(),-1);
        room.setParty({},"missing",{});const auto count=reactions.count();
        room.dispatch(Action::LocalAction);QCOMPARE(reactions.count(),count);
    }
    void worldsPrimaryReentryReturnsToBrowserRoot() {
        MockLibraryRepository library; MockTrainerRepository profiles;
        MockAdventureAdapter adapter; DevelopmentPlatformService platform;
        MockPokedexRepository dex; MockHallOfFameRepository archive; MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,dex,dex,archive,achievements);
        shell.goToPage(1); QVERIFY(shell.collectionsRoot()); shell.activate(0); shell.dispatch(Action::Confirm);
        QCOMPARE(shell.worlds()->route(), "adventures");
        const auto world = shell.worlds()->region()["id"];
        shell.dispatch(Action::SystemMenu); shell.dispatch(Action::Back);
        QCOMPARE(shell.worlds()->route(), "adventures");
        shell.goToPage(1); // Re-selecting the same primary is not a re-entry.
        QCOMPARE(shell.worlds()->route(), "adventures");
        shell.dispatch(Action::NextPage); shell.dispatch(Action::PreviousPage);
        QCOMPARE(shell.worlds()->route(), "regions");
        QCOMPARE(shell.worlds()->region()["id"], world);
        QVERIFY(!shell.multiverseFace());
        shell.dispatch(Action::NextFace); shell.dispatch(Action::Confirm); shell.dispatch(Action::Confirm);
        QCOMPARE(shell.multiverse()->route(), "games");
        const auto checkpoint = shell.navigationState();
        shell.dispatch(Action::PreviousFace); shell.dispatch(Action::NextFace);
        QCOMPARE(shell.multiverse()->route(), "games"); // Local face cycling still preserves its route.
        shell.dispatch(Action::NextPage); shell.dispatch(Action::PreviousPage);
        QVERIFY(shell.multiverseFace());
        QCOMPARE(shell.multiverse()->route(), "systems");
        shell.restoreNavigation(checkpoint); // Adventure lifecycle restore is not a tab revisit.
        QCOMPARE(shell.multiverse()->route(), "games");
        shell.dispatch(Action::Secondary); QVERIFY(shell.keyboard()->isOpen());
        shell.dispatch(Action::NextFace); QVERIFY(shell.multiverseFace());
        shell.dispatch(Action::Back); QCOMPARE(shell.multiverse()->route(), "games");
    }
    void multiverseIsolationAndModalPriority() {
        MockLibraryRepository library; MockTrainerRepository profiles;
        MockAdventureAdapter adapter; DevelopmentPlatformService platform;
        MockPokedexRepository dex; MockHallOfFameRepository archive; MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,dex,dex,archive,achievements);
        QSignalSpy launches(&shell, &ShellController::homeLaunchPressed);
        const auto pokemon = shell.currentAdventureId();
        shell.dispatch(Action::NextFace); QVERIFY(shell.multiverseHome());
        shell.dispatch(Action::ToggleContinue); shell.dispatch(Action::Right); shell.dispatch(Action::Confirm);
        QCOMPARE(shell.multiverse()->selected()["id"], "sample-orbit");
        QCOMPARE(shell.currentAdventureId(), pokemon); QCOMPARE(launches.size(), 0);
        shell.dispatch(Action::Confirm); QVERIFY(shell.notice().contains("No game"));
        shell.dispatch(Action::NextFace); QVERIFY(shell.multiverseHome()); // Notice traps X.
        shell.dispatch(Action::Back); shell.dispatch(Action::NextFace); QVERIFY(!shell.multiverseHome());
        QCOMPARE(shell.currentAdventureId(), pokemon);
        shell.goToPage(1); shell.activate(0); const auto route = shell.worlds()->navigationState();
        shell.dispatch(Action::NextFace); QVERIFY(shell.multiverseFace());
        shell.dispatch(Action::Confirm); QCOMPARE(shell.multiverse()->route(), "games");
        shell.dispatch(Action::Secondary); QVERIFY(shell.keyboard()->isOpen());
        shell.dispatch(Action::NextFace); QVERIFY(shell.multiverseFace());
        shell.dispatch(Action::Back); QVERIFY(!shell.keyboard()->isOpen());
        shell.dispatch(Action::Down); shell.dispatch(Action::Confirm);
        QCOMPARE(shell.multiverse()->focusIndex(), 1); // Missing file cannot be selected.
        shell.dispatch(Action::Left); QCOMPARE(shell.multiverse()->focusIndex(), 1);
        shell.dispatch(Action::Back); QCOMPARE(shell.multiverse()->route(), "systems");
        shell.dispatch(Action::Confirm); QCOMPARE(shell.multiverse()->route(), "games");
        const auto wheelStart=shell.multiverse()->focusIndex(),wheelCount=int(shell.multiverse()->games().size());
        for(int i=0;i<wheelCount;++i)shell.dispatch(Action::Up);
        QCOMPARE(shell.multiverse()->focusIndex(),wheelStart);
        for(int i=0;i<wheelCount;++i)shell.dispatch(Action::Down);
        QCOMPARE(shell.multiverse()->focusIndex(),wheelStart);
        shell.dispatch(Action::PreviousFace); QVERIFY(!shell.multiverseFace());
        QCOMPARE(shell.worlds()->navigationState(), route);
        shell.dispatch(Action::NextFace); QCOMPARE(shell.multiverse()->focusIndex(), 1);
        shell.dispatch(Action::Up); shell.dispatch(Action::Confirm);
        QCOMPARE(shell.page(), 0); QVERIFY(shell.multiverseHome());
        QCOMPARE(shell.multiverse()->selected()["id"], "sample-courier");
        QCOMPARE(shell.currentAdventureId(), pokemon); QCOMPARE(launches.size(), 0);
        shell.dispatch(Action::ToggleContinue); shell.dispatch(Action::NextFace); QVERIFY(shell.multiverseHome());
        shell.dispatch(Action::Back); shell.dispatch(Action::SystemMenu); shell.dispatch(Action::Secondary);
        QVERIFY(shell.multiverseHome()); shell.dispatch(Action::Back);
        shell.goToPage(2); QVERIFY(shell.resumePoints() != shell.multiverse()->choices());
        shell.dispatch(Action::Home); QVERIFY(shell.homeMenuOpen());
        shell.dispatch(Action::Confirm); QVERIFY(shell.multiverseHome());
    }
    void realMultiverseSelectionLaunchAndMissingContent() {
        class Library final:public LibraryRepository {
        public:
            QList<AdventureRegistration> records;
            mutable int mediaReads=0;
            QVariantMap artwork(const QString&)const override {
                ++mediaReads;
                return {{"marquee","file:///logo.png"},{"image","file:///screenshot.png"},
                    {"releasedate","20010321T000000"},{"desc","Full scraped description"},{"genre","Racing"},{"players","1-4"}};
            }
            QList<World> worlds()const override{return {{"hoenn","Hoenn",{}}};}
            QList<Adventure> adventures()const override{QList<Adventure> r;for(const auto& x:records)r.append(x.adventure);return r;}
            QList<ResumePoint> resumePoints()const override{return {};}
            HomeSnapshot home()const override{return {"pokemon",{},{},{}};}
            bool editable()const override{return true;}
            std::optional<AdventureRegistration> registration(const QString& id)const override{for(const auto& r:records)if(r.adventure.id==id)return r;return {};}
        } library;
        class Adapter final:public AdventureAdapter {
        public:
            QString launched;
            QString id()const override{return "fixture";}
            AdventureCapabilities capabilities(const Adventure&)const override{return {true,false,false};}
            AdventureResult launch(const Adventure& a)override{launched=a.id;return {true,{},true};}
            AdventureResult resume(const Adventure&,const ResumePoint&)override{return {false,{}};}
        } adapter;
        QTemporaryDir dir;const auto path=dir.filePath("fixture.gba");QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write("original fixture");file.close();
        AdventureRegistration pokemon;pokemon.adventure.id="pokemon";pokemon.adventure.worldId="hoenn";pokemon.adventure.title="Pokemon fixture";pokemon.contentPath=path;library.records.append(pokemon);
        for(int i=0;i<2;++i){auto r=pokemon;r.adventure.id="multi"+QString::number(i);r.adventure.title="General "+QString::number(i);r.adventure.domain="multiverse";r.adventure.worldId.clear();r.adventure.platformId=i?"snes":"gba";r.contentPath=i?dir.filePath("missing.sfc"):path;r.contentAvailable=!i;library.records.append(r);}
        MockTrainerRepository profiles;DevelopmentPlatformService platform;MockPokedexRepository dex;MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,dex,dex,archive,achievements);
        QCOMPARE(shell.multiverse()->systems().size(),1);shell.goToPage(1);shell.dispatch(Action::NextFace);shell.dispatch(Action::Confirm);shell.dispatch(Action::Confirm);
        const auto info=shell.multiverse()->detail();
        QCOMPARE(info["year"],"2001");QCOMPARE(info["description"],"Full scraped description");
        QCOMPARE(info["logo"],"file:///logo.png");QCOMPARE(info["screenshot"],"file:///screenshot.png");
        QCOMPARE(info["playInfo"].toMap()["maximum"].toInt(),4);
        shell.multiverse()->choices();
        const auto reads=library.mediaReads;
        QSignalSpy modelReset(shell.multiverse(),&MultiversePresentation::gamesChanged);
        for(int i=0;i<20;++i) {
            shell.multiverse()->dispatch(Action::Down);shell.multiverse()->games();shell.multiverse()->choices();
        }
        QCOMPARE(library.mediaReads,reads);QCOMPARE(modelReset.size(),0);
        shell.multiverse()->applySearch("absent");QVERIFY(shell.multiverse()->games().isEmpty());QCOMPARE(shell.multiverse()->systems().size(),1);
        shell.dispatch(Action::Confirm);shell.dispatch(Action::Confirm);
        QCOMPARE(shell.page(),1);QCOMPARE(adapter.launched,"multi0");QCOMPARE(shell.currentAdventureId(),"pokemon");
        shell.multiverse()->select("multi0");shell.goToPage(0);shell.dispatch(Action::NextFace);
        const auto state=shell.navigationState();shell.dispatch(Action::Confirm);QCOMPARE(adapter.launched,"multi0");
        ShellController restored(library,profiles,adapter,platform,dex,dex,archive,achievements);restored.restoreNavigation(state);
        QVERIFY(restored.multiverseHome());QCOMPARE(restored.multiverse()->selected()["id"],"multi0");QCOMPARE(restored.currentAdventureId(),"pokemon");
        QVERIFY(QFile::remove(path));library.records[1].contentAvailable=false;restored.refreshLibrary();QVERIFY(restored.multiverse()->systems().isEmpty());
        QCOMPARE(restored.multiverse()->selected()["id"],"multi0");QVERIFY(!restored.multiverse()->selected()["linked"].toBool());
        restored.restoreNavigation(QJsonObject{{"version",1}});QVERIFY(restored.multiverse()->selected().isEmpty());QVERIFY(!restored.multiverseHome());
    }
    void multiverseFilteringAndEmptyProduction() {
        MultiversePresentation sample(true);
        sample.activate(0); sample.applySearch("lantern"); QCOMPARE(sample.games().size(), 1);
        sample.dispatch(Action::ToggleContinue); QVERIFY(sample.games().isEmpty());
        sample.dispatch(Action::Confirm); QCOMPARE(sample.games().size(), 2); QVERIFY(sample.query().isEmpty());
        sample.applySearch("courier"); sample.dispatch(Action::Back);
        sample.activate(1); QVERIFY(sample.query().isEmpty());
        sample.dispatch(Action::Back); sample.activate(0); QCOMPARE(sample.query(), "courier");
        sample.select("sample-lantern"); QVERIFY(sample.selected().isEmpty());
        MultiversePresentation personal(false);
        QVERIFY(personal.systems().isEmpty());
        for (const auto action : {Action::Down, Action::Up, Action::Left, Action::Right}) personal.dispatch(action);
        QCOMPARE(personal.focusIndex(), 0);
        QVERIFY(personal.choices().isEmpty()); personal.select("sample-courier"); QVERIFY(personal.selected().isEmpty());
        personal.activate(0); QVERIFY(personal.games().isEmpty());
        personal.dispatch(Action::Confirm); QCOMPARE(personal.route(), "systems");
    }
    void personalLibraryCannotEnterSetupPreview() {
        class Library final : public LibraryRepository {
        public:
            QList<World> worlds() const override { return {}; }
            QList<Adventure> adventures() const override { return {}; }
            QList<ResumePoint> resumePoints() const override { return {}; }
            HomeSnapshot home() const override { return {}; }
            bool editable() const override { return true; }
        } library;
        MockTrainerRepository profiles; MockAdventureAdapter adapter; DevelopmentPlatformService platform;
        MockPokedexRepository dex; MockHallOfFameRepository archive; MockAchievementProvider achievements;
        achievements.enableAccountPreview();
        ShellController shell(library,profiles,adapter,platform,dex,dex,archive,achievements);
        QVERIFY(!shell.multiverse()->sample());
        shell.dispatch(Action::NextFace); QVERIFY(shell.multiverseHome());
        shell.dispatch(Action::ToggleContinue); QVERIFY(shell.resumePoints().isEmpty());
        shell.dispatch(Action::Confirm); QVERIFY(!shell.drawerOpen());
        shell.dispatch(Action::Confirm); QCOMPARE(shell.page(), 1); QVERIFY(shell.multiverseFace());
        shell.dispatch(Action::Confirm); QVERIFY(shell.multiverse()->games().isEmpty());
        shell.dispatch(Action::Home); shell.dispatch(Action::Confirm);
        shell.dispatch(Action::NextFace); QVERIFY(!shell.multiverseHome());
        shell.dispatch(Action::Confirm); QCOMPARE(shell.page(), 1); QVERIFY(!shell.multiverseFace());
        shell.dispatch(Action::Home);
        shell.dispatch(Action::SystemMenu); shell.activate(0); shell.activate(4);
        shell.activate(2); QCOMPARE(shell.service(),"settings"); QVERIFY(!shell.notice().isEmpty());
        shell.dispatch(Action::Back); shell.activate(1); QVERIFY(shell.hall()->account()->isOpen());
        shell.dispatch(Action::Back); QVERIFY(!shell.hall()->account()->isOpen());
        QCOMPARE(shell.service(),"settings"); QCOMPARE(shell.focusIndex(),1);
        shell.dispatch(Action::Back); QCOMPARE(shell.service(),"settings"); QCOMPARE(shell.settings()->category(),4); QCOMPARE(shell.focusIndex(),4);
        // Editing stays inside Settings, with vertical Save / Cancel navigation.
        shell.activate(4); shell.activate(0);
        QVERIFY(shell.trainer()->editing()); QCOMPARE(shell.service(), "settings");
        const auto before = shell.trainer()->profile();
        shell.trainer()->setDraftName("Discard this draft");
        for(int i=0;i<4;++i) shell.dispatch(Action::Down);
        QCOMPARE(shell.focusIndex(),4); shell.dispatch(Action::Confirm);
        QVERIFY(!shell.trainer()->editing()); QCOMPARE(shell.trainer()->profile(),before);
        QCOMPARE(shell.settings()->rowFocus(),0);
        shell.dispatch(Action::Back);
        achievements.setAccount({});
        shell.activate(4); shell.activate(1); shell.activate(0); QVERIFY(shell.keyboard()->isOpen());
        shell.dispatch(Action::NextPage); QVERIFY(!shell.keyboard()->isOpen());
        QVERIFY(!shell.hall()->account()->isOpen()); QVERIFY(!shell.serviceOpen());
    }
    void trainerSetupRehearsalNeverWritesProfiles() {
        MockLibraryRepository library; MockTrainerRepository profiles;
        MockAdventureAdapter adapter; DevelopmentPlatformService platform;
        MockPokedexRepository dex; MockHallOfFameRepository archive; MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,dex,dex,archive,achievements);
        const auto original = shell.trainer()->profile();
        shell.dispatch(Action::SystemMenu); shell.activate(0); shell.activate(4);
        QCOMPARE(shell.service(), "settings"); shell.activate(2);
        QCOMPARE(shell.service(), "trainer-setup");
        auto* flow = shell.trainerSetup();
        flow->activate(0); flow->activate(0); flow->activate(3);
        QVERIFY(!flow->error().isEmpty()); QCOMPARE(flow->stage(), "identity");
        shell.activate(0); QVERIFY(shell.keyboard()->isOpen());
        shell.keyboard()->activate(keyIndex(*shell.keyboard(), "R"));
        shell.keyboard()->activate(keyIndex(*shell.keyboard(), "apply"));
        // Direct text completion below also exercises validation independently of key geometry.
        shell.keyboard()->cancel(); flow->applyName(" River ");
        flow->activate(3); QCOMPARE(flow->stage(), "pin");
        flow->activate(12); QVERIFY(!flow->error().isEmpty());
        for (int i=0;i<4;++i) flow->activate(i);
        QCOMPARE(flow->pinMask().size(),4); QVERIFY(!flow->pinMask().contains("1234"));
        flow->activate(12); QCOMPARE(flow->stage(), "repeat");
        for (int i=0;i<4;++i) flow->activate(0);
        flow->activate(12); QVERIFY(!flow->error().isEmpty()); QVERIFY(flow->pinMask().isEmpty());
        for (int i=0;i<4;++i) flow->activate(i);
        flow->activate(12); QCOMPARE(flow->stage(), "review");
        QCOMPARE(flow->pinChoice(), "PIN chosen for preview");
        flow->activate(1); QCOMPARE(flow->name(), "River");
        flow->activate(3); flow->activate(13); QCOMPARE(flow->pinChoice(), "No PIN chosen");
        flow->activate(0); QCOMPARE(flow->stage(), "done");
        QCOMPARE(shell.trainer()->profile(),original);
        shell.dispatch(Action::NextPage); QVERIFY(!shell.serviceOpen());
        QVERIFY(flow->name().isEmpty()); QVERIFY(flow->pinMask().isEmpty());
    }
    void trainerSetupUnlockAndGlobalCancellation() {
        TrainerSetupPresentation flow; flow.begin(); flow.activate(1); flow.activate(1);
        QCOMPARE(flow.stage(), "unlock");
        for (int i=0;i<8;++i) flow.activate(0);
        QCOMPARE(flow.pinMask().size(),6); flow.activate(12);
        QVERIFY(!flow.error().isEmpty()); QVERIFY(flow.pinMask().isEmpty());
        for (int i=0;i<4;++i) flow.activate(i);
        flow.activate(12); QCOMPARE(flow.stage(),"done");
        flow.close(); flow.activate(1); flow.activate(1); flow.activate(0);
        flow.dispatch(Action::Back); QCOMPARE(flow.stage(),"chooser");
        QVERIFY(flow.pinMask().isEmpty()); QCOMPARE(flow.focusIndex(),1);
    }
    void sharedAdventureSelectionAndPairedCenter() {
        class Library final : public LibraryRepository {
        public:
            MockLibraryRepository sample;
            QString missing, latest = "crystal-demo";
            QList<World> worlds() const override { return sample.worlds(); }
            QList<Adventure> adventures() const override {
                auto rows=sample.adventures(); rows.removeIf([&](const auto& a){return a.id==missing;}); return rows;
            }
            QList<ResumePoint> resumePoints() const override { return sample.resumePoints(); }
            HomeSnapshot home() const override { auto h=sample.home();h.activeAdventureId=latest;return h; }
            std::optional<AdventureRegistration> registration(const QString& id) const override {
                for(const auto& a:adventures())if(a.id==id){AdventureRegistration r;r.adventure=a;r.revision=1;return r;}return {};
            }
        } library;
        class Service final : public SaveBackupService {
        public:
            bool working=false;
            QString inspected;
            std::function<void(SaveBackupSnapshot)> pending;
            bool busy() const override { return working; }
            bool supports(const AdventureRegistration&) const override { return true; }
            void inspect(const AdventureRegistration& r,QObject*,std::function<void(SaveBackupSnapshot)> done) override {
                inspected=r.adventure.id;pending=std::move(done);working=true;emit busyChanged();
            }
            void finish() {
                auto done=std::move(pending);working=false;
                done({true,true,"token",{},{{inspected,"revision",QDateTime::currentDateTimeUtc(),32,true,true,false}}});
                emit busyChanged();
            }
            void create(const AdventureRegistration&,const QString&,QObject*,std::function<void(SaveBackupResult)>) override {}
            void restore(const AdventureRegistration&,const SaveBackup&,const QString&,QObject*,std::function<void(SaveBackupResult)>) override {}
        } service;
        MockTrainerRepository profiles; MockAdventureAdapter adapter; DevelopmentPlatformService platform;
        MockPokedexRepository dex; MockHallOfFameRepository archive; MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,dex,dex,archive,achievements);
        shell.center()->configure(&service);
        QSignalSpy launched(&shell,&ShellController::homeLaunchPressed);
        shell.goToPage(2); shell.dispatch(Action::Down);
        const auto dexState=shell.pokedex()->navigationState();
        shell.dispatch(Action::ToggleContinue); QVERIFY(shell.drawerOpen());
        shell.dispatch(Action::Right); QCOMPARE(shell.focusIndex(),1);
        shell.dispatch(Action::Back); QCOMPARE(shell.pokedex()->navigationState(),dexState);
        QCOMPARE(shell.currentAdventureId(),library.latest);
        shell.dispatch(Action::ToggleContinue); shell.activate(0);
        QCOMPARE(shell.currentAdventureId(),QString("emerald-demo")); QVERIFY(launched.isEmpty());
        QVERIFY(!shell.drawerOpen()); QCOMPARE(shell.page(),2);
        shell.dispatch(Action::NextFace); QVERIFY(shell.centerFace());
        QCOMPARE(service.inspected,shell.currentAdventureId());
        // A different choice while the previous game's read is in flight must
        // never show the old shelf under the new game's name.
        shell.dispatch(Action::ToggleContinue); shell.activate(1);
        const auto chosen=shell.currentAdventureId(); QVERIFY(chosen!="emerald-demo");
        service.finish(); QTRY_VERIFY(service.busy()); QCOMPARE(service.inspected,chosen);
        QVERIFY(shell.center()->rows().isEmpty()); service.finish();
        QCOMPARE(shell.center()->rows().first().toMap()["id"].toString(),chosen);
        shell.dispatch(Action::ToggleContinue); shell.dispatch(Action::SystemMenu); shell.activate(0);
        QCOMPARE(shell.service(),QString("settings")); QVERIFY(!shell.drawerOpen());
        shell.dispatch(Action::Back); QVERIFY(!shell.menuOpen()); QVERIFY(!shell.serviceOpen());
        QVERIFY(shell.centerFace()); if(service.busy())service.finish();
        shell.dispatch(Action::LocalAction); // Party/Storage opens the existing save shelf.
        // A Party edit can have added protection copies since this shelf was read.
        // Re-entering backups must inspect the current save/token before restore.
        QVERIFY(service.busy()); service.finish();
        shell.dispatch(Action::Confirm); QVERIFY(shell.center()->confirming());
        shell.dispatch(Action::ToggleContinue); shell.dispatch(Action::NextFace);
        QVERIFY(!shell.drawerOpen()); QVERIFY(shell.centerFace()); QVERIFY(shell.center()->confirming());
        shell.dispatch(Action::Back); QVERIFY(!shell.center()->confirming());
        shell.dispatch(Action::Back); QVERIFY(shell.centerFace()); // B never flips a pair.
        shell.dispatch(Action::PreviousFace);shell.dispatch(Action::PreviousFace);shell.dispatch(Action::PreviousFace);QVERIFY(!shell.centerFace());
        QCOMPARE(shell.pokedex()->navigationState(),dexState);
        shell.dispatch(Action::ToggleContinue); shell.dispatch(Action::NextPage);
        QVERIFY(!shell.drawerOpen()); QCOMPARE(shell.currentAdventureId(),chosen);
        shell.dispatch(Action::ToggleContinue); QVERIFY(shell.drawerOpen()); shell.dispatch(Action::Back);
        shell.goToTrainerFace("journey"); shell.dispatch(Action::ToggleContinue); QVERIFY(shell.drawerOpen());
        shell.dispatch(Action::SystemMenu); shell.dispatch(Action::ToggleContinue); QVERIFY(shell.menuOpen());
        shell.dispatch(Action::Back); shell.dispatch(Action::Back);
        shell.goToPage(2); shell.pokedex()->activateControl("rail",1);
        shell.dispatch(Action::ToggleContinue); shell.dispatch(Action::NextFace);
        QVERIFY(!shell.drawerOpen()); QVERIFY(!shell.centerFace());
        shell.dispatch(Action::Back); shell.dispatch(Action::Secondary);
        QVERIFY(shell.keyboard()->isOpen()); shell.dispatch(Action::NextFace);
        QVERIFY(!shell.centerFace()); shell.dispatch(Action::Back);
        const auto state=shell.navigationState();
        library.missing=chosen; library.latest="emerald-demo"; shell.refreshLibrary();
        QCOMPARE(shell.currentAdventureId(),chosen); QVERIFY(shell.home()["adventureId"].toString().isEmpty());
        shell.restoreNavigation(state); QCOMPARE(shell.currentAdventureId(),chosen);
        shell.dispatch(Action::NextFace); QVERIFY(shell.centerFace()); QVERIFY(shell.center()->rows().isEmpty());
        QVERIFY(shell.center()->message().contains("no longer linked")); QVERIFY(launched.isEmpty());
        const auto pairedState=shell.navigationState();
        shell.goToPage(0); shell.restoreNavigation(pairedState);
        QVERIFY(shell.centerFace()); QCOMPARE(shell.currentAdventureId(),chosen);
        QVERIFY(shell.center()->rows().isEmpty()); // Restart state does not invent a fallback shelf.
    }
    void caseSymbolsAndSecretEntry() {
        TextEntryController keyboard;
        QSignalSpy accepted(&keyboard, &TextEntryController::accepted);
        keyboard.begin("Password", "", 128, true);
        tap(keyboard, Action::Confirm);
        tap(keyboard, Action::Secondary); tap(keyboard, Action::Confirm);
        QCOMPARE(keyboard.text(), "aA");
        QCOMPARE(keyboard.displayText(), QString(2, QChar(0x2022)));
        // All printable ASCII punctuation is available, including quote/backslash.
        QString entered;
        for (int page = 0; page < 2; ++page) {
            tap(keyboard, Action::ToggleContinue);
            const auto keys = keyboard.keys();
            for (int i = 0; i < 26; ++i) {
                if (!keys[i].toMap()["visible"].toBool()) continue;
                entered += keys[i].toMap()["label"].toString();
                keyboard.activate(i);
            }
        }
        QString expected;
        for (int code = 33; code <= 126; ++code)
            if (!QChar(code).isLetterOrNumber()) expected += QChar(code);
        QCOMPARE(entered, expected);
        keyboard.activate(keyIndex(keyboard, "9"));
        QCOMPARE(keyboard.text(), "aA" + expected + "9");
        QCOMPARE(keyboard.displayText(), QString(keyboard.count(), QChar(0x2022)));
        const QString result = keyboard.text();
        keyboard.activate(keyIndex(keyboard, "apply"));
        QCOMPARE(accepted.size(), 1);
        QCOMPARE(accepted.first().first().toString(), result);
        QVERIFY(keyboard.text().isEmpty()); QVERIFY(keyboard.displayText().isEmpty());
        keyboard.begin("Password", "synthetic secret", 64, true);
        keyboard.cancel();
        QVERIFY(keyboard.text().isEmpty()); QVERIFY(keyboard.displayText().isEmpty());
        keyboard.begin("Name", "", 24);
        tap(keyboard, Action::Confirm);
        QCOMPARE(keyboard.displayText(), "A"); // Other forms retain the existing default.
        QVERIFY(keyboard.metaObject()->indexOfProperty("text") < 0);
    }
    void symbolsKeepEveryVisibleKeyReachable() {
        TextEntryController keyboard;
        for (int page = 1; page <= 2; ++page) {
            keyboard.begin("Search", "", 128);
            tap(keyboard, Action::ToggleContinue, page);
            const auto keys = keyboard.keys();
            QSet<int> visited{0}; QList<QList<Action>> pending{{}};
            while (!pending.isEmpty()) {
                const auto path = pending.takeFirst();
                for (const auto direction : {Action::Up, Action::Down, Action::Left, Action::Right}) {
                    keyboard.begin("Search", "", 128);
                    tap(keyboard, Action::ToggleContinue, page);
                    for (const auto step : path) tap(keyboard, step);
                    tap(keyboard, direction);
                    const int next = keyboard.focusIndex();
                    QVERIFY(keys[next].toMap()["visible"].toBool());
                    if (!visited.contains(next)) { visited.insert(next); auto nextPath = path; nextPath.append(direction); pending.append(nextPath); }
                }
            }
            for (int i = 0; i < keys.size(); ++i)
                if (keys[i].toMap()["visible"].toBool()) QVERIFY2(visited.contains(i), qPrintable(keys[i].toMap()["id"].toString()));
        }
        keyboard.begin("Search", "", 128);
        keyboard.activate(keyIndex(keyboard, "Z"));
        tap(keyboard, Action::ToggleContinue, 2);
        QCOMPARE(focusedKey(keyboard), "A");
        const QString before = keyboard.text();
        keyboard.activate(keyIndex(keyboard, "Z")); // Hidden slots cannot insert characters.
        QCOMPARE(keyboard.text(), before);
    }
    void sessionChoicesRequireConfirmationAndCancelCleanly() {
        class Platform final : public PlatformService {
        public:
            bool canSwitchSession() const override { return true; }
            bool dedicatedSession() const override { return true; }
            QString sessionStatus() const override { return {}; }
        } platform;
        MockLibraryRepository library;
        MockTrainerRepository profiles;
        MockAdventureAdapter adapter;
        MockPokedexRepository dex;
        MockHallOfFameRepository archive;
        MockAchievementProvider achievements;
        ShellController shell(library, profiles, adapter, platform, dex, dex, archive, achievements);
        QSignalSpy requested(&shell, &ShellController::modeRequested);
        QSignalSpy exited(&shell, &ShellController::exitRequested);
        shell.dispatch(Action::SystemMenu);
        shell.activate(4);
        QVERIFY(shell.modeConfirmation());
        QVERIFY(requested.isEmpty());
        shell.dispatch(Action::Back);
        QVERIFY(!shell.modeConfirmation());
        shell.activate(5);
        shell.dispatch(Action::NextPage);
        QVERIFY(!shell.modeConfirmation());
        QVERIFY(requested.isEmpty());
        shell.dispatch(Action::SystemMenu);
        shell.activate(5);
        shell.dispatch(Action::Confirm);
        QCOMPARE(requested.size(), 1);
        QCOMPARE(requested.first().first().toString(), "steam");
        shell.activate(6);
        QVERIFY(shell.powerMenu()); QVERIFY(!shell.modeConfirmation());
        QCOMPARE(shell.focusIndex(), 2); // Fresh Power menu defaults to Cancel.
        shell.dispatch(Action::Confirm);
        QVERIFY(!shell.powerMenu()); QCOMPARE(requested.size(), 1);
        shell.activate(4); // Maintenance remains explicit, never a Power default.
        QVERIFY(shell.modeConfirmation()); shell.dispatch(Action::Confirm);
        QCOMPARE(requested.last().first().toString(), "desktop");
        QVERIFY(exited.isEmpty());
    }
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
        MockPokedexRepository guide;
        trainer.picker()->setReference(&guide);
        QVERIFY(!trainer.exists());
        trainer.beginEdit();
        trainer.setDraftName("  ERI 2  ");
        trainer.activate(1); trainer.activate(2);
        trainer.picker()->applySearch("Treecko"); trainer.dispatch(Action::Confirm);
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
        trainer.setDraftName("DISCARD"); trainer.activate(1); trainer.activate(2);
        trainer.dispatch(Action::Back); trainer.activate(4);
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
        shell.dispatch(Action::SystemMenu);
        for (int i = 0; i < 4; ++i) shell.dispatch(Action::Down);
        shell.dispatch(Action::Confirm); // Unavailable Desktop mode overlays the keyboard; Center is now usable.
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
