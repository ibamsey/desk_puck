#ifndef FEATURE_H
#define FEATURE_H

#include <Arduino.h>
#include "input_event.h"
#include "LGFX_config.h"

class Feature {
public:
    virtual ~Feature() = default;

    virtual const char* name() const = 0;

    virtual void onEnter() {}
    virtual void onExit() {}

    virtual void onTick(unsigned long now_ms) { (void)now_ms; }

    virtual void onDraw(lgfx::LGFX_Device& gfx) = 0;

    /** @return true if the event was consumed */
    virtual bool onInput(InputEvent event) {
        (void)event;
        return false;
    }

    virtual bool capturesNavigation() const { return false; }

    void setDirty() { _dirty = true; }
    bool isDirty() const { return _dirty; }
    void clearDirty() { _dirty = false; }

protected:
    bool _dirty = false;
};

#endif // FEATURE_H
