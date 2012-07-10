#ifndef _WX_MSGCATALOG_H
#define _WX_MSGCATALOG_H

#include <wx/ptr_scpd.h>

class wxPluralFormsToken
{
public:
    enum Type
    {
        T_ERROR, T_EOF, T_NUMBER, T_N, T_PLURAL, T_NPLURALS, T_EQUAL, T_ASSIGN,
        T_GREATER, T_GREATER_OR_EQUAL, T_LESS, T_LESS_OR_EQUAL,
        T_REMINDER, T_NOT_EQUAL,
        T_LOGICAL_AND, T_LOGICAL_OR, T_QUESTION, T_COLON, T_SEMICOLON,
        T_LEFT_BRACKET, T_RIGHT_BRACKET
    };
    Type type() const { return m_type; }
    void setType(Type type) { m_type = type; }
    // for T_NUMBER only
    typedef int Number;
    Number number() const { return m_number; }
    void setNumber(Number num) { m_number = num; }
private:
    Type m_type;
    Number m_number;
};

class wxPluralFormsNode;

// NB: Can't use wxDEFINE_SCOPED_PTR_TYPE because wxPluralFormsNode is not
//     fully defined yet:
class wxPluralFormsNodePtr
{
public:
    wxPluralFormsNodePtr(wxPluralFormsNode *p = NULL) : m_p(p) {}
    ~wxPluralFormsNodePtr();
    wxPluralFormsNode& operator*() const { return *m_p; }
    wxPluralFormsNode* operator->() const { return m_p; }
    wxPluralFormsNode* get() const { return m_p; }
    wxPluralFormsNode* release();
    void reset(wxPluralFormsNode *p);

private:
    wxPluralFormsNode *m_p;
};

class wxPluralFormsCalculator
{
public:
    wxPluralFormsCalculator() : m_nplurals(0), m_plural(0) {}

    // input: number, returns msgstr index
    int evaluate(int n) const;

    // input: text after "Plural-Forms:" (e.g. "nplurals=2; plural=(n != 1);"),
    // if s == 0, creates default handler
    // returns 0 if error
    static wxPluralFormsCalculator* make(const char* s = 0);

    ~wxPluralFormsCalculator() {}

    void  init(wxPluralFormsToken::Number nplurals, wxPluralFormsNode* plural);

private:
    wxPluralFormsToken::Number m_nplurals;
    wxPluralFormsNodePtr m_plural;
};

wxDECLARE_SCOPED_PTR(wxPluralFormsCalculator, wxPluralFormsCalculatorPtr);

WX_DECLARE_EXPORTED_STRING_HASH_MAP(wxString, wxMessagesHash);


class wxMsgCatalog
{
public:
#if !wxUSE_UNICODE
    wxMsgCatalog() { m_conv = NULL; }
    ~wxMsgCatalog();
#endif

    // load the catalog from disk (szDirPrefix corresponds to language)
    bool Load(const wxChar *szDirPrefix, const wxChar *szName,
              const wxChar *msgIdCharset = NULL, bool bConvertEncoding = false);

    // get name of the catalog
    wxString GetName() const { return m_name; }

    // get the translated string: returns NULL if not found
    const wxChar *GetString(const wxChar *sz, size_t n = size_t(-1)) const;

    // public variable pointing to the next element in a linked list (or NULL)
    wxMsgCatalog *m_pNext;

private:
    wxMessagesHash  m_messages; // all messages in the catalog
    wxString        m_name;     // name of the domain

#if !wxUSE_UNICODE
    // the conversion corresponding to this catalog charset if we installed it
    // as the global one
    wxCSConv *m_conv;
#endif

    wxPluralFormsCalculatorPtr  m_pluralFormsCalculator;
};


#endif
