#include <ncurses.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>

#define MAX_FILES 1024
#define MAX_FILENAME 255

void list_files(const char *path, char files[MAX_FILES][256], int *file_count) {
    DIR *dir = opendir(path);
    struct dirent *entry;
    *file_count = 0;
    if (!dir) return;
    while ((entry = readdir(dir)) != NULL && *file_count < MAX_FILES) {
        strncpy(files[*file_count], entry->d_name, 255);
        files[*file_count][255] = '\0';
        (*file_count)++;
    }
    closedir(dir);
}

int file_color(const char *dirpath, const char *filename) {
    char fullpath[1024];
    struct stat st;
    snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, filename);
    if (lstat(fullpath, &st) == 0) {
        if (S_ISDIR(st.st_mode))
            return 4;
        else if (S_ISLNK(st.st_mode))
            return 7;
        else if (st.st_mode & S_IXUSR)
            return 5;
        else if (S_ISREG(st.st_mode))
            return 6;
    }
    return 6;
}

void draw_gradient_box(int y, int x, int h, int w) {
    for (int i = 0; i < h; ++i) {
        int color = 1 + (i % 7);
        attron(COLOR_PAIR(color));
        mvhline(y + i, x, ' ', w);
        attroff(COLOR_PAIR(color));
    }
    attron(COLOR_PAIR(2) | A_BOLD);
    mvaddch(y, x, ACS_ULCORNER);
    mvaddch(y, x + w - 1, ACS_URCORNER);
    mvaddch(y + h - 1, x, ACS_LLCORNER);
    mvaddch(y + h - 1, x + w - 1, ACS_LRCORNER);
    mvhline(y, x + 1, ACS_HLINE, w - 2);
    mvhline(y + h - 1, x + 1, ACS_HLINE, w - 2);
    mvvline(y + 1, x, ACS_VLINE, h - 2);
    mvvline(y + 1, x + w - 1, ACS_VLINE, h - 2);
    attroff(COLOR_PAIR(2) | A_BOLD);
}

