#include "MyMainFrame.h"
#include <wx/msgdlg.h>
#include "config.h"

static const int MAX_SAVE_SLOTS = 16;

static BOOL CALLBACK HideSpinButton(HWND hwnd, LPARAM lParam)
{
    TCHAR cls[64] = {0};
    if (GetClassName(hwnd, cls, 64) && lstrcmp(cls, _T("msctls_updown32")) == 0)
        DestroyWindow(hwnd);
    return TRUE;
}

MyMainFrame::MyMainFrame() : MainFrame(NULL, -1), myAboutDialog(NULL)
{
    SetTitle(_T("东方夜雀食堂修改器"));
    this->supportText->SetLabel(_T("支持夜雀食堂") + wxString::FromUTF8(SUPPORTED_VERSION));

    this->DetectedText->Hide();
    this->DetectingButton->Hide();
    this->m_staticText14->Hide();
    this->moneyCtrl->Hide();
    if (wxSizer* moneySpinOwner = this->moneyCtrl->GetContainingSizer())
        moneySpinOwner->Detach(this->moneyCtrl);
    this->ChangeButton->Hide();
    this->m_textCtrl1->Hide();

    wxBoxSizer* sizer = (wxBoxSizer*)GetSizer();

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

    saveStatusText = new wxStaticText(this, wxID_ANY, wxEmptyString);
    saveStatusText->SetForegroundColour(wxColour(0,128,0));
    sizer->Add(saveStatusText, 0, wxALL|wxALIGN_CENTER_HORIZONTAL, 2);

    refreshSaveBtn->Bind(wxEVT_BUTTON, &MyMainFrame::OnRefreshSaves, this);

    // Add money via save button
    wxBoxSizer* row4 = new wxBoxSizer(wxHORIZONTAL);
    row4->Add(new wxStaticText(this, wxID_ANY, _T("金钱")), 0, wxALL|wxALIGN_CENTER_VERTICAL, 5);
    saveMoneyCtrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(100,-1));
    row4->Add(saveMoneyCtrl, 0, wxALL, 5);
    wxButton* saveMoneyBtn = new wxButton(this, wxID_ANY, _T("写入存档"));
    row4->Add(saveMoneyBtn, 0, wxALL, 5);
    sizer->Add(row4, 0, wxALIGN_CENTER_HORIZONTAL, 5);
    saveMoneyBtn->Bind(wxEVT_BUTTON, &MyMainFrame::OnSaveMoney, this);

    wxBoxSizer* rowMap = new wxBoxSizer(wxHORIZONTAL);
    mapUnlockBtn = new wxButton(this, wxID_ANY, _T("解锁全部地图（本体 + DLC1~5）"));
    bondsMaxBtn = new wxButton(this, wxID_ANY, _T("全部满好感"));
    rowMap->Add(mapUnlockBtn, 0, wxALL, 5);
    rowMap->Add(bondsMaxBtn, 0, wxALL, 5);
    sizer->Add(rowMap, 0, wxALIGN_CENTER_HORIZONTAL, 5);
    mapUnlockBtn->Bind(wxEVT_BUTTON, &MyMainFrame::OnUnlockMaps, this);
    bondsMaxBtn->Bind(wxEVT_BUTTON, &MyMainFrame::OnMaxAllBonds, this);

    wxBoxSizer* row5 = new wxBoxSizer(wxHORIZONTAL);
    row5->Add(new wxStaticText(this, wxID_ANY, _T("Boss 战")), 0, wxALL|wxALIGN_CENTER_VERTICAL, 5);
    bossChoice = new wxChoice(this, wxID_ANY);
    bossChoice->SetMinSize(wxSize(380,-1));
    row5->Add(bossChoice, 1, wxALL, 5);
    sizer->Add(row5, 0, wxALIGN_CENTER_HORIZONTAL, 5);

    wxBoxSizer* row6 = new wxBoxSizer(wxHORIZONTAL);
    bossQueueBtn = new wxButton(this, wxID_ANY, _T("方案A：排队事件（已停用）"));
    bossClearBtn = new wxButton(this, wxID_ANY, _T("方案B：标记通关（写 finishedEvents）"));
    bossInviteBtn = new wxButton(this, wxID_ANY, _T("方案C：添加邀请函（2014~2019）"));
    row6->Add(bossQueueBtn, 0, wxALL, 5);
    row6->Add(bossClearBtn, 0, wxALL, 5);
    row6->Add(bossInviteBtn, 0, wxALL, 5);
    sizer->Add(row6, 0, wxALIGN_CENTER_HORIZONTAL, 5);
    bossQueueBtn->Bind(wxEVT_BUTTON, &MyMainFrame::OnQueueBoss, this);
    bossClearBtn->Bind(wxEVT_BUTTON, &MyMainFrame::OnMarkBossCleared, this);
    bossInviteBtn->Bind(wxEVT_BUTTON, &MyMainFrame::OnAddInvitationForBoss, this);
    bossChoice->Bind(wxEVT_CHOICE, &MyMainFrame::OnBossChanged, this);

    wxBoxSizer* row7 = new wxBoxSizer(wxHORIZONTAL);
    bossDescText = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(680, 78), wxST_NO_AUTORESIZE);
    row7->Add(bossDescText, 1, wxALL, 5);
    sizer->Add(row7, 0, wxALIGN_CENTER_HORIZONTAL, 5);

    RefreshBossList();
    RefreshSaveList();
    // Hide the original AboutButton from base class and add new one at bottom
    this->AboutButton->Hide();
    wxButton* aboutBtn = new wxButton(this, wxID_ANY, _T("关于"));
    sizer->Add(aboutBtn, 0, wxALL, 5);
    aboutBtn->Bind(wxEVT_BUTTON, &MyMainFrame::OnAbout, this);
    Layout();
    Fit();
    SetMinSize(GetSize());
    CallAfter(&MyMainFrame::HideLegacySpinButton);
}

