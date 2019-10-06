// View.cpp : implementation of the CView class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"
#include "View.h"
#include "TreeNodes.h"
#include "Internals.h"
#include "UICommon.h"
#include "IntValueDlg.h"
#include "ChangeValueCommand.h"
#include "StringValueDlg.h"
#include "RenameValueCommand.h"
#include "MultiStringValueDlg.h"
#include "CreateNewValueCommand.h"
#include "BinaryValueDlg.h"

#pragma comment(lib, "ntdll")

BOOL CView::PreTranslateMessage(MSG* pMsg) {
	pMsg;
	return FALSE;
}

PCWSTR CView::GetRegTypeAsString(DWORD type) {
	switch (type) {
		case REG_SZ: return L"REG_SZ";
		case REG_DWORD: return L"REG_DWORD";
		case REG_MULTI_SZ: return L"REG_MULTI_SZ";
		case REG_QWORD: return L"REG_QDWORD";
		case REG_EXPAND_SZ: return L"REG_EXPAND_SZ";
		case REG_NONE: return L"REG_NONE";
		case REG_LINK: return L"REG_LINK";
		case REG_BINARY: return L"REG_BINARY";
		case REG_RESOURCE_REQUIREMENTS_LIST: return L"REG_RESOURCE_REQUIREMENTS_LIST";
		case REG_RESOURCE_LIST: return L"REG_RESOURCE_LIST";
		case REG_FULL_RESOURCE_DESCRIPTOR: return L"REG_FULL_RESOURCE_DESCRIPTOR";
	}
	return L"Unknown";
}

int CView::GetRegTypeIcon(DWORD type) {
	switch (type) {
		case REG_SZ:
		case REG_MULTI_SZ:
		case REG_EXPAND_SZ:
			return 2;
	}
	return 3;
}

CString CView::GetKeyDetails(TreeNodeBase* node) {
	CString text;
	if (node->GetNodeType() == TreeNodeType::RegistryKey) {
		auto keyNode = static_cast<RegKeyTreeNode*>(node);
		BYTE buffer[1024];
		auto info = reinterpret_cast<KEY_FULL_INFORMATION*>(buffer);
		ULONG len;
		auto status = ::NtQueryKey(*keyNode->GetKey(), KeyFullInformation, info, sizeof(buffer), &len);
		if (NT_SUCCESS(status)) {
			text.Format(L"Subkeys: %d  Values: %d\n", info->SubKeys, info->Values);
		}
	}
	return text;
}

CString CView::GetDataAsString(const ListItem& item) {
	ATLASSERT(m_CurrentNode && m_CurrentNode->GetNodeType() == TreeNodeType::RegistryKey);
	auto regNode = static_cast<RegKeyTreeNode*>(m_CurrentNode);

	ULONG realsize = item.ValueSize;
	ULONG size = (realsize > (1 << 12) ? (1 << 12) : realsize) / sizeof(WCHAR);
	LRESULT status;
	CString text;

	switch (item.ValueType) {
		case REG_SZ:
		case REG_EXPAND_SZ:
			text.Preallocate(size + 1);
			status = regNode->GetKey()->QueryStringValue(item.ValueName, text.GetBuffer(), &size);
			break;

		case REG_MULTI_SZ:
			text.Preallocate(size + 1);
			status = regNode->GetKey()->QueryMultiStringValue(item.ValueName, text.GetBuffer(), &size);
			if (status == ERROR_SUCCESS) {
				auto p = text.GetBuffer();
				while (*p) {
					p += ::wcslen(p);
					*p = L' ';
					p++;
				}
			}

			break;

		case REG_DWORD:
		{
			DWORD value;
			if (ERROR_SUCCESS == regNode->GetKey()->QueryDWORDValue(item.ValueName, value)) {
				text.Format(L"0x%08X (%u)", value, value);
			}
			break;
		}

		case REG_QWORD:
		{
			ULONGLONG value;
			if (ERROR_SUCCESS == regNode->GetKey()->QueryQWORDValue(item.ValueName, value)) {
				auto fmt = value < (1LL << 32) ? L"0x%08llX (%llu)" : L"0x%016llX (%llu)";
				text.Format(fmt, value, value);
			}
			break;
		}

		case REG_BINARY:
			CString digit;
			auto buffer = std::make_unique<BYTE[]>(item.ValueSize);
			if (buffer == nullptr)
				break;
			ULONG bytes = item.ValueSize;
			auto status = regNode->GetKey()->QueryBinaryValue(item.ValueName, buffer.get(), &bytes);
			if (status == ERROR_SUCCESS) {
				for (DWORD i = 0; i < min(bytes, 64); i++) {
					digit.Format(L"%02X ", buffer[i]);
					text += digit;
				}
			}
			break;
	}

	return text.GetLength() < 1024 ? text : text.Mid(0, 1024);
}

