#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct termios orig;
char *filename = "output.txt";

void disableRaw() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
}

void enableRaw() {
    tcgetattr(STDIN_FILENO, &orig);
    atexit(disableRaw);
    struct termios raw = orig;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void refreshScreen(char *buf, int len, int pos) {
    // Ekranı temizle ve metni bas
    write(STDOUT_FILENO, "\x1b[2J\x1b[H", 7);
    write(STDOUT_FILENO, buf, len);
    
    // İmleç koordinat hesaplama
    int row = 1, col = 1;
    for (int i = 0; i < pos; i++) {
        if (buf[i] == '\n') {
            row++;
            col = 1;
        } else {
            // UTF-8 karakterlerin ekranda tek sütun kaplamasını sağla
            // (10xxxxxx ile başlayan byte'lar ekranda yer kaplamaz)
            if ((buf[i] & 0xc0) != 0x80) col++;
        }
    }
    char move[32];
    sprintf(move, "\x1b[%d;%dH", row, col);
    write(STDOUT_FILENO, move, strlen(move));
}

int main(int argc, char *argv[]) {
    char buf[8192] = {0};
    int len = 0;

    if (argc > 1) {
        filename = argv[1];
        FILE *f_load = fopen(filename, "r");
        if (f_load) {
            len = fread(buf, 1, sizeof(buf) - 1, f_load);
            buf[len] = '\0';
            fclose(f_load);
        }
    }

    int pos = len; 
    enableRaw();
    refreshScreen(buf, len, pos);

    while (1) {
        unsigned char c = 0;
        if (read(STDIN_FILENO, &c, 1) == 0) continue;
        if (c == 17) break; // CTRL+Q

        if (c == 27) { // Ok Tuşları
            unsigned char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) && read(STDIN_FILENO, &seq[1], 1)) {
                if (seq[0] == '[') {
                    if (seq[1] == 'C' && pos < len) {
                        pos++;
                        // Eğer çoklu byte karakterin ortasındaysak sonuna kadar atla
                        while (pos < len && (buf[pos] & 0xc0) == 0x80) pos++;
                    }
                    if (seq[1] == 'D' && pos > 0) {
                        pos--;
                        // Çoklu byte karakterin başına kadar geri git
                        while (pos > 0 && (buf[pos] & 0xc0) == 0x80) pos--;
                    }
                }
            }
        } 
        else if (c == 127 || c == 8) { // BACKSPACE DÜZELTMESİ
            if (pos > 0) {
                int start = pos;
                // UTF-8 karakterin başlangıç byte'ını bulana kadar geri git
                do {
                    pos--;
                } while (pos > 0 && (buf[pos] & 0xc0) == 0x80);
                
                int bytes_to_delete = start - pos;
                memmove(&buf[pos], &buf[start], len - start);
                len -= bytes_to_delete;
                buf[len] = '\0';
            }
        } 
        else if (c == '\r' || c == '\n') {
            memmove(&buf[pos + 1], &buf[pos], len - pos);
            buf[pos] = '\n';
            len++; pos++;
        } 
        else if (c >= 32 || c > 127) { // Karakter ekleme
            if (len < 8191) {
                memmove(&buf[pos + 1], &buf[pos], len - pos);
                buf[pos] = c;
                len++; pos++;
            }
        }
        refreshScreen(buf, len, pos);
    }

    disableRaw();
    FILE *f_save = fopen(filename, "w");
    if (f_save) { fwrite(buf, 1, len, f_save); fclose(f_save); }
    return 0;
}
