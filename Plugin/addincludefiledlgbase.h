//////////////////////////////////////////////////////////////////////
// This file was auto-generated by codelite's wxCrafter Plugin
// wxCrafter project file: AddIncludeFile.wxcp
// Do not modify this file by hand!
//////////////////////////////////////////////////////////////////////

#ifndef _CODELITE_PLUGIN_ADDINCLUDEFILE_BASE_CLASSES_H
#define _CODELITE_PLUGIN_ADDINCLUDEFILE_BASE_CLASSES_H

// clang-format off
#include <wx/settings.h>
#include <wx/xrc/xmlres.h>
#include <wx/xrc/xh_bmp.h>
#include <wx/dialog.h>
#include <wx/iconbndl.h>
#include <wx/artprov.h>
#include <wx/sizer.h>
#include <wx/toolbar.h>
#include "clToolBar.h"
#include <wx/panel.h>
#include <wx/stc/stc.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#if wxVERSION_NUMBER >= 2900
#include <wx/persist.h>
#include <wx/persist/toplevel.h>
#include <wx/persist/bookctrl.h>
#include <wx/persist/treebook.h>
#endif

#ifdef WXC_FROM_DIP
#undef WXC_FROM_DIP
#endif
#if wxVERSION_NUMBER >= 3100
#define WXC_FROM_DIP(x) wxWindow::FromDIP(x, NULL)
#else
#define WXC_FROM_DIP(x) x
#endif

// clang-format on

class AddIncludeFileDlgBase : public wxDialog
{
protected:
    clToolBar* m_toolbar;
    wxPanel* m_mainPanel;
    wxStyledTextCtrl* m_textCtrlPreview;
    wxStaticText* m_staticText21;
    wxStaticText* m_staticTextPreview;
    wxStaticText* m_staticText5;
    wxTextCtrl* m_textCtrlFullPath;
    wxStdDialogButtonSizer* m_stdBtnSizer23;
    wxButton* m_button25;
    wxButton* m_button27;

protected:
    virtual void OnButtonOK(wxCommandEvent& event) { event.Skip(); }

public:
    clToolBar* GetToolbar() { return m_toolbar; }
    wxStyledTextCtrl* GetTextCtrlPreview() { return m_textCtrlPreview; }
    wxStaticText* GetStaticText21() { return m_staticText21; }
    wxStaticText* GetStaticTextPreview() { return m_staticTextPreview; }
    wxStaticText* GetStaticText5() { return m_staticText5; }
    wxTextCtrl* GetTextCtrlFullPath() { return m_textCtrlFullPath; }
    wxPanel* GetMainPanel() { return m_mainPanel; }
    AddIncludeFileDlgBase(wxWindow* parent, wxWindowID id = wxID_ANY,
                          const wxString& title = _("Add a missing header file"),
                          const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize(-1, -1),
                          long style = wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
    virtual ~AddIncludeFileDlgBase();
};

#endif