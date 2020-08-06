#include "ExtraRenderers.hpp"
#include "wxExtensions.hpp"
#include "GUI.hpp"
#include "I18N.hpp"

#include <wx/dc.h>
#ifdef wxHAS_GENERIC_DATAVIEWCTRL
#include "wx/generic/private/markuptext.h"
#include "wx/generic/private/rowheightcache.h"
#include "wx/generic/private/widthcalc.h"
#endif
#if wxUSE_ACCESSIBILITY
#include "wx/private/markupparser.h"
#endif // wxUSE_ACCESSIBILITY

using Slic3r::GUI::from_u8;
using Slic3r::GUI::into_u8;


//-----------------------------------------------------------------------------
// DataViewBitmapText
//-----------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(DataViewBitmapText, wxObject)

IMPLEMENT_VARIANT_OBJECT(DataViewBitmapText)

// ---------------------------------------------------------
// BitmapTextRenderer
// ---------------------------------------------------------

#if ENABLE_NONCUSTOM_DATA_VIEW_RENDERING
BitmapTextRenderer::BitmapTextRenderer(wxDataViewCellMode mode /*= wxDATAVIEW_CELL_EDITABLE*/, 
                                                 int align /*= wxDVR_DEFAULT_ALIGNMENT*/): 
wxDataViewRenderer(wxT("PrusaDataViewBitmapText"), mode, align)
{
    SetMode(mode);
    SetAlignment(align);
}
#endif // ENABLE_NONCUSTOM_DATA_VIEW_RENDERING

BitmapTextRenderer::~BitmapTextRenderer()
{
#ifdef SUPPORTS_MARKUP
    #ifdef wxHAS_GENERIC_DATAVIEWCTRL
    if (m_markupText)
        delete m_markupText;
    #endif //wxHAS_GENERIC_DATAVIEWCTRL
#endif // SUPPORTS_MARKUP
}

#ifdef SUPPORTS_MARKUP
void BitmapTextRenderer::EnableMarkup(bool enable)
{
#ifdef wxHAS_GENERIC_DATAVIEWCTRL
    if (enable) {
        if (!m_markupText)
            m_markupText = new wxItemMarkupText(wxString());
    }
    else {
        if (m_markupText) {
            delete m_markupText;
            m_markupText = nullptr;
        }
    }
#elseif
    is_markupText = enable
#endif //wxHAS_GENERIC_DATAVIEWCTRL
}
#endif // SUPPORTS_MARKUP

bool BitmapTextRenderer::SetValue(const wxVariant &value)
{
    m_value << value;

#if defined(SUPPORTS_MARKUP) && defined(wxHAS_GENERIC_DATAVIEWCTRL)
    if (m_markupText)
        m_markupText->SetMarkup(m_value.GetText());
#endif // SUPPORTS_MARKUP && wxHAS_GENERIC_DATAVIEWCTRL

    return true;
}

bool BitmapTextRenderer::GetValue(wxVariant& WXUNUSED(value)) const
{
    return false;
}

#if ENABLE_NONCUSTOM_DATA_VIEW_RENDERING && wxUSE_ACCESSIBILITY
wxString BitmapTextRenderer::GetAccessibleDescription() const
{
#ifdef SUPPORTS_MARKUP
    if (m_markupText)
        return wxMarkupParser::Strip(m_text);
#endif // SUPPORTS_MARKUP

    return m_value.GetText();
}
#endif // wxUSE_ACCESSIBILITY && ENABLE_NONCUSTOM_DATA_VIEW_RENDERING

bool BitmapTextRenderer::Render(wxRect rect, wxDC *dc, int state)
{
    int xoffset = 0;

    const wxBitmap& icon = m_value.GetBitmap();
    if (icon.IsOk())
    {
#ifdef __APPLE__
        wxSize icon_sz = icon.GetScaledSize();
#else
        wxSize icon_sz = icon.GetSize();
#endif
        dc->DrawBitmap(icon, rect.x, rect.y + (rect.height - icon_sz.y) / 2);
        xoffset = icon_sz.x + 4;
    }

#if defined(SUPPORTS_MARKUP) && defined(wxHAS_GENERIC_DATAVIEWCTRL)
    if (m_markupText)
    {
        int flags = 0;

        rect.x += xoffset;
        m_markupText->Render(GetView(), *dc, rect, flags, GetEllipsizeMode());
    }
    else
#endif // SUPPORTS_MARKUP && wxHAS_GENERIC_DATAVIEWCTRL
        RenderText(m_value.GetText(), xoffset, rect, dc, state);

    return true;
}

wxSize BitmapTextRenderer::GetSize() const
{
    if (!m_value.GetText().empty())
    {
        wxSize size;
#if defined(SUPPORTS_MARKUP) && defined(wxHAS_GENERIC_DATAVIEWCTRL)
        if (m_markupText)
        {
            wxDataViewCtrl* const view = GetView();
            wxClientDC dc(view);
            if (GetAttr().HasFont())
                dc.SetFont(GetAttr().GetEffectiveFont(view->GetFont()));

            size = m_markupText->Measure(dc);
        }
        else
#endif // SUPPORTS_MARKUP && wxHAS_GENERIC_DATAVIEWCTRL
            size = GetTextExtent(m_value.GetText());

        int lines = m_value.GetText().Freq('\n') + 1;
        size.SetHeight(size.GetHeight() * lines);

        if (m_value.GetBitmap().IsOk())
            size.x += m_value.GetBitmap().GetWidth() + 4;
        return size;
    }
    return wxSize(80, 20);
}


