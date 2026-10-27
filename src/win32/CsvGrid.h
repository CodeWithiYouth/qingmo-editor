// SPDX-License-Identifier: MIT
#ifndef CSVGRID_H
#define CSVGRID_H

#include <windows.h>
#include <string>
#include <string_view>

namespace CsvGrid {

bool Register(HINSTANCE hInstance);
HWND Create(HWND parent, HINSTANCE hInstance, int id);
void SetText(HWND hwnd, std::string_view utf8);

}

#endif
