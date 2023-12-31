#include <iostream>
#include <lilv/lilv.h>
#include <jack/jack.h>
#include <thread>

#include "utils.h"
#include "lilvutils.h"
#include "jackutils.h"
#include "lilvjacklink.h"
#include "komplete.h"
#include "project.h"
#include <filesystem>
#include "realtimethread.h"

using namespace std::string_literals;
using namespace std::chrono_literals;

int main()
{
    realtimethread::Processor rtprocessor(8192);
    
    lilvutils::World lilvworld;
    jackutils::Client jackclient("JN Live");
    lilvjacklink::Global lilvjacklinkglobal;

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

    lilvutils::Plugin plugin("http://tytel.org/helm"s);
    auto samplerate = jack_get_sample_rate(jackutils::Client::Static().get());
    lilvutils::Instance instance(plugin, samplerate);
    lilvjacklink::LinkedPluginInstance linkedplugininstance(instance, "helmpje");
    jack_activate(jackutils::Client::Static().get());

    std::cout << "Hello World!" << std::endl;
    komplete::Hid hid;
    while(true)
    {
        hid.Run();
        std::this_thread::sleep_for(2ms);
    }
    return 0;
}