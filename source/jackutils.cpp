#include "jackutils.h"
#include "lilvjacklink.h"

namespace jackutils
{
    int Client::process(jack_nframes_t nframes)
    {
        auto jacklinks = lilvjacklink::Global::StaticPtr();
        if(jacklinks)
        {
            jacklinks->process(nframes);
        }
        return 0;
    }
}
