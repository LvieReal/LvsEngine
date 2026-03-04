#include "Lvs/App/Configuration.hpp"
#include "Lvs/Launcher/BuildType.hpp"
#include "Lvs/Launcher/Launcher.hpp"

int main(int argc, char* argv[]) {
    const auto appName = Lvs::App::Configuration::GetWindowName().toUtf8();
    const auto appIcon = Lvs::App::Configuration::GetLogoPathICO().toUtf8();
    return Lvs::Launcher::Run(
        argc,
        argv,
        Lvs::Launcher::BuildType::App,
        appName.constData(),
        appIcon.constData()
    );
}
