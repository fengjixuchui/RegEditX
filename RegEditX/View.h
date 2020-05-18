// View.h : interface of the CView class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "resource.h"
#include "VirtualListView.h"

struct TreeNodeBase;
struct ITreeOperations;
struct IMainApp;

struct ListItem {
	const CString& GetName() const;
	const CString& GetType() const;

	TreeNodeBase* TreeNode{ nullptr };
	CString ValueName;
	DWORD ValueType, ValueSize = 0;
	LARGE_INTEGER LastWriteTime = { 0 };
	mutable CString Name, Type;
	bool UpDir{ false };
};

class CView : 
	public CWindowImpl<CView, CListViewCtrl>,
	public CVirtualListView<CView> {
public:
	DECLARE_WND_SUPERCLASS(nullptr, CListViewCtrl::GetWndClassName())

	BOOL PreTranslateMessage(MSG* pMsg);
	static PCWSTR GetRegTypeAsString(DWORD type);
	static int GetRegTypeIcon(DWORD type);
	static CString GetKeyDetails(TreeNodeBase*);
	CString GetDataAsString(const ListItem& item) const;
	bool CanDeleteSelected() const;
	bool CanEditValue() const;
	ListItem& GetItem(int index);
	const ListItem& GetItem(int index) const;
	bool IsViewKeys() const;

	void Update(TreeNodeBase* node, bool onlyIfTheSame = false);
	void Init(ITreeOperations*, IMainApp*);
	void GoToItem(ListItem& item);

	CString GetColumnText(HWND hWnd, int row, int column) const;
	int GetRowImage(HWND hWnd, int row) const;
	void DoSort(const SortInfo* si);
	bool IsSortable(int col) const;

	BEGIN_MSG_MAP(CView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_CONTEXTMENU, OnContextMenu)
		//REFLECTED_NOTIFY_CODE_HANDLER(LVN_ODFINDITEM, OnFindItem)
		REFLECTED_NOTIFY_CODE_HANDLER(NM_DBLCLK, OnDoubleClick)
		REFLECTED_NOTIFY_CODE_HANDLER(NM_RETURN, OnReturnKey)
		REFLECTED_NOTIFY_CODE_HANDLER(NM_RCLICK, OnRightClick)
		REFLECTED_NOTIFY_CODE_HANDLER(LVN_BEGINLABELEDIT, OnBeginRename)
		REFLECTED_NOTIFY_CODE_HANDLER(LVN_ENDLABELEDIT, OnEndRename)
		CHAIN_MSG_MAP_ALT(CVirtualListView<CView>, 1)
		DEFAULT_REFLECTION_HANDLER()
	ALT_MSG_MAP(1)
		COMMAND_ID_HANDLER(ID_EDIT_DELETE, OnDelete)
		COMMAND_ID_HANDLER(ID_EDIT_RENAME, OnEditRename)
		COMMAND_ID_HANDLER(ID_EDIT_MODIFYVALUE, OnModifyValue)
	ALT_MSG_MAP(2)
		COMMAND_ID_HANDLER(ID_VIEW_KEYSINLISTVIEW, OnViewKeys)
		COMMAND_ID_HANDLER(ID_NEW_DWORDVALUE, OnNewDwordValue)
		COMMAND_ID_HANDLER(ID_NEW_QWORDVALUE, OnNewQwordValue)
		COMMAND_ID_HANDLER(ID_NEW_STRINGVALUE, OnNewStringValue)
		COMMAND_ID_HANDLER(ID_NEW_MULTIPLESTRINGSVALUE, OnNewMultiStringValue)
		COMMAND_ID_HANDLER(ID_NEW_EXPANDSTRINGVALUE, OnNewExpandStringValue)
		COMMAND_RANGE_HANDLER(ID_VIEW_TYPE_DETAILS, ID_VIEW_TYPE_TILES, OnChangeViewType)
	END_MSG_MAP()

private:
	LRESULT OnFindItem(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnRightClick(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnDoubleClick(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnReturnKey(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDelete(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnEditRename(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnModifyValue(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnBeginRename(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnEndRename(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnNewDwordValue(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnNewQwordValue(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnNewStringValue(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnNewExpandStringValue(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnNewMultiStringValue(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnNewBinaryValue(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnViewKeys(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnChangeViewType(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT HandleNewIntValue(int size);
	LRESULT HandleNewStringValue(DWORD type);

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	static bool CompareItems(const ListItem& i1, const ListItem& i2, int col, bool asc);

private:

	CEdit m_Edit;
	TreeNodeBase* m_CurrentNode{ nullptr };
	ITreeOperations* m_TreeOperations;
	IMainApp* m_App;
	std::vector<ListItem> m_Items;
	bool m_ViewKeys{ true };
};
