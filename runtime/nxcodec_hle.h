// Sustituciones HLE de nn::codec para decodificación VP9 en Linux.
#pragma once

#include "nxrt.h"

void nxcodec_video_init(Ctx *c);
void nxcodec_video_finalize(Ctx *c);
void nxcodec_pool_preference(Ctx *c);
void nxcodec_pool_size(Ctx *c);
void nxcodec_pool_init(Ctx *c);
void nxcodec_pool_finalize(Ctx *c);
void nxcodec_vp9_work_size(Ctx *c);
void nxcodec_vp9_init(Ctx *c);
void nxcodec_vp9_init_no_work(Ctx *c);
void nxcodec_vp9_finalize(Ctx *c);
void nxcodec_vp9_register(Ctx *c);
void nxcodec_vp9_unregister(Ctx *c);
void nxcodec_vp9_start(Ctx *c);
void nxcodec_vp9_stop(Ctx *c);
void nxcodec_vp9_event(Ctx *c);
void nxcodec_vp9_submit(Ctx *c);
void nxcodec_vp9_submit_encrypted(Ctx *c);
void nxcodec_vp9_acquire(Ctx *c);
void nxcodec_vp9_release(Ctx *c);
void nxcodec_vp9_properties(Ctx *c);
void nxcodec_read_rgba(Ctx *c);
void nxcodec_read_bgra(Ctx *c);
void nxcodec_read_nv12(Ctx *c);
void nxcodec_try_wait_native_sync(Ctx *c);
void nxcodec_syncpt_create(Ctx *c);
void nxcodec_syncpt_init_holder(Ctx *c);
void nxcodec_syncpt_request(Ctx *c);
void nxcodec_syncpt_cancel(Ctx *c);
void nxcodec_syncpt_try_wait(Ctx *c);
void nxcodec_syncpt_destroy(Ctx *c);
