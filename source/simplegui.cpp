#include "simplegui.h"

namespace simplegui
{

    void Window::Paint(Cairo::Context &cr) const
    {
        // cr represents the original, unclipped cairo context.
        // translate and clip to m_Rectangle:
        cr.save();
        // create clipping path:
        cr.rectangle(m_Rectangle.get_x(), m_Rectangle.get_y(), m_Rectangle.get_width(), m_Rectangle.get_height());
        cr.clip();
        // translate to m_Rectangle:
        cr.translate(m_Rectangle.get_x(), m_Rectangle.get_y());
        DoPaint(cr);
        for(auto it = m_Children.begin(); it != m_Children.end(); ++it)
        {
            (*it)->Paint(cr);
        }
        cr.restore();
    }

    void PlainWindow::DoPaint(Cairo::Context &cr) const
    {
        cr.set_source_rgba(m_Color.Red(), m_Color.Green(), m_Color.Blue(), m_Color.Alpha());
        // fill entire context:
        cr.paint();
    }
    
    void TextWindow::DoPaint(Cairo::Context &cr) const
    {
        cr.set_source_rgba(m_Color.Red(), m_Color.Green(), m_Color.Blue(), m_Color.Alpha());
        cr.select_font_face("Sans", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_BOLD);
        cr.set_font_size(m_FontSize);

        // measure text
        // Cairo::TextExtents te;
        // cr.get_text_extents(m_Text, te);
        // double text_y = 0.0+ (Rectangle().get_height() / 2) + (te.height / 2); // Vertically centered
        double textheight = m_FontSize + 2;

        double text_y = std::round(0.0+ (Rectangle().get_height() / 2) + (textheight / 2.0)); // Vertically centered

        cr.move_to(0, text_y);
        cr.show_text(m_Text);
    }

    TSlider::TSlider(Window *parent, const Gdk::Rectangle &rect, std::string_view text, double value, const Rgba &color) : PlainWindow(parent, rect, color)
    {
        auto black = Rgba(0,0,0);
        auto inner = AddChild<PlainWindow>(Gdk::Rectangle(1,1,rect.get_width()-2, rect.get_height()-2), black);
        auto fontsize = std::max(10, inner->Rectangle().get_height() - 4);
        inner->AddChild<TextWindow>(Gdk::Rectangle(1,1,inner->Rectangle().get_width()-2, inner->Rectangle().get_height()-2), text, color, fontsize);
        int sliderwidth = (int)std::lround(std::clamp(value, 0.0, 1.0) * inner->Rectangle().get_width());
        auto slider = AddChild<PlainWindow>(Gdk::Rectangle(1,1,sliderwidth, rect.get_height()-2), color);
        slider->AddChild<TextWindow>(Gdk::Rectangle(1,1,sliderwidth-1, inner->Rectangle().get_height()-2), text, black, fontsize);
    }

    TListBox::TListBox(Window *parent, const Gdk::Rectangle &rect, const Rgba &color, int rowheight, size_t numitems, std::optional<size_t> selecteditem, size_t centereditem, const std::function<std::string(size_t)> &itemtextgetter) : PlainWindow(parent, rect, color)
    {
        int fontsize = rowheight - 6;
        auto black = Rgba(0,0,0);
        auto backgroundbox = AddChild<PlainWindow>(Gdk::Rectangle(1,1,rect.get_width()-2, rect.get_height()-2), black);
        int innerheight = backgroundbox->Rectangle().get_height();
        if(numitems > 0)
        {
            centereditem = std::clamp(centereditem, size_t(0), numitems-1);
            int centereditemtop = (innerheight - rowheight) / 2;
            {
                int dy = rowheight * int((numitems - centereditem));
                int lastitembottom = centereditemtop + dy;
                lastitembottom = std::max(innerheight, lastitembottom);
                centereditemtop = lastitembottom - dy;
            }
            {
                int dy = rowheight * int(centereditem);
                int firstitemtop = centereditemtop - dy;
                firstitemtop = std::min(0, firstitemtop);
                centereditemtop = firstitemtop + dy;
            }
            int firstitemtop = centereditemtop - rowheight * int(centereditem);
            int firstvisibleitem = std::clamp(- firstitemtop / rowheight - 1, 0, (int)numitems-1);
            int lastvisibleitem = std::clamp((innerheight - firstitemtop)/rowheight, 0, (int)numitems);
            for(size_t itemindex = (size_t)firstvisibleitem; itemindex < (size_t)lastvisibleitem; ++itemindex)
            {
                bool selected = selecteditem == itemindex;
                auto label = itemtextgetter(itemindex);
                auto top = firstitemtop + rowheight * int(itemindex);
                Gdk::Rectangle rowrect(0, top, backgroundbox->Rectangle().get_width(), rowheight);
                Window *p = nullptr;
                if(selected)
                {
                    p = backgroundbox->AddChild<PlainWindow>(rowrect, color);
                }
                else
                {
                    p = backgroundbox->AddChild<Window>(rowrect);
                }
                p->AddChild<TextWindow>(Gdk::Rectangle(2,0,p->Rectangle().get_width()-2, p->Rectangle().get_height()), itemtextgetter(itemindex), selected? black : color, fontsize);
            }
        }
    }

}