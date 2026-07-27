/*
 * liveboot.c – LiveBoot log viewer
 * Hiển thị dmesg và logcat trên màn hình khi boot
 * Build: gcc -static -O2 -o liveboot liveboot.c
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>
#include <signal.h>

#define MAX_LINE 1024
#define MAX_BUFFER 65536

/* Cấu hình mặc định */
typedef struct {
    int dark_mode;
    char logcat_levels[16];
    char logcat_buffers[16];
    char logcat_format[16];
    int logcat_no_colors;
    char dmesg_range[32];
    int lines;
    int wordwrap;
    int fallback_width;
    int fallback_height;
} Config;

Config config = {
    .dark_mode = 1,
    .logcat_levels = "WEFS",
    .logcat_buffers = "C",
    .logcat_format = "brief",
    .logcat_no_colors = 1,
    .dmesg_range = "0--1",
    .lines = 80,
    .wordwrap = 1,
    .fallback_width = 1080,
    .fallback_height = 1920
};

/* Đọc config từ file */
static void load_config(const char *path) {
    FILE *fp;
    char line[MAX_LINE];
    char key[MAX_LINE], value[MAX_LINE];
    
    fp = fopen(path, "r");
    if (!fp) return;
    
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n\r")] = '\0';
        if (line[0] == '#' || line[0] == '\0') continue;
        
        if (sscanf(line, "%[^=]=%s", key, value) == 2) {
            if (strcmp(key, "dark") == 0) {
                config.dark_mode = 1;
            } else if (strcmp(key, "logcatlevels") == 0) {
                strncpy(config.logcat_levels, value, sizeof(config.logcat_levels) - 1);
            } else if (strcmp(key, "logcatbuffers") == 0) {
                strncpy(config.logcat_buffers, value, sizeof(config.logcat_buffers) - 1);
            } else if (strcmp(key, "logcatformat") == 0) {
                strncpy(config.logcat_format, value, sizeof(config.logcat_format) - 1);
            } else if (strcmp(key, "logcatnocolors") == 0) {
                config.logcat_no_colors = 1;
            } else if (strcmp(key, "dmesg") == 0) {
                strncpy(config.dmesg_range, value, sizeof(config.dmesg_range) - 1);
            } else if (strcmp(key, "lines") == 0) {
                config.lines = atoi(value);
            } else if (strcmp(key, "wordwrap") == 0) {
                config.wordwrap = 1;
            } else if (strcmp(key, "fallbackwidth") == 0) {
                config.fallback_width = atoi(value);
            } else if (strcmp(key, "fallbackheight") == 0) {
                config.fallback_height = atoi(value);
            }
        }
    }
    fclose(fp);
}

/* Lấy kích thước màn hình */
static void get_screen_size(int *width, int *height) {
    FILE *fp;
    char line[MAX_LINE];
    int w = config.fallback_width;
    int h = config.fallback_height;
    
    fp = popen("wm size 2>/dev/null", "r");
    if (fp) {
        if (fgets(line, sizeof(line), fp)) {
            char *p = strchr(line, 'x');
            if (p) {
                char *start = strchr(line, ':');
                if (start) {
                    start++;
                    while (*start == ' ') start++;
                    *p = '\0';
                    w = atoi(start);
                    h = atoi(p + 1);
                    if (w > 0 && h > 0) {
                        *width = w;
                        *height = h;
                        pclose(fp);
                        return;
                    }
                }
            }
        }
        pclose(fp);
    }
    
    *width = w;
    *height = h;
}

/* Ghi log ra framebuffer hoặc stdout */
static void print_log(const char *log, int width, int height) {
    // Đơn giản: in ra stdout (sẽ được chuyển hướng qua daemonize)
    // Trong thực tế, có thể ghi trực tiếp vào framebuffer
    
    // Nếu có file /dev/graphics/fb0, có thể ghi trực tiếp
    // Ở đây dùng stdout để đơn giản
    printf("%s", log);
    fflush(stdout);
}

