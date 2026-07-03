///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.0.0-0-g0efcecf)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#pragma once

#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
#include <wx/string.h>
#include <wx/stattext.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/button.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/spinctrl.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>
#include <wx/frame.h>
#include <wx/hyperlink.h>
#include <wx/dialog.h>

///////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
/// Class MainFrame
///////////////////////////////////////////////////////////////////////////////
class MainFrame : public wxFrame
{
	private:

	protected:
		wxStaticText* supportText;
		wxStaticText* DetectedText;
		wxButton* DetectingButton;
		wxStaticText* m_staticText14;
		wxSpinCtrl* moneyCtrl;
		wxButton* ChangeButton;
		wxButton* AboutButton;
		wxTextCtrl* m_textCtrl1;

		// Virtual event handlers, override them in your derived class
		virtual void OnDetect( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnTextEnter( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnChange( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnAbout( wxCommandEvent& event ) { event.Skip(); }


	public:

		MainFrame( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxT("东方夜雀食堂金钱修改器"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 362,500 ), long style = wxDEFAULT_FRAME_STYLE|wxTAB_TRAVERSAL, const wxString& name = wxT("夜雀食堂金钱修改器") );

		~MainFrame();

};

///////////////////////////////////////////////////////////////////////////////
/// Class AboutDialog
///////////////////////////////////////////////////////////////////////////////
class AboutDialog : public wxDialog
{
	private:

	protected:
		wxStaticText* m_staticText3;
		wxHyperlinkCtrl* m_hyperlink2;
		wxStaticText* m_staticText5;
		wxStaticText* m_staticText7;
		wxHyperlinkCtrl* m_hyperlink3;
		wxStaticText* m_staticText10;
		wxStaticText* versionText;
		wxStaticText* m_staticText11;
		wxStaticText* supportedGameVersionText;

	public:

		AboutDialog( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxT("关于"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxDEFAULT_DIALOG_STYLE|wxSTAY_ON_TOP );

		~AboutDialog();

};