bool CView::CanDeleteSelected() const {
	if (GetSelectedCount() == 0)
		return false;

	auto& item = GetItem(GetSelectedIndex());
	if (item.TreeNode == nullptr)
		return true;
	return item.TreeNode->CanDelete();
}

bool CView::CanEditValue() const {
	auto selected = GetSelectedIndex();
	if (selected < 0)
		return false;

	return GetItem(selected).TreeNode == nullptr;
}

ListItem & CView::GetItem(int index) {
	ATLASSERT(index >= 0 && index < m_Items.size());
	return m_Items[index];
}

const ListItem & CView::GetItem(int index) const {
	ATLASSERT(index >= 0 && index < m_Items.size());
	return m_Items[index];
}

bool CView::IsViewKeys() const {
	return m_ViewKeys;
}

void CView::Update(TreeNodeBase* node, bool ifTheSame) {
	if (node == nullptr)
		node = m_CurrentNode;
	if (ifTheSame && m_CurrentNode != node)
		return;

	auto currentSelected = GetSelectedIndex();

	m_CurrentNode = node;
	node->Expand(true);
	auto& nodes = node->GetChildNodes();
	m_Items.clear();

	// add up dir
	if (m_CurrentNode->GetParent()) {
		ListItem item;
		item.TreeNode = m_CurrentNode->GetParent();
		item.UpDir = true;
		m_Items.push_back(item);
	}

	if (nodes.empty()) {
		SetItemCount(static_cast<int>(m_Items.size()));
		//return;
	}

	m_Items.reserve(nodes.size() + 64);
	auto buffer = std::make_unique<BYTE[]>(1 << 12);
	auto info = reinterpret_cast<KEY_FULL_INFORMATION*>(buffer.get());
	if (m_ViewKeys) {
		for (auto& node : nodes) {
			ListItem item;
			item.TreeNode = node;
			if (node->GetNodeType() == TreeNodeType::RegistryKey) {
				auto trueNode = static_cast<RegKeyTreeNode*>(node);
				auto key = trueNode->GetKey();
				if (key) {
					ULONG len;
					auto status = ::NtQueryKey(key->m_hKey, KeyFullInformation, info, 1 << 12, &len);
					if (NT_SUCCESS(status))
						item.LastWriteTime = info->LastWriteTime;
				}
			}
			m_Items.emplace_back(item);
		}
	}
	if (m_CurrentNode->GetNodeType() == TreeNodeType::RegistryKey) {
		auto trueNode = static_cast<RegKeyTreeNode*>(m_CurrentNode);
		auto key = trueNode->GetKey();
		if (key && key->m_hKey) {
			WCHAR valuename[256];
			DWORD valueType, size = 0, nameLen;
			for (int index = -1; ; ++index) {
				nameLen = 256;
				if (ERROR_NO_MORE_ITEMS == ::RegEnumValue(key->m_hKey, index >= 0 ? index : 0,
					valuename, &nameLen, nullptr, &valueType, nullptr, &size)) {
					if (index >= 0)
						break;
				}
				if (index < 0 && *valuename != L'\0') {
					// add a default value
					valuename[0] = L'\0';
					valueType = REG_SZ;
					size = 0;
				}
				else if(index < 0) {
					index++;
				}
				ListItem item;
				item.ValueName = valuename;
				item.ValueType = valueType;
				item.ValueSize = size;
				m_Items.push_back(item);
			}
		}
	}
	int count = static_cast<int>(m_Items.size());
	SetItemCount(count);
	RedrawItems(0, min(count, GetCountPerPage()));
	if (ifTheSame && currentSelected >= 0)
		SelectItem(currentSelected);
}

void CView::Init(ITreeOperations* to, IMainApp* app) {
	m_TreeOperations = to;
	m_App = app;
}

void CView::GoToItem(ListItem & item) {
}

