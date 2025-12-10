#include "../frames.hpp"
#include <wx/osx/frame.h>
#include "../../main.hpp"

using AdminMenuFrame = frames::AdminMenuFrame;

AdminMenuFrame::AdminMenuFrame() : wxFrame(wxGetApp().GetHomeFrame(), wxID_ANY, "Admin Menu", wxDefaultPosition, wxSize(800, 600)) {

}

AdminMenuFrame::~AdminMenuFrame() {

}
