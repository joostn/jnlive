#include "simplegui.h"

namespace simplegui
{
    Cairo::RectangleInt toCairoRect(const utils::TIntRect &rect)
    {
        return Cairo::RectangleInt(rect.Left(), rect.Top(), rect.Width(), rect.Height());
    }

    void Window::DoGetUpdateRegionExcludingChildren(const Window &other, utils::TIntRegion &region, const utils::TIntPoint &offset, const utils::TIntPoint &otheroffset) const
    {
        if(!Equals(&other))
        {
            region = region.Union(Rectangle() + offset);
            region = region.Union(other.Rectangle() + otheroffset);
        }
    }

    void Window::GetUpdateRegion(const Window *other, utils::TIntRegion &region, const utils::TIntPoint &offset, const utils::TIntPoint &otheroffset) const
    {
        if( (!other) || (!Equals(other)) )
        {
            if(other)
            {
                // if(other->Rectangle().TopLeft() != Rectangle().TopLeft())
                // {
                //     region = region.Union(Rectangle() + offset);
                //     region = region.Union(other->Rectangle() + offset);
                //     return;
                // }
                // else
                // {
                    DoGetUpdateRegionExcludingChildren(*other, region, offset, otheroffset);
                // }
            }
            else
            {
                region = region.Union(Rectangle() + offset);
                return;
            }
        }
        nhAssert(other);
        // nhAssert(other->Rectangle().TopLeft() == Rectangle().TopLeft());

        {
            auto childoffset = offset + Rectangle().TopLeft();
            auto otherchildoffset = otheroffset + other->Rectangle().TopLeft();
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
            bool samesize = ourchildren.size() == otherchildren.size();
        again1:
            if(ourit != ourchildren.end() && otherit != otherchildren.end())
            {
                if(samesize || (*ourit)->Equals(*otherit))
                {
                    // if both lists of children have the same size, assume no insertions or deletions have taken place. We then match each child in order to find the update region. This allows less pessimistic update regions because same children are probably matched.
                    // if the lists have different sizes, use 'Equals' to skip children that are not the same. This allows less pessimistic update regions when children are inserted or deleted by matching each child against its (equal) counterpart, leaving the inserted or deleted children for later.
                    // If we have combinations of inserted and deleted children or updated children, we will probably get a pessimistic estimate but that is acceptable.
                foundmatch:
                    // add the child's update region:
                    (*ourit)->GetUpdateRegion(*otherit, region, childoffset, otherchildoffset);
                    *ourit = nullptr;  // exclude the pair from further checks
                    *otherit = nullptr;
                    ourit++;
                    otherit++;
                    goto again1;
                }
                else
                {
                    // try to match ourit with otherit+n:
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
            for(const auto &win: ourchildren)
            {
                if(win)
                {
                    region = region.Union(win->Rectangle() + childoffset);
                }
            }
            for(const auto &win: otherchildren)
            {
                if(win)
                {
                    region = region.Union(win->Rectangle() + otherchildoffset);
                }
            }
        }
    }

    void Window::Paint(Cairo::Context &cr) const
    {
        double clipleft, cliptop, clipright, clipbottom;
        cr.get_clip_extents(clipleft, cliptop, clipright, clipbottom);
        auto clipbounds = utils::TDoubleRect::FromTopLeftAndBottomRight({clipleft, cliptop}, {clipright, clipbottom}).ToRectOuterPixels();
        if(!clipbounds.Intersects(Rectangle()))
        {
            return;
        }
        // cr represents the original, unclipped cairo context.
        // translate and clip to m_Rectangle:
        cr.save();
        // create clipping path:
        cr.rectangle(m_Rectangle.Left(), m_Rectangle.Top(), m_Rectangle.Width(), m_Rectangle.Height());
        cr.set_fill_rule(Cairo::FILL_RULE_WINDING); // default
        cr.clip();
        // translate to m_Rectangle:
        cr.translate(m_Rectangle.Left(), m_Rectangle.Top());
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
    void PlainWindow::DoGetUpdateRegionExcludingChildren(const Window &other, utils::TIntRegion &region, const utils::TIntPoint &offset, const utils::TIntPoint &otheroffset) const
    {
        if(auto otherplainwindow = dynamic_cast<const PlainWindow*>(&other); otherplainwindow)
        {
            if(m_Color == otherplainwindow->m_Color)
            {
                // we only need to redraw the non-overlapping parts:
                utils::TIntRegion r1(Rectangle() + offset);
                utils::TIntRegion r2(otherplainwindow->Rectangle() + otheroffset);
                auto inverseintersection = r1.Union(r2).Subtract(r1.Intersection(r2));
                region = region.Union(inverseintersection);
                return;
            }
        }
        // fallback:
        Window::DoGetUpdateRegionExcludingChildren(other, region, offset, otheroffset);
    }

    TextWindow::TextWindow(Window *parent, const utils::TIntRect &rect, std::string_view text, const utils::TFloatColor &color, int fontsize, THalign halign) : Window(parent, rect), m_Text(text), m_Color(color), m_FontSize(fontsize), m_Halign(halign)
    {
        auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, 0, 0);
        auto cr = Cairo::Context::create(surface);
        SetFont(*(cr.operator->()));

        // measure text
        Cairo::FontExtents fe;
        cr->get_font_extents(fe);
        Cairo::TextExtents te;
        cr->get_text_extents(m_Text, te);
        double textheight = fe.ascent + fe.descent;
        double textwidth = te.width;
        double text_x = 0;
        if(m_Halign == THalign::Right)
        {
            text_x = std::floor(0.0 + Rectangle().Width() - textwidth);
        }
        else if(m_Halign == THalign::Center)
        {
            text_x = std::round(0.0 + (Rectangle().Width() - textwidth) / 2);
        }

        double text_y = std::round(0.0+ (Rectangle().Height() / 2) + (textheight / 2.0) - fe.descent); // Vertically centered
        auto textcliprect_d = utils::TDoubleRect::FromTopLeftAndSize({text_x, Rectangle().Height() / 2.0 - textheight/2}, {textwidth, textheight}).SymmetricalExpand({2.0});
        m_TextClipRect = textcliprect_d.ToRectOuterPixels().Intersection(utils::TIntRect::FromSize(Rectangle().Size()));
        m_TextDrawPos = {text_x, text_y};
    }

    void TextWindow::SetFont(Cairo::Context &cr) const
    {
        cr.select_font_face("Sans", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_BOLD);
        cr.set_font_size(m_FontSize);
    }

    void TextWindow::DoGetUpdateRegionExcludingChildren(const Window &other, utils::TIntRegion &region, const utils::TIntPoint &offset, const utils::TIntPoint &otheroffset) const
    {
        if(auto othertextwindow = dynamic_cast<const TextWindow*>(&other); othertextwindow)
        {
            if(this->Equals(&other) && (offset == otheroffset))
            {
                return;
            }
            // only the actual painted region is dirty:
            region = region.Union(m_TextClipRect + offset + Rectangle().TopLeft());
            region = region.Union(othertextwindow->m_TextClipRect + otheroffset + othertextwindow->Rectangle().TopLeft());
            return;
        }
        // fallback:
        Window::DoGetUpdateRegionExcludingChildren(other, region, offset, otheroffset);
    }

    void TextWindow::DoPaint(Cairo::Context &cr) const
    {
        Window::DoPaint(cr);
        cr.set_source_rgba(m_Color.Red(), m_Color.Green(), m_Color.Blue(), m_Color.Alpha());
        SetFont(cr);
        // clip to m_TextClipRect. Not necessary if m_TextClipRect is correct, because it is based on the measured text size. But it will immediately show the problem if the measured text size is incorrect.
        cr.save();
        cr.rectangle(m_TextClipRect.Left(), m_TextClipRect.Top(), m_TextClipRect.Width(), m_TextClipRect.Height());
        cr.clip();
        cr.move_to(m_TextDrawPos.X(), m_TextDrawPos.Y());
        cr.show_text(m_Text);
        cr.restore();
    }

    TSlider::TSlider(Window *parent, const utils::TIntRect &rect, std::string_view text, double value, const utils::TFloatColor &color) : PlainWindow(parent, rect, color)
    {
        auto black = utils::TFloatColor(0,0,0);
        auto inner = AddChild<PlainWindow>(utils::TIntRect::FromSize(rect.Size()).SymmetricalExpand({-1}), black);
        auto fontsize = std::max(10, inner->Rectangle().Height() - 4);
        ;
        inner->AddChild<TextWindow>(utils::TIntRect::FromSize(inner->Rectangle().Size()).SymmetricalExpand({-1}), text, color, fontsize, TextWindow::THalign::Left);
        int sliderwidth = (int)std::lround(std::clamp(value, 0.0, 1.0) * inner->Rectangle().Width());
        auto slider = AddChild<PlainWindow>(utils::TIntRect::FromTopLeftAndSize({1,1}, {sliderwidth, rect.Height()-2}), color);
        slider->AddChild<TextWindow>(utils::TIntRect::FromTopLeftAndSize({1,1}, {sliderwidth-1, inner->Rectangle().Height()-2}), text, black, fontsize, TextWindow::THalign::Left);
    }

    TListBox::TListBox(Window *parent, const utils::TIntRect &rect, const utils::TFloatColor &color, int rowheight, size_t numitems, std::optional<size_t> selecteditem, size_t centereditem, const std::function<std::string(size_t)> &itemtextgetter) : PlainWindow(parent, rect, color)
    {
        int fontsize = rowheight - 6;
        auto black = utils::TFloatColor(0,0,0);
        auto backgroundbox = AddChild<PlainWindow>(utils::TIntRect::FromSize(rect.Size()).SymmetricalExpand(-1), black);
        int innerheight = backgroundbox->Rectangle().Height();
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
                auto rowrect = utils::TIntRect::FromTopLeftAndSize({0, top}, {backgroundbox->Rectangle().Width(), rowheight});
                Window *p = nullptr;
                if(selected)
                {
                    p = backgroundbox->AddChild<PlainWindow>(rowrect, color);
                }
                else
                {
                    p = backgroundbox->AddChild<Window>(rowrect);
                }
                p->AddChild<TextWindow>(utils::TIntRect::FromTopLeftAndSize({2,0}, {p->Rectangle().Width()-2, p->Rectangle().Height()}), itemtextgetter(itemindex), selected? black : color, fontsize, TextWindow::THalign::Left);
            }
        }
    }

    void TTriangle::DoPaint(Cairo::Context &cr) const
    {
        Window::DoPaint(cr);
        cr.set_source_rgba(m_Color.Red(), m_Color.Green(), m_Color.Blue(), m_Color.Alpha());
        int width = Rectangle().Width();
        int height = Rectangle().Height();
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