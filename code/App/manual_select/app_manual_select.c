/*
 * @File         : \code\App\manual_select\app_manual_select.c
 * @Description  : 手动模式入口选择实现
 */
#include "app_manual_select.h"

#include "app_display.h"
#include "app_log.h"

#include <string.h>

#define MANUAL_SELECT_ITEM_COUNT (3U)

static const char *s_items[MANUAL_SELECT_ITEM_COUNT] = {
    "UART Serial",
    "Key Control",
    "Return Boot",
};

typedef struct
{
    uint8_t               cursor;
    bool                  result_pending;
    App_ManualSelect_Result result;
} App_ManualSelect_Ctx;

static App_ManualSelect_Ctx s_ctx;

void App_ManualSelect_Init(void)
{
    memset(&s_ctx, 0, sizeof(s_ctx));
}

void App_ManualSelect_Reset(void)
{
    memset(&s_ctx, 0, sizeof(s_ctx));
}

void App_ManualSelect_Enter(void)
{
    s_ctx.cursor         = 0U;
    s_ctx.result_pending = false;
    s_ctx.result         = APP_MANUAL_SELECT_RESULT_NONE;
    LOG_INFO("manual select: enter");
    App_Display_MarkDirty();
}

uint8_t App_ManualSelect_GetCursor(void)
{
    return s_ctx.cursor;
}

bool App_ManualSelect_ResultPending(void)
{
    return s_ctx.result_pending;
}

App_ManualSelect_Result App_ManualSelect_ConsumeResult(void)
{
    App_ManualSelect_Result r = s_ctx.result;
    s_ctx.result_pending      = false;
    s_ctx.result              = APP_MANUAL_SELECT_RESULT_NONE;
    return r;
}

const char *App_ManualSelect_GetItemLabel(uint8_t idx)
{
    return (idx < MANUAL_SELECT_ITEM_COUNT) ? s_items[idx] : "";
}

uint8_t App_ManualSelect_GetItemCount(void)
{
    return MANUAL_SELECT_ITEM_COUNT;
}

void App_ManualSelect_OnEvent(const App_Event *e)
{
    if (e == NULL)
    {
        return;
    }

    switch (e->id)
    {
        case APP_EVENT_KEY_K1_SHORT:
            s_ctx.cursor = (uint8_t)((s_ctx.cursor + 1U) % MANUAL_SELECT_ITEM_COUNT);
            App_Display_MarkDirty();
            break;

        case APP_EVENT_KEY_K2_SHORT:
            if (s_ctx.cursor > 0U)
            {
                s_ctx.cursor--;
            }
            else
            {
                s_ctx.cursor = (uint8_t)(MANUAL_SELECT_ITEM_COUNT - 1U);
            }
            App_Display_MarkDirty();
            break;

        case APP_EVENT_KEY_K3_SHORT:
            s_ctx.result_pending = true;
            if (s_ctx.cursor == 0U)
            {
                s_ctx.result = APP_MANUAL_SELECT_RESULT_SERIAL;
            }
            else if (s_ctx.cursor == 1U)
            {
                s_ctx.result = APP_MANUAL_SELECT_RESULT_KEY;
            }
            else
            {
                s_ctx.result = APP_MANUAL_SELECT_RESULT_BOOT_WAIT;
            }
            LOG_INFO_WITH_ARG("manual select: pick %u", (unsigned)s_ctx.cursor);
            break;

        case APP_EVENT_KEY_K4_SHORT:
            s_ctx.result_pending = true;
            s_ctx.result         = APP_MANUAL_SELECT_RESULT_BOOT_WAIT;
            LOG_INFO("manual select: K4 -> boot wait");
            break;

        default:
            break;
    }
}