/* Lấy dmesg */
static void get_dmesg(char *buffer, size_t size) {
    FILE *fp;
    char line[MAX_LINE];
    size_t total = 0;
    
    // Parse dmesg range
    int start = 0, end = -1;
    char *dash = strchr(config.dmesg_range, '-');
    if (dash) {
        *dash = '\0';
        start = atoi(config.dmesg_range);
        end = atoi(dash + 1);
        *dash = '-';
    } else {
        start = atoi(config.dmesg_range);
    }
    
    fp = popen("dmesg 2>/dev/null", "r");
    if (!fp) {
        snprintf(buffer, size, "Cannot read dmesg\n");
        return;
    }
    
    int line_num = 0;
    while (fgets(line, sizeof(line), fp) && total < size - 1) {
        line_num++;
        if (start >= 0 && line_num < start) continue;
        if (end >= 0 && line_num > end) break;
        
        size_t len = strlen(line);
        if (total + len >= size - 1) break;
        strcpy(buffer + total, line);
        total += len;
    }
    pclose(fp);
}

/* Lấy logcat */
static void get_logcat(char *buffer, size_t size) {
    FILE *fp;
    char line[MAX_LINE];
    size_t total = 0;
    char cmd[MAX_LINE];
    
    snprintf(cmd, sizeof(cmd), 
             "logcat -b %s -v %s -d 2>/dev/null | head -n %d",
             config.logcat_buffers, config.logcat_format, config.lines);
    
    // Thêm filter levels nếu cần
    if (strlen(config.logcat_levels) > 0) {
        snprintf(cmd, sizeof(cmd),
                 "logcat -b %s -v %s -d *:%s 2>/dev/null | head -n %d",
                 config.logcat_buffers, config.logcat_format, 
                 config.logcat_levels, config.lines);
    }
    
    fp = popen(cmd, "r");
    if (!fp) {
        snprintf(buffer, size, "Cannot read logcat\n");
        return;
    }
    
    while (fgets(line, sizeof(line), fp) && total < size - 1) {
        size_t len = strlen(line);
        if (total + len >= size - 1) break;
        strcpy(buffer + total, line);
        total += len;
    }
    pclose(fp);
}

/* Main */
int main(int argc, char **argv) {
    char log_buffer[MAX_BUFFER];
    int width, height;
    char config_path[MAX_LINE];
    
    // Tìm file config
    if (argc > 1) {
        strncpy(config_path, argv[1], sizeof(config_path) - 1);
    } else {
        // Tìm trong module path
        const char *paths[] = {
            "/data/adb/modules/LiveBoot/config",
            "/data/adb/modules/liveboot/config",
            "/data/adb/modules/LiveBoot/loader.sh",
            NULL
        };
        for (int i = 0; paths[i]; i++) {
            if (access(paths[i], F_OK) == 0) {
                // Lấy thư mục chứa config
                strncpy(config_path, paths[i], sizeof(config_path) - 1);
                char *last_slash = strrchr(config_path, '/');
                if (last_slash) {
                    *(last_slash + 1) = '\0';
                    strcat(config_path, "config");
                }
                break;
            }
        }
    }
    
    // Load config
    load_config(config_path);
    
    // Get screen size
    get_screen_size(&width, &height);
    
    // Get logs
    log_buffer[0] = '\0';
    
    // Dmesg
    if (strlen(config.dmesg_range) > 0) {
        char dmesg_buf[MAX_BUFFER / 2];
        get_dmesg(dmesg_buf, sizeof(dmesg_buf));
        strcat(log_buffer, "========== DMESG ==========\n");
        strcat(log_buffer, dmesg_buf);
        strcat(log_buffer, "\n");
    }
    
    // Logcat
    if (config.lines > 0) {
        char logcat_buf[MAX_BUFFER / 2];
        get_logcat(logcat_buf, sizeof(logcat_buf));
        strcat(log_buffer, "========== LOGCAT ==========\n");
        strcat(log_buffer, logcat_buf);
    }
    
    // Print to framebuffer or stdout
    print_log(log_buffer, width, height);
    
    return 0;
}