#include <iostream>
#include <lilv/lilv.h>
#include <jack/jack.h>

#include "utils.h"
#include "lilvutils.h"

using namespace std::string_literals;

int jack_process(jack_nframes_t nframes, void* arg)
{
    return 0;
}

int main()
{
    lilvutils::world lilvworld;

    lilvutils::uri pluginuri("http://tytel.org/helm"s);
    lilvutils::plugin plugin(pluginuri);
    LV2_Feature* host_featuresp[] = {
        nullptr
    };    
    lilvutils::instance instance(plugin, 48000.0, host_featuresp);

    jack_port_t *midi_in, *audio_out1, *audio_out2;

    auto client = jack_client_open("LV2 Host", JackNullOption, nullptr);
    if (!client) {
        throw std::runtime_error("Failed to open JACK client.");
    }
    utils::finally fin_client([&](){ jack_client_close(client); });
    jack_set_process_callback(client, jack_process, 0);

    std::cout << "Hello World!" << std::endl;
    return 0;
}