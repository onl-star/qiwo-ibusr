#ifndef __IBUS_RIME_ENGINE_H__
#define __IBUS_RIME_ENGINE_H__

#include <ibus.h>

#define IBUS_TYPE_RIME_ENGINE \
        (ibus_rime_engine_get_type())

GType ibus_rime_engine_get_type();

void ibus_rime_sync_user_data(void);

#endif
