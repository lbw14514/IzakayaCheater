#include "MyMainFrame.h"
#include <wx/wx.h>

class MyApp : public wxApp
{
    public:
    virtual bool OnInit();
};

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit()
{
    MyMainFrame* frame = new MyMainFrame();
    frame->Show();
    return true;
}