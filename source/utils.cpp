#include <3ds.h>
#include <cstdio>

namespace Utils {
    void GetSizeString(char *string, double size) {
        int i = 0;
        const char *units[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"};
        
        while (size >= 1024.f) {
            size /= 1024.f;
            i++;
        }
        
        std::sprintf(string, "%.*f %s", (i == 0) ? 0 : 2, size, units[i]);
    }

    void SetBounds(int *set, int min, int max) {
        if (*set > max)
            *set = min;
        else if (*set < min)
            *set = max;
    }
    
    void SetMax(int *set, int value, int max) {
        if (*set > max)
            *set = value;
    }
    
    void SetMin(int *set, int value, int min) {
        if (*set < min)
            *set = value;
    }

    bool IsCancelButtonPressed(void) {
        hidScanInput();

        if (hidKeysDown() & KEY_B)
            return true;
        
        return false;
    }
}
