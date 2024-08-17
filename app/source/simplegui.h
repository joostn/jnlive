#pragma once

#include "utils.h"
#include <gdkmm.h>
#include <memory>
#include <cairomm/cairomm.h>

namespace simplegui
{
    class Window
    {
    public:
        Window(Window *m_Parent, const utils::TIntRect &rect) : m_Parent(m_Parent), m_Rectangle(rect)
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
        const utils::TIntRect &Rectangle() const
        {
            return m_Rectangle;
        }
        auto Size() const
        {
            return Rectangle().Size();
        }
        bool Equals(const Window *other) const
        {
            // we need to check both ways, because the other window might be a descendant of this one
            if(!DoGetEquals(other)) return false;
            if(!other->DoGetEquals(this)) return false;
            return true;
        }
        void GetUpdateRegion(const Window *other, utils::TIntRegion &region, const utils::TIntPoint &offset, const utils::TIntPoint &otheroffset) const;
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
            if(Rectangle() != other->Rectangle()) return false;
            return true;
        }
        virtual void DoGetUpdateRegionExcludingChildren(const Window &other, utils::TIntRegion &region, const utils::TIntPoint &offset, const utils::TIntPoint &otheroffset) const;

    private:
        utils::TIntRect m_Rectangle;
        std::vector<std::unique_ptr<Window>> m_Children;
        Window *m_Parent = nullptr;
    };

    class PlainWindow : public Window
    {
    public:
        PlainWindow(Window *parent, const utils::TIntRect &rect, const utils::TFloatColor &color) : Window(parent, rect), m_Color(color)
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
        virtual void DoGetUpdateRegionExcludingChildren(const Window &other, utils::TIntRegion &region, const utils::TIntPoint &offset, const utils::TIntPoint &otheroffset) const override;

    private:
        void DoPaint(Cairo::Context &cr) const override;

    private:
        utils::TFloatColor m_Color;
    };

    class TextWindow : public Window
    {
    public:
        enum class THalign { Left, Center, Right };
        TextWindow(Window *parent, const utils::TIntRect &rect, std::string_view text, const utils::TFloatColor &color, int fontsize, THalign halign);

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
        virtual void DoGetUpdateRegionExcludingChildren(const Window &other, utils::TIntRegion &region, const utils::TIntPoint &offset, const utils::TIntPoint &otheroffset) const override;

    private:
        void DoPaint(Cairo::Context &cr) const override;
        void SetFont(Cairo::Context &cr) const;

    private:
        std::string m_Text;
        utils::TFloatColor m_Color;
        int m_FontSize;
        THalign m_Halign;
        utils::TDoublePoint m_TextDrawPos;
        utils::TIntRect m_TextClipRect;
    };

    class TSlider : public PlainWindow
    {
    public:
        TSlider(Window *parent, const utils::TIntRect &rect, std::string_view text, double value, const utils::TFloatColor &color);
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
        TListBox(Window *parent, const utils::TIntRect &rect, const utils::TFloatColor &color, int rowheight, size_t numitems, std::optional<size_t> selecteditem, size_t centereditem, const std::function<std::string(size_t)> &itemtextgetter);

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
        TTriangle(Window *parent, const utils::TIntRect &rect, const utils::TFloatColor &color, TDirection direction) : Window(parent, rect), m_Direction(direction), m_Color(color)
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
        utils::TFloatColor m_Color;
    };
}
