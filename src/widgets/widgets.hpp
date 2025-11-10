#pragma once

#include <functional>
#include <wx/event.h>
#include <wx/wx.h>

namespace widgets {
    class Button : public wxControl {
    public:
        Button(wxWindow* parent, const wxString& label, std::function<void(wxMouseEvent&)> on_click);
        ~Button();

        void OnPaint(wxPaintEvent&);

    private:
        wxString label;

        bool hover = false;
    };

    class MenuBarButton : public wxControl {
    public:
        MenuBarButton(wxWindow* parent, const wxString& label, const uint32_t index, uint32_t* const current_index, std::function<void()> click_callback);
        ~MenuBarButton();

        void OnPaint(wxPaintEvent&);
        void OnClick(wxMouseEvent&);

    private:
        uint32_t* current_index;

        uint32_t index;

        wxString label;

        std::function<void()> click_callback;
        
        bool hover = false;
    };

    class MenuBar : public wxPanel {
    public:
        MenuBar(wxPanel* parent_panel, std::initializer_list<const char*> items, const wxOrientation orientation, std::function<void()> click_callback);
        ~MenuBar();

        uint32_t GetCurrentIndex();
    private:
        uint32_t current_index = 0;
    };

    class StyledPanel : public wxPanel {
    public:
        StyledPanel(wxWindow* parent);
        ~StyledPanel();
        
        void OnPaint(wxPaintEvent&);
    private:
    };
}
