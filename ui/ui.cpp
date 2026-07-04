///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.0.0-0-g0efcecf)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "ui.h"

///////////////////////////////////////////////////////////////////////////

MainFrame::MainFrame( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style, const wxString& name ) : wxFrame( parent, id, title, pos, size, style, name )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );
	this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_HIGHLIGHTTEXT ) );

	wxBoxSizer* bSizer1;
	bSizer1 = new wxBoxSizer( wxVERTICAL );

	supportText = new wxStaticText( this, wxID_ANY, wxT("支持"), wxDefaultPosition, wxDefaultSize, 0 );
	supportText->Wrap( -1 );
	supportText->SetFont( wxFont( 12, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxT("微软雅黑") ) );
	supportText->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_HIGHLIGHT ) );

	bSizer1->Add( supportText, 0, wxALL|wxALIGN_CENTER_HORIZONTAL, 5 );

	DetectedText = new wxStaticText( this, wxID_ANY, wxT("检测进程中..."), wxDefaultPosition, wxDefaultSize, 0 );
	DetectedText->Wrap( -1 );
	bSizer1->Add( DetectedText, 0, wxALL, 5 );

	DetectingButton = new wxButton( this, wxID_ANY, wxT("检测进程"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer1->Add( DetectingButton, 0, wxALL, 5 );


	bSizer1->Add( 0, 0, 1, wxEXPAND, 5 );

	m_staticText14 = new wxStaticText( this, wxID_ANY, wxT("金钱"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText14->Wrap( -1 );
	bSizer1->Add( m_staticText14, 0, wxALL, 5 );

	wxGridSizer* gSizer1;
	gSizer1 = new wxGridSizer( 0, 3, 0, 0 );

	moneyCtrl = new wxSpinCtrl( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( 250,-1 ), wxSP_ARROW_KEYS, 0, 1000000000, 0 );
	gSizer1->Add( moneyCtrl, 0, wxALL, 5 );


	gSizer1->Add( 0, 0, 1, wxEXPAND, 5 );

	ChangeButton = new wxButton( this, wxID_ANY, wxT("修改"), wxDefaultPosition, wxDefaultSize, 0 );
	gSizer1->Add( ChangeButton, 0, wxALL|wxALIGN_RIGHT, 5 );


	bSizer1->Add( gSizer1, 1, wxEXPAND, 5 );


	bSizer1->Add( 0, 0, 1, wxEXPAND, 5 );

	AboutButton = new wxButton( this, wxID_ANY, wxT("关于"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer1->Add( AboutButton, 0, wxALL, 5 );

	m_textCtrl1 = new wxTextCtrl( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( 0,0 ), 0 );
	m_textCtrl1->Hide();

	bSizer1->Add( m_textCtrl1, 0, wxALL, 5 );


	this->SetSizer( bSizer1 );
	this->Layout();

	this->Centre( wxBOTH );

	// Connect Events
	DetectingButton->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainFrame::OnDetect ), NULL, this );
	moneyCtrl->Connect( wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler( MainFrame::OnTextEnter ), NULL, this );
	ChangeButton->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainFrame::OnChange ), NULL, this );
	AboutButton->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainFrame::OnAbout ), NULL, this );
}

MainFrame::~MainFrame()
{
	// Disconnect Events
	DetectingButton->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainFrame::OnDetect ), NULL, this );
	moneyCtrl->Disconnect( wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler( MainFrame::OnTextEnter ), NULL, this );
	ChangeButton->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainFrame::OnChange ), NULL, this );
	AboutButton->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainFrame::OnAbout ), NULL, this );

}

AboutDialog::AboutDialog( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );

	wxBoxSizer* bSizer2;
	bSizer2 = new wxBoxSizer( wxVERTICAL );

	m_staticText3 = new wxStaticText( this, wxID_ANY, wxT("东方夜雀食堂："), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText3->Wrap( -1 );
	bSizer2->Add( m_staticText3, 0, wxALL, 5 );

	m_hyperlink2 = new wxHyperlinkCtrl( this, wxID_ANY, wxEmptyString, wxT("https://store.steampowered.com/app/1584090/__Touhou_Mystias_Izakaya/"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	bSizer2->Add( m_hyperlink2, 0, wxALL, 5 );


	bSizer2->Add( 0, 0, 1, wxEXPAND, 5 );

	m_staticText5 = new wxStaticText( this, wxID_ANY, wxT("本修改器由usfese制作。wuyulbw维护"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText5->Wrap( -1 );
	bSizer2->Add( m_staticText5, 0, wxALL, 5 );

	wxFlexGridSizer* fgSizer1;
	fgSizer1 = new wxFlexGridSizer( 0, 2, 0, 0 );
	fgSizer1->SetFlexibleDirection( wxBOTH );
	fgSizer1->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

	m_staticText7 = new wxStaticText( this, wxID_ANY, wxT("项目地址："), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText7->Wrap( -1 );
	fgSizer1->Add( m_staticText7, 0, wxALL, 5 );

	m_hyperlink3 = new wxHyperlinkCtrl( this, wxID_ANY, wxT("Github"), wxT("https://github.com/usfese/IzakayaCheater"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	fgSizer1->Add( m_hyperlink3, 0, wxALL, 5 );

	m_staticText10 = new wxStaticText( this, wxID_ANY, wxT("软件版本："), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText10->Wrap( -1 );
	fgSizer1->Add( m_staticText10, 0, wxALL, 5 );

	versionText = new wxStaticText( this, wxID_ANY, wxT("0"), wxDefaultPosition, wxDefaultSize, 0 );
	versionText->Wrap( -1 );
	fgSizer1->Add( versionText, 0, wxALL, 5 );

	m_staticText11 = new wxStaticText( this, wxID_ANY, wxT("支持游戏版本："), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText11->Wrap( -1 );
	fgSizer1->Add( m_staticText11, 0, wxALL, 5 );

	supportedGameVersionText = new wxStaticText( this, wxID_ANY, wxT("0"), wxDefaultPosition, wxDefaultSize, 0 );
	supportedGameVersionText->Wrap( -1 );
	fgSizer1->Add( supportedGameVersionText, 0, wxALL, 5 );


	bSizer2->Add( fgSizer1, 1, wxEXPAND, 5 );


	this->SetSizer( bSizer2 );
	this->Layout();
	bSizer2->Fit( this );

	this->Centre( wxBOTH );
}

AboutDialog::~AboutDialog()
{
}
