#pragma once
#define STB_TEXTEDIT_IMPLEMENTATION
#include <stddef.h>
#define STB_TEXTEDIT_CHARTYPE char
#define STB_TEXTEDIT_POSITIONTYPE int
#define STB_TEXTEDIT_UNDOSTATECOUNT 99
#define STB_TEXTEDIT_UNDOCHARCOUNT 999
typedef struct { int dummy; } STB_TexteditState;