LRESULT CView::OnGetDispInfo(int, LPNMHDR nmhdr, BOOL&) {
	ATLASSERT(m_CurrentNode);

	auto lv = reinterpret_cast<NMLVDISPINFO*>(nmhdr);
	auto& item = lv->item;
	auto index = item.iItem;
	auto col = item.iSubItem;
	auto& data = GetItem(index);

	if (lv->item.mask & LVIF_TEXT) {
		switch (col) {
			case 0:	// name
				if (data.TreeNode) {
					if (data.UpDir)
						item.pszText = L"..";
					else
						item.pszText = (PWSTR)(PCWSTR)data.TreeNode->GetText();
				}
				else
					item.pszText = *data.ValueName == L'\0' ? L"(Default)" : (PWSTR)(PCWSTR)data.ValueName;
				break;

			case 1:	// type
				if (data.TreeNode)
					item.pszText = L"Key";
				else {
					::wcscpy_s(item.pszText, item.cchTextMax, GetRegTypeAsString(data.ValueType));
				}
				break;

			case 2:	// size
				if (data.TreeNode == nullptr) {
					::StringCchPrintf(item.pszText, item.cchTextMax, L"%u", data.ValueSize);
				}
				break;

			case 3:	// data
				if (data.TreeNode == nullptr)
					::StringCchCopy(item.pszText, item.cchTextMax, GetDataAsString(data));
				break;

			case 5:	// last write
				if (data.TreeNode && data.LastWriteTime.QuadPart > 0) {
					CTime dt((FILETIME&)data.LastWriteTime);
					::StringCchCopy(item.pszText, item.cchTextMax, dt.Format(L"%c"));
				}
				break;

			case 4:	// details
				if (data.TreeNode && !data.UpDir)
					::StringCchCopy(item.pszText, item.cchTextMax, GetKeyDetails(data.TreeNode));
				break;
		}
	}
	if (lv->item.mask & LVIF_IMAGE) {
		if (data.TreeNode)
			item.iImage = data.UpDir ? 5 : 0;
		else
			item.iImage = GetRegTypeIcon(data.ValueType);
	}

	return 0;
}

LRESULT CView::OnFindItem(int, LPNMHDR hdr, BOOL &) {
	auto fi = (NMLVFINDITEM*)hdr;

	auto count = GetItemCount();
	auto str = fi->lvfi.psz;
	for (int i = fi->iStart; i < fi->iStart + count; i++) {
		auto& item = m_Items[i % count];
		if (item.TreeNode && ::_wcsnicmp(item.TreeNode->GetText(), str, ::wcslen(str)) == 0)
			return i % count;
		else if (item.TreeNode == nullptr && ::_wcsnicmp(item.ValueName, str, ::wcslen(str)) == 0)
			return i % count;
	}

	return -1;
}

LRESULT CView::OnRightClick(int, LPNMHDR hdr, BOOL &) {
	auto lv = (NMITEMACTIVATE*)hdr;
	CMenu menu;
	menu.LoadMenuW(IDR_CONTEXT);
	CPoint pt;
	::GetCursorPos(&pt);
	int index = lv->iItem < 0 ? 1 : 2;
	m_App->TrackPopupMenu(menu.GetSubMenu(index), pt.x, pt.y);

	return 0;
}

LRESULT CView::OnDoubleClick(int, LPNMHDR nmhdr, BOOL& handled) {
	auto lv = reinterpret_cast<NMITEMACTIVATE*>(nmhdr);
	if (m_Items.empty())
		return 0;

	auto& item = GetItem(lv->iItem);
	if (item.TreeNode) {
		if (item.UpDir)
			m_TreeOperations->SelectNode(m_CurrentNode->GetParent(), nullptr);
		else
			// key
			m_TreeOperations->SelectNode(m_CurrentNode, item.TreeNode->GetText());
	}
	else {
		return OnModifyValue(0, 0, nullptr, handled);
	}

	return 0;
}

LRESULT CView::OnReturnKey(int, LPNMHDR, BOOL& handled) {
	int selected = GetSelectedIndex();
	auto& item = GetItem(selected);
	if (item.TreeNode) {
		if (item.UpDir)
			m_TreeOperations->SelectNode(m_CurrentNode->GetParent(), nullptr);
		else
			// key
			m_TreeOperations->SelectNode(m_CurrentNode, item.TreeNode->GetText());
	}
	else {
		return OnModifyValue(0, 0, nullptr, handled);
	}
	return 0;
}

LRESULT CView::OnContextMenu(UINT, WPARAM, LPARAM lParam, BOOL &) {
	CPoint pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
	CPoint client(pt);
	ScreenToClient(&client);

	UINT flags;
	int index = HitTest(client, &flags);
	CMenu menu;
	menu.LoadMenu(IDR_CONTEXT);
	m_App->TrackPopupMenu(menu.GetSubMenu(index < 0 ? 1 : 2), pt.x, pt.y);
	return 0;
}

