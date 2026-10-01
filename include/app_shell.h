#ifndef APP_SHELL_H
#define APP_SHELL_H

#include "display.h"
#include "feature.h"
#include "input_event.h"

class AppShell {
public:
    bool begin(Display& display);
    void handleInput(InputEvent event);
    void tick(unsigned long now_ms);

private:
    void switchTo(size_t index);
    void drawActiveIfDirty();

    Display* _display = nullptr;
    Feature** _features = nullptr;
    size_t _feature_count = 0;
    size_t _active_index = 0;
    bool _needs_draw = false;
    unsigned long _nav_cooldown_until_ms = 0;
};

#endif // APP_SHELL_H
