#include "simplegui.h"
#include <gtkmm.h>
#include <cairo.h>

namespace simplegui
{
    class Window
    {
    public:
        Window(Window *m_Parent, const Gdk::Rectangle &rect) : m_Parent(m_Parent), m_Rectangle(rect)
        {
        }
        void Paint(Cairo::Context &cr)
        {
            DoPaint(cr);
            for(auto &child : m_Children)
            {
                child->Paint(cr);
            }
        }
        template <class TChild, class... TArgs>
        TChild *AddChild(TArgs &&...args)
        {
            auto child = std::make_unique<TChild>(this, std::forward<TArgs>(args)...);
            auto result = child.get();
            m_Children.push_back(std::move(child));
            return result;
        }
        

    protected:
        void DoPaint(Cairo::Context &cr) = 0;

    private:
        Gdk::Rectangle m_Rectangle;
        std::vector<std::unique_ptr<Window>> m_Children;
        Window *m_Parent = nullptr;
    };
}