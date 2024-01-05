#include <iostream>
#include <lilv/lilv.h>
#include <jack/jack.h>
#include <thread>

#include "utils.h"
#include "engine.h"
#include "project.h"
#include "komplete.h"
#include <filesystem>

using namespace std::string_literals;
using namespace std::chrono_literals;

int main(int argc, char** argv)
{
    uint32_t maxblocksize = 4096;
    engine::Engine eng(maxblocksize, argc, argv);
    

    std::string homedir = getenv("HOME");
    std::string projectdir = homedir + "/.config/jn-live";
    if(!std::filesystem::exists(projectdir))
    {
        std::filesystem::create_directory(projectdir);
    }
    {
        std::string projectfile = projectdir + "/project.json";
        if(!std::filesystem::exists(projectfile))
        {
            auto prj = project::TestProject();
            ProjectToFile(prj, projectfile);
        }
        {
            auto prj = project::ProjectFromFile(projectfile);
            eng.SetProject(std::move(prj));
        }
    }
    {
        std::string jackconnectionfile = projectdir + "/jackconnection.json";
        if(!std::filesystem::exists(jackconnectionfile))
        {
            std::array<std::string, 2> m_AudioOutputs {
                "Family 17h/19h HD Audio Controller Analog Stereo:playback_FL",
                "Family 17h/19h HD Audio Controller Analog Stereo:playback_FR",
            };
            std::vector<std::string> midiInputs {};
            project::TJackConnections jackconn(std::move(m_AudioOutputs), std::move(midiInputs));
            JackConnectionsToFile(jackconn, jackconnectionfile);
        }
        {
            auto jackconn = project::JackConnectionsFromFile(jackconnectionfile);
            eng.SetJackConnections(std::move(jackconn));
        }
    }

    {
        auto prj = eng.Project();
        prj = prj.ChangePart(0, 1, std::nullopt, 1.0f);
        prj = prj.ChangePart(1, 2, std::nullopt, 1.0f);
        eng.SetProject(std::move(prj));
    }

    

    std::cout << "Hello World!" << std::endl;
    //komplete::Hid hid;
    while(true)
    {
        //hid.Run();
        eng.ProcessMessages();
        std::this_thread::sleep_for(1ms);
    }
    return 0;
}