// Puente de eventos SDK -> handles del kernel Linux de NX2Linux.
#pragma once

#include "nxrt.h"

#include <stdint.h>

// Pide al SDK recompilado que inicialice el SystemEventType en la dirección
// guest indicada y devuelve su handle de escritura para notificaciones host.
// La dirección debe señalar memoria válida del título con tamaño/alineación
// requeridos por la versión de su SDK.
int nx_hle_event_create(Ctx *ctx, uint64_t system_event, uint32_t clear_mode,
                        int inter_process, uint32_t *write_handle,
                        uint32_t *sdk_result);
