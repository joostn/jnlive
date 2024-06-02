#pragma once

#include "simplegui.h"
#include <cairomm/cairomm.h>
#include <gdkmm.h>
#include <memory>

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

    protected:
        virtual void DoPaint(Cairo::Context &cr) const = 0;

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

    private:
        void DoPaint(Cairo::Context &cr) const override;

    private:
        Rgba m_Color;
    };

    class TextWindow : public Window
    {
    public:
        TextWindow(Window *parent, const Gdk::Rectangle &rect, std::string_view text, const Rgba &color, int fontsize) : Window(parent, rect), m_Text(text), m_Color(color), m_FontSize(fontsize)
        {
        }

    private:
        void DoPaint(Cairo::Context &cr) const override;

    private:
        std::string m_Text;
        Rgba m_Color;
        int m_FontSize;
    };
}
