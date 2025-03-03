#ifndef MMM_TERMINAL_COLORS
#define MMM_TERMINAL_COLORS

#define ESC "\x1b"
#define CSI ESC"["
#define RESET CSI"0m"
#define BOLD CSI"1m"
#define COL_K "0"
#define COL_R "1"
#define COL_G "2"
#define COL_Y "3"
#define COL_B "4"
#define COL_M "5"
#define COL_C "6"
#define COL_W "7"
#define FG(X) CSI"3"COL_##X"m"
#define BG(X) CSI"4"COL_##X"m"
#define FG_BR(X) CSI"9"COL_##X"m"
#define BG_BR(X) CSI"10"COL_##X"m"

#endif  // MMM_TERMINAL_COLORS