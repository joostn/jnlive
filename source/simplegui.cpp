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
        for(auto &child : m_Children)
        {
            child->Paint(cr);
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
        Cairo::TextExtents te;
        cr.get_text_extents(m_Text, te);
        double text_y = 0.0+ (Rectangle().get_height() / 2) + (te.height / 2); // Vertically centered
        cr.move_to(0, text_y);
        cr.show_text(m_Text);
    }
}