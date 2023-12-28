#include "lilvjacklink.h"

namespace lilvjacklink
{
    Link::Link()
    {
        Global::Static().m_Links.push_back(std::unique_ptr<Link>(this));
    }

    Link::~Link()
    {
        auto &links = Global::Static().m_Links;
        auto it = std::find_if(links.begin(), links.end(), [&](const std::unique_ptr<Link> &link){
            return link.get() == this;
        });
        if(it != links.end())
        {
            links.erase(it);
        }
    }

    int Link::process(jack_nframes_t nframes)
    {
        return 0;
    }

    int Global::process(jack_nframes_t nframes)
    {
        for(const auto &link: m_Links)
        {
            link->process(nframes);
        }
        return 0;
    }

}