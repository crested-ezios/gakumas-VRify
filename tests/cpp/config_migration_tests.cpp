#include <Windows.h>
#include "vr/config/VrConfigStartup.hpp"
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
using namespace GakumasLocal::Config;
namespace fs = std::filesystem;
using Json = nlohmann::json;
namespace {
fs::path root;
unsigned cases{}, assertions{}, failures{};
std::string failAt;
std::function<void(const char*)> inject;
fs::path Native(const fs::path& path) { return fs::path(L"\\\\?\\" + path.wstring()); }
void Check(bool value, const char* message) { ++assertions; if (!value) throw std::runtime_error(message); }
void Put(const fs::path& path, const std::string& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream output(Native(path), std::ios::binary | std::ios::trunc); output << bytes;
    if (!output) throw std::runtime_error("fixture write failed");
}
std::string Read(const fs::path& path) {
    std::ifstream input(Native(path), std::ios::binary);
    if (!input) throw std::runtime_error("fixture read failed");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
std::string Utf8(const fs::path& path) {
    const auto bytes = path.u8string(); return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}
void PutJson(const fs::path& path, const Json& value) { Put(path, value.dump(4) + "\n"); }
Json Get(const fs::path& path) { return Json::parse(Read(path)); }
void Point(const char* point) {
    if (inject) inject(point);
    if (failAt == point) { failAt.clear(); throw std::runtime_error("synthetic interruption"); }
}
VrConfigManagedOptions Options(const std::string& name) {
    VrConfigManagedOptions options;
    const auto installation = root / name;
    options.startup.legacyDirectory = installation / "gakumas-vr";
    options.startup.mixedConfig = installation / "gakumas-local" / "localizationConfig.json";
    options.startup.managedConfigRoot = installation / "gakumas-local" / "Configs" / "gaku.vrify";
    options.pluginDataRoot = installation / "gakumas-local" / "PluginData" / "gaku.vrify";
    options.logRoot = installation / "gakumas-local" / "Logs" / "gaku.vrify";
    options.startup.releaseDefaults = {{"schemaVersion", 1}, {"vrEnabled", true}, {"vrNativeOnly", false},
        {"vrDiagnosticsEnabled", false}, {"vrMenuLanguage", 0}, {"vrWorldScale", 1.0}};
    for (auto it=options.startup.releaseDefaults.begin(); it!=options.startup.releaseDefaults.end(); ++it)
        options.startup.knownKeys.push_back(it.key());
    options.checkpoint = Point;
    return options;
}
fs::path Target(const VrConfigManagedOptions& o) { return o.startup.managedConfigRoot / "config.json"; }
fs::path Legacy(const VrConfigManagedOptions& o) { return o.startup.legacyDirectory / "config.json"; }
fs::path OldReceipt(const VrConfigManagedOptions& o) { return o.startup.legacyDirectory / "config-layout.json"; }
fs::path State(const VrConfigManagedOptions& o) { return o.pluginDataRoot / "active-layout.json"; }
fs::path Journal(const VrConfigManagedOptions& o) { return o.pluginDataRoot / "migrations" / "layout-v2.json"; }
void Old(const VrConfigManagedOptions& o) {
    Put(Legacy(o), "{\"schemaVersion\":1,\"vrEnabled\":false,\"userUnknown\":17}\n");
    PutJson(OldReceipt(o), {{"schemaVersion",1},{"converterVersion",1},{"layout","legacy"},
        {"phase","completed"},{"configPath",Utf8(Legacy(o))}});
}
VrConfigStartupResult Apply(const VrConfigManagedOptions& o, const Json& plan) {
    return ApplyVrConfigMigration(o, plan, VrConfigPlanHash(plan));
}
void Run(const char* name, const std::function<void()>& action) {
    ++cases; failAt.clear(); inject = {};
    try { action(); std::cout << "PASS " << name << '\n'; }
    catch (const std::exception& error) { ++failures; std::cerr << "FAIL " << name << ": " << error.what() << '\n'; }
    failAt.clear(); inject = {};
}
}
int wmain(int argc, wchar_t** argv) {
    try {
        if (argc != 2 || !fs::path(argv[1]).is_absolute() || fs::exists(argv[1]))
            throw std::runtime_error("expected absent absolute synthetic root");
        root = argv[1]; fs::create_directories(root);
        Run("plan is read-only and contains hashes, not config values", [] {
            auto o=Options("plan"); Old(o); const auto source=Read(Legacy(o)), receipt=Read(OldReceipt(o));
            auto p=PlanVrConfigMigration(o);
            Check(p.at("action")=="copy-dedicated", "wrong dedicated plan");
            Check(!fs::exists(o.pluginDataRoot) && !fs::exists(o.startup.managedConfigRoot), "plan created data directories");
            Check(Read(Legacy(o))==source && Read(OldReceipt(o))==receipt, "plan changed source");
            Check(p.dump().find("userUnknown")==std::string::npos, "plan disclosed config values");
        });
        Run("dedicated copy preserves raw config and old receipt, activates once", [] {
            auto o=Options("copy"); Old(o); auto p=PlanVrConfigMigration(o);
            const auto source=Read(Legacy(o)), receipt=Read(OldReceipt(o));
            auto r=Apply(o,p); Check(r.success && r.managedLayout, "dedicated migration failed");
            Check(Read(Target(o))==source && Read(Legacy(o))==source, "copy changed source/content");
            auto j=Get(Journal(o)); Check(j.at("phase")=="completed", "journal incomplete");
            Check(Read(fs::u8path(j.at("legacyReceiptBackup").get<std::string>()))==receipt, "receipt backup differs");
            Check(Read(OldReceipt(o))==receipt && fs::exists(State(o)), "old receipt changed or no active switch");
            const auto active=Read(State(o));
            Put(Legacy(o), "{broken obsolete source"); Put(OldReceipt(o), "{broken obsolete receipt");
            Check(LoadManagedVrConfig(o).success, "restart consulted corrupt legacy remnants");
            Check(Apply(o,p).success && Read(State(o))==active, "repeat changed active switch");
        });
        Run("fresh standard install creates no legacy anchor", [] {
            auto o=Options("fresh"); auto p=PlanVrConfigMigration(o);
            Check(p.at("action")=="initialize" && Apply(o,p).success, "fresh standard init failed");
            Check(!fs::exists(o.startup.legacyDirectory), "fresh standard init created obsolete anchor");
            Check(Get(Target(o)).at("vrEnabled")==true, "fresh template policy changed");
        });
        Run("valid new config outranks corrupt legacy config and receipt", [] {
            auto o=Options("new-first"); PutJson(Target(o),{{"schemaVersion",1},{"vrEnabled",false}});
            Put(Legacy(o),"{invalid"); Put(OldReceipt(o),"{future broken receipt");
            const auto before=Read(Target(o)); auto p=PlanVrConfigMigration(o);
            Check(p.at("action")=="adopt-new" && Apply(o,p).success, "new config not preferred");
            Check(Read(Target(o))==before && Read(Legacy(o))=="{invalid", "adoption changed data");
        });
        Run("mixed whitelist and new aliases win", [] {
            auto o=Options("mixed"); PutJson(o.startup.mixedConfig,{{"vrEnabled",false},{"vrDebugEnabled",true},
                {"vrDiagnosticsEnabled",false},{"vrProbeOnly",false},{"vrNativeOnly",true},{"secretNonVrSentinel",23}});
            auto bytes=Read(o.startup.mixedConfig); auto p=PlanVrConfigMigration(o); Check(p.at("action")=="convert-mixed", "wrong mixed plan");
            Check(Apply(o,p).success, "mixed migration failed"); auto d=Get(Target(o));
            Check(d.at("vrDiagnosticsEnabled")==false && d.at("vrNativeOnly")==true, "alias priority wrong");
            Check(!d.contains("secretNonVrSentinel") && !d.contains("vrProbeOnly"), "non-VR or old alias imported");
            Check(Read(o.startup.mixedConfig)==bytes && !fs::exists(Legacy(o)), "mixed source changed or old dedicated created");
        });
        Run("schema and known type errors never downgrade", [] {
            auto o=Options("schema"); PutJson(Legacy(o),{{"schemaVersion",2},{"vrEnabled",false}});
            bool failed=false; try { (void)PlanVrConfigMigration(o); } catch (...) { failed=true; }
            Check(failed && !fs::exists(State(o)), "future schema accepted");
            PutJson(Target(o),{{"schemaVersion",1},{"vrEnabled","false"}});
            failed=false; try { (void)PlanVrConfigMigration(o); } catch (...) { failed=true; }
            Check(failed && !fs::exists(Journal(o)), "known type error accepted/fell back");
        });
        Run("wrong plan hash and roots reject before journal", [] {
            auto o=Options("hash"); Old(o); auto p=PlanVrConfigMigration(o);
            Check(!ApplyVrConfigMigration(o,p,std::string(64,'0')).success && !fs::exists(o.pluginDataRoot), "wrong hash wrote state");
            p["configRoot"]="C:/unrelated";
            Check(!Apply(o,p).success && !fs::exists(o.pluginDataRoot), "tampered roots accepted");
        });
        Run("source changed after plan refuses until new explicit plan", [] {
            auto o=Options("source-change"); Old(o); auto p=PlanVrConfigMigration(o);
            PutJson(Legacy(o),{{"schemaVersion",1},{"vrEnabled",true}});
            Check(!Apply(o,p).success && !fs::exists(Target(o)) && !fs::exists(State(o)), "stale source plan committed");
            auto next=PlanVrConfigMigration(o); Check(Apply(o,next).success, "new approved source plan did not recover");
        });
        Run("target appearing after plan is preserved", [] {
            auto o=Options("target-appears"); Old(o); auto p=PlanVrConfigMigration(o);
            PutJson(Target(o),{{"vrEnabled",true},{"targetSentinel",1}}); const auto before=Read(Target(o));
            Check(!Apply(o,p).success && Read(Target(o))==before && !fs::exists(State(o)), "competing target was overwritten/claimed");
            Check(Apply(o,PlanVrConfigMigration(o)).success && Read(Target(o))==before, "explicit adoption failed");
        });
        for(const auto* point : {"after-planned","after-staged","before-copy-commit","after-copy-commit", "after-copied", "after-receipt-backup", "before-activate", "after-activate", "after-completed"}) {
            const auto name=std::string("restart-")+point;
            Run(name.c_str(), [=] {
                auto o=Options(name); Old(o); const auto source=Read(Legacy(o)), oldReceipt=Read(OldReceipt(o));
                auto p=PlanVrConfigMigration(o); failAt=point;
                Check(!Apply(o,p).success, "fault checkpoint did not interrupt");
                Check(Apply(o,p).success, "approved plan did not resume after interruption");
                Check(Read(Target(o))==source && Read(Legacy(o))==source && Read(OldReceipt(o))==oldReceipt, "restart changed data");
                Check(Get(Journal(o)).at("phase")=="completed" && LoadManagedVrConfig(o).success, "restart did not finalize/load");
            });
        }
        Run("same-hash competitor is not claimed without file identity", [] {
            auto o=Options("identity"); Old(o); auto p=PlanVrConfigMigration(o);
            bool injected=false;
            inject=[&](const char* point) { if(!injected && std::string(point)=="before-copy-commit") { injected=true; Put(Target(o),Read(Legacy(o))); } };
            Check(!Apply(o,p).success && !fs::exists(State(o)), "competitor accepted during no-replace copy");
            Check(!Apply(o,p).success && !fs::exists(State(o)), "same hash competitor was claimed on retry");
            Check(Apply(o,PlanVrConfigMigration(o)).success, "new explicit adoption plan did not resolve preserved competitor");
        });
        Run("user edit before activate cannot be overwritten or silently adopted", [] {
            auto o=Options("before-active-edit"); Old(o); auto p=PlanVrConfigMigration(o);
            bool injected=false;
            inject=[&](const char* point) { if(!injected && std::string(point)=="before-activate") { injected=true; PutJson(Target(o),{{"vrEnabled",true},{"editSentinel",9}}); } };
            Check(!Apply(o,p).success && !fs::exists(State(o)), "edited target became active under stale plan");
            const auto before=Read(Target(o));
            Check(!Apply(o,p).success && Read(Target(o))==before, "retry overwrote target edit");
            Check(Apply(o,PlanVrConfigMigration(o)).success && Read(Target(o))==before, "explicit edited-target plan failed");
        });
        Run("completed target missing or damaged never falls back", [] {
            auto o=Options("missing"); Old(o); Check(Apply(o,PlanVrConfigMigration(o)).success,"setup failed");
            fs::remove(Target(o)); Check(!LoadManagedVrConfig(o).success,"missing target fell back");
            Put(Target(o),"{invalid"); Check(!LoadManagedVrConfig(o).success,"damaged target fell back");
        });
        Run("committed target preferences may change across restart", [] {
            auto o=Options("user-edit"); Old(o); Check(Apply(o,PlanVrConfigMigration(o)).success,"setup failed");
            PutJson(Target(o),{{"schemaVersion",1},{"vrEnabled",true},{"vrMenuLanguage",2}});
            const auto state=Read(State(o)); auto loaded=LoadManagedVrConfig(o);
            Check(loaded.success && loaded.document.at("vrMenuLanguage")==2 && Read(State(o))==state,"valid user edit rejected or switch rewritten");
        });
        Run("pending source change replans without deleting journal", [] {
            auto o=Options("replan"); Old(o); auto p=PlanVrConfigMigration(o); failAt="after-planned";
            Check(!Apply(o,p).success,"setup checkpoint failed");
            PutJson(Legacy(o),{{"schemaVersion",1},{"vrEnabled",true},{"vrMenuLanguage",3}});
            auto next=PlanVrConfigMigration(o); Check(Apply(o,next).success,"new approved plan failed");
            auto journal=Get(Journal(o)); Check(journal.at("supersededPlans").size()==1 &&
                journal.at("supersededPlans").at(0).at("planSha256")==VrConfigPlanHash(p),"old plan history missing");
        });
        Run("completed journal recovers pointer without consulting old data", [] {
            auto o=Options("pointer-recovery"); Old(o); auto p=PlanVrConfigMigration(o);
            Check(Apply(o,p).success,"setup failed"); fs::remove(State(o));
            Put(Legacy(o),"{invalid old source"); Put(OldReceipt(o),"{invalid old receipt");
            auto next=PlanVrConfigMigration(o);
            Check(next.at("action")=="already-managed" && Apply(o,next).success,"completed journal consulted old data");
            fs::remove(State(o)); fs::remove(Target(o));
            Check(!Apply(o,p).success && !fs::exists(Target(o)),"completed journal recopied old residue after target loss");
        });
        Run("migration lock and data-root I/O failure retry", [] {
            auto o=Options("lock"); Old(o); auto p=PlanVrConfigMigration(o);
            const auto lock=Journal(o).parent_path()/"layout-v2.lock";
            Put(lock,"synthetic lock sentinel");
            Check(!Apply(o,p).success && !fs::exists(Target(o)),"held migration lock ignored");
            fs::remove(lock); Check(Apply(o,p).success,"lock retry failed");
            auto blocked=Options("blocked-root"); Old(blocked); auto bp=PlanVrConfigMigration(blocked);
            Put(blocked.pluginDataRoot,"synthetic parent-file sentinel");
            Check(!Apply(blocked,bp).success && !fs::exists(Target(blocked)),"invalid data directory accepted");
            fs::remove(blocked.pluginDataRoot); Check(Apply(blocked,bp).success,"I/O retry failed");
        });
        std::cout<<cases<<" cases, "<<assertions<<" assertions, "<<failures<<" failures\n";
        return failures ? 1 : 0;
    } catch(const std::exception& e) { std::cerr<<"FATAL "<<e.what()<<'\n'; return 1; }
}