LRESULT CView::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL&) {
	LRESULT lRet = DefWindowProc(uMsg, wParam, lParam);

	SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP);

	using PGetDpiForWindow = UINT(__stdcall*)(HWND);
	static PGetDpiForWindow pGetDpiForWindow = (PGetDpiForWindow)::GetProcAddress(::GetModuleHandle(L"user32"), "GetDpiForWindow");
	auto dpi = pGetDpiForWindow ? pGetDpiForWindow(m_hWnd) : 96;
	InsertColumn(0, L"Name", LVCFMT_LEFT, 300 * dpi / 96);
	InsertColumn(1, L"Type", LVCFMT_LEFT, 100 * dpi / 96);
	InsertColumn(2, L"Size (Bytes)", LVCFMT_RIGHT, 100 * dpi / 96);
	InsertColumn(3, L"Value", LVCFMT_LEFT, 230 * dpi / 96);
	InsertColumn(4, L"Details", LVCFMT_LEFT, 200 * dpi / 96);
	InsertColumn(5, L"Last Write", LVCFMT_LEFT, 150 * dpi / 96);

	return 0;
}

LRESULT CView::OnDelete(WORD, WORD, HWND, BOOL&) {
	return LRESULT();
}

LRESULT CView::OnEditRename(WORD, WORD, HWND, BOOL&) {
	EditLabel(GetSelectedIndex());

	return 0;
}

LRESULT CView::OnModifyValue(WORD, WORD, HWND, BOOL &) {
	auto selected = GetSelectedIndex();
	ATLASSERT(selected >= 0);
	auto& item = GetItem(selected);
	ATLASSERT(item.TreeNode == nullptr);
	ATLASSERT(m_CurrentNode->GetNodeType() == TreeNodeType::RegistryKey);

	auto regNode = static_cast<RegKeyTreeNode*>(m_CurrentNode);
	auto key = regNode->GetKey();

	switch (item.ValueType) {
		case REG_DWORD:
		case REG_QWORD:
		{
			CIntValueDlg dlg(m_App->IsAllowModify());
			ULONGLONG value = 0;
			auto error = (item.ValueType == REG_DWORD) ? key->QueryDWORDValue(item.ValueName, (DWORD&)value) : key->QueryQWORDValue(item.ValueName, value);
			if (error != ERROR_SUCCESS) {
				m_App->ShowCommandError(L"Failed to read value");
				return 0;
			}
			dlg.SetValue(value);
			dlg.SetName(item.ValueName, true);
			if (dlg.DoModal() == IDOK && value != dlg.GetRealValue()) {
				auto cmd = std::make_shared<ChangeValueCommand<ULONGLONG>>(m_CurrentNode->GetFullPath(), item.ValueName, dlg.GetRealValue(), item.ValueType);
				if (!m_App->AddCommand(cmd))
					m_App->ShowCommandError(L"Failed to change value");
			}
			break;
		}

		case REG_SZ:
		case REG_EXPAND_SZ:
		{
			CStringValueDlg dlg(m_App->IsAllowModify());
			dlg.SetName(item.ValueName, true);
			auto currentType = item.ValueType == REG_SZ ? 0 : 1;
			dlg.SetType(currentType);
			WCHAR value[2048] = { 0 };
			ULONG chars = 2048;
			auto error = key->QueryStringValue(item.ValueName, value, &chars);
			if (error != ERROR_SUCCESS && !item.ValueName.IsEmpty()) {
				m_App->ShowCommandError(L"Failed to read value");
				return 0;
			}
			dlg.SetValue(value);
			if (dlg.DoModal() == IDOK && (dlg.GetValue() != value || dlg.GetType() != currentType)) {
				auto type = dlg.GetType() == 0 ? REG_SZ : REG_EXPAND_SZ;
				auto cmd = std::make_shared<ChangeValueCommand<CString>>(m_CurrentNode->GetFullPath(), item.ValueName, dlg.GetValue(), type);
				if (!m_App->AddCommand(cmd))
					m_App->ShowCommandError(L"Failed to change value");
				else {
					item.ValueType = type;
					item.ValueSize = (1 + dlg.GetValue().GetLength()) * sizeof(WCHAR);
				}
			}
			break;
		}

		case REG_MULTI_SZ:
		{
			ULONG chars = item.ValueSize / sizeof(WCHAR);
			auto buffer = std::make_unique<WCHAR[]>(chars);
			if (ERROR_SUCCESS != key->QueryMultiStringValue(item.ValueName, buffer.get(), &chars)) {
				m_App->ShowCommandError(L"Failed to read value. Try Refreshing");
				break;
			}
			for (size_t i = 0; i < chars - 1; i++)
				if (buffer[i] == L'\0')
					buffer[i] = L'\n';

			CMultiStringValueDlg dlg(m_App->IsAllowModify());
			auto value = CString(buffer.get());
			value.Replace(L"\n", L"\r\n");
			dlg.SetName(item.ValueName, true);
			dlg.SetValue(value);
			if (dlg.DoModal() == IDOK) {
				value = dlg.GetValue();
				value.TrimLeft(L"\r\n");
				value.Replace(L"\r\n", L"\x01");
				auto buffer = value.GetBuffer();
				for (int i = 0; i < value.GetLength(); i++)
					if (buffer[i] == 1) {
						buffer[i] = 0;
					}

				auto cmd = std::make_shared<ChangeValueCommand<CString>>(m_CurrentNode->GetFullPath(), item.ValueName, value, item.ValueType);
				if (!m_App->AddCommand(cmd))
					m_App->ShowCommandError(L"Failed to change value");
				else
					item.ValueSize = (1 + dlg.GetValue().GetLength()) * sizeof(WCHAR);
			}
			break;
		}

		case REG_BINARY:
		{
			auto data = std::make_unique<BYTE[]>(item.ValueSize);
			ATLASSERT(data);
			ULONG size = item.ValueSize;
			if (ERROR_SUCCESS != key->QueryBinaryValue(item.ValueName, data.get(), &size)) {
				m_App->ShowCommandError(L"Failed to read binary value");
				break;
			}
			CBinaryValueDlg dlg(m_App->IsAllowModify());
			InMemoryBuffer buffer;
			buffer.SetData(0, data.get(), item.ValueSize);
			dlg.SetBuffer(&buffer);
			dlg.SetName(item.ValueName, true);
			if (dlg.DoModal() == IDOK) {
				BinaryValue value;
				value.Size = static_cast<DWORD>(buffer.GetSize());
				value.Buffer = std::make_unique<BYTE[]>(buffer.GetSize());
				::memcpy(value.Buffer.get(), buffer.GetRawData(0), value.Size);
				auto cmd = std::make_shared<ChangeValueCommand<BinaryValue>>(m_CurrentNode->GetFullPath(), item.ValueName, value);
				if (m_App->AddCommand(cmd))
					item.ValueSize = static_cast<DWORD>(buffer.GetSize());
				else
					m_App->ShowCommandError(L"Failed to change binary value");
			}
			break;
		}

		default:
			ATLASSERT(false);
			break;
	}

	RedrawItems(selected, selected);

	return 0;
}