wxWindow* BitmapTextRenderer::CreateEditorCtrl(wxWindow* parent, wxRect labelRect, const wxVariant& value)
{
    if (!can_create_editor_ctrl())
        return nullptr;

    DataViewBitmapText data;
    data << value;

    m_was_unusable_symbol = false;

    wxPoint position = labelRect.GetPosition();
    if (data.GetBitmap().IsOk()) {
        const int bmp_width = data.GetBitmap().GetWidth();
        position.x += bmp_width;
        labelRect.SetWidth(labelRect.GetWidth() - bmp_width);
    }

    wxTextCtrl* text_editor = new wxTextCtrl(parent, wxID_ANY, data.GetText(),
                                             position, labelRect.GetSize(), wxTE_PROCESS_ENTER);
    text_editor->SetInsertionPointEnd();
    text_editor->SelectAll();

    return text_editor;
}

bool BitmapTextRenderer::GetValueFromEditorCtrl(wxWindow* ctrl, wxVariant& value)
{
    wxTextCtrl* text_editor = wxDynamicCast(ctrl, wxTextCtrl);
    if (!text_editor || text_editor->GetValue().IsEmpty())
        return false;

    std::string chosen_name = into_u8(text_editor->GetValue());
    const char* unusable_symbols = "<>:/\\|?*\"";
    for (size_t i = 0; i < std::strlen(unusable_symbols); i++) {
        if (chosen_name.find_first_of(unusable_symbols[i]) != std::string::npos) {
            m_was_unusable_symbol = true;
            return false;
        }
    }

    // The icon can't be edited so get its old value and reuse it.
    wxVariant valueOld;
    GetView()->GetModel()->GetValue(valueOld, m_item, /*colName*/0); 
    
    DataViewBitmapText bmpText;
    bmpText << valueOld;

    // But replace the text with the value entered by user.
    bmpText.SetText(text_editor->GetValue());

    value << bmpText;
    return true;
}

// ----------------------------------------------------------------------------
// BitmapChoiceRenderer
// ----------------------------------------------------------------------------

bool BitmapChoiceRenderer::SetValue(const wxVariant& value)
{
    m_value << value;
    return true;
}

bool BitmapChoiceRenderer::GetValue(wxVariant& value) const 
{
    value << m_value;
    return true;
}

bool BitmapChoiceRenderer::Render(wxRect rect, wxDC* dc, int state)
{
    int xoffset = 0;

    const wxBitmap& icon = m_value.GetBitmap();
    if (icon.IsOk())
    {
        dc->DrawBitmap(icon, rect.x, rect.y + (rect.height - icon.GetHeight()) / 2);
        xoffset = icon.GetWidth() + 4;

        if (rect.height==0)
          rect.height= icon.GetHeight();
    }

    RenderText(m_value.GetText(), xoffset, rect, dc, state);

    return true;
}

wxSize BitmapChoiceRenderer::GetSize() const
{
    wxSize sz = GetTextExtent(m_value.GetText());

    if (m_value.GetBitmap().IsOk())
        sz.x += m_value.GetBitmap().GetWidth() + 4;

    return sz;
}


wxWindow* BitmapChoiceRenderer::CreateEditorCtrl(wxWindow* parent, wxRect labelRect, const wxVariant& value)
{
    if (!can_create_editor_ctrl())
        return nullptr;

    std::vector<wxBitmap*> icons = get_extruder_color_icons();
    if (icons.empty())
        return nullptr;

    DataViewBitmapText data;
    data << value;

    auto c_editor = new wxBitmapComboBox(parent, wxID_ANY, wxEmptyString,
        labelRect.GetTopLeft(), wxSize(labelRect.GetWidth(), -1), 
        0, nullptr , wxCB_READONLY);

    int i=0;
    for (wxBitmap* bmp : icons) {
        if (i==0) {
            c_editor->Append(_L("default"), *bmp);
            ++i;
        }

        c_editor->Append(wxString::Format("%d", i), *bmp);
        ++i;
    }
    c_editor->SetSelection(atoi(data.GetText().c_str()));

    // to avoid event propagation to other sidebar items
    c_editor->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent& evt) {
            evt.StopPropagation();
            // FinishEditing grabs new selection and triggers config update. We better call
            // it explicitly, automatic update on KILL_FOCUS didn't work on Linux.
            this->FinishEditing();
    });

    return c_editor;
}

bool BitmapChoiceRenderer::GetValueFromEditorCtrl(wxWindow* ctrl, wxVariant& value)
{
    wxBitmapComboBox* c = (wxBitmapComboBox*)ctrl;
    int selection = c->GetSelection();
    if (selection < 0)
        return false;
   
    DataViewBitmapText bmpText;

    bmpText.SetText(c->GetString(selection));
    bmpText.SetBitmap(c->GetItemBitmap(selection));

    value << bmpText;
    return true;
}

