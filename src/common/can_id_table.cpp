#include "can_id_table.h"

bool CanIdTable::note(const CanFrame &f, uint32_t nowMs) {
    const uint8_t dlc = f.dlc > 8 ? 8 : f.dlc;
    CanIdEntry *e = nullptr;
    for (uint16_t i = 0; i < count_; ++i) {
        if (entries_[i].id == f.id && entries_[i].ext == f.extended) {
            e = &entries_[i];
            break;
        }
    }

    bool changed = false;
    if (!e) {
        if (count_ >= kMax) {
            full_ = true;
            return true;
        }
        e = &entries_[count_++];
        memset(e, 0, sizeof(*e));
        e->id = f.id;
        e->ext = f.extended;
        changed = true;
    } else {
        for (uint8_t b = 0; b < dlc; ++b) {
            if (b >= e->dlc || e->data[b] != f.data[b]) {
                e->changedMs[b] = nowMs ? nowMs : 1;
                changed = true;
            }
        }
        if (dlc != e->dlc) changed = true;
    }
    e->dlc = dlc;
    memcpy(e->data, f.data, dlc);
    e->count++;
    e->lastMs = nowMs;
    return changed;
}

void CanIdTable::clear() {
    count_ = 0;
    full_ = false;
}
