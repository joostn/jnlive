#pragma once

#include "simplegui.h"
#include <gdkmm.h>
#include <memory>
#include <cairomm/cairomm.h>

namespace simplegui
{
    class Rgba
    {
    private:
        double m_Red = 0.0;
        double m_Green = 0.0;
        double m_Blue = 0.0;
        double m_Alpha = 1.0;
    public:
        Rgba() = default;
        Rgba(double red, double green, double blue, double alpha = 1.0) : m_Red(red), m_Green(green), m_Blue(blue), m_Alpha(alpha)
        {
        }
        double Red() const
        {
            return m_Red;
        }
        double Green() const
        {
            return m_Green;
        }
        double Blue() const
        {
            return m_Blue;
        }
        double Alpha() const
        {
            return m_Alpha;
        }
        bool operator==(const Rgba &other) const 
        {
            return m_Red == other.m_Red && m_Green == other.m_Green && m_Blue == other.m_Blue && m_Alpha == other.m_Alpha;
        }
    };
    class Window
    {
    public:
        Window(Window *m_Parent, const Gdk::Rectangle &rect) : m_Parent(m_Parent), m_Rectangle(rect)
        {
        }
        void Paint(Cairo::Context &cr) const;
        template <class TChild, class... TArgs>
        TChild *AddChild(TArgs &&...args)
        {
            auto child = std::make_unique<TChild>(this, std::forward<TArgs>(args)...);
            auto result = child.get();
            m_Children.push_back(std::move(child));
            return result;
        }
        const Gdk::Rectangle &Rectangle() const
        {
            return m_Rectangle;
        }
        int Width() const
        {
            return m_Rectangle.get_width();
        }
        int Height() const
        {
            return m_Rectangle.get_height();
        }
        bool Equals(const Window *other) const
        {
            return DoGetEquals(other);
        }
        void GetUpdateRegion(const Window *other, Cairo::Region &region, int x_offset, int y_offset) const;
        const std::vector<std::unique_ptr<Window>> &Children() const
        {
            return m_Children;
        }

    protected:
        virtual void DoPaint(Cairo::Context &cr) const
        {
            // transparent
        }
        virtual bool DoGetEquals(const Window *other) const
        {
            // any descendant class should override this, but also call the parent class
            if(m_Rectangle.get_x() != other->m_Rectangle.get_x()) return false;
            if(m_Rectangle.get_y() != other->m_Rectangle.get_y()) return false;
            if(m_Rectangle.get_width() != other->m_Rectangle.get_width()) return false;
            if(m_Rectangle.get_height() != other->m_Rectangle.get_height()) return false;
            return true;
        }

    private:
        Gdk::Rectangle m_Rectangle;
        std::vector<std::unique_ptr<Window>> m_Children;
        Window *m_Parent = nullptr;
    };

    class PlainWindow : public Window
    {
    public:
        PlainWindow(Window *parent, const Gdk::Rectangle &rect, const Rgba &color) : Window(parent, rect), m_Color(color)
        {
        }

    protected:
        virtual bool DoGetEquals(const Window *other) const override
        {
            auto otherOurs = dynamic_cast<const decltype(this)>(other);
            if (!otherOurs) return false;
            if(!Window::DoGetEquals(other)) return false;
            bool samecolor = m_Color == otherOurs->m_Color;
            if(samecolor)
            {
                return true;
            }
            else
            {
                return false;
            }
        }

    private:
        void DoPaint(Cairo::Context &cr) const override;

    private:
        Rgba m_Color;
    };

    class TextWindow : public Window
    {
    public:
        enum class THalign { Left, Center, Right };
        TextWindow(Window *parent, const Gdk::Rectangle &rect, std::string_view text, const Rgba &color, int fontsize, THalign halign) : Window(parent, rect), m_Text(text), m_Color(color), m_FontSize(fontsize), m_Halign(halign)
        {
        }

    protected:
        virtual bool DoGetEquals(const Window *other) const override
        {
            auto otherOurs = dynamic_cast<const decltype(this)>(other);
            if (!otherOurs) return false;
            if(!Window::DoGetEquals(other)) return false;
            if(m_Color != otherOurs->m_Color) return false;
            if(m_FontSize != otherOurs->m_FontSize) return false;
            if(m_Halign != otherOurs->m_Halign) return false;
            if(m_Text != otherOurs->m_Text) return false;
            return true;
        }

    private:
        void DoPaint(Cairo::Context &cr) const override;

    private:
        std::string m_Text;
        Rgba m_Color;
        int m_FontSize;
        THalign m_Halign;
    };

    class TSlider : public PlainWindow
    {
    public:
        TSlider(Window *parent, const Gdk::Rectangle &rect, std::string_view text, double value, const Rgba &color);
    protected:
        virtual bool DoGetEquals(const Window *other) const override
        {
            auto otherOurs = dynamic_cast<const decltype(this)>(other);
            if (!otherOurs) return false;
            if(!PlainWindow::DoGetEquals(other)) return false;
            // this window will have children, those will be checked separately
            return true;
        }
    };

    class TListBox : public PlainWindow
    {
    public:
        TListBox(Window *parent, const Gdk::Rectangle &rect, const Rgba &color, int rowheight, size_t numitems, std::optional<size_t> selecteditem, size_t centereditem, const std::function<std::string(size_t)> &itemtextgetter);

    protected:
        virtual bool DoGetEquals(const Window *other) const override
        {
            auto otherOurs = dynamic_cast<const decltype(this)>(other);
            if (!otherOurs) return false;
            if(!PlainWindow::DoGetEquals(other)) return false;
            // this window will have children, those will be checked separately
            return true;
        }
    };

    class TTriangle : public Window
    {
    public:
        enum class TDirection {Up, Down, Left, Right};
        TTriangle(Window *parent, const Gdk::Rectangle &rect, const Rgba &color, TDirection direction) : Window(parent, rect), m_Direction(direction), m_Color(color)
        {
        }
        void DoPaint(Cairo::Context &cr) const override;

    protected:
        virtual bool DoGetEquals(const Window *other) const override
        {
            auto otherOurs = dynamic_cast<const decltype(this)>(other);
            if (!otherOurs) return false;
            if(!Window::DoGetEquals(other)) return false;
            if(m_Direction != otherOurs->m_Direction) return false;
            if(m_Color != otherOurs->m_Color) return false;
            return true;
        }
    private:
        TDirection m_Direction;
        Rgba m_Color;
    };
}
