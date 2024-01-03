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

int main()
{
    uint32_t maxblocksize = 4096;
    engine::Engine eng(maxblocksize);
    

    std::string homedir = getenv("HOME");
    std::string projectdir = homedir + "/.config/jn-live";
    std::string projectfile = projectdir + "/project.json";
    if(!std::filesystem::exists(projectdir))
    {
        std::filesystem::create_directory(projectdir);
    }
    if(!std::filesystem::exists(projectfile))
    {
        auto prj = project::TestProject();
        ProjectToFile(prj, projectfile);
    }
    auto prj = project::ProjectFromFile(projectfile);

    eng.SetProject(std::move(prj));


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