void MyMainFrame::HideLegacySpinButton()
{
    EnumChildWindows((HWND)GetHWND(), HideSpinButton, 0);
}

void MyMainFrame::OnAbout( wxCommandEvent& event )
{
    if (!this->myAboutDialog) {
        this->myAboutDialog = new MyAboutDialog(this);
    }
    this->myAboutDialog->ShowModal();
}

void MyMainFrame::OnRefreshSaves( wxCommandEvent& event )
{
    RefreshSaveList();
}

void MyMainFrame::RefreshSaveList()
{
    saveSlotChoice->Clear();
    int slots[MAX_SAVE_SLOTS];
    int count = 0;
    if (SaveEditor_ScanSaves(slots, &count, MAX_SAVE_SLOTS) == 0 && count > 0)
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

// Maps a save_editor error code to the status label, shared by every handler.
void MyMainFrame::SetSaveResult(int ret)
{
    switch(ret)
    {
        case 0:
            saveStatusText->SetLabel(_T("完成"));
            break;
        case 1:
            saveStatusText->SetLabel(_T("完成（已为该存档激活对应 DLC）"));
            break;
        case -1:
            saveStatusText->SetLabel(_T("存档文件未找到!"));
            break;
        case -2:
            saveStatusText->SetLabel(_T("内存不足!"));
            break;
        case -3:
            saveStatusText->SetLabel(_T("无法解析存档!"));
            break;
        case -4:
            saveStatusText->SetLabel(_T("写入失败!"));
            break;
        case -5:
            saveStatusText->SetLabel(_T("该存档没有激活对应 DLC!"));
            break;
        case -6:
            saveStatusText->SetLabel(_T("该 Boss 无方案A事件（本体终战无法用存档触发）!"));
            break;
        case -7:
            saveStatusText->SetLabel(_T("存档结构不支持该操作!"));
            break;
        default:
            saveStatusText->SetLabel(wxString::Format(_T("错误代码: %d"), ret));
    }
}

void MyMainFrame::OnSaveMoney( wxCommandEvent& event )
{
    int sel = saveSlotChoice->GetSelection();
    if (sel == wxNOT_FOUND) { saveStatusText->SetLabel(_T("请先选择存档!")); return; }
    int slot = (int)(intptr_t)saveSlotChoice->GetClientData(sel);
    unsigned long value;
    if (!saveMoneyCtrl->GetValue().ToULong(&value)) {
        saveStatusText->SetLabel(_T("请输入有效数字!"));
        return;
    }
    char path[MAX_PATH];
    if (SaveEditor_GetPath(slot, path, sizeof(path))) { saveStatusText->SetLabel(_T("路径错误!")); return; }
    int ret = SaveEditor_SetFund(path, (int)value);
    SetSaveResult(ret);
}

void MyMainFrame::OnAddInvitationForBoss( wxCommandEvent& event )
{
    int sel = saveSlotChoice->GetSelection();
    if (sel == wxNOT_FOUND) { saveStatusText->SetLabel(_T("请先选择存档!")); return; }
    int boss = bossChoice->GetSelection();
    if (boss == wxNOT_FOUND) { saveStatusText->SetLabel(_T("请先选择 Boss!")); return; }
    int answer = wxMessageBox(_T("方案C会给该存档添加邀请函（物品 2014~2019），是否继续？"), _T("确认"), wxYES_NO | wxICON_QUESTION);
    if (answer != wxYES) { saveStatusText->SetLabel(_T("已取消")); return; }
    int slot = (int)(intptr_t)saveSlotChoice->GetClientData(sel);
    SetSaveResult(SaveEditor_AddInvitationsToSlot(slot));
}

void MyMainFrame::OnUnlockMaps( wxCommandEvent& event )
{
    int sel = saveSlotChoice->GetSelection();
    if (sel == wxNOT_FOUND) { saveStatusText->SetLabel(_T("请先选择存档!")); return; }
    int slot = (int)(intptr_t)saveSlotChoice->GetClientData(sel);
    int answer = wxMessageBox(_T("会为该存档解锁全部地图（本体 + DLC1~5）并写入 DLC 激活项，是否继续？"), _T("确认"), wxYES_NO | wxICON_QUESTION);
    if (answer != wxYES) return;
    char path[MAX_PATH];
    if (SaveEditor_GetPath(slot, path, sizeof(path)) != 0) { saveStatusText->SetLabel(_T("存档路径获取失败!")); return; }
    int ret = SaveEditor_UnlockAllMaps(path);
    if (ret >= 0)
        saveStatusText->SetLabel(wxString::Format(_T("完成（新解锁 %d / %d 张地图）"), ret, SaveEditor_GetMapCount()));
    else
        SetSaveResult(ret);
}

void MyMainFrame::OnMaxAllBonds( wxCommandEvent& event )
{
    int sel = saveSlotChoice->GetSelection();
    if (sel == wxNOT_FOUND) { saveStatusText->SetLabel(_T("请先选择存档!")); return; }
    int slot = (int)(intptr_t)saveSlotChoice->GetClientData(sel);
    int answer = wxMessageBox(_T("会把这个存档里所有角色的好感改满（等级 5），是否继续？"), _T("确认"), wxYES_NO | wxICON_QUESTION);
    if (answer != wxYES) return;
    char path[MAX_PATH];
    if (SaveEditor_GetPath(slot, path, sizeof(path)) != 0) { saveStatusText->SetLabel(_T("存档路径获取失败!")); return; }
    int ret = SaveEditor_MaxAllBonds(path);
    if (ret >= 0)
        saveStatusText->SetLabel(wxString::Format(_T("完成（%d 个角色好感已满）"), ret));
    else if (ret == -8)
        saveStatusText->SetLabel(_T("该存档里没有好感数据!"));
    else
        SetSaveResult(ret);
}

void MyMainFrame::RefreshBossList()
{
    bossChoice->Clear();
    int count = SaveEditor_GetBossCount();
    for (int i = 0; i < count; i++)
    {
        bossChoice->Append(wxString::FromUTF8(SaveEditor_GetBossLabel(i)));
    }
    if (count > 0) bossChoice->SetSelection(0);
    UpdateBossButtons();
}

void MyMainFrame::UpdateBossButtons()
{
    int boss = bossChoice->GetSelection();
    bool valid = (boss != wxNOT_FOUND);
    bossQueueBtn->Enable(valid && SaveEditor_BossHasQueue(boss) != 0);
    bossClearBtn->Enable(valid && SaveEditor_BossHasClear(boss) != 0);
    bossInviteBtn->Enable(valid && SaveEditor_BossHasInvite(boss) != 0);
    bossDescText->SetLabel(wxString::FromUTF8(SaveEditor_GetBossDesc(boss)));
    bossDescText->Wrap(660);
}

void MyMainFrame::OnBossChanged( wxCommandEvent& event )
{
    UpdateBossButtons();
}

void MyMainFrame::OnQueueBoss( wxCommandEvent& event )
{
    int sel = saveSlotChoice->GetSelection();
    if (sel == wxNOT_FOUND) { saveStatusText->SetLabel(_T("请先选择存档!")); return; }
    int boss = bossChoice->GetSelection();
    if (boss == wxNOT_FOUND) { saveStatusText->SetLabel(_T("请先选择 Boss!")); return; }
    int slot = (int)(intptr_t)saveSlotChoice->GetClientData(sel);
    char path[MAX_PATH];
    if (SaveEditor_GetPath(slot, path, sizeof(path))) { saveStatusText->SetLabel(_T("路径错误!")); return; }
    SetSaveResult(SaveEditor_QueueBossEvents(path, boss));
}

void MyMainFrame::OnMarkBossCleared( wxCommandEvent& event )
{
    int sel = saveSlotChoice->GetSelection();
    if (sel == wxNOT_FOUND) { saveStatusText->SetLabel(_T("请先选择存档!")); return; }
    int boss = bossChoice->GetSelection();
    if (boss == wxNOT_FOUND) { saveStatusText->SetLabel(_T("请先选择 Boss!")); return; }
    int answer = wxMessageBox(_T("方案B会把该 Boss 标记为已通关以解锁再战，是否继续？"), _T("确认"), wxYES_NO | wxICON_QUESTION);
    if (answer != wxYES) { saveStatusText->SetLabel(_T("已取消")); return; }
    int slot = (int)(intptr_t)saveSlotChoice->GetClientData(sel);
    char path[MAX_PATH];
    if (SaveEditor_GetPath(slot, path, sizeof(path))) { saveStatusText->SetLabel(_T("路径错误!")); return; }
    SetSaveResult(SaveEditor_SetBossCleared(path, boss));
}