LRESULT CView::OnBeginRename(int, LPNMHDR, BOOL &) {
	if (!m_App->IsAllowModify())
		return TRUE;

	m_Edit = GetEditControl();
	ATLASSERT(m_Edit.IsWindow());
	return 0;
}

LRESULT CView::OnEndRename(int, LPNMHDR, BOOL &) {
	ATLASSERT(m_Edit.IsWindow());
	CString newName;
	m_Edit.GetWindowText(newName);

	int index = GetSelectedIndex();
	auto& item = GetItem(index);
	if (newName.CompareNoCase(item.ValueName) == 0)
		return 0;

	auto cmd = std::make_shared<RenameValueCommand>(m_CurrentNode->GetFullPath(), item.ValueName, newName);
	if (!m_App->AddCommand(cmd))
		m_App->ShowCommandError(L"Failed to rename value");
	else {
		item.ValueName = newName;
		RedrawItems(index, index);
	}
	return 0;
}

LRESULT CView::OnNewDwordValue(WORD, WORD, HWND, BOOL &) {
	return HandleNewIntValue(4);
}

LRESULT CView::OnNewQwordValue(WORD, WORD, HWND, BOOL &) {
	return HandleNewIntValue(8);
}

LRESULT CView::OnNewStringValue(WORD, WORD, HWND, BOOL &) {
	return HandleNewStringValue(REG_SZ);
}

LRESULT CView::OnNewExpandStringValue(WORD, WORD, HWND, BOOL &) {
	return HandleNewStringValue(REG_EXPAND_SZ);
}

