#include <iostream>
#include <lilv/lilv.h>
#include <jack/jack.h>
#include <thread>

#include "utils.h"
#include "lilvutils.h"
#include "jackutils.h"
#include "lilvjacklink.h"
#include "komplete.h"

using namespace std::string_literals;
using namespace std::chrono_literals;

int main()
{
    lilvutils::World lilvworld;
    jackutils::Client jackclient("JN Live");
    lilvjacklink::Global lilvjacklinkglobal;

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