int main() {
    char cwd[1024];
    char files[MAX_FILES][256];
    int file_count = 0, highlight = 0;

    if (!getcwd(cwd, sizeof(cwd))) return 1;

    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    nodelay(stdscr, FALSE);

    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(1, COLOR_BLACK, COLOR_CYAN);
        init_pair(2, COLOR_YELLOW, COLOR_BLUE);
        init_pair(3, COLOR_GREEN, -1);
        init_pair(4, COLOR_BLUE, -1);
        init_pair(5, COLOR_RED, -1);
        init_pair(6, COLOR_WHITE, -1);
        init_pair(7, COLOR_MAGENTA, -1);
        init_pair(8, COLOR_CYAN, -1);
        init_pair(9, COLOR_MAGENTA, COLOR_CYAN);
    }

    while (1) {
        clear();

        int height = LINES - 2;
        int width = COLS - 2;
        int split_x = COLS / 2;

        attron(COLOR_PAIR(2) | A_BOLD | A_REVERSE);
        for (int i = 1; i < COLS - 1; i++)
            mvaddch(1, i, ' ');
        mvprintw(1, 3, " Interm || %s ", cwd);
        attroff(COLOR_PAIR(2) | A_BOLD | A_REVERSE);

        attron(COLOR_PAIR(9));
        mvaddch(0, 0, ACS_DIAMOND);
        mvaddch(0, COLS - 1, ACS_DIAMOND);
        mvaddch(LINES - 1, 0, ACS_DIAMOND);
        mvaddch(LINES - 1, COLS - 1, ACS_DIAMOND);
        attroff(COLOR_PAIR(9));

        for (int i = 1; i < COLS - 1; i++) {
            attron(COLOR_PAIR(2));
            mvaddch(0, i, ACS_HLINE);
            mvaddch(LINES - 1, i, ACS_HLINE);
            attroff(COLOR_PAIR(2));
        }
        for (int i = 1; i < LINES - 1; i++) {
            attron(COLOR_PAIR(2));
            mvaddch(i, 0, ACS_VLINE);
            mvaddch(i, COLS - 1, ACS_VLINE);
            attroff(COLOR_PAIR(2));
        }
        box(stdscr, 0, 0);

        attron(COLOR_PAIR(8) | A_BOLD);
        mvprintw(3, 3, "Files: %d", file_count);
        attroff(COLOR_PAIR(8) | A_BOLD);

        list_files(cwd, files, &file_count);
        int max_display = height - 5;
        int start = 0;
        if (highlight >= max_display) start = highlight - max_display + 1;

        for (int i = 0; i < file_count && i < max_display; i++) {
            int idx = i + start;
            int color = file_color(cwd, files[idx]);
            if (idx == highlight) {
                attron(COLOR_PAIR(1) | A_BOLD | A_REVERSE);
                mvprintw(i + 4, 2, "  %-*s", split_x - 4, files[idx]);
                attroff(COLOR_PAIR(1) | A_BOLD | A_REVERSE);
            } else {
                attron(COLOR_PAIR(color));
                mvprintw(i + 4, 4, "%-*s", split_x - 6, files[idx]);
                attroff(COLOR_PAIR(color));
            }
        }

        attron(COLOR_PAIR(3) | A_BOLD);
        mvprintw(LINES - 2, 2, "↑/↓ or w/s: Move  Enter: Open Dir  e: Edit  n: New  d: Delete  q: Quit");
        attroff(COLOR_PAIR(3) | A_BOLD);

        refresh();

        int ch = getch();
        if (ch != ERR) {
            switch (ch) {
                case KEY_UP:
                case 'w':
                    if (highlight > 0) highlight--;
                    break;
                case KEY_DOWN:
                case 's':
                    if (highlight < file_count - 1) highlight++;
                    break;
                case 10: {
                    char selected_path[1024];
                    snprintf(selected_path, sizeof(selected_path), "%s/%s", cwd, files[highlight]);
                    struct stat st;
                    if (stat(selected_path, &st) == 0) {
                        if (S_ISDIR(st.st_mode)) {
                            DIR *dir = opendir(selected_path);
                            if (dir) {
                                closedir(dir);
                                strcpy(cwd, selected_path);
                                highlight = 0;
                            }
                        }
                    }
                    break;
                }
                case 'e': {
                    char selected_path[1024];
                    snprintf(selected_path, sizeof(selected_path), "%s/%s", cwd, files[highlight]);
                    struct stat st;
                    if (stat(selected_path, &st) == 0 && S_ISREG(st.st_mode)) {
                        const char *editor = getenv("EDITOR");
                        if (!editor) editor = "nano";
                        endwin();
                        char cmd[1200];
                        snprintf(cmd, sizeof(cmd), "%s \"%s\"", editor, selected_path);
                        system(cmd);
                        initscr();
                        noecho();
                        cbreak();
                        keypad(stdscr, TRUE);
                    }
                    break;
                }
                case 'n': {
                    echo();
                    curs_set(1);
                    char filename[256] = {0};
                    mvprintw(LINES - 3, 2, "New file name (with extension): ");
                    int y, x;
                    getyx(stdscr, y, x);
                    mvgetnstr(y, x, filename, sizeof(filename) - 1);
                    noecho();
                    curs_set(0);
                    if (filename[0] == '\0' || strchr(filename, '/') != NULL) {
                        mvprintw(LINES - 3, 2, "Invalid file name! Press any key...");
                        clrtoeol();
                        getch();
                        break;
                    }
                    char new_path[1024];
                    snprintf(new_path, sizeof(new_path), "%s/%s", cwd, filename);
                    FILE *fp = fopen(new_path, "w");
                    if (!fp) {
                        mvprintw(LINES - 3, 2, "Failed to create file! Press any key...");
                        clrtoeol();
                        getch();
                        break;
                    }
                    fclose(fp);
                    const char *editor = getenv("EDITOR");
                    if (!editor) editor = "nano";
                    endwin();
                    char cmd[1200];
                    snprintf(cmd, sizeof(cmd), "%s \"%s\"", editor, new_path);
                    system(cmd);
                    initscr();
                    noecho();
                    cbreak();
                    keypad(stdscr, TRUE);
                    break;
                }
                case 'd': {
                    char selected_path[1024];
                    snprintf(selected_path, sizeof(selected_path), "%s/%s", cwd, files[highlight]);
                    struct stat st;
                    if (stat(selected_path, &st) == 0 && S_ISREG(st.st_mode)) {
                        attron(COLOR_PAIR(5) | A_BOLD);
                        mvprintw(LINES - 3, 2, "Delete '%s'? This cannot be undone! (y/N): ", files[highlight]);
                        attroff(COLOR_PAIR(5) | A_BOLD);
                        int confirm = getch();
                        if (confirm == 'y' || confirm == 'Y') {
                            if (remove(selected_path) == 0) {
                                mvprintw(LINES - 3, 2, "File deleted. Press any key...");
                                clrtoeol();
                                getch();
                                if (highlight > 0) highlight--;
                            } else {
                                mvprintw(LINES - 3, 2, "Failed to delete file! Press any key...");
                                clrtoeol();
                                getch();
                            }
                        } else {
                            mvprintw(LINES - 3, 2, "Delete cancelled. Press any key...");
                            clrtoeol();
                            getch();
                        }
                    }
                    break;
                }
                case 'q':
                    endwin();
                    return 0;
            }
        }
    }
    endwin();
    return 0;
}
