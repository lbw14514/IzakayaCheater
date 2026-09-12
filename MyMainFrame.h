#pragma once
#include "ui/ui.h"
#include <wx/choice.h>
#include "MyAboutDialog.h"
#include "save_editor.h"

class MyMainFrame : public MainFrame
{
    private:
    MyAboutDialog* myAboutDialog;
    wxChoice* saveSlotChoice;
    wxButton* refreshSaveBtn;
    wxTextCtrl* saveMoneyCtrl;
    wxStaticText* saveStatusText;
    wxChoice* bossChoice;
    wxButton* bossQueueBtn;
    wxButton* bossClearBtn;
    wxButton* bossInviteBtn;
    wxButton* mapUnlockBtn;
    wxButton* bondsMaxBtn;
    wxStaticText* bossDescText;

    public:
    virtual void OnAbout( wxCommandEvent& event );
    void OnRefreshSaves( wxCommandEvent& event );
    void OnSaveMoney( wxCommandEvent& event );
    void OnQueueBoss( wxCommandEvent& event );
    void OnMarkBossCleared( wxCommandEvent& event );
    void OnAddInvitationForBoss( wxCommandEvent& event );
    void OnUnlockMaps( wxCommandEvent& event );
    void OnMaxAllBonds( wxCommandEvent& event );
    void OnBossChanged( wxCommandEvent& event );
    void RefreshSaveList();
    void RefreshBossList();
    void UpdateBossButtons();
    void HideLegacySpinButton();
    void SetSaveResult(int ret);
    MyMainFrame();
};
