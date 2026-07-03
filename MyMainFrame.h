#pragma once
#include "ui/ui.h"
#include <wx/choice.h>
#include "utils.h"
#include "MyAboutDialog.h"
#include "save_editor.h"
#include <memory>

class MyMainFrame : public MainFrame
{
    private:
    uintptr_t modBase;
    HANDLE hProc;
    MyAboutDialog* myAboutDialog;
    wxChoice* saveSlotChoice;
    wxButton* refreshSaveBtn;
    wxButton* addInvitationBtn;
    wxStaticText* saveStatusText;

    public:
    void SetModBase(uintptr_t modBase){this->modBase = modBase;}
    uintptr_t GetModBase(){return this->modBase;}
    void SetHProc(HANDLE hProc){this->hProc = hProc;}
    HANDLE GetHProc(){return this->hProc;}
    virtual inline void OnDetect( wxCommandEvent& event ){Detect();}
    virtual void OnChange( wxCommandEvent& event );
    virtual void OnAbout( wxCommandEvent& event );
    virtual void OnTextEnter( wxCommandEvent& event );
    void OnAddInvitation( wxCommandEvent& event );
    void OnRefreshSaves( wxCommandEvent& event );
    void OnTriggerFestival( wxCommandEvent& event );
    void RefreshSaveList();
    MyMainFrame();

    void Detect();
    void Change();
};
