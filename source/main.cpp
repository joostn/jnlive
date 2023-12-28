#include <iostream>
#include <lilv/lilv.h>
#include <jack/jack.h>

#include "utils.h"
#include "lilvutils.h"
#include "jackutils.h"
#include "lilvjacklink.h"

using namespace std::string_literals;

int jack_process(jack_nframes_t nframes, void* arg)
{
    return 0;
}

int main()
{
    lilvutils::World lilvworld;
    jackutils::Client jackclient("JN Live");
    lilvjacklink::Global lilvjacklinkglobal;

    lilvutils::Plugin plugin("http://tytel.org/helm"s);
    LV2_Feature* host_featuresp[] = {
        nullptr
    };    
    lilvutils::Instance instance(plugin, 48000.0, host_featuresp);

    std::cout << "Hello World!" << std::endl;
    return 0;
}