#include "widgets.hpp"
#include <cstdint>
#include <wx/osx/core/colour.h>

widgets::Circle::Circle(wxWindow* parent, const wxColour color) : wxPanel(parent), color(color) {
    Bind(wxEVT_PAINT, &Circle::OnPaint, this);
}

widgets::Circle::~Circle() {

}

void widgets::Circle::OnPaint(wxPaintEvent& event) {
    wxPaintDC dc(this);

    wxSize size = GetClientSize();

    const int radius = std::min(size.x, size.y) / 4;
    const wxPoint center(size.x / 2, size.y / 2);

    dc.SetPen(wxPen(this->color, 2));
    dc.SetBrush(wxBrush(this->color));
    dc.DrawCircle(center, radius);
}