LRESULT CView::OnNewMultiStringValue(WORD, WORD, HWND, BOOL &) {
	ATLASSERT(m_App->IsAllowModify());
	CMultiStringValueDlg dlg(true);
	dlg.SetName(L"", false);
	if (dlg.DoModal() == IDOK) {
		auto cmd = std::make_shared<CreateNewValueCommand<CString>>(m_CurrentNode->GetFullPath(),
			dlg.GetName(), dlg.GetValue(), REG_MULTI_SZ);
		if (!m_App->AddCommand(cmd))
			m_App->ShowCommandError(L"Failed to create value");
		else {
			auto index = FindItem(dlg.GetName(), false, true);
			ATLASSERT(index >= 0);
			SelectItem(index);
		}
	}

	return 0;
}

LRESULT CView::OnNewBinaryValue(WORD, WORD, HWND, BOOL &) {
	CBinaryValueDlg dlg(true);
	dlg.SetName(L"", false);
	InMemoryBuffer buffer;
	dlg.SetBuffer(&buffer);
	if (dlg.DoModal() == IDOK) {
		BinaryValue value;
		value.Size = buffer.GetSize();
		value.Buffer = std::make_unique<BYTE[]>(value.Size);
		buffer.GetData(0, value.Buffer.get(), value.Size);
		auto cmd = std::make_shared<CreateNewValueCommand<BinaryValue>>(m_CurrentNode->GetFullPath(),
			dlg.GetName(), value, REG_BINARY);
		if (!m_App->AddCommand(cmd))
			m_App->ShowCommandError(L"Failed to create value");
		else {
			auto index = FindItem(dlg.GetName(), false, true);
			ATLASSERT(index >= 0);
			SelectItem(index);
		}
	}
	return 0;
}

LRESULT CView::OnViewKeys(WORD, WORD, HWND, BOOL&) {
	m_ViewKeys = !m_ViewKeys;
	Update(m_CurrentNode, true);

	return 0;
}

LRESULT CView::OnChangeViewType(WORD, WORD id, HWND, BOOL &) {
	DWORD type;
	switch (id) {
	case ID_VIEW_TYPE_DETAILS: type = LV_VIEW_DETAILS; break;
	case ID_VIEW_TYPE_LIST: type = LV_VIEW_LIST; break;
	case ID_VIEW_TYPE_ICONS: type = LV_VIEW_ICON; break;
	case ID_VIEW_TYPE_TILES: type = LV_VIEW_TILE; break;

	default: 
		ATLASSERT(false);
	}

	SetView(type);

	return 0;
}

LRESULT CView::HandleNewIntValue(int size) {
	ATLASSERT(size == 4 || size == 8);

	CIntValueDlg dlg(true);
	dlg.SetValue(0);
	dlg.SetName(L"", false);
	if (dlg.DoModal() == IDOK) {
		if (m_CurrentNode->FindChild(dlg.GetName())) {
			m_App->ShowCommandError(L"Value name already exists");
			return 0;
		}

		auto cmd = std::make_shared<CreateNewValueCommand<ULONGLONG>>(m_CurrentNode->GetFullPath(), 
			dlg.GetName(), dlg.GetRealValue(), size == 4 ? REG_DWORD : REG_QWORD);
		if (!m_App->AddCommand(cmd))
			m_App->ShowCommandError(L"Failed to create value");
		else {
			auto index = FindItem(dlg.GetName(), false, true);
			ATLASSERT(index >= 0);
			SelectItem(index);
		}
	}

	return 0;
}

LRESULT CView::HandleNewStringValue(DWORD type) {
	ATLASSERT(type == REG_SZ || type == REG_EXPAND_SZ);

	CStringValueDlg dlg(true);
	dlg.SetName(L"", false);
	dlg.SetType(type == REG_SZ ? 0 : 1);
	if (dlg.DoModal() == IDOK) {
		if (m_CurrentNode->FindChild(dlg.GetName())) {
			m_App->ShowCommandError(L"Value name already exists");
			return 0;
		}

		auto cmd = std::make_shared<CreateNewValueCommand<CString>>(m_CurrentNode->GetFullPath(),
			dlg.GetName(), dlg.GetValue(), dlg.GetType() == 0 ? REG_SZ : REG_EXPAND_SZ);
		if (!m_App->AddCommand(cmd))
			m_App->ShowCommandError(L"Failed to create value");
		else {
			auto index = FindItem(dlg.GetName(), false, true);
			ATLASSERT(index >= 0);
			SelectItem(index);
		}

	}

	return 0;
}
