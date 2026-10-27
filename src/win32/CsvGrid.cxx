// SPDX-License-Identifier: MIT
#define NOMINMAX 1
#include "CsvGrid.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <windowsx.h>

namespace {

constexpr wchar_t windowClassName[] = L"QingMoCsvGrid";
constexpr int maxRows = 200000;
constexpr int maxColumns = 2000;

struct State {
	HWND hwnd = nullptr;
	HFONT font = nullptr;
	int dpi = 96;
	int headerHeight = 26;
	int rowHeaderWidth = 56;
	int cellHeight = 24;
	int columnWidth = 128;

	std::vector<std::vector<std::wstring>> rows;
	int columnCount = 0;

	int topRow = 0;
	int leftColumn = 0;
	int selectedRow = 0;
	int selectedColumn = 0;
	bool hasFocus = false;
	bool dragging = false;
};

State *GetState(HWND hwnd) noexcept {
	return reinterpret_cast<State *>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

std::wstring Utf8ToWide(std::string_view text) {
	if (text.empty()) {
		return {};
	}
	const int length = ::MultiByteToWideChar(CP_UTF8, 0, text.data(),
		static_cast<int>(text.size()), nullptr, 0);
	if (length <= 0) {
		return {};
	}
	std::wstring result(static_cast<std::size_t>(length), L'\0');
	::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
		result.data(), length);
	return result;
}

std::wstring ColumnName(int index) {
	std::wstring name;
	int value = index + 1;
	while (value > 0) {
		const int remainder = (value - 1) % 26;
		name.insert(name.begin(), static_cast<wchar_t>(L'A' + remainder));
		value = (value - 1) / 26;
	}
	return name;
}

void ParseCsv(std::string_view text, State &state) {
	state.rows.clear();
	state.columnCount = 0;

	if (text.starts_with("\xEF\xBB\xBF")) {
		text.remove_prefix(3);
	}

	std::vector<std::wstring> row;
	std::string field;
	bool inQuotes = false;
	bool fieldStarted = false;
	bool lastWasNewline = false;

	const auto appendField = [&] {
		std::wstring value = Utf8ToWide(field);
		// A grid cell is a single line; keep embedded CSV newlines visible as spaces.
		for (wchar_t &ch : value) {
			if (ch == L'\r' || ch == L'\n' || ch == L'\t') {
				ch = L' ';
			}
		}
		row.push_back(std::move(value));
		field.clear();
		fieldStarted = false;
	};

	const auto appendRow = [&] {
		if (!fieldStarted && row.empty()) {
			return;
		}
		appendField();
		state.columnCount = std::max(state.columnCount, static_cast<int>(row.size()));
		if (state.rows.size() < static_cast<std::size_t>(maxRows)) {
			state.rows.push_back(std::move(row));
		}
		row.clear();
	};

	for (std::size_t i = 0; i < text.size(); ++i) {
		const char ch = text[i];
		if (inQuotes) {
			if (ch == '"') {
				if (i + 1 < text.size() && text[i + 1] == '"') {
					field.push_back('"');
					++i;
				} else {
					inQuotes = false;
				}
			} else {
				field.push_back(ch);
			}
			fieldStarted = true;
			continue;
		}
		if (ch == '"' && field.empty()) {
			inQuotes = true;
			fieldStarted = true;
			continue;
		}
		if (ch == ',') {
			appendField();
			lastWasNewline = false;
			continue;
		}
		if (ch == '\r' || ch == '\n') {
			if (ch == '\r' && i + 1 < text.size() && text[i + 1] == '\n') {
				++i;
			}
			appendRow();
			lastWasNewline = true;
			continue;
		}
		field.push_back(ch);
		fieldStarted = true;
		lastWasNewline = false;
	}

	if (inQuotes || fieldStarted || !row.empty() || !lastWasNewline) {
		appendRow();
	}

	if (state.columnCount > maxColumns) {
		state.columnCount = maxColumns;
		for (std::vector<std::wstring> &r : state.rows) {
			if (r.size() > static_cast<std::size_t>(maxColumns)) {
				r.resize(maxColumns);
			}
		}
	}
	if (state.rows.empty()) {
		state.rows.push_back({L""});
		state.columnCount = 1;
	}
}

void UpdateMetrics(State &state) {
	HDC hdc = ::GetDC(state.hwnd);
	int dpi = 96;
	if (hdc) {
		dpi = ::GetDeviceCaps(hdc, LOGPIXELSX);
		::ReleaseDC(state.hwnd, hdc);
	}
	if (dpi <= 0) {
		dpi = 96;
	}
	state.dpi = dpi;
	state.headerHeight = ::MulDiv(26, dpi, 96);
	state.rowHeaderWidth = ::MulDiv(56, dpi, 96);
	state.cellHeight = ::MulDiv(24, dpi, 96);
	state.columnWidth = ::MulDiv(128, dpi, 96);

	if (state.font) {
		::DeleteObject(state.font);
		state.font = nullptr;
	}
	LOGFONTW lf {};
	lf.lfHeight = -::MulDiv(9, dpi, 72);
	lf.lfWeight = FW_NORMAL;
	lf.lfCharSet = DEFAULT_CHARSET;
	lf.lfQuality = CLEARTYPE_QUALITY;
	lstrcpynW(lf.lfFaceName, L"Segoe UI", LF_FACESIZE);
	state.font = ::CreateFontIndirectW(&lf);
}

int VisibleRows(const State &state) {
	RECT rc {};
	::GetClientRect(state.hwnd, &rc);
	const int height = rc.bottom - rc.top - state.headerHeight;
	return std::max(1, height / std::max(1, state.cellHeight));
}

int VisibleColumns(const State &state) {
	RECT rc {};
	::GetClientRect(state.hwnd, &rc);
	const int width = rc.right - rc.left - state.rowHeaderWidth;
	return std::max(1, width / std::max(1, state.columnWidth));
}

void UpdateScrollBars(State &state) {
	const int rowCount = static_cast<int>(state.rows.size());
	SCROLLINFO si {};
	si.cbSize = sizeof(si);
	si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
	si.nMin = 0;
	si.nMax = std::max(0, rowCount - 1);
	si.nPage = VisibleRows(state);
	si.nPos = state.topRow;
	::SetScrollInfo(state.hwnd, SB_VERT, &si, TRUE);

	si.nMin = 0;
	si.nMax = std::max(0, state.columnCount - 1);
	si.nPage = VisibleColumns(state);
	si.nPos = state.leftColumn;
	::SetScrollInfo(state.hwnd, SB_HORZ, &si, TRUE);
}

void ClampScroll(State &state) {
	const int rowCount = static_cast<int>(state.rows.size());
	const int maxTop = std::max(0, rowCount - VisibleRows(state));
	state.topRow = std::clamp(state.topRow, 0, maxTop);

	const int maxLeft = std::max(0, state.columnCount - VisibleColumns(state));
	state.leftColumn = std::clamp(state.leftColumn, 0, maxLeft);
}

void EnsureSelectionVisible(State &state) {
	const int rowCount = static_cast<int>(state.rows.size());
	if (rowCount == 0 || state.columnCount == 0) {
		return;
	}
	const int visibleRows = VisibleRows(state);
	if (state.selectedRow < state.topRow) {
		state.topRow = state.selectedRow;
	} else if (state.selectedRow >= state.topRow + visibleRows) {
		state.topRow = state.selectedRow - visibleRows + 1;
	}
	const int visibleColumns = VisibleColumns(state);
	if (state.selectedColumn < state.leftColumn) {
		state.leftColumn = state.selectedColumn;
	} else if (state.selectedColumn >= state.leftColumn + visibleColumns) {
		state.leftColumn = state.selectedColumn - visibleColumns + 1;
	}
	ClampScroll(state);
	UpdateScrollBars(state);
}

bool CellAt(const State &state, POINT pt, int &row, int &column) {
	RECT rc {};
	::GetClientRect(state.hwnd, &rc);
	if (pt.x < state.rowHeaderWidth || pt.y < state.headerHeight) {
		return false;
	}
	const int visibleRow = (pt.y - state.headerHeight) / std::max(1, state.cellHeight);
	const int visibleColumn = (pt.x - state.rowHeaderWidth) / std::max(1, state.columnWidth);
	row = state.topRow + visibleRow;
	column = state.leftColumn + visibleColumn;
	return row >= 0 && row < static_cast<int>(state.rows.size()) &&
	       column >= 0 && column < state.columnCount &&
	       pt.x < rc.right && pt.y < rc.bottom;
}

void ScrollVertical(State &state, int code) {
	SCROLLINFO si {};
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	::GetScrollInfo(state.hwnd, SB_VERT, &si);
	int position = si.nPos;
	switch (code) {
	case SB_TOP:
		position = si.nMin;
		break;
	case SB_BOTTOM:
		position = si.nMax;
		break;
	case SB_LINEUP:
		--position;
		break;
	case SB_LINEDOWN:
		++position;
		break;
	case SB_PAGEUP:
		position -= static_cast<int>(si.nPage);
		break;
	case SB_PAGEDOWN:
		position += static_cast<int>(si.nPage);
		break;
	case SB_THUMBTRACK:
	case SB_THUMBPOSITION:
		position = si.nTrackPos;
		break;
	default:
		return;
	}
	state.topRow = position;
	ClampScroll(state);
	UpdateScrollBars(state);
	::InvalidateRect(state.hwnd, nullptr, FALSE);
}

void ScrollHorizontal(State &state, int code) {
	SCROLLINFO si {};
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	::GetScrollInfo(state.hwnd, SB_HORZ, &si);
	int position = si.nPos;
	switch (code) {
	case SB_LEFT:
		position = si.nMin;
		break;
	case SB_RIGHT:
		position = si.nMax;
		break;
	case SB_LINELEFT:
		--position;
		break;
	case SB_LINERIGHT:
		++position;
		break;
	case SB_PAGELEFT:
		position -= static_cast<int>(si.nPage);
		break;
	case SB_PAGERIGHT:
		position += static_cast<int>(si.nPage);
		break;
	case SB_THUMBTRACK:
	case SB_THUMBPOSITION:
		position = si.nTrackPos;
		break;
	default:
		return;
	}
	state.leftColumn = position;
	ClampScroll(state);
	UpdateScrollBars(state);
	::InvalidateRect(state.hwnd, nullptr, FALSE);
}

void SelectCell(State &state, int row, int column) {
	if (row < 0 || row >= static_cast<int>(state.rows.size()) ||
	    column < 0 || column >= state.columnCount) {
		return;
	}
	state.selectedRow = row;
	state.selectedColumn = column;
	EnsureSelectionVisible(state);
	::InvalidateRect(state.hwnd, nullptr, FALSE);
}

void PaintGrid(HWND hwnd, State &state) {
	PAINTSTRUCT ps {};
	HDC screen = ::BeginPaint(hwnd, &ps);
	RECT client {};
	::GetClientRect(hwnd, &client);
	const int width = client.right - client.left;
	const int height = client.bottom - client.top;
	if (width <= 0 || height <= 0) {
		::EndPaint(hwnd, &ps);
		return;
	}

	HDC buffer = ::CreateCompatibleDC(screen);
	HBITMAP bitmap = ::CreateCompatibleBitmap(screen, width, height);
	HBITMAP oldBitmap = static_cast<HBITMAP>(::SelectObject(buffer, bitmap));

	const COLORREF background = ::GetSysColor(COLOR_WINDOW);
	const COLORREF headerBackground = ::GetSysColor(COLOR_BTNFACE);
	const COLORREF textColour = ::GetSysColor(COLOR_WINDOWTEXT);
	const COLORREF gridColour = RGB(218, 218, 218);
	const COLORREF selectedBackground = RGB(226, 239, 255);
	const COLORREF selectedBorder = RGB(0, 120, 215);

	HBRUSH backgroundBrush = ::CreateSolidBrush(background);
	HBRUSH headerBrush = ::CreateSolidBrush(headerBackground);
	HBRUSH selectedBrush = ::CreateSolidBrush(selectedBackground);
	HPEN gridPen = ::CreatePen(PS_SOLID, 1, gridColour);
	HPEN selectedPen = ::CreatePen(PS_SOLID, std::max(1, ::MulDiv(2, state.dpi, 96)), selectedBorder);

	::FillRect(buffer, &client, backgroundBrush);
	const RECT topHeader = {0, 0, width, state.headerHeight};
	const RECT leftHeader = {0, state.headerHeight, state.rowHeaderWidth, height};
	const RECT corner = {0, 0, state.rowHeaderWidth, state.headerHeight};
	::FillRect(buffer, &topHeader, headerBrush);
	::FillRect(buffer, &leftHeader, headerBrush);
	::FillRect(buffer, &corner, headerBrush);

	HFONT oldFont = static_cast<HFONT>(::SelectObject(buffer, state.font));
	::SetBkMode(buffer, TRANSPARENT);
	::SetTextColor(buffer, textColour);

	const int rowCount = static_cast<int>(state.rows.size());
	const int visibleRows = std::max(0, (height - state.headerHeight) / std::max(1, state.cellHeight));
	const int visibleColumns = std::max(0, (width - state.rowHeaderWidth) / std::max(1, state.columnWidth));

	for (int column = state.leftColumn;
	     column < std::min(state.columnCount, state.leftColumn + visibleColumns + 1); ++column) {
		const int x = state.rowHeaderWidth + (column - state.leftColumn) * state.columnWidth;
		const RECT cell = {x, 0, x + state.columnWidth, state.headerHeight};
		if (column == state.selectedColumn) {
			const RECT selectedHeader = {x + 1, 1, x + state.columnWidth - 1, state.headerHeight - 1};
			::FillRect(buffer, &selectedHeader, selectedBrush);
		}
		std::wstring name = ColumnName(column);
		::DrawTextW(buffer, name.c_str(), -1, const_cast<RECT *>(&cell),
			DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
	}

	for (int row = state.topRow;
	     row < std::min(rowCount, state.topRow + visibleRows + 1); ++row) {
		const int y = state.headerHeight + (row - state.topRow) * state.cellHeight;
		const RECT cell = {0, y, state.rowHeaderWidth, y + state.cellHeight};
		if (row == state.selectedRow) {
			const RECT selectedHeader = {1, y + 1, state.rowHeaderWidth - 1, y + state.cellHeight - 1};
			::FillRect(buffer, &selectedHeader, selectedBrush);
		}
		std::wstring number = std::to_wstring(row + 1);
		::DrawTextW(buffer, number.c_str(), -1, const_cast<RECT *>(&cell),
			DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
	}

	for (int row = state.topRow; row < std::min(rowCount, state.topRow + visibleRows); ++row) {
		const int y = state.headerHeight + (row - state.topRow) * state.cellHeight;
		for (int column = state.leftColumn;
		     column < std::min(state.columnCount, state.leftColumn + visibleColumns); ++column) {
			const int x = state.rowHeaderWidth + (column - state.leftColumn) * state.columnWidth;
			const RECT cell = {x, y, x + state.columnWidth, y + state.cellHeight};
			const bool selected = row == state.selectedRow && column == state.selectedColumn;
			if (selected) {
				::FillRect(buffer, &cell, selectedBrush);
			}
			const std::wstring *value = nullptr;
			const std::wstring empty;
			if (row < static_cast<int>(state.rows.size()) &&
			    column < static_cast<int>(state.rows[row].size())) {
				value = &state.rows[row][column];
			} else {
				value = &empty;
			}
			RECT textRect = cell;
			textRect.left += ::MulDiv(6, state.dpi, 96);
			textRect.right -= ::MulDiv(5, state.dpi, 96);
			::DrawTextW(buffer, value->c_str(), -1, &textRect,
				DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
		}
	}

	HPEN oldPen = static_cast<HPEN>(::SelectObject(buffer, gridPen));
	for (int column = state.leftColumn;
	     column <= std::min(state.columnCount, state.leftColumn + visibleColumns); ++column) {
		const int x = state.rowHeaderWidth + (column - state.leftColumn) * state.columnWidth;
		::MoveToEx(buffer, x, 0, nullptr);
		::LineTo(buffer, x, height);
	}
	for (int row = state.topRow; row <= std::min(rowCount, state.topRow + visibleRows); ++row) {
		const int y = state.headerHeight + (row - state.topRow) * state.cellHeight;
		::MoveToEx(buffer, 0, y, nullptr);
		::LineTo(buffer, width, y);
	}
	::SelectObject(buffer, oldPen);

	if (rowCount == 0 || state.columnCount == 0) {
		RECT emptyRect = {state.rowHeaderWidth, state.headerHeight, width, height};
		::DrawTextW(buffer, L"No data", -1, &emptyRect,
			DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
	} else {
		const int selectedX = state.rowHeaderWidth + (state.selectedColumn - state.leftColumn) * state.columnWidth;
		const int selectedY = state.headerHeight + (state.selectedRow - state.topRow) * state.cellHeight;
		if (selectedX >= state.rowHeaderWidth && selectedY >= state.headerHeight &&
		    selectedX < width && selectedY < height) {
			HPEN previous = static_cast<HPEN>(::SelectObject(buffer, selectedPen));
			HBRUSH previousBrush = static_cast<HBRUSH>(::SelectObject(buffer, ::GetStockObject(NULL_BRUSH)));
			::Rectangle(buffer, selectedX, selectedY, selectedX + state.columnWidth, selectedY + state.cellHeight);
			::SelectObject(buffer, previousBrush);
			::SelectObject(buffer, previous);
		}
	}

	::SelectObject(buffer, oldFont);
	::BitBlt(screen, 0, 0, width, height, buffer, 0, 0, SRCCOPY);

	::DeleteObject(selectedPen);
	::DeleteObject(gridPen);
	::DeleteObject(selectedBrush);
	::DeleteObject(headerBrush);
	::DeleteObject(backgroundBrush);
	::SelectObject(buffer, oldBitmap);
	::DeleteObject(bitmap);
	::DeleteDC(buffer);
	::EndPaint(hwnd, &ps);
}

LRESULT CALLBACK GridWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	State *state = GetState(hwnd);

	switch (message) {
	case WM_NCCREATE: {
		auto *created = new State();
		created->hwnd = hwnd;
		::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
		return TRUE;
	}

	case WM_CREATE:
		if (state) {
			UpdateMetrics(*state);
			UpdateScrollBars(*state);
		}
		return 0;

	case WM_NCDESTROY:
		if (state) {
			if (state->font) {
				::DeleteObject(state->font);
			}
			::SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
			delete state;
		}
		return 0;

	case WM_SIZE:
		if (state) {
			UpdateScrollBars(*state);
		}
		::InvalidateRect(hwnd, nullptr, FALSE);
		return 0;

	case WM_ERASEBKGND:
		return 1;

	case WM_PAINT:
		if (state) {
			PaintGrid(hwnd, *state);
		}
		return 0;

	case WM_VSCROLL:
		if (state) {
			ScrollVertical(*state, LOWORD(wParam));
		}
		return 0;

	case WM_HSCROLL:
		if (state) {
			ScrollHorizontal(*state, LOWORD(wParam));
		}
		return 0;

	case WM_MOUSEWHEEL:
		if (state) {
			const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
			const int lines = -delta / WHEEL_DELTA * 3;
			state->topRow += lines;
			ClampScroll(*state);
			UpdateScrollBars(*state);
			::InvalidateRect(hwnd, nullptr, FALSE);
		}
		return 0;

	case WM_LBUTTONDOWN:
		if (state) {
			::SetFocus(hwnd);
			POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
			int row = 0;
			int column = 0;
			if (CellAt(*state, pt, row, column)) {
				SelectCell(*state, row, column);
				state->dragging = true;
				::SetCapture(hwnd);
			}
		}
		return 0;

	case WM_MOUSEMOVE:
		if (state && state->dragging) {
			POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
			int row = 0;
			int column = 0;
			if (CellAt(*state, pt, row, column)) {
				SelectCell(*state, row, column);
			}
		}
		return 0;

	case WM_LBUTTONUP:
		if (state) {
			state->dragging = false;
			if (::GetCapture() == hwnd) {
				::ReleaseCapture();
			}
		}
		return 0;

	case WM_SETFOCUS:
		if (state) {
			state->hasFocus = true;
			::InvalidateRect(hwnd, nullptr, FALSE);
		}
		return 0;

	case WM_KILLFOCUS:
		if (state) {
			state->hasFocus = false;
			::InvalidateRect(hwnd, nullptr, FALSE);
		}
		return 0;

	case WM_GETDLGCODE:
		return DLGC_WANTARROWS;

	case WM_KEYDOWN:
		if (state) {
			const int rowCount = static_cast<int>(state->rows.size());
			const bool control = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
			int row = state->selectedRow;
			int column = state->selectedColumn;
			switch (wParam) {
			case VK_LEFT:
				--column;
				break;
			case VK_RIGHT:
				++column;
				break;
			case VK_UP:
				--row;
				break;
			case VK_DOWN:
				++row;
				break;
			case VK_PRIOR:
				row -= VisibleRows(*state);
				break;
			case VK_NEXT:
				row += VisibleRows(*state);
				break;
			case VK_HOME:
				if (control) {
					row = 0;
				}
				column = 0;
				break;
			case VK_END:
				if (control) {
					row = rowCount - 1;
				}
				column = std::max(0, state->columnCount - 1);
				break;
			default:
				return 0;
			}
			row = std::clamp(row, 0, std::max(0, rowCount - 1));
			column = std::clamp(column, 0, std::max(0, state->columnCount - 1));
			SelectCell(*state, row, column);
		}
		return 0;

	case WM_DPICHANGED:
		if (state) {
			UpdateMetrics(*state);
			UpdateScrollBars(*state);
			::InvalidateRect(hwnd, nullptr, TRUE);
		}
		return 0;

	default:
		break;
	}

	return ::DefWindowProcW(hwnd, message, wParam, lParam);
}

}

bool CsvGrid::Register(HINSTANCE hInstance) {
	WNDCLASSEXW wc {};
	wc.cbSize = sizeof(wc);
	wc.style = CS_DBLCLKS;
	wc.lpfnWndProc = GridWndProc;
	wc.hInstance = hInstance;
	wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
	wc.lpszClassName = windowClassName;
	if (::RegisterClassExW(&wc)) {
		return true;
	}
	return ::GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

HWND CsvGrid::Create(HWND parent, HINSTANCE hInstance, int id) {
	if (!Register(hInstance)) {
		return nullptr;
	}
	return ::CreateWindowExW(
		0,
		windowClassName,
		L"CSV Data",
		WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VSCROLL | WS_HSCROLL | WS_TABSTOP,
		0, 0, 0, 0,
		parent,
		reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
		hInstance,
		nullptr);
}

void CsvGrid::SetText(HWND hwnd, std::string_view utf8) {
	State *state = GetState(hwnd);
	if (!state) {
		return;
	}
	ParseCsv(utf8, *state);
	state->topRow = 0;
	state->leftColumn = 0;
	state->selectedRow = 0;
	state->selectedColumn = 0;
	UpdateScrollBars(*state);
	::InvalidateRect(hwnd, nullptr, TRUE);
}
