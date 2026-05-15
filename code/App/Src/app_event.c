

#include "app_event.h"

static App_Event s_queue[APP_EVENT_QUEUE_DEPTH];
static uint8_t   s_head;        /* 写入 */
static uint8_t   s_tail;        /* 读取 */
static uint8_t   s_count;

void App_Event_Init(void) {
    s_head  = 0U;
    s_tail  = 0U;
    s_count = 0U;
}

bool App_Event_IsEmpty(void) {
    return (s_count == 0U);
}


bool App_Event_Post(App_Event_Id id, uint32_t param) {
    if (s_count >= APP_EVENT_QUEUE_DEPTH) {
        return false;   /* 满则丢弃；保证按键扫描不卡死 */
    }
    s_queue[s_head].id    = id;
    s_queue[s_head].param = param;
    s_head = (uint8_t)((s_head + 1U) % APP_EVENT_QUEUE_DEPTH);
    s_count++;
    return true;
}

bool App_Event_Pop(App_Event *out) {
    if (s_count == 0U) {
        return false;
    }
    if (out != 0) {
        *out = s_queue[s_tail];
    }
    s_tail = (uint8_t)((s_tail + 1U) % APP_EVENT_QUEUE_DEPTH);
    s_count--;
    return true;
}

void App_Event_PostKey(const Bsp_Key_Event *kevt) {
    if (kevt == 0) return;
    App_Event_Id id = APP_EVENT_NONE;
    switch (kevt->type) {
        case BSP_KEY_EVT_SHORT:
            id = (App_Event_Id)(APP_EVENT_KEY_K1_SHORT + (uint32_t)kevt->id);
            break;
        case BSP_KEY_EVT_LONG:
            id = (App_Event_Id)(APP_EVENT_KEY_K1_LONG + (uint32_t)kevt->id);
            break;
        case BSP_KEY_EVT_HOLD:
            id = (App_Event_Id)(APP_EVENT_KEY_K1_HOLD + (uint32_t)kevt->id);
            break;
        default:
            return;
    }
    (void)App_Event_Post(id, 0U);
}
