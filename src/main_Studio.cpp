#include "Lvs/Studio/Configuration.hpp"
#include "Lvs/Launcher/BuildType.hpp"
#include "Lvs/Launcher/Launcher.hpp"

int main(int argc, char* argv[]) {
    const auto appName = Lvs::Studio::Configuration::GetStudioWindowName().toUtf8();
    const auto appIcon = Lvs::Studio::Configuration::GetLogoPathICO().toUtf8();
    return Lvs::Launcher::Run(
        argc,
        argv,
        Lvs::Launcher::BuildType::Studio,
        appName.constData(),
        appIcon.constData()
    );
}
