#include "simplegui.h"

namespace simplegui
{
    Cairo::RectangleInt gdkRect2CairoRect(const Gdk::Rectangle &rect)
    {
        return Cairo::RectangleInt(rect.get_x(), rect.get_y(), rect.get_width(), rect.get_height());
    }

    void Window::GetUpdateRegion(const Window *other, Cairo::Region &region, int x_offset, int y_offset) const
    {
        if( (!other) || (!Equals(other)) )
        {
            {
                Cairo::RectangleInt rect {
                    .x = x_offset + Rectangle().get_x(),
                    .y = y_offset + Rectangle().get_y(),
                    .width = Rectangle().get_width(),
                    .height = Rectangle().get_height()
                };
                region.do_union(rect);
            }
            if(other)
            {
                Cairo::RectangleInt otherrect {
                    .x = x_offset + other->Rectangle().get_x(),
                    .y = y_offset + other->Rectangle().get_y(),
                    .width = other->Rectangle().get_width(),
                    .height = other->Rectangle().get_height()
                };
                region.do_union(otherrect);
            }
        }
        else
        {
            int child_x_offset = x_offset + Rectangle().get_x();
            int child_y_offset = y_offset + Rectangle().get_y();
            std::vector<Window*> ourchildren;
            std::vector<Window*> otherchildren;
            for(auto &child : Children())
            {
                ourchildren.push_back(child.get());
            }
            for(auto &child : other->Children())
            {
                otherchildren.push_back(child.get());
            }
            // clear identical children:
            auto ourit = ourchildren.begin();
            auto otherit = otherchildren.begin();
        again1:
            if(ourit != ourchildren.end() && otherit != otherchildren.end())
            {
                if((*ourit)->Equals(*otherit))
                {
                foundmatch:
                    // add the child's update region:
                    (*ourit)->GetUpdateRegion(*otherit, region, child_x_offset, child_y_offset);
                    *ourit = nullptr;  // exclude the pair from further checks
                    *otherit = nullptr;
                    ourit++;
                    otherit++;
                    goto again1;
                }
                else
                {
                    // try to match ourit with otherit+n:
                    (*ourit)->Equals(*otherit);
                    auto otherit2 = otherit;
                    otherit2++;
                    while(otherit2 != otherchildren.end())
                    {
                        if((*ourit)->Equals(*otherit2))
                        {
                            // so a new child was inserted. Skip it for now
                            otherit = otherit2;
                            goto foundmatch;
                        }
                        otherit2++;
                    }
                    auto ourit2 = ourit;
                    ourit2++;
                    while(ourit2 != ourchildren.end())
                    {
                        if((*ourit2)->Equals(*otherit))
                        {
                            // a child was deleted. Skip it for now:
                            ourit = ourit2;
                            goto foundmatch;
                        }
                        ourit2++;
                    }
                    // no further matches found.
                }
            }
            // add all new and deleted windows entirely to the update region:
            for(const auto &vec: {ourchildren, otherchildren})
            {
                for(const auto &win: vec)
                {
                    if(win)
                    {
                        win->GetUpdateRegion(nullptr, region, child_x_offset, child_y_offset);
                    }

                }
            }
        }
    }

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
        Window::DoPaint(cr);
        cr.set_source_rgba(m_Color.Red(), m_Color.Green(), m_Color.Blue(), m_Color.Alpha());
        // fill entire context:
        cr.paint();
    }
    
    void TextWindow::DoPaint(Cairo::Context &cr) const
    {
        Window::DoPaint(cr);
        cr.set_source_rgba(m_Color.Red(), m_Color.Green(), m_Color.Blue(), m_Color.Alpha());
        cr.select_font_face("Sans", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_BOLD);
        cr.set_font_size(m_FontSize);

        // measure text
        Cairo::FontExtents fe;
        cr.get_font_extents(fe);
        Cairo::TextExtents te;
        cr.get_text_extents(m_Text, te);
        double textheight = fe.ascent + fe.descent;
        double textwidth = te.width;
        double text_x = 0;
        if(m_Halign == THalign::Right)
        {
            text_x = std::floor(0.0 + Rectangle().get_width() - textwidth);
        }
        else if(m_Halign == THalign::Center)
        {
            text_x = std::round(0.0 + (Rectangle().get_width() - textwidth) / 2);
        }

        double text_y = std::round(0.0+ (Rectangle().get_height() / 2) + (textheight / 2.0) - fe.descent); // Vertically centered

        cr.move_to(text_x, text_y);
        cr.show_text(m_Text);
    }

    TSlider::TSlider(Window *parent, const Gdk::Rectangle &rect, std::string_view text, double value, const utils::TFloatColor &color) : PlainWindow(parent, rect, color)
    {
        auto black = utils::TFloatColor(0,0,0);
        auto inner = AddChild<PlainWindow>(Gdk::Rectangle(1,1,rect.get_width()-2, rect.get_height()-2), black);
        auto fontsize = std::max(10, inner->Rectangle().get_height() - 4);
        inner->AddChild<TextWindow>(Gdk::Rectangle(1,1,inner->Rectangle().get_width()-2, inner->Rectangle().get_height()-2), text, color, fontsize, TextWindow::THalign::Left);
        int sliderwidth = (int)std::lround(std::clamp(value, 0.0, 1.0) * inner->Rectangle().get_width());
        auto slider = AddChild<PlainWindow>(Gdk::Rectangle(1,1,sliderwidth, rect.get_height()-2), color);
        slider->AddChild<TextWindow>(Gdk::Rectangle(1,1,sliderwidth-1, inner->Rectangle().get_height()-2), text, black, fontsize, TextWindow::THalign::Left);
    }

    TListBox::TListBox(Window *parent, const Gdk::Rectangle &rect, const utils::TFloatColor &color, int rowheight, size_t numitems, std::optional<size_t> selecteditem, size_t centereditem, const std::function<std::string(size_t)> &itemtextgetter) : PlainWindow(parent, rect, color)
    {
        int fontsize = rowheight - 6;
        auto black = utils::TFloatColor(0,0,0);
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
                p->AddChild<TextWindow>(Gdk::Rectangle(2,0,p->Rectangle().get_width()-2, p->Rectangle().get_height()), itemtextgetter(itemindex), selected? black : color, fontsize, TextWindow::THalign::Left);
            }
        }
    }

    void TTriangle::DoPaint(Cairo::Context &cr) const
    {
        Window::DoPaint(cr);
        cr.set_source_rgba(m_Color.Red(), m_Color.Green(), m_Color.Blue(), m_Color.Alpha());
        int width = Rectangle().get_width();
        int height = Rectangle().get_height();
        if(m_Direction == TDirection::Up)
        {
            cr.move_to(width/2, 0);
            cr.line_to(width, height);
            cr.line_to(0, height);
        }
        else if(m_Direction == TDirection::Right)
        {
            cr.move_to(width, height/2);
            cr.line_to(0, height);
            cr.line_to(0, 0);
        }
        else if(m_Direction == TDirection::Left)
        {
            cr.move_to(0, height/2);
            cr.line_to(width, 0);
            cr.line_to(width, height);
        }
        else
        {
            cr.move_to(width/2, height);
            cr.line_to(0, 0);
            cr.line_to(width, 0);
        }
        cr.close_path();
        cr.fill();
    }
}