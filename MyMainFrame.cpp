#include "MyMainFrame.h"
#include "utils.h"
#include <wx/msgdlg.h>
#include "config.h"

MyMainFrame::MyMainFrame() : MainFrame(NULL, -1)
{
    SetTitle(_T("东方夜雀食堂金钱修改器") + wxString::FromUTF8(VERSION));
    this->supportText->SetLabel(_T("支持夜雀食堂") + wxString::FromUTF8(SUPPORTED_VERSION));

    wxBoxSizer* sizer = (wxBoxSizer*)GetSizer();

    sizer->Add(0, 0, 0, wxEXPAND, 5);
    sizer->Add(0, 0, 0, wxEXPAND, 5);

    wxStaticText* sep = new wxStaticText(this, wxID_ANY, _T("-- 存档修改 --"));
    sep->SetFont(wxFont(10, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
    sizer->Add(sep, 0, wxALL|wxALIGN_CENTER_HORIZONTAL, 5);

    wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
    row->Add(new wxStaticText(this, wxID_ANY, _T("选择存档")), 0, wxALL|wxALIGN_CENTER_VERTICAL, 5);
    saveSlotChoice = new wxChoice(this, wxID_ANY);
    saveSlotChoice->SetMinSize(wxSize(120,-1));
    row->Add(saveSlotChoice, 0, wxALL, 5);
    refreshSaveBtn = new wxButton(this, wxID_ANY, _T("刷新"));
    row->Add(refreshSaveBtn, 0, wxALL, 5);
    sizer->Add(row, 0, wxALIGN_CENTER_HORIZONTAL, 5);

    wxBoxSizer* row2 = new wxBoxSizer(wxHORIZONTAL);
    addInvitationBtn = new wxButton(this, wxID_ANY, _T("添加邀请函到存档"));
    row2->Add(addInvitationBtn, 0, wxALL, 5);
    sizer->Add(row2, 0, wxALIGN_CENTER_HORIZONTAL, 5);

    saveStatusText = new wxStaticText(this, wxID_ANY, wxEmptyString);
    saveStatusText->SetForegroundColour(wxColour(0,128,0));
    sizer->Add(saveStatusText, 0, wxALL|wxALIGN_CENTER_HORIZONTAL, 5);

    addInvitationBtn->Bind(wxEVT_BUTTON, &MyMainFrame::OnAddInvitation, this);
    refreshSaveBtn->Bind(wxEVT_BUTTON, &MyMainFrame::OnRefreshSaves, this);

    wxBoxSizer* row3 = new wxBoxSizer(wxHORIZONTAL);
    wxButton* festivalBtn = new wxButton(this, wxID_ANY, _T("触发博丽大祭"));
    row3->Add(festivalBtn, 0, wxALL, 5);
    sizer->Add(row3, 0, wxALIGN_CENTER_HORIZONTAL, 5);
    festivalBtn->Bind(wxEVT_BUTTON, &MyMainFrame::OnTriggerFestival, this);

    RefreshSaveList();
    Layout();
}

void MyMainFrame::OnChange( wxCommandEvent& event )
{
    Change();
}

void MyMainFrame::OnAbout( wxCommandEvent& event )
{
    this->myAboutDialog = new MyAboutDialog(this);
    this->myAboutDialog->ShowModal();
}

void MyMainFrame::Detect()
{
    IzakayaResult result = GetIzakayaProcess();
    if(result.code != IzakayaCode::SUCCESS)
    {
        wxString msg;
        switch(result.code)
        {
            case IzakayaCode::PROCESS_NOT_FOUND :
                msg = _T("未找到进程！");
                break;
            case IzakayaCode::BASE_ADDRESS_NOT_FOUND :
                msg = _T("找不到模块基址！");
                break;
            case IzakayaCode::CANNOT_ATTACH_TO_PROCESS :
                msg = _T("无法附加到进程！");
                break;
        }
        wxMessageBox(msg);
        this->DetectedText->SetLabel(_T("未找到进程！"));
        return;
    }
    this->SetModBase(result.modBase);
    this->SetHProc(result.hProc);
    this->DetectedText->SetLabel(_T("已附加到进程"));
    DWORD money = ReadMoney(result.modBase, hProc);
    this->moneyCtrl->SetValue((int)money);
}

void MyMainFrame::OnRefreshSaves( wxCommandEvent& event )
{
    RefreshSaveList();
}

void MyMainFrame::RefreshSaveList()
{
    saveSlotChoice->Clear();
    int slots[16];
    int count = 0;
    if (SaveEditor_ScanSaves(slots, &count, 16) == 0 && count > 0)
    {
        for (int i = 0; i < count; i++)
        {
            wxString label = wxString::Format(_T("Mystia#%d.memory"), slots[i]);
            int idx = saveSlotChoice->Append(label);
            saveSlotChoice->SetClientData(idx, (void*)(intptr_t)slots[i]);
        }
        saveSlotChoice->SetSelection(0);
        saveStatusText->SetLabel(wxString::Format(_T("找到 %d 个存档"), count));
    }
    else
    {
        saveStatusText->SetLabel(_T("未找到存档!"));
    }
}

void MyMainFrame::OnTextEnter(wxCommandEvent& event)
{
    Change();
}

void MyMainFrame::Change(){
    if(!GetModBase() || !GetHProc())
    {
        wxMessageBox(_T("写入失败！"));
        return;
    }
    ChangeMoney(GetModBase(), GetHProc(), moneyCtrl->GetValue());
    wxMessageBox(_T("已写入！"));
}

void MyMainFrame::OnTriggerFestival( wxCommandEvent& event )
{
    int sel = saveSlotChoice->GetSelection();
    if (sel == wxNOT_FOUND) { saveStatusText->SetLabel(_T("请先选择存档!")); return; }
    int answer = wxMessageBox(_T("可能跳过部分剧情导致不完整，是否继续？"), _T("确认"), wxYES_NO | wxICON_QUESTION);
    if (answer != wxYES) { saveStatusText->SetLabel(_T("已取消")); return; }
    int slot = (int)(intptr_t)saveSlotChoice->GetClientData(sel);
    int ret = SaveEditor_TriggerFestivalSlot(slot);
    switch(ret)
    {
        case 0:
            saveStatusText->SetLabel(_T("完成"));
            break;
        case -1:
            saveStatusText->SetLabel(_T("存档文件未找到!"));
            break;
        case -3:
            saveStatusText->SetLabel(_T("无法解析存档!"));
            break;
        case -4:
            saveStatusText->SetLabel(_T("写入失败!"));
            break;
        default:
            saveStatusText->SetLabel(wxString::Format(_T("错误代码: %d"), ret));
    }
}

void MyMainFrame::OnAddInvitation( wxCommandEvent& event )
{
    int sel = saveSlotChoice->GetSelection();
    if (sel == wxNOT_FOUND) { saveStatusText->SetLabel(_T("请先选择存档!")); return; }
    int slot = (int)(intptr_t)saveSlotChoice->GetClientData(sel);
    int ret = SaveEditor_AddInvitationsToSlot(slot);
    if (ret == 0)
    {

        saveStatusText->SetLabel(_T("完成"));
    }
    else
    {
        switch(ret)
        {
            case -1: saveStatusText->SetLabel(_T("存档文件未找到!")); break;
            case -3: saveStatusText->SetLabel(_T("无法解析存档!")); break;
            case -4: saveStatusText->SetLabel(_T("写入失败!")); break;
            default: saveStatusText->SetLabel(wxString::Format(_T("错误代码: %d"), ret));
        }
    }
}
